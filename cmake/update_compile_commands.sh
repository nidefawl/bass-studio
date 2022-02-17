#!/bin/bash
BASEDIR=$(dirname "$BASH_SOURCE")
TMP_BUILD=$(realpath "$BASEDIR/../.cache/cmake_commands")
PROJECT_DIR=$(realpath "$BASEDIR/..")
cmake --log-level=WARNING -GNinja -S"${PROJECT_DIR}" -B"${TMP_BUILD}" -DFORCE_BUILD_BENCHMARKS=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_BUILD_TYPE=Debug -DCMAKE_DISABLE_PRECOMPILE_HEADERS=ON -DCMAKE_UNITY_BUILD=OFF $*
if [ ! $? -eq 0 ]; then
    echo "CMake failed"
    exit $exit_status
fi
cp "${TMP_BUILD}/compile_commands.json" "${PROJECT_DIR}/compile_commands.json"
echo "Updated ${PROJECT_DIR}/compile_commands.json"
