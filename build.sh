#!/bin/sh

pushd .

echo "Removing old build files"
rm -rf bin

echo "Creating new bin directory"
mkdir bin

srcs="src/main.cpp third_party/facedetectcnn-data.cpp third_party/facedetectcnn-model.cpp third_party/facedetectcnn.cpp"
opts="-march=native -Ofast"
#-mavx2
includes="-Isrc -Ithird_party -Ithird_party/ffmpeg/include"
libs="-Lthird_party/ffmpeg/lib -lavformat -lavcodec -lswscale -lavutil -lm -lz -lva -lva-drm -lvdpau -lX11 -lva-x11 -lx264 -lpthread"

cmd="g++ ${srcs} ${includes} ${libs} ${opts} -o ./bin/censorman"
echo "${cmd}"
exec $cmd

popd
