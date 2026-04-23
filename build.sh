
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
    # macOS/clang: use explicit .a paths — no -Wl,-Bstatic support
    LIBS="$LIB_DIR/libncnn.a $LIB_DIR/libncnn_shim.a \
          $LIB_DIR/libavformat.a $LIB_DIR/libavcodec.a \
          $LIB_DIR/libswscale.a $LIB_DIR/libswresample.a \
          $LIB_DIR/libavutil.a \
          $LIB_DIR/libexif.a \
          $LIB_DIR/libx264.a $LIB_DIR/libvpx.a \
          $LIB_DIR/libmupdf.a $LIB_DIR/libmupdf-third.a \
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
    OPTS_PROD="-march=x86-64 -O3 -ffast-math -s -static-libgcc -static-libstdc++"
    OPTS_DEBUG="-march=x86-64 -O1 -Wall -pedantic -static-libgcc -static-libstdc++"
    LIB_DIR="third_party/lib/linux"
    # Linux/gcc: use -Wl,-Bstatic for your libs, -Bdynamic for system libs
    # -lva -lva-drm etc. are GPU/display libs; keep dynamic (no static versions typically)
    LIBS="-L$LIB_DIR \
          -Wl,-Bstatic \
          -lncnn -lncnn_shim \
          -lavformat -lavcodec -lswscale -lswresample -lavutil \
          -lexif \
          -lx264 -lvpx \
          -lmupdf -lmupdf-third \
          -lz -lstdc++ \
          -Wl,-Bdynamic \
          -lm -lpthread"
    FRAMEWORKS=""
    APP_NAME=censorman
    ;;
  win32)
    CC="gcc"
    OPTS_PROD="-march=x86-64-v2 -O3 -ffast-math -s -static-libgcc -static-libstdc++"
    OPTS_DEBUG="-march=x86-64-v2 -O1 -Wall -pedantic -static-libgcc -static-libstdc++" 
    LIB_DIR="third_party/lib/win"
    LIBS="-L$LIB_DIR \
          -Wl,-Bstatic \
          -lncnn -lncnn_shim \
          -lavformat -lavcodec -lswscale -lswresample -lavutil \
          -lexif \
          -lx264 -lvpx \
          -lmupdf -lmupdf-third \
          -liconv -lz \
          -lstdc++ -lpthread \
          -Wl,-Bdynamic \
          -lws2_32 -lbcrypt" 
    FRAMEWORKS=""
    APP_NAME=censorman.exe
    ;;
esac
# ─── Select debug vs prod ────────────────────────────────────────────────────
if [ "$1" = "prod" ]; then
  OPTS=$OPTS_PROD
  # Strip symbols in prod (Linux/Windows; macOS: run strip -x manually after)
  case "$PLATFORM" in
    linux|win32) OPTS="$OPTS -s" ;;
  esac
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
