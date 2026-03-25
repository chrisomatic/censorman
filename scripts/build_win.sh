#!/bin/sh

BUILD_DIR=bin
SOURCE_DIR=src
APP_NAME=censorman.exe

pushd .

rm -rf $BUILD_DIR
mkdir $BUILD_DIR

cd $SOURCE_DIR

OPTS_PROD="-march=x86-64 -Ofast"
#OPTS_DEBUG="-march=x86-64 -Ofast -Wall -pedantic"
OPTS_DEBUG="-march=x86-64 -Ofast -Wall -pedantic -fno-omit-frame-pointer"

OPTS=$OPTS_DEBUG

SRCS="censorman/censorman.c"
INCLUDES="-I. -Ithird_party/include"
LIBS="-Lthird_party/lib/win -lncnn -lncnn_shim -lgomp -lavformat -lavcodec -lswscale -lswresample -lavutil -lexif -lm -lx264 -lvpx -lpthread -lstdc++ -lws2_32 -lbcrypt -static -liconv -lz"

CMD="gcc $OPTS $SRCS $INCLUDES $LIBS -o ../$BUILD_DIR/$APP_NAME"
echo "${CMD}"
exec $CMD

popd
