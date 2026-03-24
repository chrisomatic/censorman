#!/bin/sh

./tools/ncnn2mem models/scrfd_500m_gnkps.ncnn.param models/scrfd_500m_gnkps.ncnn.bin src/models/scrfd_face_ids.h scrfd_face_bin.h
./tools/ncnn2mem models/scrfd_person_2.5g.ncnn.param models/scrfd_person_2.5g.ncnn.bin src/models/scrfd_person_ids.h scrfd_person_bin.h
./tools/ncnn2mem models/license_plate.ncnn.param models/license_plate.ncnn.bin src/models/license_plate_ids.h license_plate_bin.h
./tools/ncnn2mem models/nudity.ncnn.param models/license_plate.ncnn.bin src/models/nudity_ids.h nudity_bin.h
