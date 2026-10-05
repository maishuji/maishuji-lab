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
awk -v w="$width" -v h="$height" 'BEGIN {
    exit !(w >= 320 && h >= 240 && w / h > 1.30 && w / h < 1.36)
}' || { echo "FAIL: unexpected capture dimensions: ${width}x${height}" >&2; exit 1; }

sample() {
    "${image_convert[@]}" "$image_path" -resize 640x480! \
        -crop "9x9+$(( $1 - 4 ))+$(( $2 - 4 ))" +repage \
        -format '%[fx:mean.r] %[fx:mean.g] %[fx:mean.b]' info:
}

inside=$(sample 278 422)
edge=$(sample 425 422)
outside=$(sample 495 422)
camera=$(sample 320 330)
panel=$(sample 100 390)
mesh_inside=$(sample 190 160)
mesh_edge=$(sample 630 160)
heading=$("${image_convert[@]}" "$image_path" -resize 640x480! \
    -crop '430x24+16+8' +repage \
    -format '%[fx:mean.r] %[fx:mean.g] %[fx:mean.b]' info:)
read -r ir ig ib <<<"$inside"
read -r er eg eb <<<"$edge"
read -r out_r out_g out_b <<<"$outside"
read -r cr cg cb <<<"$camera"
read -r pr pg pb <<<"$panel"
read -r mr mg mb <<<"$mesh_inside"
read -r xr xg xb <<<"$mesh_edge"
read -r hr hg hb <<<"$heading"

awk -v ir="$ir" -v ig="$ig" -v ib="$ib" \
    -v er="$er" -v eg="$eg" -v eb="$eb" \
    -v out_r="$out_r" -v out_g="$out_g" -v out_b="$out_b" \
    -v cr="$cr" -v cg="$cg" -v cb="$cb" \
    -v pr="$pr" -v pg="$pg" -v pb="$pb" \
    -v mr="$mr" -v mg="$mg" -v mb="$mb" \
    -v xr="$xr" -v xg="$xg" -v xb="$xb" \
    -v hr="$hr" -v hg="$hg" -v hb="$hb" 'BEGIN {
    inside_ok = ig > ir * 1.6 && ig > ib * 1.25 && ig > 0.25
    edge_ok = er > 0.40 && eg > 0.30 && eb < eg * 0.7
    outside_ok = out_r > out_g * 2.0 && out_r > out_b * 2.0 && out_r > 0.35
    camera_ok = cr > 0.5 && cg > 0.5 && cb > 0.5
    panel_ok = pb > pr * 1.5 && pb > pg * 1.2
    mesh_ok = mr + mg + mb > 0.9 && xr + xg + xb > 0.9
    heading_ok = hr + hg + hb > 0.30
    if(!inside_ok || !edge_ok || !outside_ok || !camera_ok || !panel_ok ||
       !mesh_ok || !heading_ok) {
        print "FAIL: expected heading, two visible meshes, and colored frustum map."
        printf "  inside: %.3f %.3f %.3f; edge: %.3f %.3f %.3f; outside: %.3f %.3f %.3f\n", ir,ig,ib,er,eg,eb,out_r,out_g,out_b
        printf "  camera: %.3f %.3f %.3f; panel: %.3f %.3f %.3f\n", cr,cg,cb,pr,pg,pb
        printf "  meshes: %.3f %.3f %.3f / %.3f %.3f %.3f; heading: %.3f %.3f %.3f\n", mr,mg,mb,xr,xg,xb,hr,hg,hb
        exit 1
    }
    print "PASS: Flycast rendered the labeled meshes and fixed frustum map."
}'
