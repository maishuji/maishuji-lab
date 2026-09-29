#!/usr/bin/env bash
set -euo pipefail

usage() {
    echo "Usage: $0 <flycast-capture.png>" >&2
    exit 2
}

fail() {
    echo "Flycast advanced-text frame check: $*" >&2
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
    local geometry=$1
    "${image_convert[@]}" "$image_path" -resize 640x480! \
        -crop "$geometry" +repage \
        -format '%[fx:mean.r] %[fx:mean.g] %[fx:mean.b] %[fx:standard_deviation.r] %[fx:standard_deviation.g] %[fx:standard_deviation.b]' info:
}

japanese=$(sample_region '448x56+96+68') || fail "could not sample the Japanese line."
traditional=$(sample_region '448x56+96+204') || fail "could not sample the Traditional Chinese line."
english=$(sample_region '448x56+96+340') || fail "could not sample the English line."
background_top=$(sample_region '9x9+316+36') || fail "could not sample above the text."
background_middle=$(sample_region '9x9+316+168') || fail "could not sample between the text lines."
background_bottom=$(sample_region '9x9+316+452') || fail "could not sample below the text."

read -r japanese_r japanese_g japanese_b japanese_sd_r japanese_sd_g japanese_sd_b <<<"$japanese"
read -r traditional_r traditional_g traditional_b traditional_sd_r traditional_sd_g traditional_sd_b <<<"$traditional"
read -r english_r english_g english_b english_sd_r english_sd_g english_sd_b <<<"$english"
read -r top_r top_g top_b <<<"$background_top"
read -r middle_r middle_g middle_b <<<"$background_middle"
read -r bottom_r bottom_g bottom_b <<<"$background_bottom"

awk \
    -v japanese_r="$japanese_r" -v japanese_g="$japanese_g" -v japanese_b="$japanese_b" \
    -v japanese_sd_r="$japanese_sd_r" -v japanese_sd_g="$japanese_sd_g" -v japanese_sd_b="$japanese_sd_b" \
    -v traditional_r="$traditional_r" -v traditional_g="$traditional_g" -v traditional_b="$traditional_b" \
    -v traditional_sd_r="$traditional_sd_r" -v traditional_sd_g="$traditional_sd_g" -v traditional_sd_b="$traditional_sd_b" \
    -v english_r="$english_r" -v english_g="$english_g" -v english_b="$english_b" \
    -v english_sd_r="$english_sd_r" -v english_sd_g="$english_sd_g" -v english_sd_b="$english_sd_b" \
    -v top_r="$top_r" -v top_g="$top_g" -v top_b="$top_b" \
    -v middle_r="$middle_r" -v middle_g="$middle_g" -v middle_b="$middle_b" \
    -v bottom_r="$bottom_r" -v bottom_g="$bottom_g" -v bottom_b="$bottom_b" \
    'function differs(r, g, b, ref_r, ref_g, ref_b) {
        return (r - ref_r > 0.10 || ref_r - r > 0.10 ||
                g - ref_g > 0.10 || ref_g - g > 0.10 ||
                b - ref_b > 0.10 || ref_b - b > 0.10)
    }
    function varies(r, g, b) {
        return r > 0.02 || g > 0.02 || b > 0.02
    }
    BEGIN {
        japanese_ok = varies(japanese_sd_r, japanese_sd_g, japanese_sd_b)
        traditional_ok = varies(traditional_sd_r, traditional_sd_g, traditional_sd_b)
        english_ok = varies(english_sd_r, english_sd_g, english_sd_b)
        background_ok = differs(top_r, top_g, top_b, middle_r, middle_g, middle_b) == 0 &&
                        differs(top_r, top_g, top_b, bottom_r, bottom_g, bottom_b) == 0

        if(!japanese_ok || !traditional_ok || !english_ok || !background_ok) {
            print "FAIL: expected three multilingual text lines on a uniform background."
            printf "  Japanese mean:            %.3f %.3f %.3f; variation: %.3f %.3f %.3f\n", japanese_r, japanese_g, japanese_b, japanese_sd_r, japanese_sd_g, japanese_sd_b
            printf "  Traditional Chinese mean: %.3f %.3f %.3f; variation: %.3f %.3f %.3f\n", traditional_r, traditional_g, traditional_b, traditional_sd_r, traditional_sd_g, traditional_sd_b
            printf "  English mean:             %.3f %.3f %.3f; variation: %.3f %.3f %.3f\n", english_r, english_g, english_b, english_sd_r, english_sd_g, english_sd_b
            printf "  background reference: %.3f %.3f %.3f\n", top_r, top_g, top_b
            printf "  between lines:       %.3f %.3f %.3f\n", middle_r, middle_g, middle_b
            printf "  below text:          %.3f %.3f %.3f\n", bottom_r, bottom_g, bottom_b
            exit 1
        }

        print "PASS: Flycast rendered Japanese, Traditional Chinese, and English text."
        printf "  Japanese mean:            %.3f %.3f %.3f; variation: %.3f %.3f %.3f\n", japanese_r, japanese_g, japanese_b, japanese_sd_r, japanese_sd_g, japanese_sd_b
        printf "  Traditional Chinese mean: %.3f %.3f %.3f; variation: %.3f %.3f %.3f\n", traditional_r, traditional_g, traditional_b, traditional_sd_r, traditional_sd_g, traditional_sd_b
        printf "  English mean:             %.3f %.3f %.3f; variation: %.3f %.3f %.3f\n", english_r, english_g, english_b, english_sd_r, english_sd_g, english_sd_b
    }'
