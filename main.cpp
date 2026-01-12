#include "src/AudioConfig.hpp"
#include "src/CommandLine.hpp"
#include "src/InputHook.hpp"
#include "src/Logger.hpp"
#include "src/Paster.hpp"
#include "src/Recorder.hpp"
#include "src/StatusWindow.hpp"
#include "src/Transcriber.hpp"
#include <chrono>
#include <exception>
#include <format>
#include <iostream>
#include <thread>
#include <vector>
#include <cstdio>
#include <fstream>
#include <filesystem>
#include <csignal> // For std::signal
#include <fcntl.h> // For open, O_CREAT, O_TRUNC
#include <unistd.h> // For close, dprintf, fsync
#include <cstdio>   // For fdopen, popen, pclose, FILENO

#define BACKWARD_HAS_BFD 1 // Explicitly enable BFD backend for backward-cpp
#include "backward.hpp" // For crash reporting

// APP_VERSION is defined by the Makefile
// const char* APP_VERSION = "DEV_VERSION"; // REMOVED

// Global async-signal-safe crash report file descriptor and filename storage
static int g_crash_report_fd = -1;
static char g_crash_report_filename[256] = "\0"; // Initialize to empty string

// Asynchronous-safe crash handler
void crash_handler(int sig) {
    fflush(stderr); // Flush buffered stderr before async-safe dprintf

    dprintf(STDERR_FILENO, "\n!!! CRITICAL ERROR: VoiceCLI has crashed with signal %d !!!\n", sig);
    dprintf(STDERR_FILENO, "Stack trace details are in the crash report file.\n");

    if (g_crash_report_fd != -1) {
        FILE* crash_report_file = fdopen(g_crash_report_fd, "w"); // Create FILE* from descriptor
        if (crash_report_file) {
            // Version is written here ONLY. Removed from main startup.
            dprintf(fileno(crash_report_file), "VoiceCLI Version: %s\n", APP_VERSION); 
            dprintf(fileno(crash_report_file), "\n!!! CRITICAL ERROR: VoiceCLI has crashed with signal %d !!!\n", sig);
            dprintf(fileno(crash_report_file), "Stack trace:\n");

            backward::StackTrace st;
            st.load_here(32);
            backward::Printer p;
            p.snippet = false;
            p.object = false;
            p.address = false;
            p.color_mode = backward::ColorMode::always;
            p.print(st, crash_report_file); // Print trace directly to FILE*

            dprintf(fileno(crash_report_file), "\n!!! WARNING: This crash report (and voicecli.log) contains transcribed text !!!\n");
            dprintf(fileno(crash_report_file), "!!! Please review and redact any sensitive information before sharing. !!!\n");
            fflush(crash_report_file); // Ensure buffer is flushed
            fclose(crash_report_file); // Close the FILE* and its underlying descriptor
            g_crash_report_fd = -1;
        } else {
            // Fallback if fdopen fails
            dprintf(STDERR_FILENO, "FATAL: Could not create FILE* from crash report descriptor.\n");
        }
    }

    // Prominently warn about sensitive data in logs to stderr
    dprintf(STDERR_FILENO, "\n!!! WARNING: The crash report contains transcribed text !!!\n");
    dprintf(STDERR_FILENO, "!!! Please review and redact any sensitive information before sharing. !!!\n");
    dprintf(STDERR_FILENO, "A crash report has been saved to: %s\n", g_crash_report_filename);
    dprintf(STDERR_FILENO, "Please compress this file (e.g., zip) and send it for support.\n");

    _exit(EXIT_FAILURE);
}

/**
 * @brief Trims leading and trailing whitespace from a string.
 * @param s The string to trim.
 * @return The trimmed string.
 */
std::string trim(const std::string& s) {
  auto start = s.find_first_not_of(" \t\n\r");
  if (start == std::string::npos) return "";
  auto end = s.find_last_not_of(" \t\n\r");
  return s.substr(start, end - start + 1);
}

/**
 * @brief Runs an external shell command for text post-processing.
 * 
 * Pipes the input string into the command's stdin and captures its stdout.
 * 
 * @param cmd The shell command to execute.
 * @param input The text to process.
 * @return The processed text from command's stdout.
 */
