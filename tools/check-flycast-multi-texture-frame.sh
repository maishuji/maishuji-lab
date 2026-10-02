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
    "${image_convert[@]}" "$image_path" -resize 640x480! -crop "11x11+$(( $1 - 5 ))+$(( $2 - 5 ))" +repage \
        -format '%[fx:mean.r] %[fx:mean.g] %[fx:mean.b]' info:
}

background=$(sample 40 40); left=$(sample 160 240); body=$(sample 320 240); right=$(sample 480 240)
read -r br bg bb <<<"$background"; read -r lr lg lb <<<"$left"; read -r cr cg cb <<<"$body"; read -r rr rg rb <<<"$right"
awk -v br="$br" -v bg="$bg" -v bb="$bb" -v lr="$lr" -v lg="$lg" -v lb="$lb" -v cr="$cr" -v cg="$cg" -v cb="$cb" -v rr="$rr" -v rg="$rg" -v rb="$rb" 'BEGIN {
    background_ok = bb > br * 1.3 && bb > bg * 1.1
    solar_ok = lb > lr * 1.15 && lb > lg * 1.05 && lb > 0.08
    body_ok = cr > cb * 1.15 && cg > cb * 1.05 && cr + cg > 0.18
    distinct = (cr - lr > 0.05 || lr - cr > 0.05 || cb - lb > 0.03 || lb - cb > 0.03)
    if(!background_ok || !solar_ok || !body_ok || !distinct) {
        printf "FAIL: expected a metal satellite body and blue solar wings.\n  background %.3f %.3f %.3f\n  solar %.3f %.3f %.3f\n  body %.3f %.3f %.3f\n", br,bg,bb,lr,lg,lb,cr,cg,cb; exit 1
    }
    printf "PASS: Flycast rendered one multi-texture satellite.\n  solar %.3f %.3f %.3f\n  body %.3f %.3f %.3f\n", lr,lg,lb,cr,cg,cb
}'
