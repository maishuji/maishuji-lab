#!/usr/bin/env bash
set -euo pipefail

usage() {
    echo "Usage: $0 <flycast-capture.png>" >&2
    exit 2
}

fail() {
    echo "Flycast frame check: $*" >&2
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

dimensions=$("${image_identify[@]}" -format '%w %h' "$image_path") || fail "could not read image dimensions."
read -r width height <<<"$dimensions"
awk -v width="$width" -v height="$height" 'BEGIN {
    if(width < 320 || height < 240 || height == 0 || width / height < 1.30 || width / height > 1.36)
        exit 1
}' || fail "unexpected capture dimensions: ${width:-?}x${height:-?}; expected a 4:3 game window."

# Average a 9x9 interior patch after normalizing to the PVR smoke mode. Region
# means tolerate a few resampling/edge pixels while still checking the actual
# displayed colors and their expected locations.
sample_region() {
    local center_x=$1 center_y=$2
    "${image_convert[@]}" "$image_path" -resize 640x480! \
        -crop "9x9+$((center_x - 4))+$((center_y - 4))" +repage \
        -format '%[fx:mean.r] %[fx:mean.g] %[fx:mean.b]' info:
}

red=$(sample_region 320 120) || fail "could not sample the red triangle region."
green=$(sample_region 160 350) || fail "could not sample the green triangle region."
blue=$(sample_region 480 350) || fail "could not sample the blue triangle region."
center=$(sample_region 320 240) || fail "could not sample the triangle center."
background_top=$(sample_region 320 40) || fail "could not sample above the triangle."
background_left=$(sample_region 60 240) || fail "could not sample left of the triangle."
background_right=$(sample_region 580 240) || fail "could not sample right of the triangle."
background_bottom=$(sample_region 320 430) || fail "could not sample below the triangle."

read -r red_r red_g red_b <<<"$red"
read -r green_r green_g green_b <<<"$green"
read -r blue_r blue_g blue_b <<<"$blue"
read -r center_r center_g center_b <<<"$center"
read -r background_top_r background_top_g background_top_b <<<"$background_top"
read -r background_left_r background_left_g background_left_b <<<"$background_left"
read -r background_right_r background_right_g background_right_b <<<"$background_right"
read -r background_bottom_r background_bottom_g background_bottom_b <<<"$background_bottom"

awk \
    -v red_r="$red_r" -v red_g="$red_g" -v red_b="$red_b" \
    -v green_r="$green_r" -v green_g="$green_g" -v green_b="$green_b" \
    -v blue_r="$blue_r" -v blue_g="$blue_g" -v blue_b="$blue_b" \
    -v center_r="$center_r" -v center_g="$center_g" -v center_b="$center_b" \
    -v top_r="$background_top_r" -v top_g="$background_top_g" -v top_b="$background_top_b" \
    -v left_r="$background_left_r" -v left_g="$background_left_g" -v left_b="$background_left_b" \
    -v right_r="$background_right_r" -v right_g="$background_right_g" -v right_b="$background_right_b" \
    -v bottom_r="$background_bottom_r" -v bottom_g="$background_bottom_g" -v bottom_b="$background_bottom_b" \
    'BEGIN {
        red_ok = red_r > 0.65 && red_r > red_g * 1.4 && red_r > red_b * 1.4
        green_ok = green_g > 0.65 && green_g > green_r * 1.4 && green_g > green_b * 1.4
        blue_ok = blue_b > 0.65 && blue_b > blue_r * 1.2 && blue_b > blue_g * 1.2
        center_ok = center_r > 0.25 && center_g > 0.25 && center_b > 0.25
        background_ok = top_r < 0.08 && top_g < 0.08 && top_b < 0.08 &&
                        left_r < 0.08 && left_g < 0.08 && left_b < 0.08 &&
                        right_r < 0.08 && right_g < 0.08 && right_b < 0.08 &&
                        bottom_r < 0.08 && bottom_g < 0.08 && bottom_b < 0.08

        if(!red_ok || !green_ok || !blue_ok || !center_ok || !background_ok) {
            print "FAIL: expected the colored triangle and dark background at their known positions."
            printf "  red region:        %.3f %.3f %.3f\n", red_r, red_g, red_b
            printf "  green region:      %.3f %.3f %.3f\n", green_r, green_g, green_b
            printf "  blue region:       %.3f %.3f %.3f\n", blue_r, blue_g, blue_b
            printf "  triangle center:   %.3f %.3f %.3f\n", center_r, center_g, center_b
            printf "  above triangle:    %.3f %.3f %.3f\n", top_r, top_g, top_b
            printf "  left of triangle:  %.3f %.3f %.3f\n", left_r, left_g, left_b
            printf "  right of triangle: %.3f %.3f %.3f\n", right_r, right_g, right_b
            printf "  below triangle:    %.3f %.3f %.3f\n", bottom_r, bottom_g, bottom_b
            exit 1
        }

        print "PASS: Flycast rendered the expected red-green-blue Gouraud triangle."
        printf "  red region:        %.3f %.3f %.3f\n", red_r, red_g, red_b
        printf "  green region:      %.3f %.3f %.3f\n", green_r, green_g, green_b
        printf "  blue region:       %.3f %.3f %.3f\n", blue_r, blue_g, blue_b
        printf "  triangle center:   %.3f %.3f %.3f\n", center_r, center_g, center_b
        printf "  above triangle:    %.3f %.3f %.3f\n", top_r, top_g, top_b
        printf "  left of triangle:  %.3f %.3f %.3f\n", left_r, left_g, left_b
        printf "  right of triangle: %.3f %.3f %.3f\n", right_r, right_g, right_b
        printf "  below triangle:    %.3f %.3f %.3f\n", bottom_r, bottom_g, bottom_b
    }'
