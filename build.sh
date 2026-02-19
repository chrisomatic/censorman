#!/bin/sh

pushd .

echo "Removing old build files"
rm -rf bin

echo "Creating new bin directory"
mkdir bin

if [ "$1" = "win32" ]; then

srcs="src/main.cpp third_party/facedetectcnn-data.cpp third_party/facedetectcnn-model.cpp third_party/facedetectcnn.cpp"
opts="-static -static-libgcc -static-libstdc++ -march=x86-64 -Ofast"
includes="-Isrc -Ithird_party -Ithird_party/ffmpeg/include"
#libs="-lgomp -Lthird_party/ffmpeg/lib -lavformat -lavcodec -lswscale -lavutil -lz -lx264 -liconv -lbcrypt"
libs="-Lthird_party/ncnn/lib -lncnn -lgomp -Lthird_party/ffmpeg/lib -lavformat -lavcodec -lswscale -lavutil -lz -lx264 -liconv -lbcrypt"

output="./bin/censorman.exe"

cmd="g++ ${srcs} ${includes} ${libs} ${opts} -o ${output}"
echo "${cmd}"
exec $cmd

else

srcs="src/main.cpp third_party/facedetectcnn-data.cpp third_party/facedetectcnn-model.cpp third_party/facedetectcnn.cpp"
opts="-march=x86-64 -Ofast"
#-mavx2
includes="-Isrc -Ithird_party -Ithird_party/ffmpeg/include -Ithird_party/ncnn/include"
#libs="-lgomp -Lthird_party/ffmpeg/lib -lavformat -lavcodec -lswscale -lavutil -lm -lz -lva -lva-drm -lvdpau -lX11 -lva-x11 -lx264 -lpthread"
libs="-Lthird_party/ncnn/lib -lncnn -lgomp -Lthird_party/ffmpeg/lib -lavformat -lavcodec -lswscale -lavutil -lm -lz -lva -lva-drm -lvdpau -lX11 -lva-x11 -lx264 -lpthread"

cmd="g++ ${srcs} ${includes} ${libs} ${opts} -o ./bin/censorman"
echo "${cmd}"
exec $cmd

fi

popd
