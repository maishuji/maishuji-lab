#!/usr/bin/env bash
set -euo pipefail

usage() {
    echo "Usage: $0 <smoke.cdi> [capture.png]" >&2
    exit 2
}

die() {
    echo "Flycast render test: $*" >&2
    if [[ -f "${log_file:-}" ]]; then
        tail -n 40 "$log_file" >&2
    fi
    exit 1
}

[[ $# -ge 1 && $# -le 2 ]] || usage
cdi_path=$(realpath "$1")
[[ -f "$cdi_path" ]] || { echo "CDI not found: $cdi_path" >&2; exit 2; }

for utility in flatpak xwininfo xdotool import identify convert awk grep tail mktemp realpath sleep; do
    command -v "$utility" >/dev/null 2>&1 || {
        echo "Required command not found: $utility" >&2
        exit 2
    }
done

[[ -n "${DISPLAY:-}" ]] || {
    echo "An X11 or XWayland display is required (DISPLAY is empty)." >&2
    exit 2
}
xwininfo -root -tree >/dev/null 2>&1 || {
    echo "Cannot access the X11 window tree on DISPLAY=$DISPLAY." >&2
    exit 2
}

app_id=org.flycast.Flycast
flatpak info "$app_id" >/dev/null 2>&1 || {
    echo "Install the Flycast Flatpak ($app_id) before running this test." >&2
    exit 2
}
running_apps=$(flatpak ps --columns=application 2>/dev/null || true)
if grep -Fxq "$app_id" <<<"$running_apps"; then
    echo "Close the existing Flycast instance before running this test." >&2
    exit 2
fi

capture_path=${2:-"${cdi_path%.cdi}-flycast.png"}
mkdir -p "$(dirname "$capture_path")"
capture_path=$(realpath -m "$capture_path")

work_dir=$(mktemp -d "${TMPDIR:-/tmp}/maishuji-flycast.XXXXXX")
mkdir -p "$work_dir/config" "$work_dir/data" "$work_dir/cache"
log_file="$work_dir/flycast.log"
window_title="MAISHUJI_PVR_SMOKE_${BASHPID}"
window_id=
launcher_pid=
previous_active_window=$(xdotool getactivewindow 2>/dev/null || true)

cleanup() {
    result=$?
    trap - EXIT INT TERM
    if [[ -n "$window_id" ]]; then
        xdotool windowclose "$window_id" >/dev/null 2>&1 || true
    fi
    if [[ -n "$launcher_pid" ]] && kill -0 "$launcher_pid" 2>/dev/null; then
        kill "$launcher_pid" 2>/dev/null || true
        wait "$launcher_pid" 2>/dev/null || true
    fi
    if [[ -n "$launcher_pid" ]]; then
        flatpak kill "$app_id" >/dev/null 2>&1 || true
    fi
    if [[ -n "$previous_active_window" ]]; then
        xdotool windowactivate --sync "$previous_active_window" >/dev/null 2>&1 || true
    fi
    rm -rf -- "$work_dir"
    exit "$result"
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

cdi_dir=$(dirname "$cdi_path")
flatpak run \
    --filesystem="$cdi_dir:ro" \
    --filesystem="$work_dir:rw" \
    --env="XDG_CONFIG_HOME=$work_dir/config" \
    --env="XDG_DATA_HOME=$work_dir/data" \
    --env="XDG_CACHE_HOME=$work_dir/cache" \
    "$app_id" \
    -config "window:title=$window_title" \
    "$cdi_path" >"$log_file" 2>&1 &
launcher_pid=$!

start_timeout=${FLYCAST_START_TIMEOUT:-90}
deadline=$((SECONDS + start_timeout))
while (( SECONDS < deadline )); do
    window_tree=$(xwininfo -root -tree 2>/dev/null || true)
    window_id=$(awk -v title="$window_title" 'index($0, title) && !found { print $1; found = 1 }' <<<"$window_tree")
    if [[ -n "$window_id" ]] && grep -q 'REIOS: Booting up' "$log_file"; then
        break
    fi
    if ! kill -0 "$launcher_pid" 2>/dev/null; then
        die "Flycast exited before the smoke CDI booted."
    fi
    sleep 1
done
[[ -n "$window_id" ]] || die "Timed out waiting for the Flycast game window."
grep -q 'REIOS: Booting up' "$log_file" || die "Timed out waiting for the REIOS boot marker."

xdotool windowactivate --sync "$window_id" >/dev/null 2>&1 || true
sleep 3
import -window "$window_id" "$capture_path" || die "Could not capture the Flycast window."

dimensions=$(identify -format '%w %h' "$capture_path")
read -r width height <<<"$dimensions"
awk -v width="$width" -v height="$height" 'BEGIN {
    ratio = width / height
    if(width < 320 || height < 240 || ratio < 1.30 || ratio > 1.36)
        exit 1
}' || die "Unexpected captured window dimensions: ${width}x${height}."

samples=$(convert "$capture_path" -resize 640x480! -format \
    '%[fx:p{320,120}.r] %[fx:p{320,120}.g] %[fx:p{320,120}.b] %[fx:p{160,350}.r] %[fx:p{160,350}.g] %[fx:p{160,350}.b] %[fx:p{480,350}.r] %[fx:p{480,350}.g] %[fx:p{480,350}.b] %[fx:p{320,240}.r] %[fx:p{320,240}.g] %[fx:p{320,240}.b] %[fx:p{32,32}.r] %[fx:p{32,32}.g] %[fx:p{32,32}.b]' \
    info:)
awk -v samples="$samples" -v capture="$capture_path" 'BEGIN {
    n = split(samples, p, /[[:space:]]+/)
    if(n != 15) {
        print "FAIL: could not read expected pixel samples: " samples
        exit 1
    }

    red = p[1] > 0.65 && p[1] > p[2] * 1.4 && p[1] > p[3] * 1.4
    green = p[5] > 0.65 && p[5] > p[4] * 1.4 && p[5] > p[6] * 1.4
    blue = p[9] > 0.65 && p[9] > p[7] * 1.4 && p[9] > p[8] * 1.2
    center = p[10] > 0.25 && p[11] > 0.25 && p[12] > 0.25
    background = p[13] < 0.08 && p[14] < 0.08 && p[15] < 0.08

    if(!red || !green || !blue || !center || !background) {
        print "FAIL: triangle pixel assertions failed."
        printf "  red sample:   %.3f %.3f %.3f\n", p[1], p[2], p[3]
        printf "  green sample: %.3f %.3f %.3f\n", p[4], p[5], p[6]
        printf "  blue sample:  %.3f %.3f %.3f\n", p[7], p[8], p[9]
        printf "  center:       %.3f %.3f %.3f\n", p[10], p[11], p[12]
        printf "  background:   %.3f %.3f %.3f\n", p[13], p[14], p[15]
        exit 1
    }

    print "PASS: Flycast rendered the expected red-green-blue Gouraud triangle."
    print "Capture: " capture
    printf "  red sample:   %.3f %.3f %.3f\n", p[1], p[2], p[3]
    printf "  green sample: %.3f %.3f %.3f\n", p[4], p[5], p[6]
    printf "  blue sample:  %.3f %.3f %.3f\n", p[7], p[8], p[9]
    printf "  center:       %.3f %.3f %.3f\n", p[10], p[11], p[12]
    printf "  background:   %.3f %.3f %.3f\n", p[13], p[14], p[15]
}'
