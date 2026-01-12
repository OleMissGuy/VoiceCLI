#!/bin/bash

# Navigate to the project root directory if not already there.
# The script is intended to be run from the project root.
# If you are in a subdirectory, you might need to adjust the paths or run it from the root.

# --- Git Operations ---

# 1. Commit the staged changes
# Ensure all intended changes are staged before running this script.
# Using a here-document for the commit message for better robustness.
git commit -m "$(cat <<EOF
feat: Implement and refine core VoiceCLI features

This commit introduces several key improvements to the VoiceCLI application:

- Implemented robust, async-signal-safe crash reporting with detailed stack traces and warnings, ensuring crash reports are generated reliably.
- Introduced conditional transcription logging, allowing users to opt-in via the \`-L\` flag for privacy and resource management.
- Enhanced the pasting mechanism using a reliable X11 event loop to ensure correct text transfer to the active window.
- Corrected logging to include version information and command-line arguments on application startup.
- Removed temporary crash-inducing code and debug logs, ensuring a stable release candidate.
EOF
)"

# Check if the commit was successful
if [ $? -ne 0 ]; then
    echo "Error: Git commit failed. Please check the output above."
    exit 1
fi

echo "Changes committed successfully."

# 2. Switch to the master branch
git checkout master

# Check if switching branch was successful
if [ $? -ne 0 ]; then
    echo "Error: Failed to switch to master branch. Please check the output above."
    exit 1
fi

echo "Switched to master branch."

# 3. Merge the feature/crash-reporting branch into master
git merge feature/crash-reporting

# Check if the merge was successful
if [ $? -ne 0 ]; then
    echo "Error: Git merge failed. Please check the output above."
    echo "You may need to resolve merge conflicts manually."
    exit 1
fi

echo "Merge from feature/crash-reporting into master completed successfully."

echo "All Git operations completed."

