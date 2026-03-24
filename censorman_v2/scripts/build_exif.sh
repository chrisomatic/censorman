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
  rm -rf $PWD/src/third_party/libexif/win
  mv $BUILD_DIR/exif_build src/third_party/libexif/win
else
  rm -rf $PWD/src/third_party/libexif
  mv $BUILD_DIR/exif_build src/third_party/libexif
fi

rm -rf $BUILD_DIR
