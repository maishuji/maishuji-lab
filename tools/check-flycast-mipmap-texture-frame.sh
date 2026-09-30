#!/usr/bin/env bash
set -euo pipefail

usage() {
    echo "Usage: $0 <flycast-capture.png>" >&2
    exit 2
}

fail() {
    echo "Flycast mipmap-texture frame check: $*" >&2
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

dimensions=$($image_identify -format "%w %h" "$image_path") ||
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
        -format "%[fx:mean.r] %[fx:mean.g] %[fx:mean.b] %[fx:standard_deviation.r] %[fx:standard_deviation.g] %[fx:standard_deviation.b]" info:
}

base=$(sample_region "190x100+95+330") || fail "could not sample the base-level strip."
mipmap=$(sample_region "190x100+355+330") || fail "could not sample the mipmapped strip."
background_top=$(sample_region "9x9+320+36") || fail "could not sample the sky background."
background_left=$(sample_region "9x9+36+100") || fail "could not sample the left sky background."
background_right=$(sample_region "9x9+604+100") || fail "could not sample the right sky background."

read -r base_r base_g base_b base_sd_r base_sd_g base_sd_b <<<"$base"
read -r mip_r mip_g mip_b mip_sd_r mip_sd_g mip_sd_b <<<"$mipmap"
read -r top_r top_g top_b <<<"$background_top"
read -r left_r left_g left_b <<<"$background_left"
read -r right_r right_g right_b <<<"$background_right"

awk \
    -v base_r="$base_r" -v base_g="$base_g" -v base_b="$base_b" \
    -v base_sd_r="$base_sd_r" -v base_sd_g="$base_sd_g" -v base_sd_b="$base_sd_b" \
    -v mip_r="$mip_r" -v mip_g="$mip_g" -v mip_b="$mip_b" \
    -v mip_sd_r="$mip_sd_r" -v mip_sd_g="$mip_sd_g" -v mip_sd_b="$mip_sd_b" \
    -v top_r="$top_r" -v top_g="$top_g" -v top_b="$top_b" \
    -v left_r="$left_r" -v left_g="$left_g" -v left_b="$left_b" \
    -v right_r="$right_r" -v right_g="$right_g" -v right_b="$right_b" \
    'BEGIN {
        base_visible = base_r + base_g + base_b > 0.18
        mip_visible = mip_r + mip_g + mip_b > 0.18
        base_varies = base_sd_r + base_sd_g + base_sd_b > 0.05
        mip_varies = mip_sd_r + mip_sd_g + mip_sd_b > 0.03
        filtering_visible = ((base_sd_r + base_sd_g + base_sd_b) - (mip_sd_r + mip_sd_g + mip_sd_b) > 0.005 || (mip_sd_r + mip_sd_g + mip_sd_b) - (base_sd_r + base_sd_g + base_sd_b) > 0.005 || (base_r + base_g + base_b) - (mip_r + mip_g + mip_b) > 0.005 || (mip_r + mip_g + mip_b) - (base_r + base_g + base_b) > 0.005)
        background_ok = top_b > top_r * 1.25 && top_b > top_g * 1.10 &&
                        left_b > left_r * 1.25 && left_b > left_g * 1.10 &&
                        right_b > right_r * 1.25 && right_b > right_g * 1.10 &&
                        top_r < 0.12 && left_r < 0.12 && right_r < 0.12

        if(!base_visible || !mip_visible || !base_varies || !mip_varies ||
           !filtering_visible || !background_ok) {
            print "FAIL: expected two visible checker strips with different sampling detail."
            printf "  base strip: %.3f %.3f %.3f; variation %.3f %.3f %.3f\n", base_r, base_g, base_b, base_sd_r, base_sd_g, base_sd_b
            printf "  mip strip:  %.3f %.3f %.3f; variation %.3f %.3f %.3f\n", mip_r, mip_g, mip_b, mip_sd_r, mip_sd_g, mip_sd_b
            printf "  sky:        %.3f %.3f %.3f / %.3f %.3f %.3f / %.3f %.3f %.3f\n", top_r, top_g, top_b, left_r, left_g, left_b, right_r, right_g, right_b
            exit 1
        }

        print "PASS: Flycast rendered base-level and mipmapped checker strips."
        printf "  base strip: %.3f %.3f %.3f; variation %.3f %.3f %.3f\n", base_r, base_g, base_b, base_sd_r, base_sd_g, base_sd_b
        printf "  mip strip:  %.3f %.3f %.3f; variation %.3f %.3f %.3f\n", mip_r, mip_g, mip_b, mip_sd_r, mip_sd_g, mip_sd_b
        printf "  sky:        %.3f %.3f %.3f / %.3f %.3f %.3f / %.3f %.3f %.3f\n", top_r, top_g, top_b, left_r, left_g, left_b, right_r, right_g, right_b
    }'
