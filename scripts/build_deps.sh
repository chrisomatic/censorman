#!/bin/bash
# build_deps.sh - Build third-party dependencies for the current platform
# Usage: ./build_deps.sh <target>
#   target: all | ffmpeg | ncnn

set -e

# ─── Usage ───────────────────────────────────────────────────────────────────
if [ -z "$1" ]; then
  echo "Usage: $0 <all|ffmpeg|ncnn>"
  exit 1
fi

TARGET="$1"

# ─── Detect platform ─────────────────────────────────────────────────────────
case "$(uname -s)" in
  Darwin)  PLATFORM="macos" ;;
  Linux)   PLATFORM="linux" ;;
  MINGW*|MSYS*|CYGWIN*) PLATFORM="win32" ;;
  *)
    echo "ERROR: Unsupported platform: $(uname -s)"
    exit 1
    ;;
esac

# ─── Detect architecture ─────────────────────────────────────────────────────
ARCH=$(uname -m)   # x86_64 or arm64

# ─── CPU count ───────────────────────────────────────────────────────────────
case "$PLATFORM" in
  macos) NCPU=$(sysctl -n hw.logicalcpu) ;;
  *)     NCPU=$(nproc) ;;
esac

# ─── Output lib directory ────────────────────────────────────────────────────
case "$PLATFORM" in
  macos)  LIB_DEST="src/third_party/lib/macos" ;;
  linux)  LIB_DEST="src/third_party/lib/linux" ;;
  win32)  LIB_DEST="src/third_party/lib/win"   ;;
esac

echo "Platform : $PLATFORM"
echo "Arch     : $ARCH"
echo "CPUs     : $NCPU"
echo "Target   : $TARGET"
echo "Lib dest : $LIB_DEST"
echo ""

mkdir -p "$LIB_DEST"

# ═════════════════════════════════════════════════════════════════════════════
# BUILD FUNCTIONS
# ═════════════════════════════════════════════════════════════════════════════

