#!/bin/bash

set -e

PREFIX="$PWD/ffmpeg_build"

pushd .

# Cleanup
#rm -rf ffmpeg ffmpeg_build ffmpeg_source
#mkdir -p ffmpeg_build ffmpeg_source
cd ffmpeg_source

# Download FFmpeg source
# curl -L https://ffmpeg.org/releases/ffmpeg-6.1.1.tar.bz2 | tar xj
cd ffmpeg-6.1.1

# Configure for minimal static build: MP4 container + H.264 decoder only
./configure \
  --prefix="$PREFIX" \
  --disable-everything \
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
  --enable-parser=h264 \
  --enable-bsfs \
  --cc=gcc \
  --extra-cflags="-O3 -ffunction-sections -fdata-sections" \
  --extra-ldflags="-Wl,--gc-sections"
#  --enable-libx265 \   # re-enable for encoding support

make -j8
make install

popd

rm -rf ffmpeg_source
rm -rf ffmpeg_build/share
mv ffmpeg_build ffmpeg

