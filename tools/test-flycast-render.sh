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

for utility in flatpak xwininfo xdotool awk grep tail mktemp realpath sleep timeout; do
    command -v "$utility" >/dev/null 2>&1 || {
        echo "Required command not found: $utility" >&2
        exit 2
    }
done

if command -v magick >/dev/null 2>&1; then
    image_import=(magick import)
elif command -v import >/dev/null 2>&1; then
    image_import=(import)
else
    echo "ImageMagick 6 or 7 is required (import or magick)." >&2
    exit 2
fi

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)
frame_checker="$script_dir/check-flycast-frame.sh"
[[ -x "$frame_checker" ]] || {
    echo "Frame checker is missing or not executable: $frame_checker" >&2
    exit 2
}

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
if ! running_apps=$(flatpak ps --columns=application 2>/dev/null); then
    echo "Could not check for an existing Flycast instance." >&2
    exit 2
fi
if grep -Fxq "$app_id" <<<"$running_apps"; then
    echo "Close the existing Flycast instance before running this test." >&2
    exit 2
fi

flatpak_run_help=$(flatpak run --help 2>&1) || {
    echo "Could not inspect Flatpak run options." >&2
    exit 2
}
grep -Fq -- '--instance-id-fd' <<<"$flatpak_run_help" || {
    echo "The installed Flatpak is too old to track and clean up the test instance safely." >&2
    exit 2
}

capture_path=${2:-"${cdi_path%.cdi}-flycast.png"}
mkdir -p "$(dirname "$capture_path")"
capture_path=$(realpath -m "$capture_path")
[[ "$capture_path" != "$cdi_path" ]] || {
    echo "Capture path must not overwrite the CDI: $capture_path" >&2
    exit 2
}

start_timeout=${FLYCAST_START_TIMEOUT:-90}
render_timeout=${FLYCAST_RENDER_TIMEOUT:-30}
stable_samples=${FLYCAST_STABLE_SAMPLES:-3}
capture_timeout=${FLYCAST_CAPTURE_TIMEOUT:-5}
require_runtime_markers=${FLYCAST_REQUIRE_RUNTIME_MARKERS:-0}
for timeout_value in "$start_timeout" "$render_timeout"; do
    [[ "$timeout_value" =~ ^[1-9][0-9]*$ ]] || {
        echo "Flycast timeouts must be positive whole seconds." >&2
        exit 2
    }
done
[[ "$stable_samples" =~ ^[1-9][0-9]*$ ]] || {
    echo "FLYCAST_STABLE_SAMPLES must be a positive whole number." >&2
    exit 2
}
[[ "$capture_timeout" =~ ^[1-9][0-9]*$ ]] || {
    echo "FLYCAST_CAPTURE_TIMEOUT must be a positive whole number." >&2
    exit 2
}
[[ "$require_runtime_markers" == 0 || "$require_runtime_markers" == 1 ]] || {
    echo "FLYCAST_REQUIRE_RUNTIME_MARKERS must be 0 or 1." >&2
    exit 2
}

work_dir=$(mktemp -d "${TMPDIR:-/tmp}/maishuji-flycast.XXXXXX")
mkdir -p "$work_dir/config" "$work_dir/data" "$work_dir/cache"
log_file="$work_dir/flycast.log"
instance_id_file="$work_dir/instance-id"
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

    # Use the ID returned by this invocation of `flatpak run`; killing by app
    # ID could stop a different Flycast instance opened while this test ran.
    if [[ -s "$instance_id_file" ]]; then
        instance_id=$(<"$instance_id_file")
        if [[ -n "$instance_id" ]]; then
            flatpak kill "$instance_id" >/dev/null 2>&1 || true
        fi
    fi
    if [[ -n "$launcher_pid" ]] && kill -0 "$launcher_pid" 2>/dev/null; then
        wait "$launcher_pid" 2>/dev/null || true
    fi
    if [[ -n "${instance_id:-}" ]]; then
        shutdown_deadline=$((SECONDS + 10))
        while (( SECONDS < shutdown_deadline )); do
            running_instances=$(flatpak ps --columns=instance 2>/dev/null || true)
            if ! grep -Fxq "$instance_id" <<<"$running_instances"; then
                break
            fi
            sleep 1
        done
        if grep -Fxq "$instance_id" <<<"$running_instances"; then
            echo "Flycast render test: test instance $instance_id is still running after cleanup." >&2
            (( result != 0 )) || result=1
        fi
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
    --instance-id-fd=3 \
    "$app_id" \
    -config "window:title=$window_title" \
    "$cdi_path" 3>"$instance_id_file" >"$log_file" 2>&1 &
launcher_pid=$!

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
[[ -s "$instance_id_file" ]] || die "Flatpak did not report the test instance ID needed for safe cleanup."

xdotool windowactivate --sync "$window_id" >/dev/null 2>&1 || true
render_deadline=$((SECONDS + render_timeout))
last_check_output=
stable_count=0

runtime_probes_passed() {
    grep -q 'maishuji-lab: runtime probes passed' "$log_file" &&
        grep -q 'maishuji-lab: normal shutdown passed' "$log_file" &&
        grep -q 'maishuji-lab: global destructor passed' "$log_file"
}

runtime_gate_passed() {
    [[ "$require_runtime_markers" == 0 ]] || runtime_probes_passed
}

complete_success() {
    printf '%s\n' "$last_check_output"
    echo "Stable frames: $stable_count"
    if [[ "$require_runtime_markers" == 1 ]]; then
        echo "Runtime markers: passed"
    else
        echo "Runtime probes: render-gated"
    fi
    echo "Capture: $capture_path"
    exit 0
}

while (( SECONDS < render_deadline )); do
    if (( stable_count >= stable_samples )) && runtime_gate_passed; then
        complete_success
    fi

    if ! timeout "$capture_timeout" "${image_import[@]}" -window "$window_id" "$capture_path"; then
        if (( stable_count >= stable_samples )) && runtime_gate_passed; then
            complete_success
        fi
        die "Could not capture the Flycast window."
    fi
    if last_check_output=$("$frame_checker" "$capture_path" 2>&1); then
        ((stable_count += 1))
    else
        stable_count=0
    fi
    sleep 1
done
echo "$last_check_output" >&2
die "Timed out waiting for stable rendering and completed runtime probes; the last capture is at $capture_path."
