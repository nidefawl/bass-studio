export CC=clang
export CXX=clang++
BASEDIR=$(dirname "$BASH_SOURCE")
$BASEDIR/update-compile-commands.sh -DPROJECT_DEPS_INSTALL_PATH=/data/dev/build-deps-llvm16/install