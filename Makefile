# Cross-platform Makefile for C++ program

# Include project configuration first
include project.mk

# Include source files configuration
include sources.mk

# Detect OS and architecture (Unix-first)
DETECTED_OS := $(shell uname -s)
PLATFORM := linux
EXECUTABLE := $(EXECUTABLE_NAME)
RM := rm -f
MKDIR := mkdir -p

# Detect architecture
ARCH := $(shell uname -m)
ifeq ($(ARCH),x86_64)
    ARCH := x64
else ifeq ($(ARCH),i386)
    ARCH := x86
else ifeq ($(ARCH),i686)
    ARCH := x86
endif

# Override for cross-compilation (detect MinGW by compiler name)
ifneq ($(CC),gcc)
    ifneq ($(CXX),g++)
        ifneq ($(findstring mingw,$(CC)),)
            PLATFORM := windows
        endif
    endif
endif

# Compiler settings (defaults)
CXX := g++
CC := gcc
CXXFLAGS := -std=c++$(CXX_STANDARD) -Wall -Wextra -Iinclude -Os -s -ffunction-sections -fdata-sections
CFLAGS := -std=c$(C_STANDARD) -Wall -Wextra
LDFLAGS := -Wl,--gc-sections -Wl,--strip-all

# If targeting Windows, set executable suffix and prefer MinGW compilers when available
ifeq ($(PLATFORM),windows)
    EXECUTABLE := $(EXECUTABLE_NAME).exe
    # Use CROSS_PREFIX if provided (e.g., x86_64-w64-mingw32- or i686-w64-mingw32-)
    ifdef CROSS_PREFIX
        CC := $(CROSS_PREFIX)gcc
        CXX := $(CROSS_PREFIX)g++
    else
        # Auto-detect MinGW based on ARCH
        ifeq ($(ARCH),x64)
            ifneq ($(shell which x86_64-w64-mingw32-g++ 2>/dev/null),)
                CXX := x86_64-w64-mingw32-g++
                CC := x86_64-w64-mingw32-gcc
            endif
        else ifeq ($(ARCH),x86)
            ifneq ($(shell which i686-w64-mingw32-g++ 2>/dev/null),)
                CXX := i686-w64-mingw32-g++
                CC := i686-w64-mingw32-gcc
            endif
        endif
    endif
    # Link libstdc++ and libgcc statically so .exe runs without MinGW runtime installed
    LDFLAGS += -static-libstdc++ -static-libgcc -Wl,-Bstatic -lwinpthread -Wl,-Bdynamic
endif

# Build type (Debug or Release)
BUILD_TYPE ?= Release

# Set compiler flags based on build type
ifeq ($(BUILD_TYPE),Debug)
    CXXFLAGS += -g -O0 -DDEBUG
else
    CXXFLAGS += -O3 -DNDEBUG
endif

# Build directory structure
BUILD_DIR := build/$(PLATFORM)/$(ARCH)/$(BUILD_TYPE)

# Object files and target
CPP_OBJECTS := $(CPP_SOURCES:src/%.cpp=$(BUILD_DIR)/%.o)
C_OBJECTS := $(C_SOURCES:src/%.c=$(BUILD_DIR)/%.o)
OBJECTS := $(CPP_OBJECTS) $(C_OBJECTS)
TARGET := $(BUILD_DIR)/$(EXECUTABLE)

# Default target
all: $(TARGET)

# Create build directory and compile
$(BUILD_DIR):
	$(MKDIR) $(BUILD_DIR)

$(BUILD_DIR)/%.o: src/%.cpp | $(BUILD_DIR)
	@$(MKDIR) $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: src/%.c | $(BUILD_DIR)
	@$(MKDIR) $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJECTS) | $(BUILD_DIR)
	$(CXX) $(OBJECTS) $(LDFLAGS) -o $@
	@echo "Build completed: $(TARGET)"


# Debug build
debug:
	$(MAKE) BUILD_TYPE=Debug

