#!/bin/sh

# Settings
OUTPUT_DIR=output
VERBOSE=0

VERBOSE_STR=""
if [ "${VERBOSE}" -eq 1 ]; then
    VERBOSE_STR="--verbose"
fi

rm -rf ${OUTPUT_DIR}
mkdir -p ${OUTPUT_DIR}

# Images
./bin/censorman assets/images -t blur --out_file ${OUTPUT_DIR}/images --debug --no_scale ${VERBOSE_STR}

# Videos
./bin/censorman assets/vids/nosound_1face_60s.mp4 -t blur --out_file ${OUTPUT_DIR}/vids/nosound_1face_60s.mp4 --debug --no_scale ${VERBOSE_STR}
