#!/bin/sh

BUILD_DIR=out
SOURCE_DIR=src
APP_NAME=censorman

pushd .

rm -rf $BUILD_DIR
mkdir $BUILD_DIR

cd $SOURCE_DIR

OPTS_PROD="-march=x86-64 -Ofast"
OPTS_DEBUG="-march=x86-64 -Ofast"
#-fsanitize=address -fno-omit-frame-pointer"

OPTS=$OPTS_DEBUG

SRCS="censorman/censorman.c"
INCLUDES="-I. -Ithird_party -Ithird_party/ffmpeg/include -Ithird_party/ncnn/include"
LIBS="-Lthird_party/ncnn/lib -lncnn -lgomp -Lthird_party/ffmpeg/lib -lavformat -lavcodec -lswscale -lavutil -lm -lz -lva -lva-drm -lvdpau -lX11 -lva-x11 -lx264 -lpthread"

CMD="g++ $OPTS $SRCS $INCLUDES $LIBS -o ../$BUILD_DIR/$APP_NAME"
echo "${CMD}"
exec $cmd

popd
