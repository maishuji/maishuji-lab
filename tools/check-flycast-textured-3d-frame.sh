#!/usr/bin/env bash
set -euo pipefail

usage() {
    echo "Usage: $0 <flycast-capture.png>" >&2
    exit 2
}

fail() {
    echo "Flycast textured-3D frame check: $*" >&2
    exit 1
}

[[ $# -eq 1 ]] || usage
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

dimensions=$($image_identify -format '%w %h' "$image_path") ||
    fail "could not read image dimensions."
read -r width height <<<"$dimensions"
awk -v width="$width" -v height="$height" 'BEGIN {
    if(width < 320 || height < 240 || height == 0 || width / height < 1.30 || width / height > 1.36)
        exit 1
}' || fail "unexpected capture dimensions: $width x $height; expected a 4:3 game window."

sample_region() {
    local geometry=$1
    $image_convert "$image_path" -resize 640x480! \
        -crop "$geometry" +repage \
        -format '%[fx:mean.r] %[fx:mean.g] %[fx:mean.b] %[fx:standard_deviation.r] %[fx:standard_deviation.g] %[fx:standard_deviation.b]' info:
}

cube=$(sample_region '260x260+190+110') ||
    fail "could not sample the rotating textured cube."
center=$(sample_region '9x9+320+240') ||
    fail "could not sample the cube center."
background_top=$(sample_region '9x9+320+36') ||
    fail "could not sample above the cube."
background_left=$(sample_region '9x9+36+240') ||
    fail "could not sample left of the cube."
background_right=$(sample_region '9x9+604+240') ||
    fail "could not sample right of the cube."
background_bottom=$(sample_region '9x9+320+444') ||
    fail "could not sample below the cube."

read -r cube_r cube_g cube_b cube_sd_r cube_sd_g cube_sd_b <<<"$cube"
read -r center_r center_g center_b center_sd_r center_sd_g center_sd_b <<<"$center"
read -r top_r top_g top_b <<<"$background_top"
read -r left_r left_g left_b <<<"$background_left"
read -r right_r right_g right_b <<<"$background_right"
read -r bottom_r bottom_g bottom_b <<<"$background_bottom"

awk \
    -v cube_r="$cube_r" -v cube_g="$cube_g" -v cube_b="$cube_b" \
    -v cube_sd_r="$cube_sd_r" -v cube_sd_g="$cube_sd_g" -v cube_sd_b="$cube_sd_b" \
    -v center_r="$center_r" -v center_g="$center_g" -v center_b="$center_b" \
    -v center_sd_r="$center_sd_r" -v center_sd_g="$center_sd_g" -v center_sd_b="$center_sd_b" \
    -v top_r="$top_r" -v top_g="$top_g" -v top_b="$top_b" \
    -v left_r="$left_r" -v left_g="$left_g" -v left_b="$left_b" \
    -v right_r="$right_r" -v right_g="$right_g" -v right_b="$right_b" \
    -v bottom_r="$bottom_r" -v bottom_g="$bottom_g" -v bottom_b="$bottom_b" \
    'BEGIN {
        cube_visible = cube_r + cube_g + cube_b > 0.35
        cube_varies = cube_sd_r + cube_sd_g + cube_sd_b > 0.12
        center_varies = center_sd_r + center_sd_g + center_sd_b > 0.02
        background_ok = top_b > top_r * 1.35 && top_b > top_g * 1.15 &&
                        left_b > left_r * 1.35 && left_b > left_g * 1.15 &&
                        right_b > right_r * 1.35 && right_b > right_g * 1.15 &&
                        bottom_b > bottom_r * 1.35 && bottom_b > bottom_g * 1.15 &&
                        top_r < 0.12 && left_r < 0.12 &&
                        right_r < 0.12 && bottom_r < 0.12

        if(!cube_visible || !cube_varies || !center_varies || !background_ok) {
            print "FAIL: expected a textured rotating cube on the blue background."
            printf "  cube mean:       %.3f %.3f %.3f; variation: %.3f %.3f %.3f\n", cube_r, cube_g, cube_b, cube_sd_r, cube_sd_g, cube_sd_b
            printf "  cube center:     %.3f %.3f %.3f; variation: %.3f %.3f %.3f\n", center_r, center_g, center_b, center_sd_r, center_sd_g, center_sd_b
            printf "  above cube:      %.3f %.3f %.3f\n", top_r, top_g, top_b
            printf "  left of cube:    %.3f %.3f %.3f\n", left_r, left_g, left_b
            printf "  right of cube:   %.3f %.3f %.3f\n", right_r, right_g, right_b
            printf "  below cube:      %.3f %.3f %.3f\n", bottom_r, bottom_g, bottom_b
            exit 1
        }

        print "PASS: Flycast rendered the textured rotating cube."
        printf "  cube mean:       %.3f %.3f %.3f; variation: %.3f %.3f %.3f\n", cube_r, cube_g, cube_b, cube_sd_r, cube_sd_g, cube_sd_b
        printf "  cube center:     %.3f %.3f %.3f; variation: %.3f %.3f %.3f\n", center_r, center_g, center_b, center_sd_r, center_sd_g, center_sd_b
        printf "  above cube:      %.3f %.3f %.3f\n", top_r, top_g, top_b
        printf "  left of cube:    %.3f %.3f %.3f\n", left_r, left_g, left_b
        printf "  right of cube:   %.3f %.3f %.3f\n", right_r, right_g, right_b
        printf "  below cube:      %.3f %.3f %.3f\n", bottom_r, bottom_g, bottom_b
    }'
