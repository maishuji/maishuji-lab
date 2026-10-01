#!/usr/bin/env bash
set -euo pipefail

[[ $# -eq 1 ]] || { echo "Usage: $0 <flycast-capture.png>" >&2; exit 2; }
image_path=$(realpath "$1")
[[ -f "$image_path" ]] || { echo "Image not found: $image_path" >&2; exit 2; }

if command -v magick >/dev/null 2>&1; then
    image_convert=magick
    image_identify="magick identify"
elif command -v convert >/dev/null 2>&1 && command -v identify >/dev/null 2>&1; then
    image_convert=convert
    image_identify=identify
else
    echo "ImageMagick 6 or 7 is required (convert/identify or magick)." >&2
    exit 2
fi

dimensions=$(eval "$image_identify -format '%w %h' \"$image_path\"") ||
    { echo "Could not read image dimensions." >&2; exit 1; }
read -r width height <<<"$dimensions"
awk -v width="$width" -v height="$height" 'BEGIN {
    if(width < 320 || height < 240 || height == 0 ||
       width / height < 1.30 || width / height > 1.36) exit 1
}' || { echo "Expected a 4:3 game window." >&2; exit 1; }

sample() {
    local geometry=$1
    eval "$image_convert \"$image_path\" -resize 640x480! -crop \"$geometry\" +repage -format '%[fx:mean.r] %[fx:mean.g] %[fx:mean.b] %[fx:standard_deviation.r] %[fx:standard_deviation.g] %[fx:standard_deviation.b]' info:"
}

model=$(sample '220x190+210+145')
top=$(sample '9x9+320+36')
left=$(sample '9x9+36+240')
right=$(sample '9x9+604+240')
bottom=$(sample '9x9+320+436')
read -r model_r model_g model_b model_sd_r model_sd_g model_sd_b <<<"$model"
read -r top_r top_g top_b <<<"$top"
read -r left_r left_g left_b <<<"$left"
read -r right_r right_g right_b <<<"$right"
read -r bottom_r bottom_g bottom_b <<<"$bottom"

awk \
    -v model_r="$model_r" -v model_g="$model_g" -v model_b="$model_b" \
    -v model_sd_r="$model_sd_r" -v model_sd_g="$model_sd_g" -v model_sd_b="$model_sd_b" \
    -v top_r="$top_r" -v top_g="$top_g" -v top_b="$top_b" \
    -v left_r="$left_r" -v left_g="$left_g" -v left_b="$left_b" \
    -v right_r="$right_r" -v right_g="$right_g" -v right_b="$right_b" \
    -v bottom_r="$bottom_r" -v bottom_g="$bottom_g" -v bottom_b="$bottom_b" \
    'BEGIN {
        model_ok = model_r + model_g + model_b > 0.30 &&
                   model_sd_r + model_sd_g + model_sd_b > 0.02
        if(!model_ok) {
            print "FAIL: expected a visible, varied DCM1 model."
            printf "  model: %.3f %.3f %.3f; variation %.3f %.3f %.3f\n", model_r, model_g, model_b, model_sd_r, model_sd_g, model_sd_b
            exit 1
        }
        print "PASS: Flycast rendered the decoded DCM1 model."
    }'
