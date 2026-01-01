# VoiceCLI

VoiceCLI is a highly customizable and robust voice command-line interface designed to streamline your workflow through efficient speech-to-text transcription and powerful post-processing capabilities.

## ✨ Features
*   **Smart Pause (VAD):** Intelligently pauses recording during silence and resumes on voice, preserving your session time.
*   **Intuitive UI:** A clean, always-on-top status window with a dynamic, spectrum-gradient volume meter.
*   **Configurable Hotkeys:** Customize the global double-tap trigger key (Shift, Control, Alt, Super) to suit your preferences.
*   **Extensible Post-processing:** Pipe transcribed text through any shell command or script for limitless text transformations before pasting.
*   **Robust Audio Handling:** Automatic selection of optimal audio devices with detailed capability listings.

## 🚀 Quick Start

### Build
```bash
make
```
(See [Manual - Installation](#1-installation) for detailed prerequisites).

### Run Daemon
```bash
./debug/VoiceCLI &
```

### Trigger Recording
By default, **double-tap the Shift key** (Left or Right) to start/stop recording.

### Basic Usage Flow
1.  Trigger recording.
2.  Speak your command/text.
3.  Pause automatically on silence (VAD).
4.  Speak again to resume.
5.  Press `v` to Transcribe & Paste, `a` to Abort, or `x` to Exit the program.

## 📚 Full Documentation

For a complete guide on installation, advanced features, all command-line options, and troubleshooting, please refer to the [VoiceCLI Manual](docs/MANUAL.md).

## 🤝 Contributing
Feel free to fork the repository and submit pull requests. Bug reports and feature suggestions are welcome!

## 🙏 Acknowledgments
Development of this project, including code, documentation, and feature planning, was assisted by Gemini, a large language model by Google.