# Release build
release:
	$(MAKE) BUILD_TYPE=Release

# Clean build artifacts
clean:
	$(RM) build/linux/x64/Release/*.o
	$(RM) build/linux/x64/Debug/*.o
	$(RM) build/linux/x86/Release/*.o
	$(RM) build/linux/x86/Debug/*.o
	$(RM) build/linux/x64/Release/$(EXECUTABLE_NAME)
	$(RM) build/linux/x64/Debug/$(EXECUTABLE_NAME)
	$(RM) build/linux/x86/Release/$(EXECUTABLE_NAME)
	$(RM) build/linux/x86/Debug/$(EXECUTABLE_NAME)
	$(RM) build/windows/x64/bin/repoman-cli.exe
	$(RM) build/windows/x86/bin/repoman-cli.exe
	$(RM) build/windows/x64/Release/*.o
	$(RM) build/windows/x64/Debug/*.o
	$(RM) build/windows/x86/Release/*.o
	$(RM) build/windows/x86/Debug/*.o
	$(RM) build/windows/x64/Release/*.exe
	$(RM) build/windows/x64/Debug/*.exe
	$(RM) build/windows/x86/Release/*.exe
	$(RM) build/windows/x86/Debug/*.exe
	# Remove any leftover non-.exe binaries in Windows dirs (from earlier configs)
	$(RM) build/windows/x64/Release/$(EXECUTABLE_NAME)
	$(RM) build/windows/x64/Debug/$(EXECUTABLE_NAME)
	$(RM) build/windows/x86/Release/$(EXECUTABLE_NAME)
	$(RM) build/windows/x86/Debug/$(EXECUTABLE_NAME)
	@echo "Cleaned all build artifacts"

# Clean only current configuration (scoped by PLATFORM/ARCH/BUILD_TYPE)
clean-current:
	$(RM) $(BUILD_DIR)/*.o
	$(RM) $(TARGET)
	@if [ "$(PLATFORM)" = "windows" ]; then \
		$(RM) build/windows/$(ARCH)/$(BUILD_TYPE)/$(EXECUTABLE_NAME).exe; \
	else \
		$(RM) build/linux/$(ARCH)/$(BUILD_TYPE)/$(EXECUTABLE_NAME); \
	fi
	@echo "Cleaned current configuration: $(PLATFORM)/$(ARCH)/$(BUILD_TYPE)"

# Clean all (including build directories)
clean-all:
	$(RM) -r build/
	@echo "Cleaned all build directories"

# Run the program
run: $(TARGET)
	./$(TARGET)

# Show build information
info:
	$(PROJECT_INFO)
	@echo ""
	@echo "Build Information:"
	@echo "Detected OS: $(DETECTED_OS)"
	@echo "Platform: $(PLATFORM)"
	@echo "Architecture: $(ARCH)"
	@echo "Build type: $(BUILD_TYPE)"
	@echo "Compiler: $(CXX)"
	@echo "Flags: $(CXXFLAGS)"
	@echo "Target: $(TARGET)"
	@echo ""
	@echo "Source Files:"
	$(SOURCES_INFO)

# Help
help:
	@echo "Available targets:"
	@echo "  all      - Build the program (default: Release)"
	@echo "  debug    - Build in Debug mode"
	@echo "  release  - Build in Release mode"
	@echo "  clean    - Remove build artifacts"
	@echo "  clean-all- Remove all build directories"
	@echo "  run      - Build and run the program"
	@echo "  info     - Show build information"
	@echo "  help     - Show this help message"
	@echo ""
	@echo "Usage examples:"
	@echo "  make                # Build in Release mode"
	@echo "  make debug          # Build in Debug mode"
	@echo "  make BUILD_TYPE=Debug  # Alternative debug build"
	@echo "  make run            # Build and run"
	@echo "  make build          # Alias of 'all'"

.PHONY: all debug release clean clean-current clean-all run info help build

# Alias target for convenience
build: all
