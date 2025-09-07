## RepoMan CLI

Cross-platform C++ CLI to manage Quake 3 content repositories.

https://t.me/q3unite

### Features
- Unix-first development (Linux)
- Cross-compilation to Windows via MinGW-w64
- Debug/Release configurations
- Simple Makefile-based build system
- Interactive REPL with history and completion (Linux)

### IMPORTANT: ASCII-only paths
- Program supports only ASCII file and directory names. Unicode (e.g., Cyrillic) in file names or paths is not supported and will lead to corrupted paths and indexing failures.

### Download

https://github.com/unite-q3-team/unitehub-repoman/releases

## Requirements

### Build (Linux)
- GCC 7+ or Clang 5+
- GNU Make

### Cross-compile to Windows
- MinGW-w64 cross-compilers
  - x86_64-w64-mingw32-gcc/g++ (64-bit)
  - i686-w64-mingw32-gcc/g++ (32-bit)
- Install helper: `./install-mingw.sh`

### Runtime
- Linux: none beyond glibc
- Windows: binaries are linked with static libstdc++/libgcc
- For zip extraction: on Linux, Python 3 or `unzip`; on Windows, PowerShell (Expand-Archive)

## Build

### Scripts (Make-only)
```bash
chmod +x *.sh

# Build Linux (Release by default)
./build.sh

# Debug
./build.sh debug

# Clean
./build.sh clean

# Build Linux targets (alias; Make only)
./build-all.sh release

# Cross-compile Windows
./build-cross.sh release x64
./build-cross.sh debug x86
./build-all-cross.sh
```

### Make directly
```bash
make            # Release
make debug      # Debug

# Cross to Windows (example x64)
make BUILD_TYPE=Release CC=x86_64-w64-mingw32-gcc CXX=x86_64-w64-mingw32-g++
or
make PLATFORM=windows
```

### Output locations
- Linux: `build/linux/<arch>/<BuildType>/repoman-cli`
- Windows: `build/windows/<arch>/<BuildType>/repoman-cli.exe`

## Usage

CLI shows help when no subcommand is provided. Key commands:
- `init <name> [desc]`: create repo in `repos/<name>` and write `index.json`
- `use <name>`: set current repo in `config.json`
- `add <src> <type> <rel> <name> [--author] [--desc] [--tag TAG ...]`: copy a file into repo and index it
- `list`: list items from current repo
- `remove <id>`: remove item and file
- `rename <id> <new_name>`: rename item in index
- `list-repos`: list local repos under `repos/`
- `delete-repo <name> [--force]`: delete a local repo directory
- `rename-repo <old> <new>`: rename a local repo directory
- GitHub integration (requires token): `gh-login`, `gh-list`, `gh-clone`, `gh-pull`, `gh-push`, `gh-delete`, `gh-visibility`, `gh-token-check`
- `repl`: interactive mode (Linux: with history/completion)

Notes:
- Config `config.json` is stored next to the executable directory.
- Temporary files for GitHub operations are created next to the executable.
- ASCII-only paths are enforced; Unicode paths are not supported.
