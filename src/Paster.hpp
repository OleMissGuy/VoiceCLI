#ifndef VOICECLI_SRC_PASTER_HPP
#define VOICECLI_SRC_PASTER_HPP

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/extensions/XTest.h>
#include <string>
#include <iostream>
#include <thread>
#include <chrono>
#include <cstring>

class Paster {
public:
  Paster();
  ~Paster();

  // Disable copying
  Paster(const Paster&) = delete;
  Paster& operator=(const Paster&) = delete;

  // Copies text to clipboard and simulates Ctrl+V (or Ctrl+Shift+V if useShift is true)
  void paste(const std::string& text, Window targetWindow = 0, bool useShift = false, bool verbose = false);

private:
  Display* m_display;
  Window m_window;
};

// -----------------------------------------------------------------------------
// Inline Implementations
// -----------------------------------------------------------------------------

inline Paster::Paster() : m_display(nullptr) {
  m_display = XOpenDisplay(NULL);
  if (!m_display) {
    throw std::runtime_error("Failed to open X Display for Paster.");
  }
  // Create an invisible window to own the selection
  m_window = XCreateSimpleWindow(m_display, DefaultRootWindow(m_display), 
                                 0, 0, 1, 1, 0, 0, 0);
}

inline Paster::~Paster() {
  if (m_display) {
    XDestroyWindow(m_display, m_window);
    XCloseDisplay(m_display);
  }
}

inline void Paster::paste(const std::string& text, Window targetWindow, bool useShift, bool verbose) {
  if (text.empty()) return;

  Atom clipboard = XInternAtom(m_display, "CLIPBOARD", False);
  Atom utf8String = XInternAtom(m_display, "UTF8_STRING", False);
  Atom targets = XInternAtom(m_display, "TARGETS", False);

  // 1. Set Selection Owner
  XSetSelectionOwner(m_display, clipboard, m_window, CurrentTime);
  if (XGetSelectionOwner(m_display, clipboard) != m_window) {
    std::cerr << "Failed to acquire clipboard ownership." << std::endl;
    return;
  }

  // Restore focus if a target window was provided
  if (targetWindow != 0) {
      if (verbose) {
        std::cout << "Paster: Restoring focus to Window ID: " << targetWindow << std::endl;
      }
      XSetInputFocus(m_display, targetWindow, RevertToParent, CurrentTime);
      XFlush(m_display);
  }

  // 2. Simulate Ctrl+V (Wait a bit for focus to settle if window was just closed)
  std::this_thread::sleep_for(std::chrono::milliseconds(500));

  KeyCode ctrlKey = XKeysymToKeycode(m_display, XK_Control_L);
  KeyCode shiftKey = XKeysymToKeycode(m_display, XK_Shift_L);
  KeyCode vKey = XKeysymToKeycode(m_display, XK_v);

  XTestFakeKeyEvent(m_display, ctrlKey, True, 0);  // Ctrl Down
  if (useShift) {
      XTestFakeKeyEvent(m_display, shiftKey, True, 0); // Shift Down
  }
  XTestFakeKeyEvent(m_display, vKey, True, 0);     // V Down
  XTestFakeKeyEvent(m_display, vKey, False, 0);    // V Up
  if (useShift) {
      XTestFakeKeyEvent(m_display, shiftKey, False, 0); // Shift Up
  }
  XTestFakeKeyEvent(m_display, ctrlKey, False, 0); // Ctrl Up
  XFlush(m_display);

  // 3. Serve the SelectionRequest
  // We need to wait for the target app to ask for the data.
  // We'll timeout after 5 seconds if no one asks.
  auto start = std::chrono::steady_clock::now();
  XEvent e;

  bool served = false;

  while (true) {
    // Check timeout
    if (std::chrono::steady_clock::now() - start > std::chrono::seconds(5)) {
      break; 
    }

    // Non-blocking check? XNextEvent blocks. 
    // Use XPending to poll or just block with a select? 
    // Since we just triggered the paste, it should happen fast. 
    // But if we block forever and the app ignores it, we hang.
    if (XPending(m_display) > 0) {
        XNextEvent(m_display, &e);
    } else {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        continue;
    }

    if (e.type == SelectionRequest) {
      if (e.xselectionrequest.selection == clipboard) {
        XSelectionEvent s;
        s.type = SelectionNotify;
        s.requestor = e.xselectionrequest.requestor;
        s.selection = e.xselectionrequest.selection;
        s.target = e.xselectionrequest.target;
        s.property = e.xselectionrequest.property;
        s.time = e.xselectionrequest.time;

        if (e.xselectionrequest.target == targets) {
            // They want to know what we support
            Atom supported[] = { utf8String, XA_STRING };
            XChangeProperty(m_display, s.requestor, s.property, XA_ATOM, 32, 
                            PropModeReplace, (unsigned char*)supported, 2);
        } else if (e.xselectionrequest.target == utf8String || e.xselectionrequest.target == XA_STRING) {
            // Send the text
            XChangeProperty(m_display, s.requestor, s.property, e.xselectionrequest.target, 8, 
                            PropModeReplace, (unsigned char*)text.c_str(), text.length());
            served = true;
        } else {
            // Unsupported
            s.property = None; 
        }

        XSendEvent(m_display, e.xselectionrequest.requestor, True, 0, (XEvent*)&s);
        XFlush(m_display);
        
        // If we successfully served the text, we can probably exit the loop soon.
        // Some apps might request TARGETS first, then STRING. So don't break immediately on TARGETS.
        if (served) break; 
      }
    } else if (e.type == SelectionClear) {
      // We lost ownership
      break;
    }
  }
}

#endif // VOICECLI_SRC_PASTER_HPP
