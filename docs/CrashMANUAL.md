# VoiceCLI Crash Reporting Manual

## Introduction

This manual provides an in-depth look at the VoiceCLI's crash reporting mechanism, detailing its implementation, design considerations, and how it functions to aid in debugging.

## Implementation Details

The crash reporting is integrated into the `main()` function in `main.cpp`.

1.  **Signal Registration:** The `crash_handler` function is registered for several critical signals using `std::signal()` at the very beginning of `main()`, before most other operations, to ensure it's in place to catch early crashes.
    *   Signals Handled: `SIGSEGV`, `SIGABRT`, `SIGFPE`, `SIGILL`, `SIGBUS`, `SIGTERM`.

2.  **Crash Report File Handling:** 
    *   A unique filename is generated based on the current timestamp (e.g., `CrashReport-YYYY-MM-DD,HH:MM:SS.log`).
    *   The crash report file is opened using `open()` with flags for creation, truncation, and write-only access (`O_CREAT | O_TRUNC | O_WRONLY`).
    *   The `O_CLOEXEC` flag is used to ensure the file descriptor is closed if the process executes another program (`execve`).
    *   A global static file descriptor `g_crash_report_fd` and filename `g_crash_report_filename` are used to store this information asynchronously.

3.  **`crash_handler` Function:**
    *   This function is designed to be asynchronous-signal-safe. It first flushes `stderr` and then uses `dprintf` to write essential error messages directly to `STDERR_FILENO` for immediate feedback.
    *   It then attempts to open the crash report file descriptor as a `FILE*` using `fdopen` for easier writing.
    *   **Version and Stack Trace:** The `APP_VERSION` and a detailed stack trace (obtained using `backward::StackTrace` and `backward::Printer`) are written to the crash report file.
    *   **Privacy Warnings:** Explicit warnings about sensitive data in logs are included before closing the file.
    *   **Process Termination:** Finally, it uses `_exit(EXIT_FAILURE)` to terminate the program immediately after handling the crash.

4.  **Normal Exit Cleanup:** If the application exits normally (without a crash), the `g_crash_report_fd` is closed, and the temporary crash report file is removed using `std::filesystem::remove`.

## Considerations

*   **Debug vs. Release Builds:** For production/release builds, it is recommended to conditionally compile out the crash reporting mechanism using preprocessor directives (e.g., `#ifdef DEBUG`) to avoid performance overhead and potential privacy concerns with sensitive data in crash logs. This would require modifications to the build system (Makefile) to define a `DEBUG` macro only for debug builds.

## API Usage Example

To use the crash reporting functionality in your C++ project:

1.  **Include necessary headers:**
    ```cpp
    #include <csignal>
    #include "backward.hpp"
    // Potentially other headers for your application setup
    ```

2.  **Define `APP_VERSION`:** During compilation, define `APP_VERSION` (e.g., using a Makefile or compiler flags):
    ```makefile
    # In Makefile:
    APP_VERSION="1.0.0"
    CXXFLAGS += -DAPP_VERSION=\"$(APP_VERSION)\" 
    ```

3.  **Implement and Register Signal Handler:** In your `main` function, define and register your `crash_handler` function for the signals you wish to catch:
    ```cpp
    // Assuming crash_handler is defined elsewhere, e.g., in a utility file
    extern void crash_handler(int sig);

    int main(int argc, char* argv[]) {
        // ... other setup code ...

        // Register signal handlers for crash reporting
        std::signal(SIGSEGV, crash_handler);
        std::signal(SIGABRT, crash_handler);
        std::signal(SIGFPE,  crash_handler);
        std::signal(SIGILL,  crash_handler);
        std::signal(SIGBUS,  crash_handler);
        std::signal(SIGTERM, crash_handler);

        // ... rest of your application logic ...

        return 0;
    }
    ```

4.  **Compilation:** Ensure that `backward-cpp` is available and linked during compilation. The `Makefile` in this project includes the necessary paths and links `backward-cpp` automatically. For optimal debugging of crashes, it is recommended to compile with `-O0` (no optimization) and `-g3` (full debug information) flags, particularly for debug builds.

5.  **Example Project:** For a complete, real-world example of this crash reporting mechanism in action, refer to the VoiceCLI project on GitHub: [https://github.com/OleMissGuy/VoiceCLI](https://github.com/OleMissGuy/VoiceCLI).

By following these steps, your application will be able to generate detailed crash reports, aiding in debugging and stability.
