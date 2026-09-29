#!/usr/bin/env bash
set -euo pipefail

usage() {
    echo "Usage: $0 <flycast-capture.png>" >&2
    exit 2
}

fail() {
    echo "Flycast PVR-text frame check: $*" >&2
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

first_line=$(sample_region '512x48+64+160') ||
    fail "could not sample the first text line."
second_line=$(sample_region '512x48+64+208') ||
    fail "could not sample the second text line."
background_top=$(sample_region '9x9+316+80') ||
    fail "could not sample above the text."
background_bottom=$(sample_region '9x9+316+360') ||
    fail "could not sample below the text."

read -r first_r first_g first_b <<<"$first_line"
read -r second_r second_g second_b <<<"$second_line"
read -r top_r top_g top_b <<<"$background_top"
read -r bottom_r bottom_g bottom_b <<<"$background_bottom"

awk \
    -v first_r="$first_r" -v first_g="$first_g" -v first_b="$first_b" \
    -v second_r="$second_r" -v second_g="$second_g" -v second_b="$second_b" \
    -v top_r="$top_r" -v top_g="$top_g" -v top_b="$top_b" \
    -v bottom_r="$bottom_r" -v bottom_g="$bottom_g" -v bottom_b="$bottom_b" \
    'BEGIN {
        first_ok = first_r + first_g + first_b > 0.12
        second_ok = second_r + second_g + second_b > 0.12
        background_ok = top_r < 0.08 && top_g < 0.08 && top_b < 0.08 &&
                        bottom_r < 0.08 && bottom_g < 0.08 && bottom_b < 0.08

        if(!first_ok || !second_ok || !background_ok) {
            print "FAIL: expected two bright BIOS-font lines on a dark background."
            printf "  first line:  %.3f %.3f %.3f\n", first_r, first_g, first_b
            printf "  second line: %.3f %.3f %.3f\n", second_r, second_g, second_b
            printf "  above text:  %.3f %.3f %.3f\n", top_r, top_g, top_b
            printf "  below text:  %.3f %.3f %.3f\n", bottom_r, bottom_g, bottom_b
            exit 1
        }

        print "PASS: Flycast rendered both BIOS-font lines through a PVR texture."
        printf "  first line:  %.3f %.3f %.3f\n", first_r, first_g, first_b
        printf "  second line: %.3f %.3f %.3f\n", second_r, second_g, second_b
        printf "  above text:  %.3f %.3f %.3f\n", top_r, top_g, top_b
        printf "  below text:  %.3f %.3f %.3f\n", bottom_r, bottom_g, bottom_b
    }'
