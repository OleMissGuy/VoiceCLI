#ifndef VOICECLI_SRC_AUDIOCONFIG_HPP
#define VOICECLI_SRC_AUDIOCONFIG_HPP

#include <format>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "../third_party/miniaudio.h"

struct AudioDevice {
  std::string name;
  unsigned int index;
  bool isDefault;
};

class AudioConfig {
public:
  AudioConfig();
  ~AudioConfig();

  // Disable copying
  AudioConfig(const AudioConfig&) = delete;
  AudioConfig& operator=(const AudioConfig&) = delete;

  std::vector<AudioDevice> listCaptureDevices();

private:
  ma_context_config m_config;
  ma_context* m_context;
};

// -----------------------------------------------------------------------------
// Inline Implementations
// -----------------------------------------------------------------------------

inline AudioConfig::AudioConfig() {
  m_config = ma_context_config_init();
  m_context = new ma_context();
  if (ma_context_init(NULL, 0, &m_config, m_context) != MA_SUCCESS) {
    delete m_context;
    throw std::runtime_error("Failed to initialize audio context.");
  }
}

inline AudioConfig::~AudioConfig() {
  if (m_context) {
    ma_context_uninit(m_context);
    delete m_context;
  }
}

inline std::vector<AudioDevice> AudioConfig::listCaptureDevices() {
  ma_device_info* pPlaybackInfos;
  ma_uint32 playbackCount;
  ma_device_info* pCaptureInfos;
  ma_uint32 captureCount;
  std::vector<AudioDevice> devices;

  if (ma_context_get_devices(m_context, &pPlaybackInfos, &playbackCount, &pCaptureInfos,
                             &captureCount) != MA_SUCCESS) {
    std::cerr << "Failed to retrieve audio device information." << std::endl;
    return devices;
  }

  for (ma_uint32 i = 0; i < captureCount; ++i) {
    devices.push_back({ .name = pCaptureInfos[i].name,
                        .index = i,
                        .isDefault = (bool)pCaptureInfos[i].isDefault });
  }

  return devices;
}

#endif // VOICECLI_SRC_AUDIOCONFIG_HPP
