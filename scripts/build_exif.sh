#!/bin/sh

set -e
BUILD_DIR="$PWD/build"
PREFIX="$BUILD_DIR/exif_build"

pushd .
rm -rf $BUILD_DIR
mkdir -p $BUILD_DIR
cd $BUILD_DIR

mkdir -p exif_build

git clone --branch libexif-0_6_25-release https://github.com/libexif/libexif.git libexif

cd libexif
autoreconf -i

if [ "$1" = "win32" ]; then
    ./configure \
        --prefix="$PREFIX" \
        --enable-static
        --disable-shared
        --host=x86_64-w64-mingw32 \
        --extra-cflags="$CFLAGS" \
        --extra-ldflags="$LDFLAGS"
else
    ./configure \
        --prefix="$PREFIX" \
        --enable-static \
        --disable-shared
fi

make -j$(nproc)
make install

popd

if [ "$1" = "win32" ]; then
  EXIF_DEST="src/third_party/lib/win"
elif [ "$1" = "macos" ]; then
  EXIF_DEST="src/third_party/lib/macos"
else
  EXIF_DEST="src/third_party/lib/linux"
fi

mv $BUILD_DIR/exif_build/lib/*.a $EXIF_DEST

rm -rf $BUILD_DIR
