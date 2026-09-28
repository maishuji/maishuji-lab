#!/usr/bin/env bash
set -euo pipefail

usage() {
    echo "Usage: $0 <flycast-capture.png>" >&2
    exit 2
}

fail() {
    echo "Flycast fogged-3D frame check: $*" >&2
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

if ! dimensions=$($image_identify -format '%w %h' "$image_path"); then
    fail "could not read image dimensions."
fi
read -r width height <<<"$dimensions"
awk -v width="$width" -v height="$height" 'BEGIN {
    if(width < 320 || height < 240 || height == 0 || width / height < 1.30 || width / height > 1.36)
        exit 1
}' || fail "unexpected capture dimensions: $width x $height; expected a 4:3 game window."

sample_region() {
    local geometry=$1
    $image_convert "$image_path" -resize 640x480! -crop "$geometry" +repage -format '%[fx:mean.r] %[fx:mean.g] %[fx:mean.b]' info:
}

if ! center=$(sample_region '160x160+240+160'); then fail "could not sample the projected mesh."; fi
if ! top=$(sample_region '9x9+316+36'); then fail "could not sample above the mesh."; fi
if ! left=$(sample_region '9x9+36+236'); then fail "could not sample left of the mesh."; fi
if ! right=$(sample_region '9x9+596+236'); then fail "could not sample right of the mesh."; fi
if ! bottom=$(sample_region '9x9+316+436'); then fail "could not sample below the mesh."; fi

read -r center_r center_g center_b <<<"$center"
read -r top_r top_g top_b <<<"$top"
read -r left_r left_g left_b <<<"$left"
read -r right_r right_g right_b <<<"$right"
read -r bottom_r bottom_g bottom_b <<<"$bottom"

awk -v center_r="$center_r" -v center_g="$center_g" -v center_b="$center_b" -v top_r="$top_r" -v top_g="$top_g" -v top_b="$top_b" -v left_r="$left_r" -v left_g="$left_g" -v left_b="$left_b" -v right_r="$right_r" -v right_g="$right_g" -v right_b="$right_b" -v bottom_r="$bottom_r" -v bottom_g="$bottom_g" -v bottom_b="$bottom_b" 'BEGIN {
    center_ok = center_r + center_g + center_b > 0.30 &&
                center_b > center_r * 1.40 &&
                center_b > center_g * 1.15
    background_ok = top_r < 0.08 && top_g < 0.08 && top_b < 0.08 &&
                    left_r < 0.08 && left_g < 0.08 && left_b < 0.08 &&
                    right_r < 0.08 && right_g < 0.08 && right_b < 0.08 &&
                    bottom_r < 0.08 && bottom_g < 0.08 && bottom_b < 0.08

    if(!center_ok || !background_ok) {
        print "FAIL: expected the near blue cube face to occlude the far red face."
        printf "  mesh center: %.3f %.3f %.3f\n", center_r, center_g, center_b
        printf "  above mesh:  %.3f %.3f %.3f\n", top_r, top_g, top_b
        printf "  left mesh:   %.3f %.3f %.3f\n", left_r, left_g, left_b
        printf "  right mesh:  %.3f %.3f %.3f\n", right_r, right_g, right_b
        printf "  below mesh:  %.3f %.3f %.3f\n", bottom_r, bottom_g, bottom_b
        exit 1
    }

    print "PASS: Flycast rendered the fogged 3D mesh with near-face depth ordering."
    printf "  mesh center: %.3f %.3f %.3f\n", center_r, center_g, center_b
    printf "  above mesh:  %.3f %.3f %.3f\n", top_r, top_g, top_b
    printf "  left mesh:   %.3f %.3f %.3f\n", left_r, left_g, left_b
    printf "  right mesh:  %.3f %.3f %.3f\n", right_r, right_g, right_b
    printf "  below mesh:  %.3f %.3f %.3f\n", bottom_r, bottom_g, bottom_b
}'
