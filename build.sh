#!/bin/bash
# Linux build script for RepoMan (Makefile-based)

# Default values
BUILD_TYPE="Release"
CLEAN=false
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
        clean)
            CLEAN=true
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
    echo "Usage: ./build.sh [options]"
    echo ""
    echo "Options:"
    echo "  debug     - Build in Debug mode"
    echo "  release   - Build in Release mode (default)"
    echo "  clean     - Clean build artifacts"
    echo "  help      - Show this help message"
    echo ""
    echo "Examples:"
    echo "  ./build.sh"
    echo "  ./build.sh debug"
    echo "  ./build.sh clean"
    exit 0
fi

# Clean build artifacts
if [ "$CLEAN" = true ]; then
    echo "Cleaning build artifacts..."
    rm -rf build/
    make clean-all 2>/dev/null || true
    echo "Clean completed!"
    exit 0
fi

# Build the program (Make only)
# Read metadata from project.mk
PROJECT_NAME=$(awk -F ':=' '/^PROJECT_NAME[[:space:]]*:=/ {gsub(/^\s+|\s+$/, "", $2); gsub(/[\r\n]/, "", $2); print $2}' project.mk | tr -d '\r')
EXECUTABLE_NAME=$(awk -F ':=' '/^EXECUTABLE_NAME[[:space:]]*:=/ {gsub(/^\s+|\s+$/, "", $2); gsub(/[\r\n]/, "", $2); print $2}' project.mk | tr -d '\r')
EXECUTABLE_NAME=${EXECUTABLE_NAME:-repoman-cli}

echo "Building ${PROJECT_NAME:-Project} (Make)..."
echo "Build type: $BUILD_TYPE"

make BUILD_TYPE="$BUILD_TYPE"
if [ $? -ne 0 ]; then
    echo "Make build failed!"
    exit 1
fi
echo "Build completed successfully!"
