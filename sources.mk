# Source files configuration for Make
# Add your source files here for easy management

# C++ source files
CPP_SOURCES := \
    src/main.cpp \
    src/system/fs.cpp \
    src/system/logger.cpp \
    src/system/config.cpp \
    src/utils/hash.cpp \
    src/utils/path.cpp \
    src/utils/git.cpp \
    src/utils/zip.cpp \
    src/utils/liner.cpp \
    src/core/types.cpp \
    src/core/repo.cpp \
    src/cli/cli.cpp \


# C source files (if any)
C_SOURCES := \


# Header files (for dependency tracking)
HEADER_FILES := \
    src/system/fs.h \
    src/system/logger.h \
    src/system/config.h \
    src/utils/hash.h \
    src/utils/path.h \
    src/utils/git.h \
    src/core/types.h \
    src/core/repo.h \
    src/cli/cli.h \



# Combine all source files
ALL_SOURCES := $(CPP_SOURCES) $(C_SOURCES)

# Print source information (when running make info)
SOURCES_INFO := \
    echo "C++ sources: $(CPP_SOURCES)"; \
    echo "C sources: $(C_SOURCES)"; \
    echo "Header files: $(HEADER_FILES)"; \
    echo "Total sources: $(ALL_SOURCES)"
