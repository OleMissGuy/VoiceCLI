#ifndef VOICECLI_SRC_INPUTHOOK_HPP
#define VOICECLI_SRC_INPUTHOOK_HPP

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>
#include <algorithm>
#include <cctype>

#include "Logger.hpp"

class InputHook {
public:
  InputHook();
  ~InputHook();

  // Disable copying
  InputHook(const InputHook&) = delete;
  InputHook& operator=(const InputHook&) = delete;

  // Blocks and monitors input until a trigger is detected. Returns true if triggered.
  bool monitor(const std::string& keyName, bool verbose = false);

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

inline bool InputHook::monitor(const std::string& keyName, bool verbose) {
  m_running = true;
  
  std::string lowerKey = keyName;
  std::transform(lowerKey.begin(), lowerKey.end(), lowerKey.begin(),
      [](unsigned char c){ return std::tolower(c); });
  
  KeySym symL = XK_Shift_L;
  KeySym symR = XK_Shift_R;

  if (lowerKey == "control") {
      symL = XK_Control_L;
      symR = XK_Control_R;
  } else if (lowerKey == "alt") {
      symL = XK_Alt_L;
      symR = XK_Alt_R;
  } else if (lowerKey == "super") {
      symL = XK_Super_L;
      symR = XK_Super_R;
  } else if (lowerKey != "shift") {
      std::cerr << "Warning: Unknown trigger key '" << keyName << "'. Defaulting to Shift." << std::endl;
  }

  KeyCode codeL = XKeysymToKeycode(m_display, symL);
  KeyCode codeR = XKeysymToKeycode(m_display, symR);

  if (verbose) {
    std::cout << "InputHook: Monitoring for " << keyName << " double-tap (Left or Right)..." << std::endl;
  }

  int state = 0;
  auto lastTime = std::chrono::steady_clock::now();
  const auto timeout = std::chrono::milliseconds(400);

  KeyCode triggeringKey = 0;
  char keyMap[32];

  while (m_running) {
    XQueryKeymap(m_display, keyMap);

    bool isLPressed = (keyMap[codeL / 8] & (1 << (codeL % 8)));
    bool isRPressed = (keyMap[codeR / 8] & (1 << (codeR % 8)));
    
    auto now = std::chrono::steady_clock::now();

    switch (state) {
    case 0: // Idle
      if (isLPressed) {
        state = 1;
        triggeringKey = codeL;
      } else if (isRPressed) {
        state = 1;
        triggeringKey = codeR;
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
        std::cout << "TRIGGER DETECTED (" << keyName << ")!" << std::endl;
      }
      Logger::instance().log(std::format("InputHook: {} double-tap trigger detected.", keyName));
      
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
