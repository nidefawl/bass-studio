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
- CMake v3.8+
- Python 3
- Compiler with C++17 feature support:
    - Windows: Visual Studio 2019 or llvm-mingw toolchain
    - Linux: clang 
    - MacOS: clang

Build instructions
==================

Clone both repositories:

```
# create folder C:/dev or some other location (avoid spaces in path)
cd C:/dev
git clone 'https://github.com/nidefawl/daw-deps.git'
git clone 'https://github.com/nidefawl/daw.git'
```

Windows + Visual Studio
==========================
## Using cmake-gui 
#### Building dependencies:
- Set path to source code to C:/dev/daw-deps/
- Set the build directory to C:/dev/build-deps-msvc/
- Click configure, generate, open project
- Rightclick ALL_BUILD and Build

This will produce both debug and release version of all required libraries

#### Building project:
- Set path to source code to C:/dev/daw/
- Set the build directory to C:/dev/daw/build-daw-msvc/
- Click configure once
- Set variable *DAW_DEPS_PATH to C:/dev/daw-deps/ 
- Set DEPS_BUILD_FOLDER to C:/dev/build-deps-msvc/
- Configure, Generate and Open Project
- Select either Debug or RelWithDebInfo as build type
- Rightclick Solution/Project and Build

## Using CMake to build from command line
- Run the x64 Native Tools Command Prompt for VS 2019
- Or run vcvars64.bat of your Visual Studio version from a command prompt:
```
"C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat"
```
- Building dependencies:
```
cd C:/dev
cmake -E make_directory "build-deps-msvc"
cmake -G "Visual Studio 16 2019" -S daw-deps -B "build-deps-msvc"
cmake --build "build-deps-msvc" --target ALL_BUILD -j 8 --
```
- Building project:
```
cd C:/dev
cmake -E make_directory "build-daw-msvc"
cmake -G "Visual Studio 16 2019" -DDAW_DEPS_PATH=daw-deps -DDEPS_BUILD_FOLDER=build-deps-msvc -S "daw" -B "build-daw-msvc"
cmake --build "build-daw-msvc" --config RelWithDebInfo --target ALL_BUILD -j 8 --
```
- Ninja can also be used to build the project. Use *-G "Ninja"* for the generator and *--target all* instead


Windows + Clang/GCC
===================

## Using Clang based mingw-w64 toolchain
- Get the compiler at https://github.com/mstorsjo/llvm-mingw/releases
- Get ninja.exe https://github.com/ninja-build/ninja/releases
- Make sure the llvm-mingw/bin and cmake/bin and ninja.exe is in your PATH
- Building dependencies using the python script:
```
cd C:/dev/
python daw-deps/build.py ./build-deps-clang ./install-deps-clang
```
- Building project:
```
cd C:/dev/
cmake -E make_directory "build-daw-clang"
cmake -G "Ninja" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_BUILD_TYPE=Debug -DDAW_DEPS_PATH=daw-deps -DDEPS_BUILD_FOLDER=install-deps-clang -S "daw" -B "build-daw-clang"
cmake --build "build-daw-clang" --target all -j 8 --
```
- Copy libc++.dll and libunwind.dll from the llvm-mingw/bin directory to ./run/
Note: I had difficulties debugging with GDB llvm releases versions newer than 9.0.0


## Other toolchains: 
- Not supported: GCC MinGW64 from MSys2
- Not supported: clang-cl from the Windows LLVM release

macOS + clang
=============

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
cd %DAW%/build/
ctest --verbose 
ctest --output-on-failure # to only see tests that failed
```