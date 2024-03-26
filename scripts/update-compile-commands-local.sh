export CC=clang
export CXX=clang++
# check if zsh, if then find out the script path, otherwise use BASH_SOURCE
if [ -n "$ZSH_VERSION" ]; then
  SCRIPT_PATH=${(%):-%x}
else
  SCRIPT_PATH=${BASH_SOURCE[0]}
fi
BASEDIR=$(dirname "$BASH_SOURCE")
$BASEDIR/update-compile-commands.sh -DPROJECT_DEPS_INSTALL_PATH:PATH=C:/dev/daw-deps-install/llvm-mingw-20240221-ucrt-x86_64 -DPROJECT_DEPS_PATH:PATH=C:/dev/daw-deps
 