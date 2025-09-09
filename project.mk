# Project configuration for Make
# Edit these settings to customize your project

# Project name
PROJECT_NAME := RepoMan

# Executable name (without extension)
EXECUTABLE_NAME := repoman-cli

# Project version
PROJECT_VERSION := 0.1.0

# Project description
PROJECT_DESCRIPTION := A simple cross-platform C++ CLI program

# Author information
PROJECT_AUTHOR := q3unite
PROJECT_EMAIL := admin@q3unite.su

# License
PROJECT_LICENSE := MIT

# C++ standard
CXX_STANDARD := 17

# C standard (for C files)
C_STANDARD := 99

# Print project information (when running make info)
PROJECT_INFO := \
    echo "Project: $(PROJECT_NAME)"; \
    echo "Version: $(PROJECT_VERSION)"; \
    echo "Description: $(PROJECT_DESCRIPTION)"; \
    echo "Author: $(PROJECT_AUTHOR)"; \
    echo "Executable: $(EXECUTABLE_NAME)"; \
    echo "C++ Standard: $(CXX_STANDARD)"; \
    echo "C Standard: $(C_STANDARD)"
