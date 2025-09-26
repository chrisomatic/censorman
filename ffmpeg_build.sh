#!/bin/bash

set -e

BUILD_DIR="$PWD/build"
PREFIX="$BUILD_DIR/ffmpeg_build"

pushd .

rm -rf $PWD/third_party/ffmpeg
rm -rf $BUILD_DIR
mkdir $BUILD_DIR

cd $BUILD_DIR

# Cleanup
mkdir -p ffmpeg_build ffmpeg_source

# Download x264 source
git clone https://code.videolan.org/videolan/x264.git x264
cd x264

./configure \
  --prefix=$PREFIX \
  --enable-static \
  --disable-cli \
  --disable-opencl

make -j$(nproc)
make install

cd ../ffmpeg_source

# Download FFmpeg source
git clone https://git.ffmpeg.org/ffmpeg.git ffmpeg --depth 1 --branch n8.0
# git clone https://git.ffmpeg.org/ffmpeg.git ffmpeg # master

cd ffmpeg

if [ "$1" = "win32" ]; then

./configure \
  --prefix="$PREFIX" \
  --pkg-config-flags="--static" \
  --extra-cflags="-I$PREFIX/include -O3 -march=native -ffunction-sections -fdata-sections" \
  --extra-ldflags="-L$PREFIX/lib -Wl,--gc-sections" \
  --extra-libs="-lpthread -lm" \
  --enable-hardcoded-tables \
  --disable-everything \
  --disable-libdrm \
  --enable-static \
  --disable-shared \
  --disable-doc \
  --disable-programs \
  --disable-network \
  --disable-debug \
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
  --toolchain=msvc \
  --arch=x86_64 \
  --target-os=win64

else

./configure \
  --prefix="$PREFIX" \
  --pkg-config-flags="--static" \
  --extra-cflags="-I$PREFIX/include -O3 -march=native -ffunction-sections -fdata-sections" \
  --extra-ldflags="-L$PREFIX/lib -Wl,--gc-sections" \
  --extra-libs="-lpthread -lm" \
  --enable-hardcoded-tables \
  --disable-everything \
  --disable-libdrm \
  --enable-static \
  --disable-shared \
  --disable-doc \
  --disable-programs \
  --disable-network \
  --disable-debug \
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
  --cc=gcc

fi

make -j$(nproc)
make install

popd

rm -rf $BUILD_DIR/ffmpeg_source
rm -rf $BUILD_DIR/x264
rm -rf $BUILD_DIR/ffmpeg_build/share

mv $BUILD_DIR/ffmpeg_build third_party/ffmpeg
rm -rf $BUILD_DIR

