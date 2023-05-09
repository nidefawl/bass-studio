About
=========
- Bass Studio is a DAW which supports VST2 and CLAP plugins.
- Releases are provided for Windows, MacOS and Linux for the x86_64 architecture.
- The program requires Hardware OpenGL acceleration.
- This Project is available under Open Source license since May 4th 2023. Previous releases have been published at https://github.com/nidefawl/bass/releases. The latest build can be found at https://github.com/nidefawl/bass-studio/actions
- Only 64 bit builds are supported
- All dependencies are built from source and have to be build with the same compiler version
- I have been working on this solo since 2017.

Screenshots
=========
[![Screenshot 1](https://i.imgur.com/gxTUcoa.jpeg)](https://i.imgur.com/gxTUcoa.jpeg)
[![Screenshot 2](https://i.imgur.com/ezsmeET.jpeg)](https://i.imgur.com/ezsmeET.jpeg)

Video
=========
CLAP Demo (Diva)
[![CLAP Demo (Diva)](https://img.youtube.com/vi/iW27lEGNil8/maxresdefault.jpg)](https://youtu.be/iW27lEGNil8)

Community
=========
Feel free to join the Discord Server which I recently created. Hit me a message if you have questions. https://discord.gg/MxsgB7Ex

Donations
=========
If you like the software please consider a donation https://ko-fi.com/nidefawl

Runtime & Installation
======================
- Read-Write resources are stored in "C:\Users\<user>\AppData\Roaming\bass\":
  - logfile, settings.json
  - plugin database (SQLite)
  - view-layout json files (F1-F10 keys)
  - theme presets (important)
- Read-only resources are stored in "./res"
    - Fonts, Icons, GLSL shader code
- Extract daw-userdata.zip to "C:\Users\<user>\AppData\Roaming\bass\"
- Build scripts will generate the executable to "./run/" 
- The program must be executed inside ./run/ to find the sibling ./res/ directory 

Build requirements
==================
- CMake v3.23+
- Python 3.8+
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
git clone --recurse-submodules 'https://github.com/nidefawl/daw-deps.git'
git clone 'https://github.com/nidefawl/bass-studio.git'
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

- If dependencies were built at some other location `-PROJECT_DEPS_INSTALL_PATH=D:/somefolder/build-deps/build/install` has to be provided.


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
- Not tested: GCC MinGW64 from MSys2
- Not supported: clang-cl from the Windows LLVM release

Linux
=====
Required system dependeciesn:  

```
libx11-dev
libxrandr-dev
libxinerama-dev
libxcursor-dev
libxi-dev
libasound2-dev
python3-distutils
libgtk-3-dev
```
- Build deps and daw same way as on windows
- Copy user-data to ~/daw

MacOS
=============
- MacOS version is experimental/broken/untested
- install brew
```
/usr/bin/ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"
```
- install clang and cmake
```
brew install llvm
brew install cmake
```
- Build deps with
```
python ../daw-deps/build.py -DCMAKE_OSX_DEPLOYMENT_TARGET=10.12
```
- Build daw same way as on windows

Running tests
=============

```
cd C:/dev/daw/build/
ctest --verbose 
ctest --output-on-failure # to only see tests that failed
```