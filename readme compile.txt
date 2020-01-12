DEPS_BUILD_FOLDER:PATH=E:/dev/builds/daw-deps/clang-libc++-git
clang with external mingw:
cmake ../.. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Debug -DDAW_DEPS_PATH=D:\dev\daw-deps\ -DDEPS_BUILD_FOLDER=D:\dev\daw-deps\build-clang-stdc++ -DCMAKE_CXX_FLAGS=--target=x86_64-pc-windows-gnu -DCMAKE_C_FLAGS=--target=x86_64-pc-windows-gnu
clang-mingw (using libc++, see: https://github.com/mstorsjo/llvm-mingw)
cmake ../.. -G "Ninja" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_BUILD_TYPE=Debug -DDAW_DEPS_PATH=D:\dev\daw-deps\ -DDEPS_BUILD_FOLDER=E:\dev\builds\daw-deps\clang-libc++-git


msvc currently requires a patch to its std thread header:
C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Tools\MSVC\14.16.27023\include\thread

class thread::id
	{	// thread id
public:
.....
	//START EDIT
	int32_t get() {
		return static_cast<int32_t>(_Id);
	}
	//END EDIT
	
.....
.....

Maybe find a better workaround like: https://hackernoon.com/c-telltales-pt-1-human-readable-thread-id-92caa554a35f






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

