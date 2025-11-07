#!/bin/bash

# KanchoNet 빌드 스크립트 (Linux)

set -e

echo "======================================="
echo "  KanchoNet Build Script (Linux)"
echo "======================================="
echo ""

# 빌드 타입 설정 (기본값: Release)
BUILD_TYPE="${1:-Release}"

echo "Build type: $BUILD_TYPE"
echo ""

# 빌드 디렉토리 생성
mkdir -p build
cd build

# CMake 설정
echo "Running CMake..."
cmake .. -DCMAKE_BUILD_TYPE=$BUILD_TYPE

# 빌드
echo "Building..."
make -j$(nproc)

echo ""
echo "======================================="
echo "  Build completed!"
echo "======================================="
echo ""
echo "Binaries location:"
echo "  - Library: build/lib/libKanchoNet.a"
echo "  - Examples: build/bin/"
echo ""
echo "To run example servers:"
echo "  ./build/bin/EchoServer"
echo "  ./build/bin/ChatServer"
echo "  ./build/bin/ProtobufServer"
echo ""

