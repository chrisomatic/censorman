#!/bin/bash

set -e

# Set up paths
BUILD_DIR="$PWD/build"
PREFIX="$BUILD_DIR/ffmpeg_build"

# Make sure we're in the MINGW64 shell
if [[ "$MSYSTEM" != "MINGW64" ]]; then
  echo "Please run this script from the MSYS2 MINGW64 shell."
  exit 1
fi

# Use mingw-w64 compilers
export CC=gcc
export CXX=g++
export PKG_CONFIG=pkg-config

echo "Cleaning old builds..."
rm -rf "$BUILD_DIR"
rm -rf "$PWD/third_party/ffmpeg"

mkdir -p "$BUILD_DIR"
mkdir -p "$BUILD_DIR/ffmpeg_build"
mkdir -p "$BUILD_DIR/ffmpeg_source"

pushd "$BUILD_DIR"

########################
# Build x264 (static)
########################

echo "Cloning x264..."
git clone https://code.videolan.org/videolan/x264.git
cd x264

./configure \
  --prefix="$PREFIX" \
  --enable-static \
  --disable-cli \
  --host=x86_64-w64-mingw32

make -j$(nproc)
make install

cd ..

########################
# Build FFmpeg (static)
########################

cd ffmpeg_source

echo "Cloning FFmpeg..."
git clone https://git.ffmpeg.org/ffmpeg.git ffmpeg --depth 1 --branch n8.0

cd ffmpeg

PKG_CONFIG_PATH="$PREFIX/lib/pkgconfig"

./configure \
  --prefix="$PREFIX" \
  --pkg-config="$PKG_CONFIG" \
  --pkg-config-flags="--static" \
  --extra-cflags="-I$PREFIX/include -O3 -march=native" \
  --extra-ldflags="-L$PREFIX/lib -static" \
  --extra-libs="-lz" \
  --enable-static \
  --disable-shared \
  --disable-programs \
  --disable-doc \
  --disable-network \
  --disable-everything \
  --enable-small \
  --enable-avformat \
  --enable-avcodec \
  --enable-avutil \
  --enable-swscale \
  --enable-protocol=file \
  --enable-demuxer=mov \
  --enable-decoder=h264 \
  --enable-decoder=hevc \
  --enable-parser=h264 \
  --enable-parser=hevc \
  --enable-muxer=mp4 \
  --enable-encoder=libx264 \
  --enable-encoder=mpeg4 \
  --enable-gpl \
  --enable-libx264 \
  --enable-bsfs \
  --disable-debug

make -j$(nproc)
make install

popd

########################
# Clean Up
########################

echo "Cleaning up temporary build files..."

rm -rf "$BUILD_DIR/ffmpeg_source"
rm -rf "$BUILD_DIR/x264"
rm -rf "$BUILD_DIR/ffmpeg_build/share"

mkdir -p third_party
mv "$BUILD_DIR/ffmpeg_build" third_party/ffmpeg

rm -rf "$BUILD_DIR"

echo "✅ Build complete. Static libs and headers are in: third_party/ffmpeg"
