#!/bin/bash
myrealpath() {
    [[ $1 = /* ]] && echo "$1" || echo "$PWD/${1#./}"
}
# check if zsh, if then find out the script path, otherwise use BASH_SOURCE
if [ -n "$ZSH_VERSION" ]; then
  SCRIPT_PATH=${(%):-%x}
else
  SCRIPT_PATH=${BASH_SOURCE[0]}
fi
BASEDIR=$(dirname "$SCRIPT_PATH")
if [ ! -d "${BASEDIR}/../.cache/" ]; then
    mkdir -p "${BASEDIR}/../.cache/"
fi
TMP_BUILD=$(myrealpath "${BASEDIR}/../.cache/cmake_commands")
if [ -d "${TMP_BUILD}" ]; then
    rm -Rf "${TMP_BUILD}"
fi
PROJECT_DIR=$(myrealpath "${BASEDIR}/..")
builtin type -P "ninja" &> /dev/null
[[ $? -ne 0 ]] && echo "ninja not found" && exit 1
builtin type -P "cmake" &> /dev/null
[[ $? -ne 0 ]] && echo "cmake not found" && exit 1
builtin type -P "$CC" &> /dev/null
[[ $? -ne 0 ]] && echo "CC not found" && exit 1
builtin type -P "$CXX" &> /dev/null
[[ $? -ne 0 ]] && echo "CXX not found" && exit 1
cmake --log-level=WARNING -GNinja -S"${PROJECT_DIR}" -B"${TMP_BUILD}" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_BUILD_TYPE=Debug -DCMAKE_DISABLE_PRECOMPILE_HEADERS=ON -DCMAKE_UNITY_BUILD=OFF -DPROJECT_NO_WINDRES=ON -DPROJECT_WORKING_DIR:PATH=../../run $*
if [ ! $? -eq 0 ]; then
    echo "CMake failed"
    exit $exit_status
fi
mkdir -p "${PROJECT_DIR}/build"
mv "${TMP_BUILD}/compile_commands.json" "${PROJECT_DIR}/build/compile_commands.json"
echo "Updated ${PROJECT_DIR}/build/compile_commands.json"
