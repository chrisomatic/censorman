# Censorman

Detect features in images and video, and write them back with some transformations on those regions!

That's the power of Censor Man!

```
      "All in a day's work!"
                - Censor Man
   _O_
 /|-x-|\
/  \_/  \
   / \
 _/   \_

```

## Usage

```
    [USAGE]
        censorman <in_file> -o <out_file> -d {class_list} -t {transform_list} [-c confidence_threshold][-k thread_count] [--debug] [--image <texture_image_path>] [--block_scale <block_scale>] [--is_quiet]

    [DESCRIPTION]
        Takes an image file, detects regions of human faces (for now), applies transformations on those regions and writes back an output image file

    [ARGUMENTS]
        in_file:              Path to input image file (or folder) (.jpg, .png, .bmp)
        out_file:             Path to output image file (.jpg, .png, .bmp)
        class_list:           {face}
        transform_list:       {pixelate, blur, blackout, scramble, texture}
        confidence_threshold: Discard any boxes lower than this (0 - 100)
        thread_count:         How many threads to use to detect (default to number of cores)
        debug:                Print debug info and draw boxes on output image
        texture_image_path:   Used with 'texture' transform
        block_scale:          Value between 0.0 and 1.0. Used to scale blocks in pixelate transform
        is_quiet:             Suppress standard log output
```
