#!/usr/bin/env bash
set -euo pipefail

usage() {
    echo "Usage: $0 <flycast-capture.png>" >&2
    exit 2
}

fail() {
    echo "Flycast controller-PVR frame check: $*" >&2
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

background=$(sample_region 40 40) || fail "could not sample the blue background."
corner_top_left=$(sample_region 196 116) || fail "could not sample the top-left transparent corner."
corner_top_right=$(sample_region 444 116) || fail "could not sample the top-right transparent corner."
corner_bottom_left=$(sample_region 196 364) || fail "could not sample the bottom-left transparent corner."
corner_bottom_right=$(sample_region 444 364) || fail "could not sample the bottom-right transparent corner."
yellow=$(sample_region 368 220) || fail "could not sample the yellow button."
green=$(sample_region 389 204) || fail "could not sample the green button."
blue=$(sample_region 405 225) || fail "could not sample the blue button."
red=$(sample_region 389 243) || fail "could not sample the red button."

read -r background_r background_g background_b <<<"$background"
read -r top_left_r top_left_g top_left_b <<<"$corner_top_left"
read -r top_right_r top_right_g top_right_b <<<"$corner_top_right"
read -r bottom_left_r bottom_left_g bottom_left_b <<<"$corner_bottom_left"
read -r bottom_right_r bottom_right_g bottom_right_b <<<"$corner_bottom_right"
read -r yellow_r yellow_g yellow_b <<<"$yellow"
read -r green_r green_g green_b <<<"$green"
read -r blue_r blue_g blue_b <<<"$blue"
read -r red_r red_g red_b <<<"$red"

awk \
    -v background_r="$background_r" -v background_g="$background_g" -v background_b="$background_b" \
    -v top_left_r="$top_left_r" -v top_left_g="$top_left_g" -v top_left_b="$top_left_b" \
    -v top_right_r="$top_right_r" -v top_right_g="$top_right_g" -v top_right_b="$top_right_b" \
    -v bottom_left_r="$bottom_left_r" -v bottom_left_g="$bottom_left_g" -v bottom_left_b="$bottom_left_b" \
    -v bottom_right_r="$bottom_right_r" -v bottom_right_g="$bottom_right_g" -v bottom_right_b="$bottom_right_b" \
    -v yellow_r="$yellow_r" -v yellow_g="$yellow_g" -v yellow_b="$yellow_b" \
    -v green_r="$green_r" -v green_g="$green_g" -v green_b="$green_b" \
    -v blue_r="$blue_r" -v blue_g="$blue_g" -v blue_b="$blue_b" \
    -v red_r="$red_r" -v red_g="$red_g" -v red_b="$red_b" \
    'function close_to_background(r, g, b) {
        return (r - background_r < 0.08 && background_r - r < 0.08 &&
                g - background_g < 0.08 && background_g - g < 0.08 &&
                b - background_b < 0.08 && background_b - b < 0.08)
    }
    BEGIN {
        background_ok = background_b > background_g * 1.7 &&
                        background_g > background_r * 1.5
        transparent_ok = close_to_background(top_left_r, top_left_g, top_left_b) &&
                         close_to_background(top_right_r, top_right_g, top_right_b) &&
                         close_to_background(bottom_left_r, bottom_left_g, bottom_left_b) &&
                         close_to_background(bottom_right_r, bottom_right_g, bottom_right_b)
        yellow_ok = yellow_r > 0.30 && yellow_g > 0.25 && yellow_b < 0.40
        green_ok = green_g > 0.30 && green_r < 0.70 && green_b < 0.50
        blue_ok = blue_b > 0.30 && blue_g > 0.25 && blue_r < 0.50
        red_ok = red_r > 0.30 && red_g < 0.50 && red_b < 0.50

        if(!background_ok || !transparent_ok || !yellow_ok || !green_ok || !blue_ok || !red_ok) {
            print "FAIL: expected the transparent controller PVR over the blue background."
            printf "  background:       %.3f %.3f %.3f\n", background_r, background_g, background_b
            printf "  transparent corners: %.3f %.3f %.3f | %.3f %.3f %.3f | %.3f %.3f %.3f | %.3f %.3f %.3f\n", top_left_r, top_left_g, top_left_b, top_right_r, top_right_g, top_right_b, bottom_left_r, bottom_left_g, bottom_left_b, bottom_right_r, bottom_right_g, bottom_right_b
            printf "  yellow button:    %.3f %.3f %.3f\n", yellow_r, yellow_g, yellow_b
            printf "  green button:     %.3f %.3f %.3f\n", green_r, green_g, green_b
            printf "  blue button:      %.3f %.3f %.3f\n", blue_r, blue_g, blue_b
            printf "  red button:       %.3f %.3f %.3f\n", red_r, red_g, red_b
            exit 1
        }

        print "PASS: Flycast rendered the transparent controller PVR asset."
        printf "  background:    %.3f %.3f %.3f\n", background_r, background_g, background_b
        printf "  yellow button: %.3f %.3f %.3f\n", yellow_r, yellow_g, yellow_b
        printf "  green button:  %.3f %.3f %.3f\n", green_r, green_g, green_b
        printf "  blue button:   %.3f %.3f %.3f\n", blue_r, blue_g, blue_b
        printf "  red button:    %.3f %.3f %.3f\n", red_r, red_g, red_b
    }'
