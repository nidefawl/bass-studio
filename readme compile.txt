#######################################
##      Win10 build instructions     ##
#######################################

clang mingw
===========
clang-mingw (using libc++, see: https://github.com/mstorsjo/llvm-mingw)
cmake ../.. -G "Ninja" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_BUILD_TYPE=Debug -DDAW_DEPS_PATH=D:\dev\daw-deps\ -DDEPS_BUILD_FOLDER=C:/dev/build-deps/install

On newer cmake versions:
cd %DAW%
cmake -E make_directory "build"
cmake -G "Ninja" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_BUILD_TYPE=Debug -DDAW_DEPS_PATH=D:/dev/daw-deps -DDEPS_BUILD_FOLDER=C:/dev/build-deps/install -S . -B "build"
cmake --build "build" --target all -j 8 --

MSVC
====

"C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat"
or 
x64 Native Tools Command Prompt for VS 2019

Using Ninja build files
-----------------------
cd %DAW%
set DAW_BUILD_DIR="build/build-msvc-ninja"
cmake -E make_directory %DAW_BUILD_DIR%
cmake -G "Ninja" -DDAW_DEPS_PATH=D:/dev/daw-deps -DDEPS_BUILD_FOLDER=D:/dev/daw-deps/build-msvc2019-d -S . -B %DAW_BUILD_DIR%
cmake --build %DAW_BUILD_DIR% --config RelWithDebInfo --target all -j 8 --


Using SLN/vcxproj build files
-----------------------------
cd %DAW%
set DAW_BUILD_DIR="build/build-msvc-2019"
cmake -E make_directory %DAW_BUILD_DIR%
cmake -G "Visual Studio 16 2019" -DDAW_DEPS_PATH=D:/dev/daw-deps -DDEPS_BUILD_FOLDER=D:/dev/daw-deps/build-msvc2019-d -S . -B %DAW_BUILD_DIR%
cmake --build %DAW_BUILD_DIR% --config RelWithDebInfo --target ALL_BUILD -j 8 --


#######################################
##      macOS build instructions     ##
#######################################


#install brew
/usr/bin/ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"
#install clang and cmake
brew install llvm
brew install cmake

export BUILD_DIR=~/dev

mkdir $BUILD_DIR
cd ~$BUILD_DIR
mkdir build
mkdir build-deps
git clone 'ssh://michael@debian/~/projects/daw-deps.git'
git clone 'ssh://michael@debian/~/projects/daw.git'
cd build-deps
python $BUILD_DIR/daw-deps/build.py ./build ./install
mkdir $BUILD_DIR/build
cmake $BUILD_DIR/daw/ -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DDAW_DEPS_PATH=$BUILD_DIR/daw-deps/ -DDEPS_BUILD_FOLDER=$BUILD_DIR/build-deps/install/

