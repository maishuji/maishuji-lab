#!/usr/bin/env bash
set -euo pipefail

usage() {
    echo "Usage: $0 <flycast-capture.png>" >&2
    exit 2
}

fail() {
    echo "Flycast particle-frame check: $*" >&2
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

dimensions=$("${image_identify[@]}" -format '%w %h' "$image_path") ||
    fail "could not read image dimensions."
read -r width height <<<"$dimensions"
awk -v width="$width" -v height="$height" 'BEGIN {
    if(width < 320 || height < 240 || height == 0 || width / height < 1.30 || width / height > 1.36)
        exit 1
}' || fail "unexpected capture dimensions: ${width:-?}x${height:-?}; expected a 4:3 game window."

sample_region() {
    local geometry=$1
    "${image_convert[@]}" "$image_path" -resize 640x480! \
        -crop "$geometry" +repage \
        -format '%[fx:mean.r] %[fx:mean.g] %[fx:mean.b]' info:
}

left_particles=$(sample_region '160x300+40+90') ||
    fail "could not sample the red particle region."
center_particles=$(sample_region '160x300+240+90') ||
    fail "could not sample the green particle region."
right_particles=$(sample_region '160x300+440+90') ||
    fail "could not sample the blue particle region."
background_top=$(sample_region '9x9+316+36') ||
    fail "could not sample above the particles."
background_bottom=$(sample_region '9x9+316+436') ||
    fail "could not sample below the particles."

read -r left_r left_g left_b <<<"$left_particles"
read -r center_r center_g center_b <<<"$center_particles"
read -r right_r right_g right_b <<<"$right_particles"
read -r top_r top_g top_b <<<"$background_top"
read -r bottom_r bottom_g bottom_b <<<"$background_bottom"

awk \
    -v left_r="$left_r" -v left_g="$left_g" -v left_b="$left_b" \
    -v center_r="$center_r" -v center_g="$center_g" -v center_b="$center_b" \
    -v right_r="$right_r" -v right_g="$right_g" -v right_b="$right_b" \
    -v top_r="$top_r" -v top_g="$top_g" -v top_b="$top_b" \
    -v bottom_r="$bottom_r" -v bottom_g="$bottom_g" -v bottom_b="$bottom_b" \
    'BEGIN {
        left_ok = left_r > 0.02 && left_r > left_g * 1.80 && left_r > left_b * 1.40
        center_ok = center_g > 0.02 && center_g > center_r * 1.50 && center_g > center_b * 1.10
        right_ok = right_b > 0.02 && right_b > right_r * 1.30 && right_b > right_g * 1.10
        background_ok = top_r < 0.08 && top_g < 0.08 && top_b < 0.08 &&
                        bottom_r < 0.08 && bottom_g < 0.08 && bottom_b < 0.08

        if(!left_ok || !center_ok || !right_ok || !background_ok) {
            print "FAIL: expected red, green, and blue particle batches on a dark background."
            printf "  red region:    %.3f %.3f %.3f\n", left_r, left_g, left_b
            printf "  green region:  %.3f %.3f %.3f\n", center_r, center_g, center_b
            printf "  blue region:   %.3f %.3f %.3f\n", right_r, right_g, right_b
            printf "  above batch:   %.3f %.3f %.3f\n", top_r, top_g, top_b
            printf "  below batch:   %.3f %.3f %.3f\n", bottom_r, bottom_g, bottom_b
            exit 1
        }

        print "PASS: Flycast rendered red, green, and blue particle batches."
        printf "  red region:    %.3f %.3f %.3f\n", left_r, left_g, left_b
        printf "  green region:  %.3f %.3f %.3f\n", center_r, center_g, center_b
        printf "  blue region:   %.3f %.3f %.3f\n", right_r, right_g, right_b
        printf "  above batch:   %.3f %.3f %.3f\n", top_r, top_g, top_b
        printf "  below batch:   %.3f %.3f %.3f\n", bottom_r, bottom_g, bottom_b
    }'
