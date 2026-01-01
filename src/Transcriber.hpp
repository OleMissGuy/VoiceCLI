#ifndef VOICECLI_SRC_TRANSCRIBER_HPP
#define VOICECLI_SRC_TRANSCRIBER_HPP

#include <string>
#include <vector>
#include <stdexcept>
#include <iostream>
#include <format>

#include "whisper.h"
#include "Logger.hpp"
#include "../third_party/miniaudio.h"

/**
 * @brief Handles speech-to-text transcription using the Whisper model.
 * 
 * Manages the Whisper context and performs inference on WAV audio files.
 */
class Transcriber {
public:
  /**
   * @brief Initializes the Whisper context with a pre-trained model.
   * @param modelPath Path to the ggml-*.bin Whisper model file.
   * @throws std::runtime_error If model loading fails.
   */
  Transcriber(const std::string& modelPath);
  ~Transcriber();

  // Disable copying
  Transcriber(const Transcriber&) = delete;
  Transcriber& operator=(const Transcriber&) = delete;

  /**
   * @brief Transcribes a WAV audio file to text.
   * 
   * Loads the audio file using miniaudio, converts to the required 16kHz float format,
   * and runs Whisper inference.
   * 
   * @param wavPath Path to the input WAV file.
   * @return The transcribed text string.
   * @throws std::runtime_error If audio loading or inference fails.
   */
  std::string transcribe(const std::string& wavPath);

private:
  struct whisper_context* m_ctx;
};

// -----------------------------------------------------------------------------
// Inline Implementations
// -----------------------------------------------------------------------------

inline Transcriber::Transcriber(const std::string& modelPath) : m_ctx(nullptr) {
  whisper_log_set(whisper_log_callback, nullptr);
  
  struct whisper_context_params cparams = whisper_context_default_params();
  m_ctx = whisper_init_from_file_with_params(modelPath.c_str(), cparams);
  
  if (m_ctx == nullptr) {
    throw std::runtime_error("Failed to initialize Whisper context. Check model path.");
  }
}

inline Transcriber::~Transcriber() {
  if (m_ctx) {
    whisper_free(m_ctx);
  }
}

/**
 * @brief Transcribes a WAV audio file to text.
 * 
 * Loads the audio file using miniaudio, converts to the required 16kHz float format,
 * and runs Whisper inference.
 * 
 * @param wavPath Path to the input WAV file.
 * @return The transcribed text string.
 * @throws std::runtime_error If audio loading or inference fails.
 */
inline std::string Transcriber::transcribe(const std::string& wavPath) {
  // 1. Decode WAV file using miniaudio
  ma_decoder decoder;
  ma_decoder_config config = ma_decoder_config_init(ma_format_f32, 1, 16000);
  
  if (ma_decoder_init_file(wavPath.c_str(), &config, &decoder) != MA_SUCCESS) {
    throw std::runtime_error("Failed to load WAV file: " + wavPath);
  }

  // Calculate total frames
  ma_uint64 frameCount;
  ma_decoder_get_length_in_pcm_frames(&decoder, &frameCount);
  
  std::vector<float> pcmf32(frameCount);
  ma_uint64 framesRead;
  
  if (ma_decoder_read_pcm_frames(&decoder, pcmf32.data(), frameCount, &framesRead) != MA_SUCCESS) {
    ma_decoder_uninit(&decoder);
    throw std::runtime_error("Failed to read WAV frames.");
  }
  
  ma_decoder_uninit(&decoder);

  // 2. Run Whisper Inference
  whisper_full_params wparams = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
  wparams.print_progress   = false;
  wparams.print_special    = false;
  wparams.print_realtime   = false;
  wparams.print_timestamps = false;
  wparams.translate        = false;
  wparams.no_context       = true;
  wparams.single_segment   = true; // Treat as one command usually

  if (whisper_full(m_ctx, wparams, pcmf32.data(), pcmf32.size()) != 0) {
    throw std::runtime_error("Failed to run Whisper inference.");
  }

  // 3. Collect Output
  std::string result = "";
  const int n_segments = whisper_full_n_segments(m_ctx);
  for (int i = 0; i < n_segments; ++i) {
    const char* text = whisper_full_get_segment_text(m_ctx, i);
    result += text;
  }

  return result;
}

/**
 * @brief Callback for routing Whisper (ggml) library logs to our Logger.
 * 
 * Handles multi-line log messages by buffering them until a newline or a non-continuation
 * level is encountered. Logs to Logger::error for GGML_LOG_LEVEL_ERROR, otherwise to Logger::log.
 * 
 * @param level The log level from ggml.
 * @param text The log message part.
 * @param user_data User-defined data (unused).
 */
inline void whisper_log_callback(ggml_log_level level, const char * text, void * user_data) {
    (void)user_data;
    static std::string buffer;
    static ggml_log_level last_level = GGML_LOG_LEVEL_INFO;

    if (level != GGML_LOG_LEVEL_CONT) {
        if (!buffer.empty()) {
            if (last_level == GGML_LOG_LEVEL_ERROR) Logger::instance().error("Whisper: " + buffer);
            else Logger::instance().log("Whisper: " + buffer);
            buffer.clear();
        }
        last_level = level;
    }

    if (text) buffer += text;

    if (!buffer.empty() && buffer.back() == '\n') {
        buffer.pop_back();
        if (last_level == GGML_LOG_LEVEL_ERROR) Logger::instance().error("Whisper: " + buffer);
        else Logger::instance().log("Whisper: " + buffer);
        buffer.clear();
    }
}

#endif // VOICECLI_SRC_TRANSCRIBER_HPP
