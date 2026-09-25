#!/usr/bin/env bash
set -euo pipefail

usage() {
    echo "Usage: $0 <flycast-capture.png>" >&2
    exit 2
}

fail() {
    echo "Flycast textured-frame check: $*" >&2
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

opaque=$(sample_region 104 240) || fail "could not sample the opaque quad."
punch=$(sample_region 320 240) || fail "could not sample the punch-through quad."
translucent=$(sample_region 536 240) || fail "could not sample the translucent quad."
background_top=$(sample_region 320 48) || fail "could not sample above the quads."
background_left=$(sample_region 8 240) || fail "could not sample left of the quads."
background_right=$(sample_region 632 240) || fail "could not sample right of the quads."
background_bottom=$(sample_region 320 432) || fail "could not sample below the quads."

read -r opaque_r opaque_g opaque_b <<<"$opaque"
read -r punch_r punch_g punch_b <<<"$punch"
read -r translucent_r translucent_g translucent_b <<<"$translucent"
read -r top_r top_g top_b <<<"$background_top"
read -r left_r left_g left_b <<<"$background_left"
read -r right_r right_g right_b <<<"$background_right"
read -r bottom_r bottom_g bottom_b <<<"$background_bottom"

awk \
    -v opaque_r="$opaque_r" -v opaque_g="$opaque_g" -v opaque_b="$opaque_b" \
    -v punch_r="$punch_r" -v punch_g="$punch_g" -v punch_b="$punch_b" \
    -v translucent_r="$translucent_r" -v translucent_g="$translucent_g" -v translucent_b="$translucent_b" \
    -v top_r="$top_r" -v top_g="$top_g" -v top_b="$top_b" \
    -v left_r="$left_r" -v left_g="$left_g" -v left_b="$left_b" \
    -v right_r="$right_r" -v right_g="$right_g" -v right_b="$right_b" \
    -v bottom_r="$bottom_r" -v bottom_g="$bottom_g" -v bottom_b="$bottom_b" \
    'BEGIN {
        opaque_ok = opaque_r + opaque_g + opaque_b > 0.20 &&
                    opaque_b > opaque_r * 1.15
        punch_ok = punch_r + punch_g + punch_b > 0.20 &&
                   punch_b > punch_r * 1.15
        translucent_ok = translucent_r + translucent_g + translucent_b > 0.12 &&
                        translucent_b > translucent_r * 1.10
        background_ok = top_r < 0.08 && top_g < 0.08 && top_b < 0.08 &&
                        left_r < 0.08 && left_g < 0.08 && left_b < 0.08 &&
                        right_r < 0.08 && right_g < 0.08 && right_b < 0.08 &&
                        bottom_r < 0.08 && bottom_g < 0.08 && bottom_b < 0.08

        if(!opaque_ok || !punch_ok || !translucent_ok || !background_ok) {
            print "FAIL: expected three textured quad centers and a dark surrounding background."
            printf "  opaque center:      %.3f %.3f %.3f\n", opaque_r, opaque_g, opaque_b
            printf "  punch center:       %.3f %.3f %.3f\n", punch_r, punch_g, punch_b
            printf "  translucent center: %.3f %.3f %.3f\n", translucent_r, translucent_g, translucent_b
            printf "  above quads:        %.3f %.3f %.3f\n", top_r, top_g, top_b
            printf "  left of quads:      %.3f %.3f %.3f\n", left_r, left_g, left_b
            printf "  right of quads:     %.3f %.3f %.3f\n", right_r, right_g, right_b
            printf "  below quads:        %.3f %.3f %.3f\n", bottom_r, bottom_g, bottom_b
            exit 1
        }

        print "PASS: Flycast rendered opaque, punch-through, and translucent textured quads."
        printf "  opaque center:      %.3f %.3f %.3f\n", opaque_r, opaque_g, opaque_b
        printf "  punch center:       %.3f %.3f %.3f\n", punch_r, punch_g, punch_b
        printf "  translucent center: %.3f %.3f %.3f\n", translucent_r, translucent_g, translucent_b
        printf "  above quads:        %.3f %.3f %.3f\n", top_r, top_g, top_b
        printf "  left of quads:      %.3f %.3f %.3f\n", left_r, left_g, left_b
        printf "  right of quads:     %.3f %.3f %.3f\n", right_r, right_g, right_b
        printf "  below quads:        %.3f %.3f %.3f\n", bottom_r, bottom_g, bottom_b
    }'
