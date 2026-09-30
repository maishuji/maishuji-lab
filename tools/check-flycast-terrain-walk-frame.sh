#!/usr/bin/env bash
set -euo pipefail

usage() {
    echo "Usage: $0 <flycast-capture.png>" >&2
    exit 2
}

fail() {
    echo "Flycast terrain-walk frame check: $*" >&2
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

terrain=$(sample_region '540x300+50+150') ||
    fail "could not sample the terrain."
far_terrain=$(sample_region '260x45+190+250') ||
    fail "could not sample the far terrain."
near_terrain=$(sample_region '260x45+190+395') ||
    fail "could not sample the near terrain."
center=$(sample_region '40x40+300+235') ||
    fail "could not sample the player marker."
background_top=$(sample_region '9x9+320+36') ||
    fail "could not sample the sky background."
background_left=$(sample_region '9x9+36+80') ||
    fail "could not sample the left sky background."
background_right=$(sample_region '9x9+604+80') ||
    fail "could not sample the right sky background."

read -r terrain_r terrain_g terrain_b terrain_sd_r terrain_sd_g terrain_sd_b <<<"$terrain"
read -r far_r far_g far_b far_sd_r far_sd_g far_sd_b <<<"$far_terrain"
read -r near_r near_g near_b near_sd_r near_sd_g near_sd_b <<<"$near_terrain"
read -r center_r center_g center_b center_sd_r center_sd_g center_sd_b <<<"$center"
read -r top_r top_g top_b <<<"$background_top"
read -r left_r left_g left_b <<<"$background_left"
read -r right_r right_g right_b <<<"$background_right"

awk \
    -v terrain_r="$terrain_r" -v terrain_g="$terrain_g" -v terrain_b="$terrain_b" \
    -v terrain_sd_r="$terrain_sd_r" -v terrain_sd_g="$terrain_sd_g" -v terrain_sd_b="$terrain_sd_b" \
    -v far_r="$far_r" -v far_g="$far_g" -v far_b="$far_b" \
    -v far_sd_r="$far_sd_r" -v far_sd_g="$far_sd_g" -v far_sd_b="$far_sd_b" \
    -v near_r="$near_r" -v near_g="$near_g" -v near_b="$near_b" \
    -v near_sd_r="$near_sd_r" -v near_sd_g="$near_sd_g" -v near_sd_b="$near_sd_b" \
    -v center_r="$center_r" -v center_g="$center_g" -v center_b="$center_b" \
    -v center_sd_r="$center_sd_r" -v center_sd_g="$center_sd_g" -v center_sd_b="$center_sd_b" \
    -v top_r="$top_r" -v top_g="$top_g" -v top_b="$top_b" \
    -v left_r="$left_r" -v left_g="$left_g" -v left_b="$left_b" \
    -v right_r="$right_r" -v right_g="$right_g" -v right_b="$right_b" \
    'BEGIN {
        terrain_visible = terrain_r + terrain_g + terrain_b > 0.30
        terrain_varies = terrain_sd_r + terrain_sd_g + terrain_sd_b > 0.16
        far_visible = far_r + far_g + far_b > 0.45 &&
                      far_sd_r + far_sd_g + far_sd_b > 0.08
        near_visible = near_r + near_g + near_b > 0.45 &&
                       near_sd_r + near_sd_g + near_sd_b > 0.08
        marker_visible = center_r > 0.38 && center_b > 0.25 &&
                       center_g < 0.28 && center_r > center_g * 1.6 &&
                       center_b > center_g * 1.25
        background_ok = top_b > top_r * 1.35 && top_b > top_g * 1.15 &&
                        left_b > left_r * 1.35 && left_b > left_g * 1.15 &&
                        right_b > right_r * 1.35 && right_b > right_g * 1.15 &&
                        top_r < 0.12 && left_r < 0.12 && right_r < 0.12

        if(!terrain_visible || !terrain_varies || !far_visible ||
           !near_visible || !marker_visible || !background_ok) {
            print "FAIL: expected a tiled terrain with a visible player marker."
            printf "  terrain mean: %.3f %.3f %.3f; variation: %.3f %.3f %.3f\n", terrain_r, terrain_g, terrain_b, terrain_sd_r, terrain_sd_g, terrain_sd_b
            printf "  far terrain:  %.3f %.3f %.3f; variation: %.3f %.3f %.3f\n", far_r, far_g, far_b, far_sd_r, far_sd_g, far_sd_b
            printf "  near terrain: %.3f %.3f %.3f; variation: %.3f %.3f %.3f\n", near_r, near_g, near_b, near_sd_r, near_sd_g, near_sd_b
            printf "  marker:       %.3f %.3f %.3f; variation: %.3f %.3f %.3f\n", center_r, center_g, center_b, center_sd_r, center_sd_g, center_sd_b
            printf "  sky top:      %.3f %.3f %.3f\n", top_r, top_g, top_b
            printf "  sky left:     %.3f %.3f %.3f\n", left_r, left_g, left_b
            printf "  sky right:    %.3f %.3f %.3f\n", right_r, right_g, right_b
            exit 1
        }

        print "PASS: Flycast rendered the terrain-walk scene."
        printf "  terrain mean: %.3f %.3f %.3f; variation: %.3f %.3f %.3f\n", terrain_r, terrain_g, terrain_b, terrain_sd_r, terrain_sd_g, terrain_sd_b
        printf "  far terrain:  %.3f %.3f %.3f; variation: %.3f %.3f %.3f\n", far_r, far_g, far_b, far_sd_r, far_sd_g, far_sd_b
        printf "  near terrain: %.3f %.3f %.3f; variation: %.3f %.3f %.3f\n", near_r, near_g, near_b, near_sd_r, near_sd_g, near_sd_b
        printf "  marker:       %.3f %.3f %.3f; variation: %.3f %.3f %.3f\n", center_r, center_g, center_b, center_sd_r, center_sd_g, center_sd_b
        printf "  sky top:      %.3f %.3f %.3f\n", top_r, top_g, top_b
        printf "  sky left:     %.3f %.3f %.3f\n", left_r, left_g, left_b
        printf "  sky right:    %.3f %.3f %.3f\n", right_r, right_g, right_b
    }'
