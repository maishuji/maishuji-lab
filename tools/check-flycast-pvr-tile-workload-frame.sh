#!/usr/bin/env bash
set -euo pipefail

[[ $# -eq 1 ]] || { echo "Usage: $0 <flycast-capture.png>" >&2; exit 2; }
image_path=$(realpath "$1")
[[ -f "$image_path" ]] || { echo "Image not found: $image_path" >&2; exit 2; }

if command -v magick >/dev/null 2>&1; then
    image_convert=(magick); image_identify=(magick identify)
elif command -v convert >/dev/null 2>&1 && command -v identify >/dev/null 2>&1; then
    image_convert=(convert); image_identify=(identify)
else
    echo "ImageMagick 6 or 7 is required." >&2; exit 2
fi

dimensions=$("${image_identify[@]}" -format '%w %h' "$image_path")
read -r width height <<<"$dimensions"
awk -v w="$width" -v h="$height" 'BEGIN { exit !(w >= 320 && h >= 240 && w / h > 1.30 && w / h < 1.36) }' || {
    echo "FAIL: unexpected capture dimensions: ${width}x${height}" >&2; exit 1;
}

sample() {
    "${image_convert[@]}" "$image_path" -resize 640x480! \
        -crop "11x11+$(( $1 - 5 ))+$(( $2 - 5 ))" +repage \
        -format '%[fx:mean.r] %[fx:mean.g] %[fx:mean.b]' info:
}

background=$(sample 320 40)
friendly=$(sample 160 190)
friendly_gap=$(sample 120 190)
hostile_edge=$(sample 350 130)
hostile_center=$(sample 480 240)
read -r br bg bb <<<"$background"
read -r fr fg fb <<<"$friendly"
read -r gr gg gb <<<"$friendly_gap"
read -r er eg eb <<<"$hostile_edge"
read -r cr cg cb <<<"$hostile_center"

awk \
    -v br="$br" -v bg="$bg" -v bb="$bb" \
    -v fr="$fr" -v fg="$fg" -v fb="$fb" \
    -v gr="$gr" -v gg="$gg" -v gb="$gb" \
    -v er="$er" -v eg="$eg" -v eb="$eb" \
    -v cr="$cr" -v cg="$cg" -v cb="$cb" 'BEGIN {
        background_ok = bb > br * 1.3 && bb > bg * 1.1
        friendly_ok = fg > fr * 1.25 && fg > fb * 1.15 && fg > 0.12
        gap_ok = gg > gr * 1.25 && gg > gb * 1.05
        outer_ok = er > eg * 1.5 && er > eb * 1.7 && er > 0.20
        center_ok = cr > cg * 1.25 && cr > cb * 1.7 && cr > 0.20
        depth_visible = (er - cr > 0.025 || cr - er > 0.025 || eg - cg > 0.025 || cg - eg > 0.025)
        if(!background_ok || !friendly_ok || !gap_ok || !outer_ok ||
           !center_ok || !depth_visible) {
            printf "FAIL: expected green low-coverage and red nested-coverage panels.\n"
            printf "  background: %.3f %.3f %.3f\n", br,bg,bb
            printf "  friendly:   %.3f %.3f %.3f\n", fr,fg,fb
            printf "  gap:        %.3f %.3f %.3f\n", gr,gg,gb
            printf "  outer bad:  %.3f %.3f %.3f\n", er,eg,eb
            printf "  center bad: %.3f %.3f %.3f\n", cr,cg,cb
            exit 1
        }
        print "PASS: Flycast rendered the PVR tile-workload comparison."
        printf "  friendly: %.3f %.3f %.3f; hostile edge: %.3f %.3f %.3f; hostile center: %.3f %.3f %.3f\n", fr,fg,fb,er,eg,eb,cr,cg,cb
    }'
