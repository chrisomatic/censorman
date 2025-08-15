#!/bin/sh

pushd .

echo "Removing old build files"
rm -rf build
rm -rf bin

echo "Creating new build directories"
mkdir build
mkdir bin

cd build

srcs="../main.cpp ../models/facedetectcnn-data.cpp ../models/facedetectcnn-model.cpp ../models/facedetectcnn.cpp"
opts="-O3"
includes="-I../include -I../ffmpeg/include"
libs="-L../ffmpeg/lib -lavformat -lavcodec -lavutil -lswscale -lm -lz"

cmd="g++ ${srcs} ${includes} ${libs} -o ../bin/censorman"
echo "${cmd}"
exec $cmd

popd
