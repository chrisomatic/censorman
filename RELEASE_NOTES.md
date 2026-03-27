# Censorman V2

**Change Log**

- Integrated Tencent NCNN inference engine [https://github.com/tencent/NCNN](https://github.com/tencent/NCNN)
- Added new detection CNN (Convolutional Neural Network) models
    - **face** (InsightFace SCRFD face - 500m GNKPS)
    - **person** (InsightFace SCRFD person - 500m)
    - **license\_plate** (YOLONet-v8 trained - 2.5g)
    - **nudity** (NudeNET - 2.5g)
- Added support for MacOS
- Added pre-pass to detect video discontinuities and quick motion
- Rewrote codebase in pure C with bespoke base library (Arenas, Strings, CmdLine, Math, Lists)
- Added "--facial_features" option to break face boxes down to eyes, nose, and mouth
- Updated Bounding Box (BBX) File format to V2 which includes multiple assets and box detection types
- Added support for multiple detection types per run with configurable confidence thresholds and NMS IoU thresholds
- Added support for running detection on multiple video and/or image assets in a single run
- Reworked bounding box interpolation for videos to anchor from middle of the box
- Added audio distortion feature (ring modulation) with configurable distortion Hz
- Entire program is threaded, and threads are used on parallalizable video work (frame loading, detection, applying filters, frame saving)
- Enhanced --debug markup with padding boxes and labels
- Added support for comma-delimited assets on command-line
- Added bilinear scaling with letterbox padding and rotation routines for images
- Added Stopwatch API for time printing and simple diagnostics
- Added support for audio passthrough for video files
- Added support for VP9 video codec
- Added support for AAC audio codec
- Added support for more image formats: BMP, PSD, TGA, HDR, PIC, GIF(unanimated), PNM
- Added support for EXIF metadata orientation for image files
- Enabled audio decoders and parsers (mp3,opus,vorbis)
- Fixed video rotation metadata on output
- Fixed bug with pixelate filter
