#!/usr/bin/env bash
set -euo pipefail

if (($# == 0)); then
    echo "usage: $0 command [argument ...]" >&2
    exit 2
fi

tool_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=toolchain.lock
source "$tool_dir/toolchain.lock"

kos_env="${KOS_ENV:-${KOS_BASE:-/opt/toolchains/dc/kos}/environ.sh}"
if [[ ! -r "$kos_env" ]]; then
    printf 'KOS environment script not found or unreadable: %s\n' "$kos_env" >&2
    echo "Open the project in its pinned Dreamcast dev container or set KOS_ENV." >&2
    exit 1
fi

# Start from a clean KOS environment so this wrapper works from login shells
# which have already sourced KOS as well as from CI's non-login shell.
while IFS= read -r name; do
    case "$name" in
        KOS_*|DC_*) unset "$name" ;;
    esac
done < <(compgen -e)

export KOS_SUBARCH="$TOOLCHAIN_KOS_SUBARCH"
ulimit -c 0 2>/dev/null || true
set +u
source "$kos_env"
set -u

# KOS's environment script selects a floating-point ABI from a compiler probe.
# Reapply the image lock afterward so headers, libraries, compile flags, and
# link flags always use the same ABI even if that probe chooses differently.
KOS_CFLAGS="${KOS_CFLAGS//-m4-single-only/}"
KOS_CFLAGS="${KOS_CFLAGS//-m4-single/}"
KOS_LDFLAGS="${KOS_LDFLAGS//-m4-single-only/}"
KOS_LDFLAGS="${KOS_LDFLAGS//-m4-single/}"
export KOS_SH4_PRECISION="$TOOLCHAIN_SH4_PRECISION"
export KOS_CFLAGS="$KOS_CFLAGS $KOS_SH4_PRECISION"
export KOS_LDFLAGS="$KOS_LDFLAGS $KOS_SH4_PRECISION"

exec "$@"
