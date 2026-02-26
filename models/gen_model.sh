#!/bin/sh

python -m onnxsim onnx/scrfd_person_2.5g.onnx scrfd_person_2.5g.onnx
../third_party/pnnx scrfd_person_2.5g.onnx inputshape=[1,3,640,640]
