About
=====
- Only 64 bit builds are supported
- All dependencies are built from source and have to be build with the same compiler version

Runtime & Installation
======================
- Read-Write resources are stored in "C:\User\AppData\Roaming\daw\":
  - logfile, settings.json
  - plugin database (SQLite)
  - view-layout json files (F1-F10 keys)
  - theme presets (important)
- Read-only resources are stored in "./res"
    - Fonts, Icons, GLSL shader code
- Extract daw-userdata.zip to "C:\User\AppData\Roaming\daw\"
- Build scripts will generate the executable to "./run/" 
- The program must be started inside ./run/ to find the sibling ./res/ directory 

Build requirements
==================
- CMake v3.20+
- Python 3
- Compiler with C++17 feature support:
    - Windows: Visual Studio 2019 or llvm-mingw 14.0.0 toolchain
    - Linux: clang minimum version 12
    - MacOS: clang - untested

Build instructions
==================

Clone both repositories:

```
# create folder C:/dev or some other location (avoid spaces in path)
cd C:/dev
git clone 'https://github.com/nidefawl/daw-deps.git'
git clone 'https://github.com/nidefawl/daw.git'
```

Windows + Visual Studio 2022
==========================

## Building dependencies:
Open command prompt and build dependencies in *build-deps* next to *daw-deps*
```
"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" amd64
cd C:/dev/
mkdir build-deps
cd build-deps
python ../daw-deps/build.py
```
This will produce both debug and release version of all required libraries in *C:/dev/build-deps/install*

## Building project:
```
"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" amd64
cd C:/dev/daw
cmake -DPROJECT_DEPS_PATH:PATH=C:/dev/daw-deps -S. -Bbuild
cmake --build build -config Release
```
- Copy soxr-msvc-release.dll from ../build-deps/install/soxr/bin to ./run

- If dependencies were build at some other location `-PROJECT_DEPS_INSTALL_PATH=D:/somefolder/build-deps/build/install` has to be provided.


Windows + Clang/GCC
===================

## Using Clang based mingw-w64 toolchain
- Get the compiler at https://github.com/mstorsjo/llvm-mingw/releases
- Get ninja.exe https://github.com/ninja-build/ninja/releases
- Make sure the llvm-mingw/bin and cmake/bin and ninja.exe is in your PATH
- Building dependencies using the python script:
```
cd C:/dev/
mkdir build-deps
cd build-deps
python ../daw-deps/build.py
```

- Building project:
```
cd C:/dev/daw
cmake -G "Ninja" -DCMAKE_BUILD_TYPE=Debug -DPROJECT_DEPS_PATH:PATH=C:/dev/daw-deps -S. -Bbuild
cmake --build build
```
- Copy libc++.dll and libunwind.dll from the llvm-mingw/bin directory to ./run/
- Copy libsoxr-clang-release.dll from ../build-deps/install/soxr/bin to ./run

## Other toolchains: 
- Not supported: GCC MinGW64 from MSys2
- Not supported: clang-cl from the Windows LLVM release

macOS + clang
=============
# OUTDATED MACOS BUILD INSTRUCTIONS
- install brew
```
/usr/bin/ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"
```
- install clang and cmake
```
brew install llvm
brew install cmake
```
- building
```
export BUILD_DIR=~/dev
mkdir $BUILD_DIR
cd ~$BUILD_DIR
mkdir build
mkdir build-deps
git clone 'https://github.com/nidefawl/daw-deps.git'
git clone 'https://github.com/nidefawl/daw.git'
cd build-deps
python $BUILD_DIR/daw-deps/build.py ./build ./install
mkdir $BUILD_DIR/build
cmake $BUILD_DIR/daw/ -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DDAW_DEPS_PATH=$BUILD_DIR/daw-deps/ -DDEPS_BUILD_FOLDER=$BUILD_DIR/build-deps/install/
```

Running tests
=============

```
cd C:/dev/daw/build/
ctest --verbose 
ctest --output-on-failure # to only see tests that failed
```