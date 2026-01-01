#ifndef VOICECLI_SRC_STATUSWINDOW_HPP
#define VOICECLI_SRC_STATUSWINDOW_HPP

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <string>
#include <iostream>
#include <cmath>
#include <algorithm>
#include <vector>

class StatusWindow {
public:
  StatusWindow();
  ~StatusWindow();

  void show(const std::string& initialText);
  void updateText(const std::string& text, float volumeLevel = -1.0f);
  void setBackgroundColor(const std::string& colorName);
  bool checkForInput(char& outKey);
  void close();
  char waitForKey();

private:
  Display* m_display;
  Window m_window;
  GC m_gc;
  int m_screen;
  bool m_visible;
  unsigned long m_currentBg;
  unsigned long m_fgColor;
  std::vector<unsigned long> m_gradientColors;
  std::string m_lastColorName;
  XFontStruct* m_font;
};

// -----------------------------------------------------------------------------
// Inline Implementations
// -----------------------------------------------------------------------------

inline StatusWindow::StatusWindow() : m_display(nullptr), m_visible(false), m_font(nullptr) {
  m_display = XOpenDisplay(NULL);
  if (!m_display) {
    throw std::runtime_error("Failed to open X Display for Status Window.");
  }
  m_screen = DefaultScreen(m_display);
  m_currentBg = WhitePixel(m_display, m_screen);
  m_fgColor = BlackPixel(m_display, m_screen);
  m_lastColorName = "white";

  // Generate Gradient Ramp (Green -> Yellow -> Red)
  Colormap cm = DefaultColormap(m_display, m_screen);
  m_gradientColors.resize(64);
  
  for (int i = 0; i < 64; ++i) {
      float t = (float)i / 63.0f;
      int r, g, b;
      
      if (t < 0.5f) {
          // Green to Yellow
          float localT = t * 2.0f;
          r = (int)(255.0f * localT);
          g = 255;
          b = 0;
      } else {
          // Yellow to Red
          float localT = (t - 0.5f) * 2.0f;
          r = 255;
          g = (int)(255.0f * (1.0f - localT));
          b = 0;
      }
      
      XColor col;
      col.red = r * 257; // Scale 8-bit to 16-bit
      col.green = g * 257;
      col.blue = b * 257;
      col.flags = DoRed | DoGreen | DoBlue;
      
      if (XAllocColor(m_display, cm, &col)) {
          m_gradientColors[i] = col.pixel;
      } else {
          m_gradientColors[i] = BlackPixel(m_display, m_screen);
      }
  }

  // Try to load a larger, cleaner font
  const char* fontNames[] = { 
      "-adobe-helvetica-bold-r-normal--18-*-*-*-*-*-*-*", // Reduced from 20
      "-misc-fixed-bold-r-normal--16-*-*-*-*-*-*-*",      // Reduced from 18
      "10x20", 
      "9x15", 
      "fixed" 
  };
  for (const char* name : fontNames) {
      m_font = XLoadQueryFont(m_display, name);
      if (m_font) break;
  }
  if (!m_font) {
      // Fallback if nothing found (rare)
      std::cerr << "Warning: Could not load any preferred font." << std::endl;
  }
}

inline StatusWindow::~StatusWindow() {
  close();
  if (m_font) {
      XFreeFont(m_display, m_font);
  }
  if (m_display) {
    XCloseDisplay(m_display);
  }
}

inline void StatusWindow::setBackgroundColor(const std::string& colorName) {
  if (!m_visible) return;
  if (colorName == m_lastColorName) return;

  XColor color;
  Colormap colormap = DefaultColormap(m_display, m_screen);
  if (XParseColor(m_display, colormap, colorName.c_str(), &color) &&
      XAllocColor(m_display, colormap, &color)) {
    m_currentBg = color.pixel;
    XSetWindowBackground(m_display, m_window, m_currentBg);
    // XClearWindow(m_display, m_window); // updateText will handle clearing
    m_lastColorName = colorName;
  }
}

inline void StatusWindow::show(const std::string& initialText) {
  if (m_visible) return;

  m_currentBg = WhitePixel(m_display, m_screen); // Reset to white
  m_fgColor = BlackPixel(m_display, m_screen);

  // Screen dimensions for centering
  int screenWidth = DisplayWidth(m_display, m_screen);
  int screenHeight = DisplayHeight(m_display, m_screen);
  int winW = 400;
  int winH = 350;
  int x = (screenWidth - winW) / 2;
  int y = (screenHeight - winH) / 2;

  m_window = XCreateSimpleWindow(m_display, DefaultRootWindow(m_display), 
                                 x, y, winW, winH, 
                                 1, m_fgColor, m_currentBg);

  // Set Window Title
  XStoreName(m_display, m_window, "VoiceCLI Status");

  // Inform Window Manager of position
  XSizeHints hints;
  hints.flags = PPosition | PSize;
  hints.x = x;
  hints.y = y;
  hints.width = winW;
  hints.height = winH;
  XSetWMNormalHints(m_display, m_window, &hints);

  // Always On Top
  Atom wmState = XInternAtom(m_display, "_NET_WM_STATE", False);
  Atom wmStateAbove = XInternAtom(m_display, "_NET_WM_STATE_ABOVE", False);
  XChangeProperty(m_display, m_window, wmState, XA_ATOM, 32, PropModeReplace, 
                  (unsigned char*)&wmStateAbove, 1);

  // Select Inputs
  XSelectInput(m_display, m_window, ExposureMask | KeyPressMask | StructureNotifyMask);

  // Map (Show) Window
  XMapWindow(m_display, m_window);
  m_visible = true;

  // Wait for MapNotify
  XEvent e;
  while (1) {
    XNextEvent(m_display, &e);
    if (e.type == MapNotify) break;
  }
  
  // Create Graphics Context
  m_gc = XCreateGC(m_display, m_window, 0, 0);
  XSetForeground(m_display, m_gc, m_fgColor);
  XSetBackground(m_display, m_gc, m_currentBg);
  
  if (m_font) {
      XSetFont(m_display, m_gc, m_font->fid);
  }

  // Final move to be sure (some WMs need this after map)
  XMoveWindow(m_display, m_window, x, y);
  XRaiseWindow(m_display, m_window);
  XSetInputFocus(m_display, m_window, RevertToParent, CurrentTime);
  
  updateText(initialText);
}

