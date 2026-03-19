#!/bin/sh

BUILD_DIR=bin
SOURCE_DIR=src
APP_NAME=censorman

pushd .

rm -rf $BUILD_DIR
mkdir $BUILD_DIR

cd $SOURCE_DIR

OPTS_PROD="-march=x86-64 -Ofast"
OPTS_DEBUG="-march=x86-64 -Ofast -Wall -pedantic"
#OPTS_DEBUG="-march=x86-64 -Ofast -Wall -pedantic -fsanitize=address -fno-omit-frame-pointer"

OPTS=$OPTS_DEBUG

SRCS="censorman/censorman.c"
INCLUDES="-I. -Ithird_party -Ithird_party/stb -Ithird_party/ffmpeg/include -Ithird_party/ncnn/include"
LIBS="-Lthird_party/ncnn/lib -lncnn -lncnn_shim -lgomp -Lthird_party/ffmpeg/lib -lavformat -lavcodec -lswscale -lswresample -lavutil -lm -lz -lva -lva-drm -lvdpau -lX11 -lva-x11 -lx264 -lvpx -lpthread -lstdc++ "

CMD="gcc $OPTS $SRCS $INCLUDES $LIBS -o ../$BUILD_DIR/$APP_NAME"
echo "${CMD}"
exec $CMD

popd
