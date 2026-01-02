# VoiceCLI - Detailed Manual

This manual provides comprehensive documentation for VoiceCLI, a robust and customizable voice command-line interface.

## Table of Contents
1.  [Installation](#1-installation)
    *   [1.1. Building Whisper.cpp](#11-building-whispercpp)
2.  [Basic Usage](#2-basic-usage)
    *   [How to Use VoiceCLI: A Workflow Guide](#21-how-to-use-voicecli-a-workflow-guide)
3.  [Features](#3-features)
    *   [Audio Device Selection](#31-audio-device-selection)
    *   [Real-time Volume Meter](#32-real-time-volume-meter)
    *   [Smart Pause (VAD)](#33-smart-pause-vad)
    *   [Configurable Hotkeys](#34-configurable-hotkeys)
    *   [Post-processing](#35-post-processing)
4.  [Command-line Options](#4-command-line-options)
5.  [Troubleshooting](#5-troubleshooting)

---

## 1. Installation

### Prerequisites
*   **Operating System:** Linux with an X11 graphical environment (e.g., Ubuntu, Fedora, Arch Linux).
*   **Hardware:** A functional microphone and a modern CPU (e.g., Intel Core i5/Ryzen 5 or newer) with at least 4GB of RAM for the Whisper model (using `base.en` model or similar).
*   A C++20 compatible compiler (e.g., g++-10 or newer).
*   `make` utility.
*   X11 development libraries (e.g., `libx11-dev`, `libxtst-dev` on Debian/Ubuntu).
*   `miniaudio` and `whisper.cpp` dependencies (included in `third_party/`).

### 1.1. Building Whisper.cpp
VoiceCLI relies on the `whisper.cpp` library. You must build it first before building VoiceCLI.

Navigate to the `third_party/whisper.cpp` directory and run `make`:
```bash
cd third_party/whisper.cpp
make
# (Optional: Download a model)
./models/download-ggml-model.sh base.en
cd ../../
```
**Note:** The `download-ggml-model.sh` script will download the `base.en` model by default, which is configured in VoiceCLI. You can choose other models if desired, but remember to update the `--model` flag when running VoiceCLI.

### Building VoiceCLI
Navigate to the project root and run `make`:
```bash
make
```
This will build the `VoiceCLI` executable in the `debug/` directory. For a release build, use `make release`.

## 2. Basic Usage

To run VoiceCLI, simply execute the `VoiceCLI` daemon:
```bash
./debug/VoiceCLI
```
By default, it will monitor for a **double-tap on the Shift key** to start/stop recording.

### 2.1. How to Use VoiceCLI: A Workflow Guide

VoiceCLI operates on a simple **"trigger, speak, paste"** model, seamlessly integrating with any application where you can type text (e.g., text editors, web browsers, terminal prompts).

1.  **Launch the Daemon:** Start VoiceCLI in the background, typically when your system starts.
    ```bash
    ./debug/VoiceCLI &
    ```

2.  **Trigger Recording:** When you're ready to speak, **place the cursor where you want the transcribed text to appear**, and then trigger voice recognition using the configured hotkey (default: **double-tap `Shift`**).
    *   The `StatusWindow` will appear, indicating that recording has started.

3.  **Speak Naturally:** Begin speaking your text or commands.
    *   The **Volume Meter** will provide visual feedback on your audio level.
    *   If you pause for longer than the VAD timeout, VoiceCLI will automatically enter **Smart Pause** mode, showing "LISTENING... (Paused)". The session timer will also halt.
    *   Simply resume speaking to automatically exit Smart Pause.

4.  **Issue In-Recording Commands (via `StatusWindow`):**
    While the `StatusWindow` is active, you can use these keys:
    *   `v`: **Paste + Space** - Transcribes current speech, pastes it into the last focused window, and adds a trailing space.
    *   `s`: **Paste Only** - Transcribes current speech and pastes it without a trailing space.
    *   `t`: **Terminal Paste** - Transcribes, pastes into the last focused window (simulating `Ctrl+Shift+V`), and adds a trailing space. Useful for terminal emulators.
    *   `r`: **Restart Session** - Clears the current recording and restarts a new session.
    *   `p`: **Pause / Resume** - Manually toggles recording pause. The session timer will halt.
    *   `+`: **Extend Time** - Adds `max-rec-time` minutes to the session limit.
    *   `a` or `Esc`: **Abort Transcribing** - Stops recording and discards the transcription. The window closes.
    *   `x` or `Ctrl+C`: **Exit Program** - Exits the VoiceCLI daemon.

5.  **Pasting the Result:** After you press `v`, `s`, or `t`:
    *   VoiceCLI will transcribe your speech (applying any configured post-processing).
    *   It will then simulate the appropriate paste command (`Ctrl+V` or `Ctrl+Shift+V`) into the window that was active *before* you triggered VoiceCLI.
    *   The `StatusWindow` will close automatically.

This workflow allows you to quickly dictate text or commands without manually switching applications or copy-pasting.

## 3. Features

### 3.1. Audio Device Selection
VoiceCLI automatically attempts to select the most suitable audio input device, prioritizing:
1.  The system's default capture device.
2.  Devices that natively support 1 (mono) channel.
3.  Devices that support 16-bit or higher audio formats.

You can explicitly list available devices or select a specific one:
*   **List Devices:** `./debug/VoiceCLI -l`
*   **Select Device:** `./debug/VoiceCLI -d <index>` (e.g., `-d 0`)

### 3.2. Real-time Volume Meter
The `StatusWindow` displays a dynamic volume bar that provides instant feedback on your audio input level.
*   The bar progresses from **Green** (optimal level) to **Yellow** (approaching peak) to **Red** (potential clipping).
*   The meter operates on a logarithmic (dB) scale, with a -40dB floor for increased responsiveness.

### 3.3. Smart Pause (VAD)
Voice Activity Detection (VAD) intelligently manages your recording session:
*   **Auto-Pause:** If silence is detected for a configurable `vad-timeout`, recording will automatically pause (audio is no longer written to disk), and the UI will show "LISTENING... (Paused)". The session timer also pauses.
*   **Auto-Resume:** When voice activity is detected again, recording automatically resumes, and the timer restarts.
*   **Manual Override:** You can manually pause/resume with `p`.

Configure VAD sensitivity and timeout:
*   `--vad-threshold <val>`: Set silence detection sensitivity (0.0 to 1.0, default 0.05). Higher values require louder sound to be considered "speech."
*   `--vad-timeout <ms>`: Set the duration of silence (in milliseconds) before auto-pausing (default 2000ms).

### 3.4. Configurable Hotkeys
Customize the trigger key for starting and stopping recording:
*   **Default:** Double-tap `Shift` (Left or Right).
*   **Configuration:** Use `--trigger-key <key>` to specify `Control`, `Alt`, or `Super`. (Case-insensitive)
    *   Example: `./debug/VoiceCLI --trigger-key Control`

### 3.5. Post-processing
Extend VoiceCLI's capabilities by piping transcribed text through any shell command before it's pasted.
*   **Usage:** `--post-process "<your_command_here>"`
*   **Mechanism:** VoiceCLI sends the raw transcription to your command's `stdin` and pastes whatever is output to `stdout`.
*   **Example (Python script to convert to snake_case):**
    ```python
    #!/usr/bin/env python3
    import sys
    text = sys.stdin.read().strip()
    if "snake case" in text.lower():
        text = text.lower().replace("snake case ", "").replace(" ", "_")
    sys.stdout.write(text)
    ```
    Then run: `./debug/VoiceCLI --post-process "./path/to/your/script.py"`

## 4. Command-line Options

```text
Usage: VoiceCLI [OPTIONS]

Options:
  -h, --help                Show this help message
  -l, --list-audio-devices  List available audio capture devices
  -d, --device-index <idx>  Select a specific audio capture device by index
  -m, --model <path>        Path to Whisper model file (default: models/ggml-base.en.bin)
  -M, --max-rec-time <min>  Set max record time per session (default: 5 min)
  -r, --sample-rate <hz>    Set recording sample rate (default: 16000)
  -t, --test-record         Record 5 seconds of audio to verify input
  -v, --verbose             Enable verbose output (e.g., print selected device)
  -S, --vad-threshold <val> Set VAD sensitivity (0.0 to 1.0, default 0.05)
  -T, --vad-timeout <ms>    Set VAD silence timeout in ms (default 2000)
  -k, --trigger-key <key>   Set double-tap trigger key (Shift, Control, Alt, Super; default Shift)
  -P, --post-process <cmd>  Shell command to process text before pasting
```

## 5. Troubleshooting

*   **"Failed to open X Display"**: Ensure you are running VoiceCLI in an X11 environment (not SSH without X forwarding).
*   **"No microphone available"**: Verify your microphone is connected and detected by your system. Use `-l` flag to see available devices.
*   **"Failed to initialize Whisper context"**: Check the `modelPath`. Ensure the `ggml-base.en.bin` (or your chosen model) is present at the specified path.
*   **Input not recognized in Auto Pause**: This was a known issue and has been addressed. Ensure you are running the latest version. If it persists, ensure your X11 libraries are up to date.
*   **Post-processing script not working**:
    *   Ensure the script is executable (`chmod +x your_script.py`).
    *   Verify the path to the script is correct.
    *   Test your script independently by piping text to its stdin: `echo "hello world" | ./your_script.py`.
    *   Check `voicecli.log` for any errors from `runPostProcess`.