inline void StatusWindow::updateText(const std::string& text, float volumeLevel) {
  if (!m_visible) return;

  XSetForeground(m_display, m_gc, m_fgColor);
  XSetBackground(m_display, m_gc, m_currentBg);

  // Clear area (naive redraw)
  XClearWindow(m_display, m_window);

  int lineHeight = 20;
  if (m_font) {
      lineHeight = m_font->ascent + m_font->descent + 2;
  }

  int y = 30; // Start margin
  // If font is large, start lower to fit ascender
  if (m_font) y = m_font->ascent + 10;

  std::string line;
  std::string::size_type pos = 0;
  std::string::size_type prev = 0;
  
  std::string s = text;
  while ((pos = s.find('\n', prev)) != std::string::npos) {
      line = s.substr(prev, pos - prev);
      XDrawString(m_display, m_window, m_gc, 20, y, line.c_str(), line.length());
      y += lineHeight;
      prev = pos + 1;
  }
  line = s.substr(prev);
  if (!line.empty()) {
      XDrawString(m_display, m_window, m_gc, 20, y, line.c_str(), line.length());
  }

  // Draw Volume Bar
  if (volumeLevel >= 0.0f) {
      int barX = 20;
      int winH = 350; // Updated height
      int barH = 15;
      int barY = winH - 40; // 40px from bottom
      int barW = 360; // winW (400) - 2*margin(20)
      
      // Draw border
      XSetForeground(m_display, m_gc, m_fgColor);
      XDrawRectangle(m_display, m_window, m_gc, barX, barY, barW, barH);
      
      // Calculate dB and percentage
      // Range: -40dB (0%) to 0dB (100%)
      float db = 20.0f * std::log10(volumeLevel + 1e-9f);
      float floor = -40.0f;
      float pct = (db - floor) / (0.0f - floor);
      pct = std::clamp(pct, 0.0f, 1.0f);
      
      int fillW = (int)(barW * pct);
      
      // Draw Gradient Fill
      if (fillW > 0) {
          for (int x = 0; x < fillW; ++x) {
              float pos = (float)x / (float)barW; // Position in bar (0.0 - 1.0)
              
              // We want the COLOR to represent the dB level at that point in the bar.
              // Mapping:
              // 0.0 -> Green
              // 0.75 (-10dB) -> Yellow
              // 0.925 (-3dB) -> Red
              
              int idx;
              if (pos < 0.75f) {
                  // Interpolate 0 to 0.5 in gradient ramp (Green to Yellow)
                  float t = pos / 0.75f;
                  idx = (int)(t * 31.0f);
              } else {
                  // Interpolate 0.5 to 1.0 in gradient ramp (Yellow to Red)
                  float t = (pos - 0.75f) / 0.25f;
                  idx = 32 + (int)(t * 31.0f);
              }
              
              if (idx < 0) idx = 0;
              if (idx > 63) idx = 63;
              
              XSetForeground(m_display, m_gc, m_gradientColors[idx]);
              XDrawLine(m_display, m_window, m_gc, barX + x, barY + 1, barX + x, barY + barH - 1);
          }
      }
  }
  
  XFlush(m_display);
}

inline bool StatusWindow::checkForInput(char& outKey) {
  if (!m_visible) return false;
  
  bool keyFound = false;
  
  // Drain the event queue to prevent buildup of ignored events (Expose, etc.)
  while (XPending(m_display) > 0) {
    XEvent e;
    XNextEvent(m_display, &e);
    
    if (e.type == KeyPress) {
        char buffer[10];
        KeySym keysym;
        int count = XLookupString(&e.xkey, buffer, sizeof(buffer), &keysym, NULL);
        if (count == 1) {
            outKey = buffer[0];
            keyFound = true;
        }
    }
  }
  return keyFound;
}

inline char StatusWindow::waitForKey() {
  if (!m_visible) return 0;
  
  XEvent e;
  while (true) {
    XNextEvent(m_display, &e);
    if (e.type == KeyPress) {
        char buffer[10];
        KeySym keysym;
        int count = XLookupString(&e.xkey, buffer, sizeof(buffer), &keysym, NULL);
        if (count == 1) {
            return buffer[0];
        }
    }
  }
  return 0;
}

inline void StatusWindow::close() {
  if (m_visible) {
    XFreeGC(m_display, m_gc);
    XDestroyWindow(m_display, m_window);
    m_visible = false;
  }
}

#endif // VOICECLI_SRC_STATUSWINDOW_HPP