# Cross-platform Makefile for C++ program

# Include project configuration first
include project.mk

# Include source files configuration
include sources.mk

# Detect OS and architecture (Unix-first)
DETECTED_OS := $(shell uname -s)
PLATFORM := linux
EXECUTABLE := $(EXECUTABLE_NAME)
GUI_EXECUTABLE_NAME := repoman-gui
GUI_EXECUTABLE := $(GUI_EXECUTABLE_NAME)
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
CXXFLAGS := -std=c++$(CXX_STANDARD) -Wall -Wextra -Iinclude -Os -s -ffunction-sections -fdata-sections -MMD -MP
CFLAGS := -std=c$(C_STANDARD) -Wall -Wextra
LDFLAGS := -Wl,--gc-sections -Wl,--strip-all

# If targeting Windows, set executable suffix and prefer MinGW compilers when available
ifeq ($(PLATFORM),windows)
    EXECUTABLE := $(EXECUTABLE_NAME).exe
    GUI_EXECUTABLE := $(GUI_EXECUTABLE_NAME).exe
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
    # Enable C++ threads and link runtime statically so .exe runs without MinGW runtime installed
    CXXFLAGS += -pthread
    LDFLAGS += -pthread -static-libstdc++ -static-libgcc -Wl,-Bstatic -lwinpthread -Wl,-Bdynamic
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
DEPS := $(OBJECTS:.o=.d)
LIB_OBJECTS := $(filter-out $(BUILD_DIR)/main.o,$(OBJECTS))

# ======================
# GUI (Dear ImGui) setup
# ======================
IMGUIDIR := third_party/imgui
FONTS_SRC_DIR := third_party/fonts
FONTS_GEN_DIR := src/fonts
ASSETS_SRC_DIR := assets
ASSETS_GEN_DIR := src/assets
# Core ImGui sources
IMGUI_CORE := \
    $(IMGUIDIR)/imgui.cpp \
    $(IMGUIDIR)/imgui_draw.cpp \
    $(IMGUIDIR)/imgui_tables.cpp \
    $(IMGUIDIR)/imgui_widgets.cpp

# Platform backends
IMGUI_BACKENDS_LINUX := \
    $(IMGUIDIR)/backends/imgui_impl_glfw.cpp \
    $(IMGUIDIR)/backends/imgui_impl_opengl2.cpp
IMGUI_BACKENDS_WINDOWS := \
    $(IMGUIDIR)/backends/imgui_impl_win32.cpp \
    $(IMGUIDIR)/backends/imgui_impl_opengl2.cpp

GUI_CPP_SOURCES := \
    src/gui/main_gui.cpp \
    src/gui/menus/main_window.cpp \
    src/gui/menus/github_window.cpp \
    src/gui/menus/filters_window.cpp \
    src/gui/menus/about_window.cpp \
    src/gui/menus/help_window.cpp

# Generated font headers
FONT_HEADERS := \
    $(FONTS_GEN_DIR)/unispace_rg.h \
    $(FONTS_GEN_DIR)/unispace_it.h \
    $(FONTS_GEN_DIR)/unispace_bd.h \
    $(FONTS_GEN_DIR)/unispace_bd_it.h

# Generated image headers
ASSET_HEADERS := \
    $(ASSETS_GEN_DIR)/logo_png.h

ifeq ($(PLATFORM),windows)
GUI_VENDOR_SOURCES := $(IMGUI_CORE) $(IMGUI_BACKENDS_WINDOWS)
GUI_LDLIBS := -lgdi32 -luser32 -limm32 -lole32 -lopengl32 -lshell32 -ldwmapi -lgdiplus
else
GUI_VENDOR_SOURCES := $(IMGUI_CORE) $(IMGUI_BACKENDS_LINUX)
GUI_LDLIBS := -lglfw -lGL -ldl -lpthread -lX11 -lXrandr -lXi -lXcursor -lXinerama -lpng -lz
endif

GUI_CPP_OBJECTS := $(GUI_CPP_SOURCES:src/gui/%.cpp=$(BUILD_DIR)/gui/%.o)
# Convert ImGui core sources to object files
IMGUI_CORE_OBJECTS := $(IMGUI_CORE:$(IMGUIDIR)/%.cpp=$(BUILD_DIR)/vendor/third_party/imgui/%.o)
# Convert ImGui backend sources to object files  
ifeq ($(PLATFORM),windows)
IMGUI_BACKEND_OBJECTS := $(IMGUI_BACKENDS_WINDOWS:$(IMGUIDIR)/backends/%.cpp=$(BUILD_DIR)/vendor/third_party/imgui/backends/%.o)
else
IMGUI_BACKEND_OBJECTS := $(IMGUI_BACKENDS_LINUX:$(IMGUIDIR)/backends/%.cpp=$(BUILD_DIR)/vendor/third_party/imgui/backends/%.o)
endif
GUI_VENDOR_OBJECTS := $(IMGUI_CORE_OBJECTS) $(IMGUI_BACKEND_OBJECTS) $(BUILD_DIR)/vendor/third_party/stb_image.o
GUI_OBJECTS := $(GUI_CPP_OBJECTS) $(GUI_VENDOR_OBJECTS)
GUI_TARGET := $(BUILD_DIR)/$(GUI_EXECUTABLE)
GUI_DEPS := $(GUI_OBJECTS:.o=.d)

