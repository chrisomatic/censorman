#!/bin/sh

pushd .

echo "Removing old build files"
rm -rf bin

echo "Creating new bin directory"
mkdir bin

srcs="main.cpp models/facedetectcnn-data.cpp models/facedetectcnn-model.cpp models/facedetectcnn.cpp"
opts="-march=native -Ofast"
# -mavx2
includes="-Iinclude"
libs="-lm -lz"
#includes="-I./include -I./ffmpeg/include"
#libs="-L./ffmpeg/lib -lavformat -lavcodec -lavutil -lswscale -lm -lz"

cmd="g++ ${srcs} ${includes} ${libs} ${opts} -o ./bin/censorman"
echo "${cmd}"
exec $cmd

popd
