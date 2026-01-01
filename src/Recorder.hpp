#ifndef VOICECLI_SRC_RECORDER_HPP
#define VOICECLI_SRC_RECORDER_HPP

#include <string>
#include <iostream>
#include <vector>
#include <stdexcept>
#include <filesystem>
#include <atomic>
#include <cmath>
#include <algorithm>

#include "../third_party/miniaudio.h"

class Recorder {
public:
  Recorder(ma_device_id* pDeviceID, unsigned int sampleRate = 16000);
  ~Recorder();

  // Disable copying
  Recorder(const Recorder&) = delete;
  Recorder& operator=(const Recorder&) = delete;

  // Control
  void start(const std::string& outputFile);
  void stop();
  void pause();
  void resume();
  bool isRecording() const;
  float getCurrentLevel() const;
  void setWriting(bool writing);

private:
  static void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount);

  ma_device m_device;
  ma_device_config m_deviceConfig;
  ma_encoder m_encoder;
  ma_encoder_config m_encoderConfig;
  bool m_isRecording;
  bool m_isInitialized;
  std::atomic<float> m_currentLevel;
  std::atomic<bool> m_isWriting;
};

// -----------------------------------------------------------------------------
// Inline Implementations
// -----------------------------------------------------------------------------

inline void Recorder::data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
  Recorder* pRecorder = (Recorder*)pDevice->pUserData;
  if (pRecorder && pRecorder->m_isRecording) {
    if (pRecorder->m_isWriting.load(std::memory_order_relaxed)) {
        ma_encoder_write_pcm_frames(&pRecorder->m_encoder, pInput, frameCount, NULL);
    }

    // Level Meter Calculation (Peak)
    float maxVal = 0.0f;
    const float* samples = (const float*)pInput;
    for (ma_uint32 i = 0; i < frameCount; ++i) {
        float val = std::abs(samples[i]);
        if (val > maxVal) maxVal = val;
    }

    float current = pRecorder->m_currentLevel.load(std::memory_order_relaxed);
    if (maxVal > current) {
        pRecorder->m_currentLevel.store(maxVal, std::memory_order_relaxed);
    } else {
        // Smooth decay
        pRecorder->m_currentLevel.store(current * 0.90f, std::memory_order_relaxed);
    }
  }
  (void)pOutput; // Unused
}

inline Recorder::Recorder(ma_device_id* pDeviceID, unsigned int sampleRate) 
    : m_isRecording(false), m_isInitialized(false), m_currentLevel(0.0f), m_isWriting(true) {
  // Configure Device
  m_deviceConfig = ma_device_config_init(ma_device_type_capture);
  m_deviceConfig.capture.pDeviceID = pDeviceID; 
  
  // Let's set the format for high quality voice
  m_deviceConfig.capture.format = ma_format_f32;
  m_deviceConfig.capture.channels = 1; // Mono is usually best for voice command
  m_deviceConfig.sampleRate = sampleRate;
  m_deviceConfig.dataCallback = data_callback;
  m_deviceConfig.pUserData = this;
}

inline Recorder::~Recorder() {
  stop();
}

inline bool Recorder::isRecording() const {
  return m_isRecording;
}

inline float Recorder::getCurrentLevel() const {
    return m_currentLevel.load(std::memory_order_relaxed);
}

inline void Recorder::setWriting(bool writing) {
    m_isWriting.store(writing, std::memory_order_relaxed);
}

inline void Recorder::start(const std::string& outputFile) {
  if (m_isRecording) return;

  // Initialize Encoder (WAV file)
  m_encoderConfig = ma_encoder_config_init(ma_encoding_format_wav, ma_format_f32, 1, m_deviceConfig.sampleRate);
  if (ma_encoder_init_file(outputFile.c_str(), &m_encoderConfig, &m_encoder) != MA_SUCCESS) {
    throw std::runtime_error("Failed to initialize audio output file.");
  }
  
  m_isWriting.store(true); // Default to writing

  // Initialize Device (we do this here to ensure fresh start)
  if (ma_device_init(NULL, &m_deviceConfig, &m_device) != MA_SUCCESS) {
    ma_encoder_uninit(&m_encoder);
    throw std::runtime_error("Failed to initialize capture device.");
  }

  if (ma_device_start(&m_device) != MA_SUCCESS) {
    ma_device_uninit(&m_device);
    ma_encoder_uninit(&m_encoder);
    throw std::runtime_error("Failed to start capture device.");
  }

  m_isRecording = true;
  m_isInitialized = true;
}

inline void Recorder::stop() {
  if (m_isInitialized) {
    ma_device_uninit(&m_device);
    m_isInitialized = false;
  }
  
  if (m_isRecording) {
    ma_encoder_uninit(&m_encoder);
    m_isRecording = false;
  }
}

inline void Recorder::pause() {
  if (m_isInitialized && m_isRecording) {
    ma_device_stop(&m_device);
    // We don't change m_isRecording here because we want to keep the encoder open.
    // But conceptually we are "paused". 
    // Data callback won't be called, so nothing written.
  }
}

inline void Recorder::resume() {
  if (m_isInitialized && m_isRecording) {
    ma_device_start(&m_device);
  }
}

#endif // VOICECLI_SRC_RECORDER_HPP
