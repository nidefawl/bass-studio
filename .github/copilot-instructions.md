---
name: bass-studio-copilot-instructions
description: Bass Studio DAW workspace instructions for AI code navigation and development
---

# Bass Studio - AI Development Instructions

Bass Studio is a professional **C++20 Digital Audio Workstation (DAW)** with VST2/CLAP plugin hosting, multi-track editing, and GPU-accelerated OpenGL UI. This document guides AI agents through the codebase architecture, build process, and conventions.

## Quick Reference

**Project Type**: C++20, CMake (Ninja Multi-Config), desktop application  
**Key Targets**: `bass` (DAW), `pluginscanner`, `host` (CLI), `test` suite  
**Platforms**: Linux (Clang/GCC), Windows (MSVC/LLVM-MinGW), macOS (partial)  
**Test Coverage**: 50+ unit/integration tests via CTest  
**Solo Developer**: Michael Hept (GitHub: @nidefawl)  

---

## Build System

### Prerequisites
```bash
# Linux/Ubuntu dependencies (from README.md)
sudo apt install libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libasound2-dev python3-distutils libgtk-3-dev

# Python 3.8+, CMake 3.23+, Clang/GCC compiler required
```

### Standard Build Flow

```bash
# Build all dependencies first (separate repo)
git clone --recurse-submodules https://github.com/nidefawl/daw-deps.git
mkdir build-deps && cd build-deps
python3 ../daw-deps/build.py

# Configure Bass Studio
cd ../bass-studio
cmake -S. -Bbuild -G"Ninja Multi-Config" \
  -DPROJECT_DEPS_PATH:PATH=../daw-deps \
  -DPROJECT_DEPS_INSTALL_PATH=../build-deps/install

# Build main app (RelWithDebInfo recommended for dev)
cmake --build build --config RelWithDebInfo --target bass

# Run tests
ctest --build-config RelWithDebInfo -V
```

### Configurations
- **Debug**: Full symbols for debugging
- **Release**: LTO, section optimizations, stripped
- **RelWithDebInfo**: *Recommended for development* — optimized + debug info

### Key CMake Modules

| Module | File | Purpose |
|--------|------|---------|
| Compiler Config | [cmake/CompilerConfig.cmake](cmake/CompilerConfig.cmake) | C++ standard, sanitizers, compiler detection |
| Build Config | [cmake/BuildConfig.cmake](cmake/BuildConfig.cmake) | Output paths, version stamping, build options |
| Warning Flags | [cmake/WarningFlags.cmake](cmake/WarningFlags.cmake) | Strict compiler warnings (error on warnings) |
| Library Search | [cmake/FindLibraries.cmake](cmake/FindLibraries.cmake) | PortAudio, GLFW, SQLiteCpp, soxr, etc. |
| Git Versioning | [cmake/GetGitRevision.cmake](cmake/GetGitRevision.cmake) | Automatic version from git tags |

### Build Targets

| Target | Output | Purpose |
|--------|--------|---------|
| `bass` | `run/bass-clang-{config}` | Main DAW application |
| `pluginscanner` | `run/pluginscanner-clang-{config}` | Discovers and indexes VST plugins |
| `host` | `run/host-clang-{config}` | Command-line audio host for testing |
| `RUN_TESTS` | CTest results | Execute all unit/integration tests |
| `PLUGIN_*` | `run/PLUGIN_*-clang-{config}.so` | Individual built-in plugins (if enabled) |

---

## Project Architecture

### Directory Structure

```
src/
├── host/              # Audio engine, plugin hosting, track routing
│   ├── host.hpp       # Core Host API
│   ├── plugin/        # VST2/CLAP plugin hosting
│   ├── daw/           # DAW application logic (main_daw.cpp)
│   ├── cli/           # CLI host (main_cmdline.cpp)
│   └── pluginscanner/ # Plugin discovery (main_scanner.cpp)
├── gui/               # OpenGL/NanoVG UI framework
│   ├── gui.hpp        # GUI system API
│   ├── track/         # Track editor widgets
│   ├── clip/          # Audio/MIDI clip editors
│   └── controls/      # Parameter controls, menus
├── dsp/               # DSP algorithms (FFT, resampling)
├── plugins/           # Built-in plugins (Gain, EQ, SampleCrush, TapeDelay, etc.)
├── file/              # Project serialization, audio I/O
├── midi/              # MIDI processing, arpeggiator
├── wave/              # Audio file handling
├── platform/          # OS-specific code
│   ├── win/           # Win32 API, WASAPI, registry
│   ├── linux/         # X11, ALSA, D-Bus
│   ├── macos/         # Cocoa, CoreAudio (partial)
│   └── mingw/         # MinGW-specific workarounds
└── thirdparty/        # VST3 SDK, other vendored code
```

### Core Components