# ─── ffmpeg (includes x264 and libvpx) ───────────────────────────────────────
build_ffmpeg() {
  echo "════════════════════════════════════"
  echo " Building ffmpeg (x264 + vpx)"
  echo "════════════════════════════════════"

  BUILD_DIR="$PWD/build_ffmpeg"
  PREFIX="$BUILD_DIR/ffmpeg_build"

  rm -rf "$BUILD_DIR"
  mkdir -p "$BUILD_DIR/ffmpeg_source"

  # ── Platform-specific compile flags ────────────────────────────────────────
  case "$PLATFORM" in
    macos)
      export CFLAGS="-O3 -arch $ARCH"
      export CXXFLAGS="$CFLAGS"
      export LDFLAGS="-arch $ARCH"
      FFMPEG_CC="clang"
      FFMPEG_EXTRA_CFLAGS="-I$PREFIX/include -O3 -arch $ARCH -ffunction-sections -fdata-sections"
      FFMPEG_EXTRA_LDFLAGS="-L$PREFIX/lib -Wl,-dead_strip -arch $ARCH"
      FFMPEG_EXTRA_LIBS="-lpthread -lm -framework CoreFoundation -framework CoreMedia -framework CoreVideo -framework VideoToolbox -framework AudioToolbox -framework Security"
      ;;
    linux)
      export CFLAGS="-static -static-libgcc -static-libstdc++ -O3 -march=x86-64"
      export CXXFLAGS="$CFLAGS"
      export LDFLAGS="-static"
      FFMPEG_CC="gcc"
      FFMPEG_EXTRA_CFLAGS="-I$PREFIX/include -O3 -march=x86-64 -ffunction-sections -fdata-sections"
      FFMPEG_EXTRA_LDFLAGS="-L$PREFIX/lib -Wl,--gc-sections"
      FFMPEG_EXTRA_LIBS="-lpthread -lm"
      ;;
    win32)
      export CFLAGS="-static -static-libgcc -static-libstdc++ -O3 -march=x86-64"
      export CXXFLAGS="$CFLAGS"
      export LDFLAGS="-static"
      FFMPEG_CC="gcc"
      FFMPEG_EXTRA_CFLAGS="-I$PREFIX/include -O3 -march=x86-64 -ffunction-sections -fdata-sections"
      FFMPEG_EXTRA_LDFLAGS="-L$PREFIX/lib -Wl,--gc-sections -static-libgcc -static-libstdc++"
      FFMPEG_EXTRA_LIBS="-lpthread -lm -lws2_32 -lbcrypt"
      ;;
  esac

  # ── x264 ───────────────────────────────────────────────────────────────────
  echo "  → Cloning x264..."
  cd "$BUILD_DIR"
  git clone https://code.videolan.org/videolan/x264.git x264
  cd x264

  case "$PLATFORM" in
    win32)
      ./configure \
        --prefix="$PREFIX" \
        --enable-static \
        --disable-cli \
        --disable-opencl \
        --host=x86_64-w64-mingw32 \
        --extra-cflags="$CFLAGS" \
        --extra-ldflags="$LDFLAGS"
      ;;
    *)
      ./configure \
        --prefix="$PREFIX" \
        --enable-static \
        --disable-cli \
        --disable-opencl \
        --extra-cflags="$CFLAGS" \
        --extra-ldflags="$LDFLAGS"
      ;;
  esac

  make -j$NCPU
  make install

  # ── libvpx ─────────────────────────────────────────────────────────────────
  echo "  → Cloning libvpx..."
  cd "$BUILD_DIR"
  git clone https://chromium.googlesource.com/webm/libvpx.git libvpx
  cd libvpx

  case "$PLATFORM" in
    macos)
      if [ "$ARCH" = "arm64" ]; then
        VPX_TARGET="arm64-darwin20-gcc"
      else
        VPX_TARGET="x86_64-darwin20-gcc"
      fi
      ;;
    linux)  VPX_TARGET="" ;;           # auto-detect
    win32)  VPX_TARGET="x86_64-win64-gcc" ;;
  esac

  VPX_TARGET_FLAG=""
  [ -n "$VPX_TARGET" ] && VPX_TARGET_FLAG="--target=$VPX_TARGET"

  ./configure \
    --prefix="$PREFIX" \
    $VPX_TARGET_FLAG \
    --disable-shared \
    --enable-static \
    --disable-examples \
    --disable-tools \
    --disable-docs \
    --disable-unit-tests \
    --extra-cflags="$CFLAGS"

  make -j$NCPU
  make install

  # ── FFmpeg ─────────────────────────────────────────────────────────────────
  echo "  → Cloning FFmpeg..."
  cd "$BUILD_DIR/ffmpeg_source"

  export PKG_CONFIG_PATH="$PREFIX/lib/pkgconfig"
  export PKG_CONFIG="pkg-config"

  git clone https://git.ffmpeg.org/ffmpeg.git ffmpeg --depth 1 --branch n8.0
  cd ffmpeg

  FFMPEG_CONFIGURE_EXTRA=""
  if [ "$PLATFORM" = "win32" ]; then
    FFMPEG_CONFIGURE_EXTRA="--target-os=mingw32 --arch=x86_64 --disable-w32threads --enable-pthreads"
  fi

  ./configure \
    --prefix="$PREFIX" \
    --pkg-config-flags="--static" \
    --extra-cflags="$FFMPEG_EXTRA_CFLAGS" \
    --extra-ldflags="$FFMPEG_EXTRA_LDFLAGS" \
    --extra-libs="$FFMPEG_EXTRA_LIBS" \
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
    --enable-swresample \
    --enable-protocol=file \
    --enable-demuxer=mov \
    --enable-demuxer=aac \
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
    --enable-encoder=libx264 \
    --enable-encoder=mpeg4 \
    --enable-encoder=libvpx_vp9 \
    --enable-encoder=aac \
    --enable-gpl \
    --enable-libx264 \
    --enable-libvpx \
    --enable-bsfs \
    --cc=$FFMPEG_CC \
    $FFMPEG_CONFIGURE_EXTRA

  make -j$NCPU
  make install

  # ── Copy libs and clean up ─────────────────────────────────────────────────
  mv "$PREFIX/lib/"*.a "$LIB_DEST/"
  rm -rf "$BUILD_DIR"
  echo "  ✓ ffmpeg libs installed to $LIB_DEST"
}

