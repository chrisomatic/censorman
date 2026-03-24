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
INCLUDES="-I. -Ithird_party/include"
LIBS="-Lthird_party/lib/linux -Lthird_party/lib/macos -lncnn -lncnn_shim -lgomp -lavformat -lavcodec -lswscale -lswresample -lavutil -lexif -lm -lz -lva -lva-drm -lvdpau -lX11 -lva-x11 -lx264 -lvpx -lpthread -lstdc++ "

CMD="gcc $OPTS $SRCS $INCLUDES $LIBS -o ../$BUILD_DIR/$APP_NAME"
echo "${CMD}"
exec $CMD

popd
