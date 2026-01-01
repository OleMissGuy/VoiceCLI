#ifndef VOICECLI_SRC_AUDIOCONFIG_HPP
#define VOICECLI_SRC_AUDIOCONFIG_HPP

#include <format>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>
#include <cstring>

#include "../third_party/miniaudio.h"

struct AudioDevice {
  std::string name;
  unsigned int index;
  bool isDefault;
  std::vector<unsigned int> supportedChannels;
  std::vector<ma_format> supportedFormats;
};

class AudioConfig {
public:
  /**
   * @brief Initializes the miniaudio context.
   * @throws std::runtime_error if context initialization fails.
   */
  AudioConfig();

  /**
   * @brief Uninitializes the miniaudio context.
   */
  ~AudioConfig();

  // Disable copying
  AudioConfig(const AudioConfig&) = delete;
  AudioConfig& operator=(const AudioConfig&) = delete;

  /**
   * @brief Enumerates all available audio capture devices.
   * 
   * Queries the miniaudio context for capture devices and retrieves detailed
   * capabilities (channels, formats) for each.
   * 
   * @return A vector of AudioDevice structures containing device details.
   */
  std::vector<AudioDevice> listCaptureDevices();

  /**
   * @brief Retrieves the miniaudio device ID for a given device index.
   * 
   * @param index The 0-based index of the capture device.
   * @return A pointer to the internal ma_device_id, or nullptr if the index is invalid.
   */
  ma_device_id* getCaptureDeviceID(unsigned int index);

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

inline ma_device_id* AudioConfig::getCaptureDeviceID(unsigned int index) {
  ma_device_info* pPlaybackInfos;
  ma_uint32 playbackCount;
  ma_device_info* pCaptureInfos;
  ma_uint32 captureCount;

  if (ma_context_get_devices(m_context, &pPlaybackInfos, &playbackCount, &pCaptureInfos,
                             &captureCount) != MA_SUCCESS) {
    return nullptr;
  }

  if (index >= captureCount) {
    return nullptr;
  }

  return &pCaptureInfos[index].id;
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
    ma_device_info detailedInfo;
    // Initialize detailedInfo to zero/defaults just in case
    std::memset(&detailedInfo, 0, sizeof(detailedInfo));
    
    // Try to get detailed info. If it fails, we fall back to the basic info we already have.
    // We pass the ID from the enumeration result.
    if (ma_context_get_device_info(m_context, ma_device_type_capture, &pCaptureInfos[i].id, &detailedInfo) != MA_SUCCESS) {
        // Fallback: use the info from enumeration (which lacks format details but has name/id)
        detailedInfo = pCaptureInfos[i]; 
    }

    std::vector<unsigned int> channels;
    std::vector<ma_format> formats;
    
    for (ma_uint32 j = 0; j < detailedInfo.nativeDataFormatCount; ++j) {
        auto& fmtInfo = detailedInfo.nativeDataFormats[j];
        
        // Channels
        if (fmtInfo.channels == 0) {
            channels.push_back(0); 
        } else {
            bool exists = false;
            for(auto c : channels) if(c == fmtInfo.channels) exists = true;
            if(!exists) channels.push_back(fmtInfo.channels);
        }

        // Formats
        if (fmtInfo.format == ma_format_unknown) {
             formats.push_back(ma_format_unknown);
        } else {
            bool exists = false;
            for(auto f : formats) if(f == fmtInfo.format) exists = true;
            if(!exists) formats.push_back(fmtInfo.format);
        }
    }

    devices.push_back({ .name = pCaptureInfos[i].name, // Use name from enum, reliable
                        .index = i,
                        .isDefault = (bool)pCaptureInfos[i].isDefault,
                        .supportedChannels = channels,
                        .supportedFormats = formats });
  }

  return devices;
}

#endif // VOICECLI_SRC_AUDIOCONFIG_HPP
