#ifndef VOICECLI_SRC_COMMANDLINE_HPP
#define VOICECLI_SRC_COMMANDLINE_HPP

#include <getopt.h>
#include <iostream>
#include <string>
#include <vector>

struct AppConfig {
  bool listAudioDevices = false;
  bool showHelp = false;
  bool testRecord = false;
  bool verbose = false;
  unsigned int sampleRate = 16000;
  std::string modelPath = "models/ggml-base.en.bin";
  unsigned int maxRecordTime = 5; // Minutes
};

class CommandLine {
public:
  CommandLine(int argc, char* argv[]);
  ~CommandLine();

  // Disable copying
  CommandLine(const CommandLine&) = delete;
  CommandLine& operator=(const CommandLine&) = delete;

  const AppConfig& getConfig() const;
  void printHelp() const;

private:
  AppConfig m_config;
  std::string m_binaryName;
};

// -----------------------------------------------------------------------------
// Inline Implementations
// -----------------------------------------------------------------------------

inline CommandLine::CommandLine(int argc, char* argv[]) {
  m_binaryName = (argc > 0) ? argv[0] : "VoiceCLI";

  static struct option long_options[] = {
    { "help", no_argument, 0, 'h' },
    { "list-audio-devices", no_argument, 0, 'l' },
    { "model", required_argument, 0, 'm' },
    { "max-record-time", required_argument, 0, 'M' },
    { "sample-rate", required_argument, 0, 'r' },
    { "test-record", no_argument, 0, 't' },
    { "verbose", no_argument, 0, 'v' },
    { 0, 0, 0, 0 }
  };

  int opt;
  int option_index = 0;

  while ((opt = getopt_long(argc, argv, "hlm:M:r:tv", long_options, &option_index)) != -1) {
    switch (opt) {
    case 'h':
      m_config.showHelp = true;
      break;
    case 'l':
      m_config.listAudioDevices = true;
      break;
    case 'm':
      m_config.modelPath = optarg;
      break;
    case 'M':
      try {
        m_config.maxRecordTime = std::stoul(optarg);
      } catch (...) {
        std::cerr << "Invalid max record time. Using default 5 minutes." << std::endl;
      }
      break;
    case 'r':
      try {
        m_config.sampleRate = std::stoul(optarg);
      } catch (...) {
        std::cerr << "Invalid sample rate provided. Using default 16000Hz." << std::endl;
      }
      break;
    case 't':
      m_config.testRecord = true;
      break;
    case 'v':
      m_config.verbose = true;
      break;
    case '?':
      // getopt_long prints its own error message
      m_config.showHelp = true;
      break;
    default:
      break;
    }
  }
}

inline CommandLine::~CommandLine() {
}

inline const AppConfig& CommandLine::getConfig() const {
  return m_config;
}

inline void CommandLine::printHelp() const {
  std::cout << "Usage: " << m_binaryName << " [OPTIONS]\n\n"
            << "Options:\n"
            << "  -h, --help                Show this help message\n"
            << "  -l, --list-audio-devices  List available audio capture devices\n"
            << "  -m, --model <path>        Path to Whisper model file (default: models/ggml-base.en.bin)\n"
            << "  -M, --max-record-time <min> Set max recording time per session (default: 5 min)\n"
            << "  -r, --sample-rate <hz>    Set recording sample rate (default: 16000)\n"
            << "  -t, --test-record         Record 5 seconds of audio to verify input\n"
            << "  -v, --verbose             Enable verbose output (e.g., print selected device)\n"
            << std::endl;
}

#endif // VOICECLI_SRC_COMMANDLINE_HPP
