#ifndef VOICECLI_SRC_INPUTHOOK_HPP
#define VOICECLI_SRC_INPUTHOOK_HPP

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

#include "Logger.hpp"

class InputHook {
public:
  InputHook();
  ~InputHook();

  // Disable copying
  InputHook(const InputHook&) = delete;
  InputHook& operator=(const InputHook&) = delete;

  // Blocks and monitors input until a trigger is detected. Returns true if triggered.
  bool monitor(bool verbose = false);

private:
  Display* m_display;
  bool m_running;
};

// -----------------------------------------------------------------------------
// Inline Implementations
// -----------------------------------------------------------------------------

inline InputHook::InputHook() : m_display(nullptr), m_running(false) {
  m_display = XOpenDisplay(NULL);
  if (!m_display) {
    throw std::runtime_error("Failed to open X Display.");
  }
}

inline InputHook::~InputHook() {
  if (m_display) {
    XCloseDisplay(m_display);
  }
}

inline bool InputHook::monitor(bool verbose) {
  m_running = true;
  if (verbose) {
    std::cout << "InputHook: Monitoring for Shift double-tap (Left or Right)..." << std::endl;
  }

  int state = 0;
  auto lastTime = std::chrono::steady_clock::now();
  const auto timeout = std::chrono::milliseconds(400);

  KeyCode shiftL = XKeysymToKeycode(m_display, XK_Shift_L);
  KeyCode shiftR = XKeysymToKeycode(m_display, XK_Shift_R);
  KeyCode triggeringKey = 0;
  char keyMap[32];

  while (m_running) {
    XQueryKeymap(m_display, keyMap);

    bool isLPressed = (keyMap[shiftL / 8] & (1 << (shiftL % 8)));
    bool isRPressed = (keyMap[shiftR / 8] & (1 << (shiftR % 8)));
    
    auto now = std::chrono::steady_clock::now();

    switch (state) {
    case 0: // Idle
      if (isLPressed) {
        state = 1;
        triggeringKey = shiftL;
      } else if (isRPressed) {
        state = 1;
        triggeringKey = shiftR;
      }
      break;

    case 1: // Waiting for release
      {
        bool stillPressed = (keyMap[triggeringKey / 8] & (1 << (triggeringKey % 8)));
        if (!stillPressed) {
          state = 2;
          lastTime = now;
        }
      }
      break;

    case 2: // Waiting for second press
      if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastTime) > timeout) {
        state = 0;
      } else {
        bool rePressed = (keyMap[triggeringKey / 8] & (1 << (triggeringKey % 8)));
        if (rePressed) {
          state = 3;
        }
      }
      break;

    case 3: // Triggered
      if (verbose) {
        std::cout << "TRIGGER DETECTED!" << std::endl;
      }
      Logger::instance().log("InputHook: Shift double-tap trigger detected.");
      
      // Wait for release
      while (true) {
         XQueryKeymap(m_display, keyMap);
         if (!(keyMap[triggeringKey / 8] & (1 << (triggeringKey % 8)))) break;
         std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }
      return true;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  return false;
}

#endif // VOICECLI_SRC_INPUTHOOK_HPP
