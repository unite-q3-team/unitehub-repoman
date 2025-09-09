#!/bin/bash
# Script to install MinGW cross-compilation tools on Linux

echo "Installing MinGW cross-compilation tools..."

# Detect Linux distribution
if command -v apt-get &> /dev/null; then
    # Ubuntu/Debian
    echo "Detected Ubuntu/Debian system"
    sudo apt-get update
    sudo apt-get install -y mingw-w64 gcc-mingw-w64-x86-64 gcc-mingw-w64-i686
    sudo apt-get install -y libglfw3-dev xorg-dev libgl1-mesa-dev libpng-dev zlib1g-dev
    
elif command -v dnf &> /dev/null; then
    # Fedora/CentOS/RHEL
    echo "Detected Fedora/CentOS/RHEL system"
    sudo dnf install -y mingw64-gcc-c++ mingw32-gcc-c++
    
elif command -v pacman &> /dev/null; then
    # Arch Linux
    echo "Detected Arch Linux system"
    sudo pacman -S mingw-w64-gcc
    
elif command -v zypper &> /dev/null; then
    # openSUSE
    echo "Detected openSUSE system"
    sudo zypper install -y mingw64-cross-gcc-c++ mingw32-cross-gcc-c++
    
else
    echo "Unsupported Linux distribution. Please install MinGW manually:"
    echo "  - Ubuntu/Debian: sudo apt-get install mingw-w64"
    echo "  - Fedora/CentOS: sudo dnf install mingw64-gcc-c++"
    echo "  - Arch Linux: sudo pacman -S mingw-w64-gcc"
    exit 1
fi

echo ""
echo "MinGW installation completed!"
echo ""
echo "Available cross-compilers:"
echo "  x86_64-w64-mingw32-gcc    - Windows 64-bit"
echo "  i686-w64-mingw32-gcc      - Windows 32-bit"
echo ""
echo "Testing cross-compilers..."

# Test x64 compiler
if command -v x86_64-w64-mingw32-gcc &> /dev/null; then
    echo "✓ x86_64-w64-mingw32-gcc is available"
    x86_64-w64-mingw32-gcc --version | head -1
else
    echo "✗ x86_64-w64-mingw32-gcc is not available"
fi

# Test x86 compiler
if command -v i686-w64-mingw32-gcc &> /dev/null; then
    echo "✓ i686-w64-mingw32-gcc is available"
    i686-w64-mingw32-gcc --version | head -1
else
    echo "✗ i686-w64-mingw32-gcc is not available"
fi

echo ""
echo "Cross-compilation setup completed!"
