#ifndef VOICECLI_SRC_LOGGER_HPP
#define VOICECLI_SRC_LOGGER_HPP

#include <string>
#include <fstream>
#include <iostream>
#include <chrono>
#include <format>
#include <mutex>
#include <filesystem>

class Logger {
public:
  static Logger& instance();
  
  void setLogFile(const std::string& path);
  void log(const std::string& message);
  void error(const std::string& message);

private:
  Logger() = default;
  ~Logger();
  
  // Disable copying
  Logger(const Logger&) = delete;
  Logger& operator=(const Logger&) = delete;

  std::ofstream m_file;
  std::mutex m_mutex;
};

// -----------------------------------------------------------------------------
// Inline Implementations
// -----------------------------------------------------------------------------

inline Logger& Logger::instance() {
  static Logger instance;
  return instance;
}

inline Logger::~Logger() {
  if (m_file.is_open()) {
    m_file.close();
  }
}

inline void Logger::setLogFile(const std::string& path) {
  std::lock_guard<std::mutex> lock(m_mutex);
  if (m_file.is_open()) {
    m_file.close();
  }
  // Open in append mode
  m_file.open(path, std::ios::app);
  if (!m_file.is_open()) {
    std::cerr << "Failed to open log file: " << path << std::endl;
  }
}

inline void Logger::log(const std::string& message) {
  std::lock_guard<std::mutex> lock(m_mutex);
  
  auto now = std::chrono::system_clock::now();
  auto time = std::chrono::system_clock::to_time_t(now);
  auto tm = *std::localtime(&time);
  
  // std::put_time is tricky with std::format in C++20 on some compilers, 
  // so let's use a simple buffer or manual format for the timestamp.
  // Or just use strftime.
  char timeBuffer[32];
  std::strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M:%S", &tm);

  std::string formatted = std::format("[{}] [INFO] {}", timeBuffer, message);
  
  // Write to file if open
  if (m_file.is_open()) {
    m_file << formatted << std::endl;
  }
  
  // Also usually good to see in debug stdout, but maybe we keep stdout clean for the CLI output?
  // Let's only write to file as requested.
}

inline void Logger::error(const std::string& message) {
  std::lock_guard<std::mutex> lock(m_mutex);
  
  auto now = std::chrono::system_clock::now();
  auto time = std::chrono::system_clock::to_time_t(now);
  auto tm = *std::localtime(&time);
  
  char timeBuffer[32];
  std::strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M:%S", &tm);

  std::string formatted = std::format("[{}] [ERROR] {}", timeBuffer, message);
  
  if (m_file.is_open()) {
    m_file << formatted << std::endl;
  }
  std::cerr << formatted << std::endl;
}

#endif // VOICECLI_SRC_LOGGER_HPP
