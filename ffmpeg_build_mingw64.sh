#!/bin/bash

set -e

BUILD_DIR="$PWD/build"
PREFIX="$BUILD_DIR/ffmpeg_build"

# Set static build flags globally
export CFLAGS="-static -static-libgcc -static-libstdc++ -O3 -march=x86-64"
export CXXFLAGS="$CFLAGS"
export LDFLAGS="-static"

pushd .

# Clean previous builds
rm -rf "$PWD/third_party/ffmpeg"
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"

cd "$BUILD_DIR"

# Create working directories
mkdir -p ffmpeg_build ffmpeg_source

#########################
# Build x264 (static)
#########################
echo "Cloning x264..."
git clone https://code.videolan.org/videolan/x264.git x264
cd x264

./configure \
  --prefix="$PREFIX" \
  --enable-static \
  --disable-cli \
  --disable-opencl \
  --host=x86_64-w64-mingw32 \
  --extra-cflags="$CFLAGS" \
  --extra-ldflags="$LDFLAGS"

make -j$(nproc)
make install
cd ..

#########################
# Build FFmpeg (static)
#########################
cd ffmpeg_source
echo "Cloning FFmpeg..."
git clone https://git.ffmpeg.org/ffmpeg.git ffmpeg --depth 1 --branch n8.0

cd ffmpeg

# Point pkg-config to x264's .pc file
export PKG_CONFIG_PATH="$PREFIX/lib/pkgconfig"

./configure \
  --prefix="$PREFIX" \
  --pkg-config-flags="--static" \
  --extra-cflags="$CFLAGS -I$PREFIX/include" \
  --extra-ldflags="$LDFLAGS -L$PREFIX/lib" \
  --extra-libs="-lpthread -lm -liconv -lbcrypt" \
  --cc=gcc \
  --disable-everything \
  --disable-libdrm \
  --enable-static \
  --disable-shared \
  --disable-doc \
  --disable-programs \
  --disable-network \
  --disable-debug \
  --enable-small \
  --enable-gpl \
  --enable-libx264 \
  --enable-avformat \
  --enable-avcodec \
  --enable-avutil \
  --enable-swscale \
  --enable-hardcoded-tables \
  --enable-protocol=file \
  --enable-demuxer=mov \
  --enable-decoder=h264 \
  --enable-decoder=hevc \
  --enable-parser=h264 \
  --enable-parser=hevc \
  --enable-muxer=mp4 \
  --enable-encoder=libx264 \
  --enable-encoder=mpeg4 \
  --enable-bsfs

make -j$(nproc)
make install

#########################
# Cleanup
#########################

popd

# Clean up temporary source dirs
rm -rf "$BUILD_DIR/ffmpeg_source"
rm -rf "$BUILD_DIR/x264"
rm -rf "$BUILD_DIR/ffmpeg_build/share"

# Move final static build to third_party
mv "$BUILD_DIR/ffmpeg_build" third_party/ffmpeg

# Final cleanup
rm -rf "$BUILD_DIR"

echo "✅ Static build of FFmpeg and x264 complete."
