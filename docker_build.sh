#!/bin/sh
# build_linux_docker.sh - Build censorman for linux/amd64 against GLIBC 2.36
# Run this from the root of your censorman project
set -e

IMAGE="censorman-builder"
CONTAINER="censorman-extract"
OUT="bin/censorman"

echo "Building Docker image..."
docker build --platform=linux/amd64 -f Dockerfile.build -t $IMAGE .

echo "Extracting binary..."
docker create --name $CONTAINER $IMAGE
mkdir -p bin
docker cp $CONTAINER:/censorman/bin/censorman ./$OUT
docker rm $CONTAINER

echo ""
echo "Done. Binary at ./$OUT"
echo ""
ldd ./$OUT