# ─── ncnn ────────────────────────────────────────────────────────────────────
build_ncnn() {
  echo "════════════════════════════════════"
  echo " Building ncnn"
  echo "════════════════════════════════════"

  BUILD_DIR="$PWD/build_ncnn"
  PREFIX="$BUILD_DIR/ncnn_build"

  rm -rf "$BUILD_DIR"
  mkdir -p "$BUILD_DIR/ncnn_source"

  cd "$BUILD_DIR/ncnn_source"
  git clone --depth=1 https://github.com/Tencent/ncnn.git ncnn
  cd ncnn

  case "$PLATFORM" in
    win32)
      cmake -D CMAKE_BUILD_TYPE=Release \
            -D CMAKE_INSTALL_PREFIX="$PREFIX" \
            -D CMAKE_SYSTEM_NAME=Windows \
            -D CMAKE_C_COMPILER=gcc \
            -D CMAKE_CXX_COMPILER=g++ \
            -D CMAKE_RC_COMPILER=windres \
            -D NCNN_OPENMP=OFF \
            -D NCNN_BUILD_TESTS=OFF \
            -D NCNN_BUILD_EXAMPLES=OFF \
            -D NCNN_BUILD_TOOLS=ON \
            -D NCNN_SHARED_LIB=OFF \
            -D CMAKE_C_FLAGS="-static-libgcc" \
            -D CMAKE_CXX_FLAGS="-static-libgcc -static-libstdc++" \
            -G "Unix Makefiles" \
            .
      ;;
    macos)
      cmake -D CMAKE_BUILD_TYPE=Release \
            -D CMAKE_INSTALL_PREFIX="$PREFIX" \
            -D CMAKE_OSX_ARCHITECTURES="$ARCH" \
            -D NCNN_OPENMP=OFF \
            -D NCNN_BUILD_TESTS=OFF \
            -D NCNN_BUILD_EXAMPLES=OFF \
            -D NCNN_BUILD_TOOLS=ON \
            -D NCNN_SHARED_LIB=OFF \
            .
      ;;
    linux)
      cmake -D CMAKE_BUILD_TYPE=Release \
            -D CMAKE_INSTALL_PREFIX="$PREFIX" \
            -D NCNN_OPENMP=OFF \
            -D NCNN_BUILD_TESTS=OFF \
            -D NCNN_BUILD_EXAMPLES=OFF \
            -D NCNN_BUILD_TOOLS=ON \
            -D NCNN_SHARED_LIB=OFF \
            .
      ;;
  esac

  cmake --build . --config Release -j$NCPU
  cmake --build . --config Release --target install

  # ── Build ncnn_shim ────────────────────────────────────────────────────────
  echo "  → Building ncnn_shim..."
  cd "$OLDPWD"

  case "$PLATFORM" in
    win32)
      g++ -c src/ncnn_shim.cpp \
          -I src/third_party/include/ncnn \
          -static-libgcc -static-libstdc++ \
          -o ncnn_shim.o
      ;;
    macos)
      clang++ -c src/ncnn_shim.cpp \
          -arch $ARCH \
          -I src/third_party/include/ncnn \
          -o ncnn_shim.o
      ;;
    linux)
      g++ -c src/ncnn_shim.cpp \
          -I src/third_party/include/ncnn \
          -o ncnn_shim.o
      ;;
  esac

  ar rcs "$LIB_DEST/libncnn_shim.a" ncnn_shim.o
  rm ncnn_shim.o

  mv "$PREFIX/lib/"*.a "$LIB_DEST/"
  rm -rf "$BUILD_DIR"
  echo "  ✓ ncnn libs installed to $LIB_DEST"
}

# ═════════════════════════════════════════════════════════════════════════════
# DISPATCH
# ═════════════════════════════════════════════════════════════════════════════

case "$TARGET" in
  all)
    build_ffmpeg
    build_ncnn
    ;;
  ffmpeg) build_ffmpeg ;;
  ncnn)   build_ncnn   ;;
  *)
    echo "ERROR: Unknown target '$TARGET'"
    echo "Usage: $0 <all|ffmpeg|ncnn>"
    exit 1
    ;;
esac

echo ""
echo "Done. Libs are in $LIB_DEST"
