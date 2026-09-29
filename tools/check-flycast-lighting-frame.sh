#!/usr/bin/env bash
set -euo pipefail

usage() {
    echo "Usage: $0 <flycast-capture.png>" >&2
    exit 2
}

fail() {
    echo "Flycast lighting-frame check: $*" >&2
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
    local center_x=$1 center_y=$2
    "${image_convert[@]}" "$image_path" -resize 640x480! \
        -crop "9x9+$((center_x - 4))+$((center_y - 4))" +repage \
        -format '%[fx:mean.r] %[fx:mean.g] %[fx:mean.b]' info:
}

base=$(sample_region 180 240) || fail "could not sample the base quad."
lit=$(sample_region 460 240) || fail "could not sample the lit quad."
lit_top_left=$(sample_region 390 170) || fail "could not sample the red offset corner."
lit_bottom_left=$(sample_region 390 310) || fail "could not sample the green offset corner."
lit_top_right=$(sample_region 530 170) || fail "could not sample the blue offset corner."
lit_bottom_right=$(sample_region 530 310) || fail "could not sample the purple offset corner."
background_top=$(sample_region 316 36) || fail "could not sample above the quads."
background_left=$(sample_region 36 240) || fail "could not sample left of the quads."
background_right=$(sample_region 604 240) || fail "could not sample right of the quads."
background_bottom=$(sample_region 316 436) || fail "could not sample below the quads."

read -r base_r base_g base_b <<<"$base"
read -r lit_r lit_g lit_b <<<"$lit"
read -r red_r red_g red_b <<<"$lit_top_left"
read -r green_r green_g green_b <<<"$lit_bottom_left"
read -r blue_r blue_g blue_b <<<"$lit_top_right"
read -r purple_r purple_g purple_b <<<"$lit_bottom_right"
read -r top_r top_g top_b <<<"$background_top"
read -r left_r left_g left_b <<<"$background_left"
read -r right_r right_g right_b <<<"$background_right"
read -r bottom_r bottom_g bottom_b <<<"$background_bottom"

awk \
    -v base_r="$base_r" -v base_g="$base_g" -v base_b="$base_b" \
    -v lit_r="$lit_r" -v lit_g="$lit_g" -v lit_b="$lit_b" \
    -v red_r="$red_r" -v red_g="$red_g" -v red_b="$red_b" \
    -v green_r="$green_r" -v green_g="$green_g" -v green_b="$green_b" \
    -v blue_r="$blue_r" -v blue_g="$blue_g" -v blue_b="$blue_b" \
    -v purple_r="$purple_r" -v purple_g="$purple_g" -v purple_b="$purple_b" \
    -v top_r="$top_r" -v top_g="$top_g" -v top_b="$top_b" \
    -v left_r="$left_r" -v left_g="$left_g" -v left_b="$left_b" \
    -v right_r="$right_r" -v right_g="$right_g" -v right_b="$right_b" \
    -v bottom_r="$bottom_r" -v bottom_g="$bottom_g" -v bottom_b="$bottom_b" \
    'BEGIN {
        base_ok = base_r + base_g + base_b > 0.25
        lit_ok = lit_r + lit_g + lit_b > base_r + base_g + base_b + 0.10
        red_ok = red_r > red_g * 1.15 && red_r > red_b * 1.25
        green_ok = green_g > green_r * 1.50 && green_g > green_b * 1.15
        blue_ok = blue_b > blue_r * 1.80 && blue_b > blue_g * 1.60
        purple_ok = purple_r > purple_g * 1.25 && purple_b > purple_g * 1.80
        background_ok = top_r < 0.08 && top_g < 0.08 && top_b < 0.08 &&
                        left_r < 0.08 && left_g < 0.08 && left_b < 0.08 &&
                        right_r < 0.08 && right_g < 0.08 && right_b < 0.08 &&
                        bottom_r < 0.08 && bottom_g < 0.08 && bottom_b < 0.08

        if(!base_ok || !lit_ok || !red_ok || !green_ok || !blue_ok ||
           !purple_ok || !background_ok) {
            print "FAIL: expected a base quad and four colored offset regions."
            printf "  base center:       %.3f %.3f %.3f\n", base_r, base_g, base_b
            printf "  lit center:        %.3f %.3f %.3f\n", lit_r, lit_g, lit_b
            printf "  lit top-left:      %.3f %.3f %.3f\n", red_r, red_g, red_b
            printf "  lit bottom-left:   %.3f %.3f %.3f\n", green_r, green_g, green_b
            printf "  lit top-right:     %.3f %.3f %.3f\n", blue_r, blue_g, blue_b
            printf "  lit bottom-right:  %.3f %.3f %.3f\n", purple_r, purple_g, purple_b
            printf "  above quads:       %.3f %.3f %.3f\n", top_r, top_g, top_b
            printf "  left of quads:     %.3f %.3f %.3f\n", left_r, left_g, left_b
            printf "  right of quads:    %.3f %.3f %.3f\n", right_r, right_g, right_b
            printf "  below quads:       %.3f %.3f %.3f\n", bottom_r, bottom_g, bottom_b
            exit 1
        }

        print "PASS: Flycast rendered the base quad and colored PVR offset regions."
        printf "  base center:       %.3f %.3f %.3f\n", base_r, base_g, base_b
        printf "  lit center:        %.3f %.3f %.3f\n", lit_r, lit_g, lit_b
        printf "  lit top-left:      %.3f %.3f %.3f\n", red_r, red_g, red_b
        printf "  lit bottom-left:   %.3f %.3f %.3f\n", green_r, green_g, green_b
        printf "  lit top-right:     %.3f %.3f %.3f\n", blue_r, blue_g, blue_b
        printf "  lit bottom-right:  %.3f %.3f %.3f\n", purple_r, purple_g, purple_b
    }'
