#!/bin/sh

set -e

BUILD_DIR="$PWD/build"
PREFIX="$BUILD_DIR/ncnn_build"

pushd .

rm -rf $PWD/src/third_party/ncnn
# rm -rf $BUILD_DIR
# mkdir $BUILD_DIR

cd $BUILD_DIR

# mkdir -p ncnn_build ncnn_source

cd ncnn_source

# Download NCNN Source
# git clone --depth=1 https://github.com/Tencent/ncnn.git ncnn
cd ncnn

cmake -D CMAKE_BUILD_TYPE=Release \
      -D CMAKE_INSTALL_PREFIX="$PREFIX" \
      -D NCNN_OPENMP=OFF \
      -D NCNN_BUILD_TESTS=OFF \
      -D NCNN_BUILD_EXAMPLES=OFF \
      -D NCNN_BUILD_TOOLS=OFF \
      .
cmake --build . --config Release -j$(nproc)
cmake --build . --config Release --target install

popd

# Copy built files
mv $BUILD_DIR/ncnn_build src/third_party/ncnn

# Build NCNN Shim lib
g++ -c src/ncnn_shim.cpp -I src/third_party/ncnn/include/ncnn -o ncnn_shim.o
ar rcs src/third_party/ncnn/lib/libncnn_shim.a ncnn_shim.o

# Cleanup
rm ncnn_shim.o
# rm -rf $BUILD_DIR
