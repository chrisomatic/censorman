@echo off

set parent=%~dp0
pushd %parent%
cd %parent%

set PREFIX="%parent%\\ffmpeg_build"

# Cleanup
rmdir /S /Q ffmpeg_build ffmpeg_source
mkdir /P ffmpeg_build ffmpeg_source
cd ffmpeg_source

# Download FFmpeg source
curl -L https://ffmpeg.org/releases/ffmpeg-6.1.1.tar.bz2 | tar xj
cd ffmpeg-6.1.1

# Configure for minimal static build: MP4 container + H.264 decoder only
./configure \
  --prefix="%PREFIX%" \
  --toolchain=msvc \
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
  --extra-cflags="-Os -ffunction-sections -fdata-sections" \
  --extra-ldflags="-Wl,--gc-sections"

make -j8
make install
