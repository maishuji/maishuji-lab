#!/usr/bin/env bash
set -euo pipefail

usage() {
    echo "Usage: $0 <flycast-capture.png>" >&2
    exit 2
}

fail() {
    echo "Flycast lifecycle frame check: $*" >&2
    exit 1
}

[[ $# -eq 1 ]] || usage
image_path=$(realpath "$1")
[[ -f "$image_path" ]] || { echo "Image not found: $image_path" >&2; exit 2; }

if command -v magick >/dev/null 2>&1; then
    image_convert=(magick)
    image_identify=(magick identify)
elif command -v convert >/dev/null 2>&1 && command -v identify >/dev/null 2>&1; then
    image_convert=(convert)
    image_identify=(identify)
else
    echo "ImageMagick 6 or 7 is required (convert/identify or magick)." >&2
    exit 2
fi

dimensions=$("${image_identify[@]}" -format '%w %h' "$image_path") || \
    fail "could not read image dimensions."
read -r width height <<<"$dimensions"
awk -v width="$width" -v height="$height" 'BEGIN {
    if(width < 320 || height < 240 || height == 0 || width / height < 1.30 || width / height > 1.36)
        exit 1
}' || fail "unexpected capture dimensions; expected a 4:3 game window."

sample_region() {
    local center_x=$1 center_y=$2
    "${image_convert[@]}" "$image_path" -resize 640x480! \
        -crop "9x9+$((center_x - 4))+$((center_y - 4))" +repage \
        -format '%[fx:mean.r] %[fx:mean.g] %[fx:mean.b]' info:
}

top=$(sample_region 320 40) || fail "could not sample above the lifecycle frame."
center=$(sample_region 320 240) || fail "could not sample the lifecycle frame center."
left=$(sample_region 60 240) || fail "could not sample the left lifecycle frame."
right=$(sample_region 580 240) || fail "could not sample the right lifecycle frame."
bottom=$(sample_region 320 430) || fail "could not sample below the lifecycle frame."

read -r top_r top_g top_b <<<"$top"
read -r center_r center_g center_b <<<"$center"
read -r left_r left_g left_b <<<"$left"
read -r right_r right_g right_b <<<"$right"
read -r bottom_r bottom_g bottom_b <<<"$bottom"

awk \
    -v top_r="$top_r" -v top_g="$top_g" -v top_b="$top_b" \
    -v center_r="$center_r" -v center_g="$center_g" -v center_b="$center_b" \
    -v left_r="$left_r" -v left_g="$left_g" -v left_b="$left_b" \
    -v right_r="$right_r" -v right_g="$right_g" -v right_b="$right_b" \
    -v bottom_r="$bottom_r" -v bottom_g="$bottom_g" -v bottom_b="$bottom_b" \
    'BEGIN {
        dark_ok = top_r < 0.08 && top_g < 0.08 && top_b < 0.08 &&
                  center_r < 0.08 && center_g < 0.08 && center_b < 0.08 &&
                  left_r < 0.08 && left_g < 0.08 && left_b < 0.08 &&
                  right_r < 0.08 && right_g < 0.08 && right_b < 0.08 &&
                  bottom_r < 0.08 && bottom_g < 0.08 && bottom_b < 0.08

        if(!dark_ok) {
            print "FAIL: expected the lifecycle example to leave a dark frame."
            printf "  above:  %.3f %.3f %.3f\n", top_r, top_g, top_b
            printf "  center: %.3f %.3f %.3f\n", center_r, center_g, center_b
            printf "  left:   %.3f %.3f %.3f\n", left_r, left_g, left_b
            printf "  right:  %.3f %.3f %.3f\n", right_r, right_g, right_b
            printf "  below:  %.3f %.3f %.3f\n", bottom_r, bottom_g, bottom_b
            exit 1
        }

        print "PASS: Flycast rendered the expected dark lifecycle frame."
        printf "  above:  %.3f %.3f %.3f\n", top_r, top_g, top_b
        printf "  center: %.3f %.3f %.3f\n", center_r, center_g, center_b
        printf "  left:   %.3f %.3f %.3f\n", left_r, left_g, left_b
        printf "  right:  %.3f %.3f %.3f\n", right_r, right_g, right_b
        printf "  below:  %.3f %.3f %.3f\n", bottom_r, bottom_g, bottom_b
    }'