std::string runPostProcess(const std::string& cmd, const std::string& input) {
    if (cmd.empty()) return input;

    // Write input to temp file
    std::string tempIn = "/tmp/voicecli_pp_in.txt";
    std::ofstream ofs(tempIn);
    if (!ofs) {
        Logger::instance().error("Failed to write post-process input file: " + tempIn);
        return input;
    }
    ofs << input;
    ofs.close();

    // Construct command: cmd < tempIn
    std::string fullCmd = cmd + " < " + tempIn;
    
    // Execute and read stdout
    std::string output;
    char buffer[128]; // Small buffer, handles line-by-line
    FILE* pipe = popen(fullCmd.c_str(), "r");
    if (!pipe) {
        Logger::instance().error("Failed to execute post-process command: " + fullCmd);
        std::filesystem::remove(tempIn);
        return input;
    }
    
    while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
        output += buffer;
    }
    pclose(pipe);
    
    // Cleanup
    std::filesystem::remove(tempIn);
    
    return trim(output);
}

/**
 * @brief Selects the desired audio device based on user preference or system default.
 * 
 * @param audio The AudioConfig instance to query devices from.
 * @param userIndex Optional user-specified device index.
 * @return The selected AudioDevice, or std::nullopt if none found.
 */
std::optional<AudioDevice> getSelectedDevice(AudioConfig& audio, std::optional<unsigned int> userIndex) {
  auto devices = audio.listCaptureDevices();
  
  if (userIndex) {
    for (const auto& dev : devices) {
      if (dev.index == *userIndex) return dev;
    }
    std::cerr << "Warning: Requested device index " << *userIndex << " not found. Falling back to default." << std::endl;
  }

  // Strict preference for System Default
  for (const auto& dev : devices) {
    if (dev.isDefault) {
      return dev;
    }
  }

  // Fallback if no default marked (rare)
  if (!devices.empty()) {
    return devices[0];
  }

  return std::nullopt;
}

/**
 * @brief Retrieves the X11 Window ID of the currently focused window.
 * 
 * @return The focused Window ID, or 0 on failure.
 */
Window getCurrentFocus() {
  Display* d = XOpenDisplay(NULL);
  if (!d) return 0;

  Window focus;
  int revert;
  XGetInputFocus(d, &focus, &revert);
  XCloseDisplay(d);
  return focus;
}

