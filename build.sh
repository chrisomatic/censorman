#!/bin/sh
# build.sh - Build censorman for the current platform
# Usage: ./build.sh [prod]
#   (no args) = debug build
#   prod       = optimized production build

set -e

BUILD_DIR=bin
SOURCE_DIR=src
APP_NAME=censorman

# ─── Detect platform ────────────────────────────────────────────────────────
case "$(uname -s)" in
  Darwin)  PLATFORM="macos" ;;
  Linux)   PLATFORM="linux" ;;
  MINGW*|MSYS*|CYGWIN*) PLATFORM="win32" ;;
  *)
    echo "ERROR: Unsupported platform: $(uname -s)"
    exit 1
    ;;
esac

# ─── Detect architecture ────────────────────────────────────────────────────
ARCH=$(uname -m)   # x86_64 or arm64

echo "Platform : $PLATFORM"
echo "Arch     : $ARCH"
echo "Mode     : ${1:-debug}"

# ─── Compiler & flags by platform ───────────────────────────────────────────
case "$PLATFORM" in
  macos)
    CC="clang"
    OPTS_PROD="-arch $ARCH -O3 -ffast-math"
    OPTS_DEBUG="-arch $ARCH -O1 -Wall -pedantic"
    LIB_DIR="third_party/lib/macos"
    LIBS="-L$LIB_DIR -lncnn -lncnn_shim \
          -lavformat -lavcodec -lswscale -lswresample -lavutil \
          -lx264 -lvpx \
          -lm -lz -lc++ -lpthread -liconv"
    FRAMEWORKS="-framework CoreFoundation \
                -framework CoreMedia \
                -framework CoreVideo \
                -framework CoreServices \
                -framework VideoToolbox \
                -framework AudioToolbox \
                -framework Security"
    APP_NAME=censorman
    ;;
  linux)
    CC="gcc"
    OPTS_PROD="-march=x86-64 -O3 -ffast-math"
    OPTS_DEBUG="-march=x86-64 -O1 -Wall -pedantic"
    LIB_DIR="third_party/lib/linux"
    LIBS="-L$LIB_DIR -lncnn -lncnn_shim \
          -lavformat -lavcodec -lswscale -lswresample -lavutil \
          -lm -lz \
          -lva -lva-drm -lvdpau -lX11 -lva-x11 \
          -lx264 -lvpx -lpthread -lstdc++"
    FRAMEWORKS=""
    APP_NAME=censorman
    ;;
  win32)
    CC="gcc"
    OPTS_PROD="-march=x86-64 -O3 -ffast-math"
    OPTS_DEBUG="-march=x86-64 -O1 -Wall -pedantic"
    LIB_DIR="third_party/lib/win"
    LIBS="-L$LIB_DIR -lncnn -lncnn_shim \
          -lavformat -lavcodec -lswscale -lswresample -lavutil \
          -lm -lx264 -lvpx \
          -lpthread -lstdc++ -lws2_32 -lbcrypt \
          -static -liconv -lz"
    FRAMEWORKS=""
    APP_NAME=censorman.exe
    ;;
esac

# ─── Select debug vs prod ────────────────────────────────────────────────────
if [ "$1" = "prod" ]; then
  OPTS=$OPTS_PROD
else
  OPTS=$OPTS_DEBUG
fi

# ─── Build ───────────────────────────────────────────────────────────────────
pushd . > /dev/null
rm -rf $BUILD_DIR
mkdir $BUILD_DIR
cd $SOURCE_DIR

SRCS="censorman/censorman.c"
INCLUDES="-I. -Ithird_party/include"

CMD="$CC $OPTS $SRCS $INCLUDES $LIBS $FRAMEWORKS -o ../$BUILD_DIR/$APP_NAME"
echo ""
echo "CMD: $CMD"
echo ""
exec $CMD

popd > /dev/null
