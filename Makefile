CXX      := g++
CXXFLAGS := -std=c++20 -Wall -Wextra
# Include paths for Whisper and GGML
INCLUDES := -Ithird_party/whisper.cpp/include -Ithird_party/whisper.cpp/ggml/include

# Library paths
WHISPER_BUILD := third_party/whisper.cpp/build
LIBS     := $(WHISPER_BUILD)/src/libwhisper.a \
            $(WHISPER_BUILD)/ggml/src/libggml.a \
            $(WHISPER_BUILD)/ggml/src/libggml-cpu.a \
            $(WHISPER_BUILD)/ggml/src/libggml-base.a

# System libraries
LDFLAGS  := -ldl -lpthread -lm -lX11 -lXtst -lgomp

SRC      := main.cpp src/miniaudio_impl.cpp
TARGET   := VoiceCLI

# Output directories
DEBUG_DIR   := debug
RELEASE_DIR := release

.PHONY: all clean debug release run run-release

# Default target
all: debug

# Debug build
debug: $(DEBUG_DIR)/$(TARGET)

$(DEBUG_DIR)/$(TARGET): $(SRC)
	@mkdir -p $(DEBUG_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -g -O0 $(SRC) $(LIBS) -o $@ $(LDFLAGS)

# Release build
release: $(RELEASE_DIR)/$(TARGET)

$(RELEASE_DIR)/$(TARGET): $(SRC)
	@mkdir -p $(RELEASE_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -O3 -DNDEBUG $(SRC) $(LIBS) -o $@ $(LDFLAGS)

# Run target (defaults to debug)
run: debug
	./$(DEBUG_DIR)/$(TARGET)

# Run release target
run-release: release
	./$(RELEASE_DIR)/$(TARGET)

# Clean
clean:
	rm -rf $(DEBUG_DIR) $(RELEASE_DIR)
