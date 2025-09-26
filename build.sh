#!/bin/sh

pushd .

echo "Removing old build files"
rm -rf bin

echo "Creating new bin directory"
mkdir bin

if [ "$1" = "win32" ]; then

srcs="src/main.cpp third_party/facedetectcnn-data.cpp third_party/facedetectcnn-model.cpp third_party/facedetectcnn.cpp"

includes="-Isrc -Ithird_party -Ithird_party/ffmpeg/include"

libs="-Lthird_party/ffmpeg/lib \
      -Wl,-Bstatic \
      -lavformat -lavcodec -lswscale -lavutil -lpthread -lz -lx264 \
      -liconv -static-libgcc -static-libstdc++ \
      -lstdc++ -lgcc -Wl,-Bdynamic -lbcrypt"

opts="-static -march=x86-64 -Ofast -D__USE_MINGW_ANSI_STDIO=0"

output="./bin/censorman.exe"

cmd="g++ ${srcs} ${includes} ${libs} ${opts} -o ${output}"
echo "${cmd}"
exec $cmd

else

srcs="src/main.cpp third_party/facedetectcnn-data.cpp third_party/facedetectcnn-model.cpp third_party/facedetectcnn.cpp"
opts="-march=native -Ofast"
#-mavx2
includes="-Isrc -Ithird_party -Ithird_party/ffmpeg/include"
libs="-Lthird_party/ffmpeg/lib -lavformat -lavcodec -lswscale -lavutil -lm -lz -lva -lva-drm -lvdpau -lX11 -lva-x11 -lx264 -lpthread"

cmd="g++ ${srcs} ${includes} ${libs} ${opts} -o ./bin/censorman"
echo "${cmd}"
exec $cmd

fi

popd
