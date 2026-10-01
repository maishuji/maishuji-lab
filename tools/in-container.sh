#!/usr/bin/env bash
set -euo pipefail

usage() {
    printf "%s\n" \
        "Usage: tools/in-container.sh TARGET [MAKE_VARIABLE=VALUE ...]" \
        "Example: make in-container TARGET=dreamcast-model-loader-cdi" >&2
}

if (($# == 0)); then
    usage
    exit 2
fi

target="$1"
shift

if [[ -z "$target" || ! "$target" =~ ^[[:alnum:]_.-]+$ ]]; then
    printf "%s\n" "Invalid Make target: $target" >&2
    usage
    exit 2
fi

case "$target" in
    flycast-*|host-*|in-container)
        printf "%s\n" "Target $target must run on the host and cannot be dispatched into the toolchain container." >&2
        exit 2
        ;;
esac

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd -- "$script_dir/.." && pwd)"
# shellcheck source=toolchain.lock
source "$script_dir/toolchain.lock"

is_container_environment() {
    [[ -f /.dockerenv || -f /run/.containerenv ||
       -n "${REMOTE_CONTAINERS:-}" ||
       -n "${DEV_CONTAINERS:-}" ||
       -n "${CODESPACES:-}" ]]
}

if is_container_environment; then
    exec make "$target" "$@"
fi

container_runtime="${CONTAINER_RUNTIME:-docker}"
runtime_path="$(command -v "$container_runtime" 2>/dev/null || true)"
if [[ -z "$runtime_path" ]]; then
    printf "%s\n" "Container runtime not found: $container_runtime" >&2
    printf "%s\n" "Install Docker, or set CONTAINER_RUNTIME to an installed compatible runtime." >&2
    exit 1
fi

host_uid="$(id -u)"
host_gid="$(id -g)"
printf "Dispatching Make target %s through %s (%s).\n" "$target" "$container_runtime" "$TOOLCHAIN_IMAGE" >&2

exec "$runtime_path" run \
    --rm \
    --user "$host_uid:$host_gid" \
    --platform "$TOOLCHAIN_IMAGE_PLATFORM" \
    --volume "$repo_root:/workspace" \
    --workdir /workspace \
    "$TOOLCHAIN_IMAGE" \
    make "$target" "$@"
