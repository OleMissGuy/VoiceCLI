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
  std::optional<unsigned int> deviceIndex;
  float vadThreshold = 0.05f;
  unsigned int vadTimeoutMs = 2000;
  std::string triggerKey = "Shift";
  std::string postProcessCommand = "";
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
    { "device-index", required_argument, 0, 'd' },
    { "model", required_argument, 0, 'm' },
    { "max-rec-time", required_argument, 0, 'M' },
    { "sample-rate", required_argument, 0, 'r' },
    { "test-record", no_argument, 0, 't' },
    { "verbose", no_argument, 0, 'v' },
    { "vad-threshold", required_argument, 0, 'S' },
    { "vad-timeout", required_argument, 0, 'T' },
    { "trigger-key", required_argument, 0, 'k' },
    { "post-process", required_argument, 0, 'P' },
    { 0, 0, 0, 0 }
  };

  int opt;
  int option_index = 0;

  while ((opt = getopt_long(argc, argv, "hld:m:M:r:tvS:T:k:P:", long_options, &option_index)) != -1) {
    switch (opt) {
    case 'h':
      m_config.showHelp = true;
      break;
    case 'l':
      m_config.listAudioDevices = true;
      break;
    case 'd':
      try {
        m_config.deviceIndex = std::stoul(optarg);
      } catch (...) {
        std::cerr << "Invalid device index provided." << std::endl;
      }
      break;
    case 'm':
      m_config.modelPath = optarg;
      break;
    case 'M':
      try {
        unsigned int val = std::stoul(optarg);
        if (val == 0) throw std::invalid_argument("must be > 0");
        m_config.maxRecordTime = val;
      } catch (...) {
        std::cerr << "Invalid max record time (must be integer > 0). Using default 5 minutes." << std::endl;
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
    case 'S':
      try {
        float val = std::stof(optarg);
        if (val < 0.0f || val > 1.0f) throw std::invalid_argument("out of range");
        m_config.vadThreshold = val;
      } catch (...) {
        std::cerr << "Invalid VAD threshold (must be 0.0-1.0). Using default 0.05." << std::endl;
      }
      break;
    case 'T':
      try {
        unsigned int val = std::stoul(optarg);
        if (val < 100) std::cerr << "Warning: VAD timeout " << val << "ms is very short." << std::endl;
        m_config.vadTimeoutMs = val;
      } catch (...) {
        std::cerr << "Invalid VAD timeout provided. Using default 2000ms." << std::endl;
      }
      break;
    case 'k':
      m_config.triggerKey = optarg;
      break;
    case 'P':
      m_config.postProcessCommand = optarg;
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
            << "  -d, --device-index <idx>  Select a specific audio capture device by index\n"
            << "  -m, --model <path>        Path to Whisper model file (default: models/ggml-base.en.bin)\n"
            << "  -M, --max-rec-time <min>  Set max record time per session (default: 5 min)\n"
            << "  -r, --sample-rate <hz>    Set recording sample rate (default: 16000)\n"
            << "  -t, --test-record         Record 5 seconds of audio to verify input\n"
            << "  -v, --verbose             Enable verbose output (e.g., print selected device)\n"
            << "  -S, --vad-threshold <val> Set VAD sensitivity (0.0 to 1.0, default 0.05)\n"
            << "  -T, --vad-timeout <ms>    Set VAD silence timeout in ms (default 2000)\n"
            << "  -k, --trigger-key <key>   Set double-tap trigger key (Shift, Control, Alt, Super; default Shift)\n"
            << "  -P, --post-process <cmd>  Shell command to process text before pasting\n"
            << std::endl;
}

#endif // VOICECLI_SRC_COMMANDLINE_HPP
