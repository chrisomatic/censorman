#!/bin/sh

BUILD_DIR=bin
SOURCE_DIR=src
APP_NAME=censorman.exe

pushd .

rm -rf $BUILD_DIR
mkdir $BUILD_DIR

cd $SOURCE_DIR

OPTS_PROD="-march=x86-64 -Ofast"
OPTS_DEBUG="-march=x86-64 -Ofast"
#-fsanitize=address -fno-omit-frame-pointer"

OPTS=$OPTS_DEBUG

SRCS="censorman/censorman.c"
INCLUDES="-I. -Ithird_party -Ithird_party/stb -Ithird_party/ffmpeg/win/include -Ithird_party/ncnn/win/include"
LIBS="-Lthird_party/ncnn/win/lib -lncnn -lncnn_shim -lgomp -Lthird_party/ffmpeg/win/lib -lavformat -lavcodec -lswscale -lavutil -lm -lx264 -lvpx -lpthread -lstdc++ -lws2_32 -lbcrypt -static -liconv -lz"

CMD="gcc $OPTS $SRCS $INCLUDES $LIBS -o ../$BUILD_DIR/$APP_NAME"
echo "${CMD}"
exec $CMD

popd
