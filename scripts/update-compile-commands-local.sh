export CC=clang
export CXX=clang++
# check if zsh, if then find out the script path, otherwise use BASH_SOURCE
if [ -n "$ZSH_VERSION" ]; then
  SCRIPT_PATH=${(%):-%x}
else
  SCRIPT_PATH=${BASH_SOURCE[0]}
fi
BASEDIR=$(dirname "$SCRIPT_PATH")
$BASEDIR/update-compile-commands.sh -DPROJECT_DEPS_INSTALL_PATH:PATH=/data/dev/build-deps/install -DPROJECT_DEPS_PATH:PATH=/data/dev/daw-deps
 