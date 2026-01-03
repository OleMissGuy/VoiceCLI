# VoiceCLI Crash Reporting

## Overview

The VoiceCLI application incorporates an **asynchronous-signal-safe crash reporting mechanism**. Its primary purpose is to capture critical runtime errors (like segmentation faults, illegal instructions, etc.) and generate a detailed report that aids in debugging and understanding the application's state at the time of failure. This mechanism ensures that even when the application encounters a fatal error, it attempts to save diagnostic information before terminating.

## Key Features

*   **Signal Handling:** Catches common fatal signals such as `SIGSEGV`, `SIGABRT`, `SIGFPE`, `SIGILL`, `SIGBUS`, and `SIGTERM`.
*   **Asynchronous-Signal Safety:** All operations within the crash handler are designed to be signal-safe to prevent further corruption or undefined behavior. This includes using `dprintf` to `STDERR_FILENO` for immediate error output and carefully managing file descriptors.
*   **Detailed Crash Reports:** Generates timestamped log files (`CrashReport-YYYY-MM-DD,HH:MM:SS.log`) that include:
    *   Application version (`APP_VERSION`).
    *   The specific signal that caused the crash.
    *   A complete stack trace using the `backward-cpp` library.
    *   Privacy warnings about sensitive data that might be present in the logs.
*   **File Descriptor Management:** Properly opens, manages, and closes the file descriptor for the crash report, ensuring it's closed on `execve` using `O_CLOEXEC`.

## Integrating Crash Reporting

To integrate the crash reporting mechanism into your C++ project:

1.  **Include Headers:** Ensure you include `<csignal>` for signal handling and `"backward.hpp"` for stack trace generation.
2.  **Define `APP_VERSION`:** The application version should be defined via a preprocessor macro (e.g., `-DAPP_VERSION=\"your_version\"`) during compilation, typically managed by the build system (like Makefiles).
3.  **Register Signal Handlers:** In your `main()` function, register a signal handler function (e.g., `crash_handler`) for the desired signals using `std::signal()`:
    ```cpp
    #include <csignal>
    #include "backward.hpp"

    // Your signal handler function
    void crash_handler(int sig) { /* ... implementation ... */ }

    int main(int argc, char* argv[]) {
        // ... setup code ...
        std::signal(SIGSEGV, crash_handler);
        std::signal(SIGABRT, crash_handler);
        // ... register other signals ...
        // ... rest of your main function ...
        return 0;
    }
    ```
4.  **Build System:** Ensure that the `backward-cpp` library is correctly included in your build path and linked. The current project uses a Makefile that handles this integration automatically.

## Usage

Upon a detected crash, the crash handler will execute, generate a detailed report in the project's root directory, and terminate the application.
