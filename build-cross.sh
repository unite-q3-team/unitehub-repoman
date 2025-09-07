#!/bin/bash
# Cross-compilation build script for Windows from Linux

# Default values
BUILD_TYPE="Release"
TARGET_ARCH="x64"
HELP=false

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        debug)
            BUILD_TYPE="Debug"
            shift
            ;;
        release)
            BUILD_TYPE="Release"
            shift
            ;;
        x64)
            TARGET_ARCH="x64"
            shift
            ;;
        x86)
            TARGET_ARCH="x86"
            shift
            ;;
        help)
            HELP=true
            shift
            ;;
        *)
            echo "Unknown option: $1"
            echo "Use 'help' for usage information"
            exit 1
            ;;
    esac
done

# Show help
if [ "$HELP" = true ]; then
    echo "Usage: ./build-cross.sh [options]"
    echo ""
    echo "Options:"
    echo "  debug     - Build in Debug mode"
    echo "  release   - Build in Release mode (default)"
    echo "  x64       - Build for Windows x64 (default)"
    echo "  x86       - Build for Windows x86"
    echo "  help      - Show this help message"
    echo ""
    echo "Examples:"
    echo "  ./build-cross.sh                    # Release x64"
    echo "  ./build-cross.sh debug x86          # Debug x86"
    echo "  ./build-cross.sh release x64        # Release x64"
    exit 0
fi

# Check if MinGW is installed
if [ "$TARGET_ARCH" = "x64" ]; then
    if ! command -v x86_64-w64-mingw32-gcc &> /dev/null; then
        echo "Error: x86_64-w64-mingw32-gcc not found!"
        echo "Please install MinGW: ./install-mingw.sh"
        exit 1
    fi
    CROSS_CC="x86_64-w64-mingw32-gcc"
    CROSS_CXX="x86_64-w64-mingw32-g++"
    CROSS_PREFIX="x86_64-w64-mingw32"
else
    if ! command -v i686-w64-mingw32-gcc &> /dev/null; then
        echo "Error: i686-w64-mingw32-gcc not found!"
        echo "Please install MinGW: ./install-mingw.sh"
        exit 1
    fi
    CROSS_CC="i686-w64-mingw32-gcc"
    CROSS_CXX="i686-w64-mingw32-g++"
    CROSS_PREFIX="i686-w64-mingw32"
fi

# Read metadata from project.mk
PROJECT_NAME=$(awk -F ':=' '/^PROJECT_NAME[[:space:]]*:=/ {gsub(/^\s+|\s+$/, "", $2); gsub(/[\r\n]/, "", $2); print $2}' project.mk | tr -d '\r')
EXECUTABLE_NAME=$(awk -F ':=' '/^EXECUTABLE_NAME[[:space:]]*:=/ {gsub(/^\s+|\s+$/, "", $2); gsub(/[\r\n]/, "", $2); print $2}' project.mk | tr -d '\r')
EXECUTABLE_NAME=${EXECUTABLE_NAME:-repoman-cli}

echo "Cross-compiling ${PROJECT_NAME:-Project} for Windows $TARGET_ARCH..."
echo "Build type: $BUILD_TYPE"
echo "Cross-compiler: $CROSS_CXX"

# Create necessary directories
mkdir -p build/windows/$TARGET_ARCH/bin
mkdir -p build/windows/$TARGET_ARCH/lib

# Set compiler flags
if [ "$BUILD_TYPE" = "Debug" ]; then
    CXXFLAGS="-g -O0 -DDEBUG"
else
    CXXFLAGS="-O3 -DNDEBUG"
fi

# Build using Make with cross-compilation
echo "Building with Make..."
make PLATFORM=windows ARCH="$TARGET_ARCH" BUILD_TYPE="$BUILD_TYPE" clean-current
make PLATFORM=windows ARCH="$TARGET_ARCH" BUILD_TYPE="$BUILD_TYPE" CC="$CROSS_CC" CXX="$CROSS_CXX" CXXFLAGS="-std=c++17 -Wall -Wextra $CXXFLAGS"

if [ $? -eq 0 ]; then
    echo ""
    echo "Cross-compilation completed successfully!"
    WINDOWS_EXE="build/windows/$TARGET_ARCH/$BUILD_TYPE/${EXECUTABLE_NAME}.exe"
    echo "Windows executable: $WINDOWS_EXE"
    echo ""
    echo "File information:"
    file "$WINDOWS_EXE"
    ls -lh "$WINDOWS_EXE"
else
    echo "Cross-compilation failed!"
    exit 1
fi
