#!/bin/bash

set -e

PREFIX="$PWD/ffmpeg_build"

pushd .

# Cleanup
rm -rf x264 ffmpeg ffmpeg_build ffmpeg_source
mkdir -p ffmpeg_build ffmpeg_source

# Download x264 source
git clone https://code.videolan.org/videolan/x264.git
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
curl -L https://ffmpeg.org/releases/ffmpeg-6.1.1.tar.bz2 | tar xj
cd ffmpeg-6.1.1

# Configure for minimal static build: MP4 container + H.264 decoder only
./configure \
  --prefix="$PREFIX" \
  --pkg-config-flags="--static" \
  --extra-cflags="-I$PREFIX/include -O3 -ffunction-sections -fdata-sections" \
  --extra-ldflags="-L$PREFIX/lib -Wl,--gc-sections" \
  --extra-libs="-lpthread -lm" \
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
  --enable-muxer=mp4 \
  --enable-encoder=libx264 \
  --enable-gpl \
  --enable-libx264 \
  --enable-encoder=mpeg4 \
  --enable-bsfs \
  --cc=gcc

make -j$(nproc)
make install

popd

rm -rf ffmpeg_source
#rm -rf ffmpeg_build/share
rm -rf x264

mv ffmpeg_build ffmpeg

