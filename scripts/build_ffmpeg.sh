#!/bin/bash
set -e
BUILD_DIR="$PWD/build"
PREFIX="$BUILD_DIR/ffmpeg_build"

# Set static build flags globally
export CFLAGS="-static -static-libgcc -static-libstdc++ -O3 -march=x86-64"
export CXXFLAGS="$CFLAGS"
export LDFLAGS="-static"

pushd .
rm -rf $BUILD_DIR
mkdir -p $BUILD_DIR
cd $BUILD_DIR

mkdir -p ffmpeg_build ffmpeg_source

#########################
# Build x264 (static)
#########################
echo "Cloning x264..."
git clone https://code.videolan.org/videolan/x264.git x264
cd x264

if [ "$1" = "win32" ]; then
  ./configure \
    --prefix="$PREFIX" \
    --enable-static \
    --disable-cli \
    --disable-opencl \
    --host=x86_64-w64-mingw32 \
    --extra-cflags="$CFLAGS" \
    --extra-ldflags="$LDFLAGS"
else
  ./configure \
    --prefix="$PREFIX" \
    --enable-static \
    --disable-cli \
    --disable-opencl
fi

make -j$(nproc)
make install

#########################
# Build libvpx (static)
#########################
echo "Cloning libvpx..."
cd $BUILD_DIR
git clone https://chromium.googlesource.com/webm/libvpx.git libvpx
cd libvpx

if [ "$1" = "win32" ]; then
  ./configure \
    --prefix="$PREFIX" \
    --target=x86_64-win64-gcc \
    --disable-shared \
    --enable-static \
    --disable-examples \
    --disable-tools \
    --disable-docs \
    --disable-unit-tests \
    --extra-cflags="$CFLAGS"
else
  ./configure \
    --prefix="$PREFIX" \
    --disable-shared \
    --enable-static \
    --disable-examples \
    --disable-tools \
    --disable-docs \
    --disable-unit-tests
fi

make -j$(nproc)
make install

#########################
# Build FFmpeg (static)
#########################
cd $BUILD_DIR/ffmpeg_source

# *** THE FIX: tell pkg-config where to find your freshly installed .pc files ***
export PKG_CONFIG_PATH="$PREFIX/lib/pkgconfig"
export PKG_CONFIG="pkg-config"

git clone https://git.ffmpeg.org/ffmpeg.git ffmpeg --depth 1 --branch n8.0
cd ffmpeg

if [ "$1" = "win32" ]; then
  ./configure \
    --prefix="$PREFIX" \
    --pkg-config-flags="--static" \
    --extra-cflags="-I$PREFIX/include -O3 -march=x86-64 -ffunction-sections -fdata-sections" \
    --extra-ldflags="-L$PREFIX/lib -Wl,--gc-sections -static-libgcc -static-libstdc++" \
    --extra-libs="-lpthread -lm -lws2_32 -lbcrypt" \
    --enable-hardcoded-tables \
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
    --enable-decoder=hevc \
    --enable-decoder=vp9 \
    --enable-decoder=aac \
    --enable-decoder=aac_latm \
    --enable-decoder=mp3 \
    --enable-decoder=opus \
    --enable-decoder=vorbis \
    --enable-parser=h264 \
    --enable-parser=hevc \
    --enable-parser=aac \
    --enable-parser=aac_latm \
    --enable-parser=mp3 \
    --enable-parser=opus \
    --enable-muxer=mp4 \
    --enable-demuxer=aac \
    --enable-encoder=libx264 \
    --enable-encoder=mpeg4 \
    --enable-encoder=libvpx_vp9 \
    --enable-encoder=aac \
    --enable-gpl \
    --enable-libx264 \
    --enable-libvpx \
    --enable-bsfs \
    --cc=gcc \
    --target-os=mingw32 \
    --arch=x86_64 \
    --disable-w32threads \
    --enable-pthreads
else
  ./configure \
    --prefix="$PREFIX" \
    --pkg-config-flags="--static" \
    --extra-cflags="-I$PREFIX/include -O3 -march=x86-64 -ffunction-sections -fdata-sections" \
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
    --enable-decoder=vp9 \
    --enable-decoder=aac \
    --enable-decoder=aac_latm \
    --enable-decoder=mp3 \
    --enable-decoder=opus \
    --enable-decoder=vorbis \
    --enable-parser=h264 \
    --enable-parser=hevc \
    --enable-parser=aac \
    --enable-parser=aac_latm \
    --enable-parser=mp3 \
    --enable-parser=opus \
    --enable-muxer=mp4 \
    --enable-demuxer=aac \
    --enable-encoder=libx264 \
    --enable-encoder=mpeg4 \
    --enable-encoder=libvpx_vp9 \
    --enable-encoder=aac \
    --enable-gpl \
    --enable-libx264 \
    --enable-libvpx \
    --enable-bsfs \
    --cc=gcc
fi

make -j$(nproc)
make install

popd
rm -rf $BUILD_DIR/ffmpeg_source
rm -rf $BUILD_DIR/x264
rm -rf $BUILD_DIR/libvpx
rm -rf $BUILD_DIR/ffmpeg_build/share

if [ "$1" = "win32" ]; then
  FFMPEG_DEST="src/third_party/lib/win"
elif [ "$1" = "macos" ]; then
  FFMPEG_DEST="src/third_party/lib/macos"
else
  FFMPEG_DEST="src/third_party/lib/linux"
fi

mv $BUILD_DIR/ffmpeg_build/lib/*.a $FFMPEG_DEST

rm -rf $BUILD_DIR
