clang with external mingw:
cmake ../.. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Debug -DDAW_DEPS_PATH=D:\dev\daw-deps\ -DDEPS_BUILD_FOLDER=D:\dev\daw-deps\build-clang-stdc++ -DCMAKE_CXX_FLAGS=--target=x86_64-pc-windows-gnu -DCMAKE_C_FLAGS=--target=x86_64-pc-windows-gnu
clang-mingw (using libc++, see: https://github.com/mstorsjo/llvm-mingw)
cmake ../.. -G "MinGW Makefiles" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_BUILD_TYPE=Debug -DDAW_DEPS_PATH=D:\dev\daw-deps\ -DDEPS_BUILD_FOLDER=E:\dev\builds\daw-deps\clang-libc++-git


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