| Component | Header | Purpose |
|-----------|--------|---------|
| **Host Engine** | [src/host/host.hpp](src/host/host.hpp) | Central audio engine; track/effect routing, automation, plugin hosting |
| **Plugin Hosting** | [src/host/plugin/](src/host/plugin/) | VST2 and CLAP plugin support with exception safety |
| **GUI System** | [src/gui/gui.hpp](src/gui/gui.hpp) | Hardware-accelerated 2D rendering; track/clip editors, automation lanes |
| **Audio Processing** | [src/dsp/](src/dsp/) | FFT, resampling algorithms, DSP utilities |
| **File I/O** | [src/file/](src/file/) | Project persistence (JSON), audio loader (via libsndfile) |
| **MIDI System** | [src/midi/](src/midi/) | MIDI clip editing, arpeggiator, note sequencing |
| **Plug-ins** | [src/plugins/](src/plugins/) | Built-in effects: Gain, EQ, StereoWidth, SampleCrush, TapeDelay, LFO, Macro, KickXP |

### Audio Engine Workflow

1. **Track Graph**: Audio stream flows through track routing (main mix, submix, effect sends)
2. **Clip Processing**: Audio/MIDI clips render with sample-accurate automation
3. **Plugin Dispatch**: VST2/CLAP host processes each track's effect chain
4. **Output Mixing**: Final mix sent to PortAudio output device
5. **Thread Safety**: Multi-threaded processing with task scheduling

### Type Conventions

| Pattern | Example | Meaning |
|---------|---------|---------|
| `*_t` | `audio_buffer_t` | Type alias or typedef |
| `_i32`, `_i64` | `track_index_i32` | Explicitly-sized integer variable |
| `gui*` | `guitrack`, `guictrl_base` | GUI component |
| `.hpp` | `host.hpp` | C++ header (primary) |
| `.cpp` | `host.cpp` | C++ implementation |
| `.h` | Rare | Third-party or template-heavy |

---

## Dependencies

