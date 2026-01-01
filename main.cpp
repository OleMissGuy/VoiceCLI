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

// Helper to trim whitespace
std::string trim(const std::string& s) {
  auto start = s.find_first_not_of(" \t\n\r");
  if (start == std::string::npos) return "";
  auto end = s.find_last_not_of(" \t\n\r");
  return s.substr(start, end - start + 1);
}

// Helper to find the default device
std::optional<AudioDevice> getDefaultDevice(AudioConfig& audio) {
  auto devices = audio.listCaptureDevices();
  for (const auto& dev : devices) {
    if (dev.isDefault) {
      return dev;
    }
  }
  if (!devices.empty()) {
    return devices[0]; // Fallback to first if no default marked
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
          std::cout << std::format("[{}{}] {}", dev.index, dev.isDefault ? "*" : "", dev.name) << std::endl;
        }
      }
      return 0;
    }

    // Determine which device we WILL use
    auto selectedDevice = getDefaultDevice(audio);

    if (config.verbose) {
      if (selectedDevice) {
        std::string msg = std::format("Selected Input Device: [{}] {}", selectedDevice->index, selectedDevice->name);
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

      std::cout << "Starting 5-second test recording to " << outFile << "..." << std::endl;
      Logger::instance().log("Starting test recording...");

      StatusWindow win;
      win.show("Initializing Recorder...");

      Recorder rec(selectedDevice->index, config.sampleRate);
      rec.start(outFile);

      for (int i = 50; i > 0; --i) {
        float timeRemaining = i * 0.1f;
        win.updateText(std::format("Recording... {:.1f}s", timeRemaining));
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }

      rec.stop();
      win.updateText("Transcribing...");
      std::cout << "Recording complete. Transcribing..." << std::endl;

      // Transcribe
      try {
        Transcriber transcriber(modelPath);
        std::string text = transcriber.transcribe(outFile);

        win.updateText("Done!");
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        std::cout << "---------------------------------------------------" << std::endl;
        std::cout << "Transcription Result:" << std::endl;
        std::cout << text << std::endl;
        std::cout << "---------------------------------------------------" << std::endl;

        Logger::instance().log("Transcription: " + text);

      } catch (const std::exception& e) {
        std::string err = std::format("Transcription Failed: {}", e.what());
        std::cerr << err << std::endl;
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
      if (!input.monitor(config.verbose)) {
        break; // Stop if monitor fails
      }

      // Capture currently focused window before we take over
      Window activeWin = getCurrentFocus();
      Logger::instance().log(std::format("Captured Active Window ID: {}", activeWin));

      // --- Triggered: Start Recording Session ---
      StatusWindow win;
      win.show("Starting Recording...");

      std::string tempFile = "/tmp/voicecli_rec.wav";
      Recorder rec(selectedDevice->index, config.sampleRate);

      try {
        rec.start(tempFile);
      } catch (const std::exception& e) {
        Logger::instance().error(std::format("Failed to start recorder: {}", e.what()));
        continue;
      }

      auto startTime = std::chrono::steady_clock::now();
      auto maxDuration = std::chrono::minutes(config.maxRecordTime);
      bool finishAndTranscribe = false;
      bool appendSpace = true;
      bool useTerminalPaste = false;

      bool isPaused = false;
      bool isTimeout = false;
      std::chrono::steady_clock::duration totalPausedDuration = std::chrono::seconds(0);
      auto lastPauseStart = std::chrono::steady_clock::now();

      // Recording Loop
      while (true) {
        auto now = std::chrono::steady_clock::now();

        // Calculate active recording duration
        auto totalSinceStart = now - startTime;
        auto currentPauseSession = isPaused ? (now - lastPauseStart) : std::chrono::seconds(0);
        auto activePaused = totalPausedDuration + currentPauseSession;
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
        } else if (isPaused) {
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

                if (isTimeout) header = "TIME LIMIT REACHED!";

                else if (isPaused) header = std::format("PAUSED - {:02d}:{:02d} remaining", minutes, seconds);

                else header = std::format("RECORDING... {:02d}:{:02d} remaining", minutes, seconds);

        

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

        

                        

        

                        win.updateText(status);

        

                

        

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

        

                            }

        

                          } else if (key == 'p') {

        

                            if (isTimeout) {

        

                            } else if (isPaused) {

        

                              totalPausedDuration += (now - lastPauseStart);

        

                              isPaused = false;

        

                              rec.resume();

        

                            } else {

        

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

        

                            Logger::instance().log("Recording session restarted by user.");

        

                          } else if (key == 'v' || key == 's' || key == 't') {

        

                            finishAndTranscribe = true;

        

                            appendSpace = (key == 'v' || key == 't');

        

                            useTerminalPaste = (key == 't');

        

                            break;

        

                          } else if (key == 'a') {
            Logger::instance().log("Recording aborted by user.");
            break;
          } else if (key == 'x') {
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