#!/bin/bash

set -e
set -o pipefail

mkdir -p .log
LOG_FILE=".log/build_linux.log"

echo "Build started at $(date)" > "$LOG_FILE"

echo "🧹 Cleaning old build directory..." | tee -a "$LOG_FILE"
rm -rf build
mkdir build

echo "🐧 Linux version is compiling..." | tee -a "$LOG_FILE"

{
    cmake --preset linux-native
    cmake --build --preset linux
    cmake --install build/linux
} 2>&1 | tee -a "$LOG_FILE"

mkdir -p build/linux/.log
cp "$LOG_FILE" build/linux/.log/build_linux.log

echo "Linux build done! Check 'build/linux' directory." | tee -a "$LOG_FILE"