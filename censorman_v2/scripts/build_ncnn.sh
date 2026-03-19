#!/bin/bash
set -e
BUILD_DIR="$PWD/build"
PREFIX="$BUILD_DIR/ncnn_build"

pushd .
rm -rf $BUILD_DIR
mkdir -p $BUILD_DIR
cd $BUILD_DIR
mkdir -p ncnn_source

cd ncnn_source
git clone --depth=1 https://github.com/Tencent/ncnn.git ncnn
cd ncnn

if [ "$1" = "win32" ]; then
  cmake -D CMAKE_BUILD_TYPE=Release \
        -D CMAKE_INSTALL_PREFIX="$PREFIX" \
        -D CMAKE_SYSTEM_NAME=Windows \
        -D CMAKE_C_COMPILER=gcc \
        -D CMAKE_CXX_COMPILER=g++ \
        -D CMAKE_RC_COMPILER=windres \
        -D NCNN_OPENMP=OFF \
        -D NCNN_BUILD_TESTS=OFF \
        -D NCNN_BUILD_EXAMPLES=OFF \
        -D NCNN_BUILD_TOOLS=OFF \
        -D NCNN_SHARED_LIB=OFF \
        -D CMAKE_C_FLAGS="-static-libgcc" \
        -D CMAKE_CXX_FLAGS="-static-libgcc -static-libstdc++" \
        -G "Unix Makefiles" \
        .
else
  cmake -D CMAKE_BUILD_TYPE=Release \
        -D CMAKE_INSTALL_PREFIX="$PREFIX" \
        -D NCNN_OPENMP=OFF \
        -D NCNN_BUILD_TESTS=OFF \
        -D NCNN_BUILD_EXAMPLES=OFF \
        -D NCNN_BUILD_TOOLS=ON \
        -D NCNN_SHARED_LIB=OFF \
        .
fi

cmake --build . --config Release -j$(nproc)
cmake --build . --config Release --target install

popd

if [ "$1" = "win32" ]; then
  NCNN_DEST="src/third_party/ncnn/win"
else
  NCNN_DEST="src/third_party/ncnn"
fi

rm -rf $PWD/$NCNN_DEST
mv $BUILD_DIR/ncnn_build $NCNN_DEST

# Build NCNN shim lib
if [ "$1" = "win32" ]; then
  g++ -c src/ncnn_shim.cpp \
      -I $NCNN_DEST/include/ncnn \
      -static-libgcc -static-libstdc++ \
      -o ncnn_shim.o
else
  g++ -c src/ncnn_shim.cpp \
      -I $NCNN_DEST/include/ncnn \
      -o ncnn_shim.o
fi

ar rcs $NCNN_DEST/lib/libncnn_shim.a ncnn_shim.o

# Cleanup
rm ncnn_shim.o
rm -rf $BUILD_DIR