int main(int argc, char* argv[]) {
  // Generate timestamped filename for async-safe crash report
  auto now = std::chrono::system_clock::now();
  auto time_t_now = std::chrono::system_clock::to_time_t(now);
  std::tm tm = *std::localtime(&time_t_now); // Technically not async-signal-safe, but best-effort for naming
  std::strftime(g_crash_report_filename, sizeof(g_crash_report_filename), "CrashReport-%Y-%m-%d,%H:%M:%S.log", &tm); 

  // Open crash report file descriptor for async-signal-safe writes
  // O_CLOEXEC ensures the FD is closed on execve, which is generally good practice
  g_crash_report_fd = open(g_crash_report_filename, O_CREAT | O_TRUNC | O_WRONLY | O_CLOEXEC, 0644); 
  // Version is written here ONLY in the crash handler.
  // Removed redundant dprintf for version from main startup.

  // Register crash handler FIRST, before anything else that might crash
  std::signal(SIGSEGV, crash_handler);
  std::signal(SIGABRT, crash_handler);
  std::signal(SIGFPE,  crash_handler);
  std::signal(SIGILL,  crash_handler);
  std::signal(SIGBUS,  crash_handler);
  std::signal(SIGTERM, crash_handler);

  // Initialize Logger
  std::string homeDir = getenv("HOME");
  std::string logPath = homeDir + "/.VoiceCLI/voicecli.log";
  Logger::instance().setLogFile(logPath);
  Logger::instance().log("Application Started");

  // Log the command line arguments used to start the application
  std::string cmdLine;
  for (int i = 0; i < argc; ++i) {
    cmdLine += std::string(argv[i]) + " ";
  }
  Logger::instance().log("Command Line: " + cmdLine);

  // Removed try-catch from main, crash_handler will handle exceptions
  CommandLine cmd(argc, argv);
  const auto& config = cmd.getConfig();

  if (config.showHelp) {
    cmd.printHelp();
    return 0;
  }

  if (config.showVersion) {
    std::cout << "VoiceCLI Version: " << APP_VERSION << std::endl;
    return 0;
  }

  AudioConfig audio;
  std::string modelPath = config.modelPath; // Declared at broader scope

  if (config.listAudioDevices) {
    auto devices = audio.listCaptureDevices();
    std::cout << "--- Available Capture Devices ---" << std::endl;
    if (devices.empty()) {
      std::cout << "No capture devices found." << std::endl;
    } else {
      for (const auto& dev : devices) {
         std::string fmtStr;
         for(auto f : dev.supportedFormats) {
             switch(f) {
                 case ma_format_u8: fmtStr += "u8 "; break;
                 case ma_format_s16: fmtStr += "s16 "; break;
                 case ma_format_s24: fmtStr += "s24 "; break;
                 case ma_format_s32: fmtStr += "s32 "; break;
                 case ma_format_f32: fmtStr += "f32 "; break;
                 case ma_format_unknown: fmtStr += "Any "; break;
                 default: break;
             }
         }
         std::string chanStr;
         for(auto c : dev.supportedChannels) {
             if(c == 0) chanStr += "Any ";
             else chanStr += std::to_string(c) + " ";
         }
         
         std::cout << std::format("[{}{}] {} (Channels: [{}], Formats: [{}])", 
             dev.index, dev.isDefault ? "*" : "", dev.name, chanStr, fmtStr) << std::endl;
      }
    }
    return 0;
  }

  // Determine which device we WILL use
  auto selectedDevice = getSelectedDevice(audio, config.deviceIndex);

  if (config.verbose) {
    std::cout << "Log file: " << Logger::instance().getLogFilePath() << std::endl;
    if (selectedDevice) {
      std::string chanStr;
      for(auto c : selectedDevice->supportedChannels) {
          if(c == 0) chanStr += "Any ";
          else chanStr += std::to_string(c) + " ";
      }
      std::string fmtStr;
      for(auto f : selectedDevice->supportedFormats) {
           switch(f) {
               case ma_format_u8: fmtStr += "u8 "; break;
               case ma_format_s16: fmtStr += "s16 "; break;
               case ma_format_s24: fmtStr += "s24 "; break;
               case ma_format_s32: fmtStr += "s32 "; break;
               case ma_format_f32: fmtStr += "f32 "; break;
               case ma_format_unknown: fmtStr += "Any "; break;
               default: break;
           }
       }
      std::string msg = std::format("Selected Input Device: [{}] {} (Supported Channels: [{}], Formats: [{}])", 
          selectedDevice->index, selectedDevice->name, chanStr, fmtStr);
      std::cout << msg << std::endl;
      Logger::instance().log(msg);
    } else {
      std::cout << "Warning: No audio capture devices found!" << std::endl;
    }
    // Prominently warn about sensitive data in logs if verbose
    std::cout << "\n!!! WARNING: VoiceCLI logs contain transcribed text, which may include sensitive information. !!!" << std::endl;
    std::cout << "!!! Please review and redact 'voicecli.log' before sharing, especially crash reports. !!!\n" << std::endl;
  }

  if (!selectedDevice) {
    Logger::instance().error("No microphone available. Exiting.");
    return 1;
  }

  // --- Test Record Mode ---
  if (config.testRecord) {
    std::string outFile = "/tmp/voicecli_test.wav";

    if (config.verbose) {
      std::cout << "Starting 5-second test recording to " << outFile << "..." << std::endl;
    }
    Logger::instance().log("Starting test recording...");

    StatusWindow win;
    win.show("Initializing Recorder...");

    Recorder rec(audio.getCaptureDeviceID(selectedDevice->index), config.sampleRate);
    rec.start(outFile);

    for (int i = 50; i > 0; --i) {
      float timeRemaining = i * 0.1f;
      win.updateText(std::format("Recording... {:.1f}s", timeRemaining), rec.getCurrentLevel());
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    rec.stop();
    win.updateText("Transcribing...");
    if (config.verbose) {
      std::cout << "Recording complete. Transcribing..." << std::endl;
    }

    // Transcribe
    try {
      Transcriber transcriber(modelPath);
      std::string text = transcriber.transcribe(outFile);

      win.updateText("Done!");
      std::this_thread::sleep_for(std::chrono::milliseconds(500));

      if (config.verbose) {
          std::cout << "---------------------------------------------------" << std::endl;
          std::cout << "Transcription Result:" << std::endl;
          std::cout << text << std::endl;
          std::cout << "---------------------------------------------------" << std::endl;
      }

      Logger::instance().log("Transcription: " + text);

    } catch (const std::exception& e) {
      std::string err = std::format("Transcription Failed: {}", e.what());
      if (config.verbose) std::cerr << err << std::endl;
      Logger::instance().error(err);
    }

    return 0;
  }

  // Default behavior (No args): Run the Daemon
  if (config.verbose) {
    std::cout << "VoiceCLI Daemon starting..." << std::endl;
  }
  InputHook input;

  // Pre-load model to avoid delay on first record
  Logger::instance().log("Loading model: " + modelPath);
  Transcriber transcriber(modelPath);
  Logger::instance().log("Model loaded. Ready.");

  bool shouldExit = false;
  while (!shouldExit) {
    // 1. Wait for global trigger (Hotkeys)
    if (!input.monitor(config.triggerKey, config.verbose)) {
      break; // Stop if monitor fails
    }

    // Capture currently focused window before we take over
    Window activeWin = getCurrentFocus();
    Logger::instance().log(std::format("Captured Active Window ID: {}", activeWin));

    // 2. Setup Recording Session
    StatusWindow win;
    win.show("Starting Recording...");

    std::string tempFile = "/tmp/voicecli_rec.wav";
    Recorder rec(audio.getCaptureDeviceID(selectedDevice->index), config.sampleRate);

    try {
      rec.start(tempFile);
    } catch (const std::exception& e) {
      Logger::instance().error(std::format("Failed to start recorder: {}", e.what()));
      continue;
    }

    auto startTime = std::chrono::steady_clock::now();
    auto maxDuration = std::chrono::minutes(config.maxRecordTime);
    auto lastSpeechTime = std::chrono::steady_clock::now();
    bool isAutoPaused = false;
    bool finishAndTranscribe = false;
    bool appendSpace = true;
    bool useTerminalPaste = false;

    bool isPaused = false;
    bool isTimeout = false;
    std::chrono::steady_clock::duration totalPausedDuration = std::chrono::seconds(0);
    auto lastPauseStart = std::chrono::steady_clock::now();
    
    std::chrono::steady_clock::duration totalAutoPausedDuration = std::chrono::seconds(0);
    auto lastAutoPauseStart = std::chrono::steady_clock::now();

    // 3. Recording Loop
    while (true) {
      auto now = std::chrono::steady_clock::now();

      // VAD Logic (Smart Pause)
      float currentLevel = rec.getCurrentLevel();
      if (currentLevel > config.vadThreshold) {
           lastSpeechTime = now;
           if (isAutoPaused) {
               isAutoPaused = false;
               rec.setWriting(true);
               totalAutoPausedDuration += (now - lastPauseStart);
               Logger::instance().log("VAD: Voice detected. Resuming.");
           }
      }
      
      if (!isPaused && !isTimeout && !isAutoPaused && (now - lastSpeechTime > std::chrono::milliseconds(config.vadTimeoutMs))) {
           isAutoPaused = true;
           rec.setWriting(false);
           lastAutoPauseStart = now;
           Logger::instance().log("VAD: Silence detected. Auto-pausing.");
      }

      // Calculate active recording duration
      auto totalSinceStart = now - startTime;
      auto currentPauseSession = isPaused ? (now - lastPauseStart) : std::chrono::seconds(0);
      auto currentAutoPauseSession = isAutoPaused ? (now - lastAutoPauseStart) : std::chrono::seconds(0);
      auto activePaused = totalPausedDuration + currentPauseSession + totalAutoPausedDuration + currentAutoPauseSession;
      auto elapsed = totalSinceStart - activePaused;

      auto remaining = maxDuration - elapsed;
      int secondsLeft = std::chrono::duration_cast<std::chrono::seconds>(remaining).count();

      // Session Timeout Logic
      if (!isTimeout && remaining <= std::chrono::seconds(0)) {
        rec.pause();
        isTimeout = true;
        isPaused = true;
        lastPauseStart = now;
        secondsLeft = 0;
        Logger::instance().log("Recording time limit reached.");
      }

      // UI Color Updates
      if (isTimeout) {
        win.setBackgroundColor("red");
      } else if (isPaused || isAutoPaused) {
        win.setBackgroundColor("white");
      } else {
        if (secondsLeft < 30) {
          win.setBackgroundColor("red");
        }
        else if (secondsLeft < 60) {
          win.setBackgroundColor("yellow");
        }
        else {
          win.setBackgroundColor("white");
        }
      }

      // UI Text Construction
      int minutes = secondsLeft / 60;
      int seconds = secondsLeft % 60;
      std::string header;

      if (isTimeout) {
        header = "TIME LIMIT REACHED!";
      } else if (isPaused) {
        header = std::format("PAUSED - {:02d}:{:02d} remaining", minutes, seconds);
      } else if (isAutoPaused) {
        header = std::format("LISTENING... (Paused) {:02d}:{:02d}", minutes, seconds);
      } else {
        header = std::format("RECORDING... {:02d}:{:02d} remaining", minutes, seconds);
      }

      std::string status = std::format(R"( 
{} 
----------------------------------
Commands:
  v    Paste + Space
  s    Paste Only
  t    Terminal Paste
  r    Restart Session
  p    Pause / Resume
  +    Extend Time {} min
  a    Abort Transcribing
  x    Exit Program)", 
          header, config.maxRecordTime);

      win.updateText(status, rec.getCurrentLevel());

      // 4. Handle Window Interaction
      char key = 0;
      if (win.checkForInput(key)) {
        if (key == '+') {
          maxDuration += std::chrono::minutes(config.maxRecordTime);
          if (isTimeout) {
            isTimeout = false;
            totalPausedDuration += (now - lastPauseStart);
            isPaused = false;
            rec.resume();
            lastSpeechTime = now;
          }
        } else if (key == 'p') {
          if (isTimeout) {
          } else if (isPaused) {
            totalPausedDuration += (now - lastPauseStart);
            isPaused = false;
            rec.resume();
            rec.setWriting(true);
            lastSpeechTime = now;
          } else {
            if (isAutoPaused) {
                isAutoPaused = false;
                totalAutoPausedDuration += (now - lastAutoPauseStart);
            }
            lastPauseStart = now;
            isPaused = true;
            rec.pause();
          }
        } else if (key == 'r') {
          // Restart
          rec.stop();
          rec.start(tempFile);
          startTime = std::chrono::steady_clock::now();
          maxDuration = std::chrono::minutes(config.maxRecordTime);
          totalPausedDuration = std::chrono::seconds(0);
          isPaused = false;
          isTimeout = false;
          lastSpeechTime = now;
          Logger::instance().log("Recording session restarted by user.");
        } else if (key == 'v' || key == 's' || key == 't') {
          finishAndTranscribe = true;
          appendSpace = (key == 'v' || key == 't');
          useTerminalPaste = (key == 't');
          break;
        } else if (key == 'a' || key == 27) { // 'a' or Esc
          Logger::instance().log("Recording aborted by user.");
          break;
        } else if (key == 'x' || key == 3) { // 'x' or Ctrl+C
          Logger::instance().log("Exit requested by user via recording window.");
          shouldExit = true;
          break;
        }
      }

      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // 5. Finalize and Transcribe
    rec.stop();

    if (finishAndTranscribe) {
      win.setBackgroundColor("white");
      win.updateText("Recognition in progress...");
      std::this_thread::sleep_for(std::chrono::milliseconds(50));

      try {
        Transcriber transcriber(modelPath);
        std::string rawText = transcriber.transcribe(tempFile);
        std::string text = trim(rawText);

        if (!config.postProcessCommand.empty()) {
            Logger::instance().log("Running post-process: " + config.postProcessCommand);
            text = runPostProcess(config.postProcessCommand, text);
        }

        if (!text.empty()) {
          if (appendSpace) text += " ";

          if (config.verbose) {
            std::cout << "--- Transcription ---" << std::endl;
            std::cout << text << std::endl;
            std::cout << "---------------------" << std::endl;
          }
          // Log transcription only if --log-transcriptions is enabled
          if (config.logTranscriptions) {
            Logger::instance().log("Transcribed: " + text);
          }

          win.close();

          // Paste text
          Logger::instance().log("Pasting text...");
          Paster paster;
          paster.paste(text, activeWin, useTerminalPaste, config.verbose);
        } else {
          win.updateText("No speech detected.");
          Logger::instance().log("Transcription complete: No speech detected.");
          std::this_thread::sleep_for(std::chrono::seconds(1));
        }

      } catch (const std::exception& e) {
        Logger::instance().error(std::format("Transcription error: {}", e.what()));
        win.updateText("Error during transcription!");
        std::this_thread::sleep_for(std::chrono::seconds(2));
      }
    }
  }

  // Cleanup crash report file if no crash occurred and application exits normally
  if (g_crash_report_fd != -1) {
      close(g_crash_report_fd); // Close the file descriptor
      // Attempt to remove the temp crash report file if it wasn't renamed due to a crash.
      // This logic is tricky and might need refinement for full robustness.
      // For now, we assume the crash handler handles the renaming/closing.
      // If the application exits normally, we should clean up the temporary file.
      std::error_code ec;
      std::filesystem::remove(g_crash_report_filename, ec);
      if (ec) {
          Logger::instance().error("Failed to remove temporary crash report file: " + std::string(g_crash_report_filename) + " Error: " + ec.message());
      }
  }

  return 0;
}
