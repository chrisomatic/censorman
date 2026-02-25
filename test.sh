#!/bin/sh

OUTPUT_DIR=output

rm -rf ${OUTPUT_DIR}
mkdir -p ${OUTPUT_DIR}

# Images
./bin/censorman assets/images -t blur --out_file ${OUTPUT_DIR}/images --debug --no_scale

# Videos
./bin/censorman assets/vids/nosound_1face_60s.mp4 -t blur --out_file ${OUTPUT_DIR}/nosound_1face_60s.mp4 --debug --no_scale
