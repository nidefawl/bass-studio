#!/bin/bash
BASEDIR=$(dirname "$BASH_SOURCE")
TMP_BUILD=$(realpath "$BASEDIR/../.cache/cmake_commands")
PROJECT_DIR=$(realpath "$BASEDIR/..")
builtin type -P "ninja" &> /dev/null
[[ $? -ne 0 ]] && echo "ninja not found" && exit 1
builtin type -P "cmake" &> /dev/null
[[ $? -ne 0 ]] && echo "cmake not found" && exit 1
builtin type -P "clang" &> /dev/null
[[ $? -ne 0 ]] && echo "clang not found" && exit 1
builtin type -P "clang++" &> /dev/null
[[ $? -ne 0 ]] && echo "clang++ not found" && exit 1
CC=clang
CXX=clang++
cmake --log-level=WARNING -GNinja -S"${PROJECT_DIR}" -B"${TMP_BUILD}" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_BUILD_TYPE=Debug -DCMAKE_DISABLE_PRECOMPILE_HEADERS=ON -DCMAKE_UNITY_BUILD=OFF $*
if [ ! $? -eq 0 ]; then
    echo "CMake failed"
    exit $exit_status
fi
cp "${TMP_BUILD}/compile_commands.json" "${PROJECT_DIR}/compile_commands.json"
echo "Updated ${PROJECT_DIR}/compile_commands.json"
