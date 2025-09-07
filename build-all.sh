#!/bin/bash
# Build script for Linux targets (Make-only)

# Default values
BUILD_TYPE="Release"
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
    echo "Usage: ./build-all.sh [debug|release]"
    echo "Builds Linux (native) targets using Make."
    exit 0
fi

echo "Building targets with Make..."
echo "Build type: $BUILD_TYPE"

# Linux x64
make BUILD_TYPE="$BUILD_TYPE"
if [ $? -ne 0 ]; then
    echo "Make build failed for Linux x64!"
    exit 1
fi

echo "Build completed successfully!"
