#ifndef VOICECLI_SRC_LOGGER_HPP
#define VOICECLI_SRC_LOGGER_HPP

#include <string>
#include <fstream>
#include <iostream>
#include <chrono>
#include <format>
#include <mutex>
#include <filesystem>

/**
 * @brief Thread-safe singleton logger.
 * 
 * Writes log messages to a file and optionally to stderr.
 */
class Logger {
public:
  /**
   * @brief Access the singleton instance.
   */
  static Logger& instance();
  
  /**
   * @brief Sets the path for the log file.
   * 
   * @param path Path to the log file.
   */
  void setLogFile(const std::string& path);

  /**
   * @brief Logs an informational message.
   * 
   * Thread-safe. Adds timestamp and [INFO] tag.
   * 
   * @param message The message to log.
   */
  void log(const std::string& message);

  /**
   * @brief Logs an error message.
   * 
   * Thread-safe. Adds timestamp and [ERROR] tag. Also prints to stderr.
   * 
   * @param message The error message to log.
   */
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

inline Logger::~Logger() {
  if (m_file.is_open()) {
    m_file.close();
  }
}

inline Logger& Logger::instance() {
  static Logger instance;
  return instance;
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

inline void Logger::log(const std::string& message) {
  std::lock_guard<std::mutex> lock(m_mutex);
  
  auto now = std::chrono::system_clock::now();
  auto time = std::chrono::system_clock::to_time_t(now);
  auto tm = *std::localtime(&time);
  
  char timeBuffer[32];
  std::strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M:%S", &tm);

  std::string formatted = std::format("[{}] [INFO] {}", timeBuffer, message);
  
  if (m_file.is_open()) {
    m_file << formatted << std::endl;
  }
}

inline void Logger::setLogFile(const std::string& path) {
  std::lock_guard<std::mutex> lock(m_mutex);
  if (m_file.is_open()) {
    m_file.close();
  }
  m_file.open(path, std::ios::app);
  if (!m_file.is_open()) {
    std::cerr << "Failed to open log file: " << path << std::endl;
  }
}

#endif // VOICECLI_SRC_LOGGER_HPP
