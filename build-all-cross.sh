#!/bin/bash
# Build all Windows versions using cross-compilation from Linux

# Read executable name from project.mk
EXECUTABLE_NAME=$(awk -F ':=' '/^EXECUTABLE_NAME[[:space:]]*:=/ {gsub(/^\s+|\s+$/, "", $2); gsub(/[\r\n]/, "", $2); print $2}' project.mk | tr -d '\r')
EXECUTABLE_NAME=${EXECUTABLE_NAME:-repoman-cli}

echo "Building Linux binaries (native) and all Windows versions using cross-compilation..."

# Build Linux x64 (Release and Debug)
echo ""
echo "=== Building Linux x64 Release ==="
make BUILD_TYPE=Release
echo ""
echo "=== Building Linux x64 Debug ==="
make BUILD_TYPE=Debug

# Check if MinGW is installed
if ! command -v x86_64-w64-mingw32-gcc &> /dev/null; then
    echo "Error: x86_64-w64-mingw32-gcc not found!"
    echo "Please install MinGW: ./install-mingw.sh"
    exit 1
fi

if ! command -v i686-w64-mingw32-gcc &> /dev/null; then
    echo "Error: i686-w64-mingw32-gcc not found!"
    echo "Please install MinGW: ./install-mingw.sh"
    exit 1
fi

# Create directory structure
mkdir -p build/windows/x64/bin
mkdir -p build/windows/x86/bin

echo ""
echo "=== Building Windows x64 Release ==="
./build-cross.sh release x64
if [ $? -ne 0 ]; then
    echo "Failed to build Windows x64 Release"
    exit 1
fi

echo ""
echo "=== Building Windows x64 Debug ==="
./build-cross.sh debug x64
if [ $? -ne 0 ]; then
    echo "Failed to build Windows x64 Debug"
    exit 1
fi

echo ""
echo "=== Building Windows x86 Release ==="
./build-cross.sh release x86
if [ $? -ne 0 ]; then
    echo "Failed to build Windows x86 Release"
    exit 1
fi

echo ""
echo "=== Building Windows x86 Debug ==="
./build-cross.sh debug x86
if [ $? -ne 0 ]; then
    echo "Failed to build Windows x86 Debug"
    exit 1
fi

echo ""
echo "=== All builds completed successfully! ==="
echo ""
echo "Linux binaries:"
find build/linux -name "*" -type f | sort
echo ""
echo "Built executables:"
find build/windows -name "${EXECUTABLE_NAME}.exe" -type f | sort

echo ""
echo "File sizes:"
find build/windows -name "${EXECUTABLE_NAME}.exe" -type f -exec ls -lh {} \; | sort