# Default target
all: fonts assets $(TARGET)

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

# Build GUI target (ensure fonts/assets first)
$(GUI_TARGET): imgui_fetch fonts assets $(GUI_OBJECTS) $(LIB_OBJECTS) | $(BUILD_DIR)
	$(CXX) $(GUI_OBJECTS) $(LIB_OBJECTS) $(LDFLAGS) $(GUI_LDLIBS) -o $@
	@echo "Build completed: $(GUI_TARGET)"

# Compile GUI app objects with extra includes for ImGui backends
$(BUILD_DIR)/gui/%.o: src/gui/%.cpp | $(BUILD_DIR)
	@$(MKDIR) $(dir $@)
	$(CXX) $(CXXFLAGS) -Isrc -Iinclude -Ithird_party -I$(IMGUIDIR) -I$(IMGUIDIR)/backends -c $< -o $@

# Ensure main_gui.cpp sees generated headers before compiling
$(BUILD_DIR)/gui/main_gui.o: $(FONT_HEADERS) $(ASSET_HEADERS)

# Vendor ImGui objects
# Map core ImGui sources
$(BUILD_DIR)/vendor/third_party/imgui/%.o: $(IMGUIDIR)/%.cpp | $(BUILD_DIR) imgui_fetch
	@$(MKDIR) $(dir $@)
	$(CXX) $(CXXFLAGS) -Iinclude -I$(IMGUIDIR) -I$(IMGUIDIR)/backends -c $< -o $@

# Map backend sources
$(BUILD_DIR)/vendor/third_party/imgui/backends/%.o: $(IMGUIDIR)/backends/%.cpp | $(BUILD_DIR) imgui_fetch
	@$(MKDIR) $(dir $@)
	$(CXX) $(CXXFLAGS) -Iinclude -I$(IMGUIDIR) -I$(IMGUIDIR)/backends -c $< -o $@

# Build stb_image.c into vendor bucket
$(BUILD_DIR)/vendor/third_party/stb_image.o: third_party/stb_image.c | $(BUILD_DIR)
	@$(MKDIR) $(dir $@)
	$(CXX) $(CXXFLAGS) -Ithird_party -c $< -o $@

# Fetch Dear ImGui if missing
imgui_fetch:
	@if [ ! -d "$(IMGUIDIR)/.git" ]; then \
		echo "Fetching Dear ImGui..."; \
		mkdir -p third_party; \
		if [ -d "$(IMGUIDIR)" ]; then \
			echo "Removing existing incomplete ImGui directory..."; \
			rm -rf "$(IMGUIDIR)"; \
		fi; \
		git clone --depth 1 https://github.com/ocornut/imgui.git $(IMGUIDIR); \
	fi

# Font conversion (requires Python3)
fonts: $(FONT_HEADERS)

assets: $(ASSET_HEADERS)

$(ASSETS_GEN_DIR)/logo_png.h: assets/logo.png tools/otf_to_header.py
	@$(MKDIR) $(ASSETS_GEN_DIR)
	python3 tools/otf_to_header.py "$<" "$@" logo_png

$(FONTS_GEN_DIR)/unispace_rg.h: $(FONTS_SRC_DIR)/Unispace\ Rg.otf tools/otf_to_header.py
	@$(MKDIR) $(FONTS_GEN_DIR)
	python3 tools/otf_to_header.py "$<" "$@" unispace_rg

$(FONTS_GEN_DIR)/unispace_it.h: $(FONTS_SRC_DIR)/Unispace\ It.otf tools/otf_to_header.py
	@$(MKDIR) $(FONTS_GEN_DIR)
	python3 tools/otf_to_header.py "$<" "$@" unispace_it

$(FONTS_GEN_DIR)/unispace_bd.h: $(FONTS_SRC_DIR)/Unispace\ Bd.otf tools/otf_to_header.py
	@$(MKDIR) $(FONTS_GEN_DIR)
	python3 tools/otf_to_header.py "$<" "$@" unispace_bd

$(FONTS_GEN_DIR)/unispace_bd_it.h: $(FONTS_SRC_DIR)/Unispace\ Bd\ It.otf tools/otf_to_header.py
	@$(MKDIR) $(FONTS_GEN_DIR)
	python3 tools/otf_to_header.py "$<" "$@" unispace_bd_it


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
	# Remove only object and dependency files; keep binaries, config, repos, logs
	-$(RM) $(CPP_OBJECTS) $(C_OBJECTS) $(DEPS)
	-$(RM) $(GUI_CPP_OBJECTS) $(GUI_DEPS)
	-$(RM) -r $(BUILD_DIR)/vendor
	@echo "Cleaned objects in: $(BUILD_DIR) (kept executables, repos, config)"

# Optional: remove built executables for current config
clean-bin:
	-$(RM) $(TARGET)
	-$(RM) $(GUI_TARGET)
	@echo "Removed binaries for: $(PLATFORM)/$(ARCH)/$(BUILD_TYPE)"

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
	@echo "  gui      - Build GUI application (Dear ImGui)"
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

.PHONY: all debug release clean clean-current clean-all run info help build gui imgui_fetch

# Alias target for convenience
build: all

# GUI convenience target (ensure fonts)
gui: fonts $(GUI_TARGET)

# Include auto-generated dependency files (if they exist)
-include $(DEPS)
-include $(GUI_DEPS)
