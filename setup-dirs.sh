#!/bin/bash
# Setup script to create all necessary directories

echo "Creating directory structure for all platforms and architectures..."

# Create main build directory
mkdir -p build

# Create Windows directories
mkdir -p build/windows/x64/bin
mkdir -p build/windows/x64/lib
mkdir -p build/windows/x86/bin
mkdir -p build/windows/x86/lib

# Create Linux directories
mkdir -p build/linux/x64/bin
mkdir -p build/linux/x64/lib
mkdir -p build/linux/x86/bin
mkdir -p build/linux/x86/lib

echo "Directory structure created successfully!"
echo ""
echo "Structure:"
echo "build/"
echo "├── windows/"
echo "│   ├── x64/"
echo "│   │   ├── bin/"
echo "│   │   └── lib/"
echo "│   └── x86/"
echo "│       ├── bin/"
echo "│       └── lib/"
echo "└── linux/"
echo "    ├── x64/"
echo "    │   ├── bin/"
echo "    │   └── lib/"
echo "    └── x86/"
echo "        ├── bin/"
echo "        └── lib/"