All external libraries are **built from source** using the same compiler version via the separate [**daw-deps**](https://github.com/nidefawl/daw-deps) repository.

### External Libraries

| Category | Libraries |
|----------|-----------|
| **Audio I/O** | PortAudio, PortMidi, soxr (resampling), kissfft |
| **GUI** | GLFW, NanoVG, stb_image |
| **Persistence** | SQLite (via SQLiteCpp), nlohmann_json, libarchive |
| **DSP** | tinyspline (automation curves), muParser (expressions) |
| **Platform** | zlib, X11/ALSA (Linux), CoreAudio (macOS), WASAPI (Windows) |
| **Optional** | pybind11 (Python embedding) |

### Dependency Management

```bash
# All deps built in separate repo with matching compiler
# Install to ../build-deps/install/ (configurable)
# Link via CMake cache variable: -DPROJECT_DEPS_INSTALL_PATH=...

# To rebuild deps:
cd ../build-deps && python3 ../daw-deps/build.py
```

---

## Testing

### Running Tests

```bash
# Run all tests after build
ctest --build-config RelWithDebInfo -V

# Run tests with output on failure only
ctest --build-config RelWithDebInfo --output-on-failure

# Run a specific test
ctest -R test_host --build-config RelWithDebInfo -V
```

### Test Coverage

- **50+ test files** in [test/](test/)
- Audio processing (DSP, resampling, FFT)
- File I/O (MIDI, audio, project serialization)
- Plugin hosting (VST2/CLAP exception safety)
- GUI rendering and interaction
- Thread safety and memory leak detection

### Key Test Files

| File | Tests |
|------|-------|
| [test/test_host.cpp](test/test_host.cpp) | Audio engine, track routing, plugin dispatch |
| [test/test_vstplugins.cpp](test/test_vstplugins.cpp) | VST2 plugin hosting, parameter automation |
| [test/test_clap_plugin_host.cpp](test/test_clap_plugin_host.cpp) | CLAP plugin hosting, event handling |
| [test/test_projectfile.cpp](test/test_projectfile.cpp) | Project serialization, audio/MIDI import |
| [test/test_gui.cpp](test/test_gui.cpp) | GUI rendering, event handling |

---

## Debugging and Development

### IDE Setup

```bash
# Update clangd compilation database
./scripts/update-compile-commands-local.sh

# Clean clangd cache if symbol resolution stalls
rm -rf .cache/clangd
```

### CLI Tools

```bash
# Plugin discovery (lists found VST paths)
./run/pluginscanner-clang-debug

# Headless audio playback test
./run/host-clang-debug [audio-file]

# DAW application
./run/bass-clang-debug
```

### Key Entry Points

| File | Purpose |
|------|---------|
| [src/host/daw/main_daw.cpp](src/host/daw/main_daw.cpp) | DAW initialization, window creation |
| [src/host/pluginscanner/main_scanner.cpp](src/host/pluginscanner/main_scanner.cpp) | Plugin discovery and indexing |
| [src/host/cli/main_cmdline.cpp](src/host/cli/main_cmdline.cpp) | CLI host for headless audio playback |
| [src/include/buildinfo.h](src/include/buildinfo.h) | Version, product name, copyright info |

### Compilation Database

```bash
# Generate compile_commands.json for clangd
./scripts/update-compile-commands-local.sh

# Filter by file pattern (optional)
cat build/compile_commands.json | grep "pluginhost"
```

### Common Development Tasks

| Task | Command |
|------|---------|
| Clean rebuild | `rm -rf build && cmake ... && ninja -C build bass` |
| Incremental build | `ninja -C build bass` |
| Build with tests | `cmake --build build --config RelWithDebInfo --target RUN_TESTS` |
| Build single plugin | `cmake --build build --config RelWithDebInfo --target PLUGIN_GAIN_VST2` |
| Run with sanitizers | Set `-DCMAKE_CXX_FLAGS=-fsanitize=thread` in CMake config |
| Force rebuild | `touch src/file/to/change.cpp && ninja -C build` |

---

## Platform-Specific Notes

### Linux

- **Compiler**: Clang 12+ or GCC 7+ recommended
- **Audio Backend**: ALSA via PortAudio
- **GUI**: X11 window system
- **Command**: `export CC=clang CXX=clang++` before CMake

### Windows

- **Compiler**: MSVC 2022 (preferred for debugging) or LLVM-MinGW 14.0+
- **Audio Backend**: WASAPI via PortAudio
- **DLL Requirements**: Copy `soxr-msvc-release.dll` to `./run/`
- **MinGW Note**: VST2 plugin dispatch compiled without LTO/optimizations for SEH assembly

### macOS

- ⚠️ **Status**: Experimental/untested (no Mac available)
- **Compiler**: Clang (via Xcode or Homebrew)
- **Audio Backend**: CoreAudio
- **Note**: May require deployment target adjustment (e.g., `-DCMAKE_OSX_DEPLOYMENT_TARGET=10.12`)

---

## Known Issues and Gotchas

### Critical Issues (from [TODO-2026.md](TODO-2026.md))

- ⚠️ **Custom routing unstable** after project reload
- ⚠️ **Wave clip resampling broken** (related to task system refactor)
- ⚠️ **Notes disappear after scroll** (GLFW mouse event issue)
- ⚠️ **Wave clip zoom not restored** when reopening clip editor
- ⚠️ **GPU synth latency compensation** incorrect vs tape-based effects

### Compiler-Specific Quirks

| Compiler | Quirk |
|----------|-------|
| **MSVC** | `/MP7` parallel compilation, address sanitizer available for debug builds |
| **Clang** | SSE4.2 forced for GLM, LTO optional via CMake flag |
| **GCC** | C extensions allowed; C++ standard extensions configurable |
| **MinGW** | VST2 dispatch uses SEH assembly; compiled without LTO |

### Performance Optimization

- **Unity builds** enabled by default (faster compilation)
- **Selective skip** for fast iteration (no PCH/unity):
  - Host plugin dispatch (VST2 SEH)
  - `src/util/fp_math.cpp` (floating-point precision)
  - GUI tessellation code
  - EQ plugin implementation

---

## Development Workflow

### 1. Setup

```bash
git clone --recurse-submodules https://github.com/nidefawl/bass-studio.git
cd bass-studio
# Build dependencies in separate workspace (see README.md)
# Then configure CMake as shown in Build System section
```

### 2. Code Navigation

Use **clangd** integration (via `compile_commands.json`) for:
- Go-to-definition (`Ctrl+`Click`)
- Find-references (`Ctrl+F12`)
- Hover tooltips
- Rename symbols

Update the compilation database if it becomes stale:
```bash
./scripts/update-compile-commands-local.sh
```

### 3. Testing

```bash
# Before commit
ctest --build-config RelWithDebInfo -V

# Debug failing test
ctest -R test_name --verbose --rerun-failed
```

### 4. Building Release

```bash
cmake --build build --config Release --target bass
# Binary in ./run/bass-clang-release
```

---

## Documentation and Resources

| File | Content |
|------|---------|
| [README.md](README.md) | Build instructions, features, download links |
| [TODO-2026.md](TODO-2026.md) | Known issues, roadmap, feature requests |
| [Notes.md](Notes.md) | Design notes, architecture decisions |
| [CMakeLists.txt](CMakeLists.txt) | Build configuration and target definitions |
| [CONTRIBUTING.md](CONTRIBUTING.md) | *If present* — contribution guidelines |

---

## For C/C++ Code Navigation

When analyzing or modifying code, use clangd-based tools for accurate symbol resolution:

- **Go to definition**: Jump to symbol declarations
- **Find references**: Locate all usages of a symbol
- **Hover info**: View type signatures and documentation
- **Rename**: Safe refactoring across the codebase

These tools provide semantic understanding beyond simple text search and are essential for navigating the multi-file plugin hosting and DSP infrastructure.

---

## Quick Checklist for New Tasks

- [ ] Read [Build System](#build-system) section for compilation flow
- [ ] Check [Known Issues](#known-issues-and-gotchas) for relevant gotchas
- [ ] Review architecture in [Project Architecture](#project-architecture) for component ownership
- [ ] Use clangd tools for symbol navigation (not text search)
- [ ] Run tests after changes: `ctest --build-config RelWithDebInfo -V`
- [ ] Update [compile_commands.json](#compilation-database) if clangd stalls

---

**Last Updated**: April 2026  
**Maintainer**: Michael Hept (@nidefawl)

