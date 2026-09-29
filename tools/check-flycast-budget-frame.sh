#!/usr/bin/env bash
set -euo pipefail

usage() {
    echo "Usage: $0 <flycast-capture.png>" >&2
    exit 2
}

fail() {
    echo "Flycast PVR-budget frame check: $*" >&2
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

opaque_top=$(sample_region 110 210) || fail "could not sample the opaque triangle."
opaque_bottom=$(sample_region 110 365) || fail "could not sample the opaque quad."
punch_top=$(sample_region 320 210) || fail "could not sample the punch-through triangle."
punch_bottom=$(sample_region 320 365) || fail "could not sample the textured quad."
translucent_top=$(sample_region 530 210) || fail "could not sample the translucent triangle."
translucent_bottom=$(sample_region 530 365) || fail "could not sample the translucent quad."
background_top=$(sample_region 320 60) || fail "could not sample above the budget panels."
background_gap=$(sample_region 215 240) || fail "could not sample between the budget panels."
background_bottom=$(sample_region 320 455) || fail "could not sample below the budget panels."

read -r opaque_top_r opaque_top_g opaque_top_b <<<"$opaque_top"
read -r opaque_bottom_r opaque_bottom_g opaque_bottom_b <<<"$opaque_bottom"
read -r punch_top_r punch_top_g punch_top_b <<<"$punch_top"
read -r punch_bottom_r punch_bottom_g punch_bottom_b <<<"$punch_bottom"
read -r translucent_top_r translucent_top_g translucent_top_b <<<"$translucent_top"
read -r translucent_bottom_r translucent_bottom_g translucent_bottom_b <<<"$translucent_bottom"
read -r top_r top_g top_b <<<"$background_top"
read -r gap_r gap_g gap_b <<<"$background_gap"
read -r bottom_r bottom_g bottom_b <<<"$background_bottom"

awk \
    -v opaque_top_r="$opaque_top_r" -v opaque_top_g="$opaque_top_g" -v opaque_top_b="$opaque_top_b" \
    -v opaque_bottom_r="$opaque_bottom_r" -v opaque_bottom_g="$opaque_bottom_g" -v opaque_bottom_b="$opaque_bottom_b" \
    -v punch_top_r="$punch_top_r" -v punch_top_g="$punch_top_g" -v punch_top_b="$punch_top_b" \
    -v punch_bottom_r="$punch_bottom_r" -v punch_bottom_g="$punch_bottom_g" -v punch_bottom_b="$punch_bottom_b" \
    -v translucent_top_r="$translucent_top_r" -v translucent_top_g="$translucent_top_g" -v translucent_top_b="$translucent_top_b" \
    -v translucent_bottom_r="$translucent_bottom_r" -v translucent_bottom_g="$translucent_bottom_g" -v translucent_bottom_b="$translucent_bottom_b" \
    -v top_r="$top_r" -v top_g="$top_g" -v top_b="$top_b" \
    -v gap_r="$gap_r" -v gap_g="$gap_g" -v gap_b="$gap_b" \
    -v bottom_r="$bottom_r" -v bottom_g="$bottom_g" -v bottom_b="$bottom_b" \
    'BEGIN {
        opaque_top_ok = opaque_top_r + opaque_top_g + opaque_top_b > 0.20
        opaque_bottom_ok = opaque_bottom_r + opaque_bottom_g + opaque_bottom_b > 0.20
        punch_top_ok = punch_top_r + punch_top_g + punch_top_b > 0.20
        punch_bottom_ok = punch_bottom_r + punch_bottom_g + punch_bottom_b > 0.10
        translucent_top_ok = translucent_top_r + translucent_top_g + translucent_top_b > 0.12
        translucent_bottom_ok = translucent_bottom_r + translucent_bottom_g + translucent_bottom_b > 0.12
        background_ok = top_r < 0.08 && top_g < 0.08 && top_b < 0.08 &&
                        gap_r < 0.08 && gap_g < 0.08 && gap_b < 0.08 &&
                        bottom_r < 0.08 && bottom_g < 0.08 && bottom_b < 0.08

        if(!opaque_top_ok || !opaque_bottom_ok || !punch_top_ok ||
           !punch_bottom_ok || !translucent_top_ok || !translucent_bottom_ok ||
           !background_ok) {
            print "FAIL: expected all three PVR list panels on a dark background."
            printf "  opaque top:         %.3f %.3f %.3f\n", opaque_top_r, opaque_top_g, opaque_top_b
            printf "  opaque bottom:      %.3f %.3f %.3f\n", opaque_bottom_r, opaque_bottom_g, opaque_bottom_b
            printf "  punch top:          %.3f %.3f %.3f\n", punch_top_r, punch_top_g, punch_top_b
            printf "  punch bottom:       %.3f %.3f %.3f\n", punch_bottom_r, punch_bottom_g, punch_bottom_b
            printf "  translucent top:    %.3f %.3f %.3f\n", translucent_top_r, translucent_top_g, translucent_top_b
            printf "  translucent bottom: %.3f %.3f %.3f\n", translucent_bottom_r, translucent_bottom_g, translucent_bottom_b
            printf "  above panels:       %.3f %.3f %.3f\n", top_r, top_g, top_b
            printf "  panel gap:          %.3f %.3f %.3f\n", gap_r, gap_g, gap_b
            printf "  below panels:       %.3f %.3f %.3f\n", bottom_r, bottom_g, bottom_b
            exit 1
        }

        print "PASS: Flycast rendered opaque, punch-through, and translucent budget panels."
        printf "  opaque top:         %.3f %.3f %.3f\n", opaque_top_r, opaque_top_g, opaque_top_b
        printf "  opaque bottom:      %.3f %.3f %.3f\n", opaque_bottom_r, opaque_bottom_g, opaque_bottom_b
        printf "  punch top:          %.3f %.3f %.3f\n", punch_top_r, punch_top_g, punch_top_b
        printf "  punch bottom:       %.3f %.3f %.3f\n", punch_bottom_r, punch_bottom_g, punch_bottom_b
        printf "  translucent top:    %.3f %.3f %.3f\n", translucent_top_r, translucent_top_g, translucent_top_b
        printf "  translucent bottom: %.3f %.3f %.3f\n", translucent_bottom_r, translucent_bottom_g, translucent_bottom_b
    }'
