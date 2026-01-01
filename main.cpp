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

// Helper to trim whitespace
std::string trim(const std::string& s) {
  auto start = s.find_first_not_of(" \t\n\r");
  if (start == std::string::npos) return "";
  auto end = s.find_last_not_of(" \t\n\r");
  return s.substr(start, end - start + 1);
}

// Helper to run post-processing command
std::string runPostProcess(const std::string& cmd, const std::string& input) {
    if (cmd.empty()) return input;

    // Write input to temp file
    std::string tempIn = "/tmp/voicecli_pp_in.txt";
    std::ofstream ofs(tempIn);
    if (!ofs) {
        Logger::instance().error("Failed to write post-process input file.");
        return input;
    }
    ofs << input;
    ofs.close();

    // Construct command: cmd < tempIn
    std::string fullCmd = cmd + " < " + tempIn;
    
    // Execute and read stdout
    std::string output;
    char buffer[128];
    FILE* pipe = popen(fullCmd.c_str(), "r");
    if (!pipe) {
        Logger::instance().error("Failed to execute post-process command.");
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

// Helper to find the desired or default device
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

// Helper to get currently focused window
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
  // Initialize Logger
  Logger::instance().setLogFile("voicecli.log");
  Logger::instance().log("Application Started");

  try {
    CommandLine cmd(argc, argv);
    const auto& config = cmd.getConfig();

    if (config.showHelp) {
      cmd.printHelp();
      return 0;
    }

    AudioConfig audio;

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
    }

    if (!selectedDevice) {
      Logger::instance().error("No microphone available. Exiting.");
      return 1;
    }

    // --- Test Record Mode ---
    if (config.testRecord) {
      std::string outFile = "/tmp/voicecli_test.wav";
      std::string modelPath = config.modelPath;

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
    Logger::instance().log("Loading model: " + config.modelPath);
    Transcriber transcriber(config.modelPath);
    Logger::instance().log("Model loaded. Ready.");

    bool shouldExit = false;
    while (!shouldExit) {
      if (!input.monitor(config.triggerKey, config.verbose)) {
        break; // Stop if monitor fails
      }

      // Capture currently focused window before we take over
      Window activeWin = getCurrentFocus();
      Logger::instance().log(std::format("Captured Active Window ID: {}", activeWin));

      // --- Triggered: Start Recording Session ---
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

      // Recording Loop
      while (true) {
        auto now = std::chrono::steady_clock::now();

        // VAD Logic (Smart Pause)
        float currentLevel = rec.getCurrentLevel();
        if (currentLevel > config.vadThreshold) {
             lastSpeechTime = now;
             if (isAutoPaused) {
                 isAutoPaused = false;
                 rec.setWriting(true);
                 totalAutoPausedDuration += (now - lastAutoPauseStart);
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

        // Timeout Logic
        if (!isTimeout && remaining <= std::chrono::seconds(0)) {
          rec.pause();
          isTimeout = true;
          isPaused = true;
          lastPauseStart = now;
          secondsLeft = 0;
          Logger::instance().log("Recording time limit reached.");
        }

        // Background Color Logic
        if (isTimeout) {
          win.setBackgroundColor("red");
        } else if (isPaused || isAutoPaused) {
          win.setBackgroundColor("white");
        } else {
          if (secondsLeft < 30) {
            win.setBackgroundColor("red");
          } else if (secondsLeft < 60) {
            win.setBackgroundColor("yellow");
          } else {
            win.setBackgroundColor("white");
          }
        }

        // UI Text Logic
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

        std::string status = std::format(
            "{}\n"
            "----------------------------------\n"
            "Commands:\n"
            "  v    Paste + Space\n"
            "  s    Paste Only\n"
            "  t    Terminal Paste\n"
            "  r    Restart Session\n"
            "  p    Pause / Resume\n"
            "  +    Extend Time {} min\n"
            "  a    Abort Transcribing\n"
            "  x    Exit Program",
            header, config.maxRecordTime);

        win.updateText(status, rec.getCurrentLevel());

        // Check Input
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

      rec.stop();

      if (finishAndTranscribe) {
        win.setBackgroundColor("white");
        win.updateText("Recognition in progress...");
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        try {
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
            Logger::instance().log("Transcribed: " + text);

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

  } catch (const std::exception& e) {
    std::string err = std::format("Fatal Error: {}", e.what());
    std::cerr << err << std::endl;
    Logger::instance().error(err);
    return 1;
  }

  return 0;
}