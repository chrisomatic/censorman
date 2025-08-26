#!/bin/sh

pushd .

echo "Removing old build files"
rm -rf bin

echo "Creating new bin directory"
mkdir bin

srcs="main.cpp models/facedetectcnn-data.cpp models/facedetectcnn-model.cpp models/facedetectcnn.cpp"
opts="-march=native -Ofast"
#-mavx2
includes="-Iinclude -Iffmpeg/include"
libs="-Lffmpeg/lib -lavformat -lavcodec -lavutil -lswscale -lm -lz -lva -lva-drm -lvdpau -lX11 -lva-x11 -lx264 -lpthread"
#libs="-lm -lz"

cmd="g++ ${srcs} ${includes} ${libs} ${opts} -o ./bin/censorman"
echo "${cmd}"
exec $cmd

popd
