#!/bin/sh

set -e

BUILD_DIR="$PWD/build"
PREFIX="$BUILD_DIR/ncnn_build"

pushd .

rm -rf $PWD/third_party/ncnn
rm -rf $BUILD_DIR
mkdir $BUILD_DIR

cd $BUILD_DIR

mkdir -p ncnn_build ncnn_source

cd ncnn_source

# Download NCNN Source
git clone --depth=1 https://github.com/Tencent/ncnn.git ncnn
cd ncnn

cmake -D CMAKE_BUILD_TYPE=Release \
      -D CMAKE_INSTALL_PREFIX="$PREFIX" \
      .
cmake --build . --config Release
cmake --build . --config Release --target install

popd

# Copy built files
mv $BUILD_DIR/ncnn_build third_party/ncnn

# Cleanup
rm -rf $BUILD_DIR
