#!/usr/bin/env bash
set -euo pipefail

usage() {
    echo "Usage: $0 <flycast-capture.png>" >&2
    exit 2
}

fail() {
    echo "Flycast pixel-sprite frame check: $*" >&2
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
}' || fail "unexpected capture dimensions: ${width:-?}x${height:-?}; expected a 4:3 game window."

# Normalize to the Dreamcast output size. The broad regions tolerate the
# sprites moving during capture while keeping their expected left/right roles.
sample_region() {
    local geometry=$1
    "${image_convert[@]}" "$image_path" -resize 640x480! \
        -crop "$geometry" +repage \
        -format '%[fx:mean.r] %[fx:mean.g] %[fx:mean.b]' info:
}

left_sprite=$(sample_region '140x100+50+140') || \
    fail "could not sample the snapped sprite region."
right_sprite=$(sample_region '140x100+335+140') || \
    fail "could not sample the subpixel sprite region."
background_top=$(sample_region '9x9+316+36') || \
    fail "could not sample above the sprites."
background_bottom=$(sample_region '9x9+316+430') || \
    fail "could not sample below the sprites."

read -r left_r left_g left_b <<<"$left_sprite"
read -r right_r right_g right_b <<<"$right_sprite"
read -r top_r top_g top_b <<<"$background_top"
read -r bottom_r bottom_g bottom_b <<<"$background_bottom"

awk \
    -v left_r="$left_r" -v left_g="$left_g" -v left_b="$left_b" \
    -v right_r="$right_r" -v right_g="$right_g" -v right_b="$right_b" \
    -v top_r="$top_r" -v top_g="$top_g" -v top_b="$top_b" \
    -v bottom_r="$bottom_r" -v bottom_g="$bottom_g" -v bottom_b="$bottom_b" \
    'BEGIN {
        left_ok = left_r > 0.10 && left_r > left_g * 1.25 && left_r > left_b * 1.15
        right_ok = right_g > 0.10 && right_b > 0.10 &&
                   right_g > right_r * 1.25 && right_b > right_r * 1.25
        background_ok = top_r < 0.08 && top_g < 0.08 && top_b < 0.08 &&
                        bottom_r < 0.08 && bottom_g < 0.08 && bottom_b < 0.08

        if(!left_ok || !right_ok || !background_ok) {
            print "FAIL: expected red and green-blue textured sprites on a dark background."
            printf "  left sprite region:  %.3f %.3f %.3f\n", left_r, left_g, left_b
            printf "  right sprite region: %.3f %.3f %.3f\n", right_r, right_g, right_b
            printf "  above sprites:       %.3f %.3f %.3f\n", top_r, top_g, top_b
            printf "  below sprites:       %.3f %.3f %.3f\n", bottom_r, bottom_g, bottom_b
            exit 1
        }

        print "PASS: Flycast rendered both atlas sprite regions."
        printf "  left sprite region:  %.3f %.3f %.3f\n", left_r, left_g, left_b
        printf "  right sprite region: %.3f %.3f %.3f\n", right_r, right_g, right_b
        printf "  above sprites:       %.3f %.3f %.3f\n", top_r, top_g, top_b
        printf "  below sprites:       %.3f %.3f %.3f\n", bottom_r, bottom_g, bottom_b
    }'
