#!/usr/bin/env bash
set -euo pipefail

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

warn_if_host_environment() {
    if is_container_environment; then
        return
    fi

    printf "Hint: no Docker or Dev Container marker was detected; this check is using host KOS at %s.\n" "${KOS_BASE:-unset}" >&2
    printf "Hint: reopen this project in its pinned Dev Container, or intentionally align the local toolchain with tools/toolchain.lock.\n" >&2
}

fail() {
    printf 'Dreamcast toolchain check failed: %s\n' "$1" >&2
    warn_if_host_environment
    exit 1
}

require_equal() {
    local name="$1" expected="$2" actual="$3"
    [[ "$actual" == "$expected" ]] ||
        fail "$name is '$actual'; expected '$expected' from tools/toolchain.lock"
}

[[ -n "${KOS_BASE:-}" && -d "$KOS_BASE" ]] || fail "KOS_BASE is unset or missing; run through tools/with-kos.sh"
[[ -n "${KOS_CCPLUS:-}" && -x "$KOS_CCPLUS" ]] || fail "KOS C++ compiler is missing: ${KOS_CCPLUS:-unset}"
command -v cmake >/dev/null || fail 'cmake is not on PATH'
command -v make >/dev/null || fail 'make is not on PATH'
command -v kos-c++ >/dev/null || fail 'kos-c++ is not on PATH'

grep -Fq "\"image\": \"$TOOLCHAIN_IMAGE\"" "$repo_root/.devcontainer/devcontainer.json" ||
    fail 'devcontainer image digest does not match tools/toolchain.lock'
grep -Fq "image: $TOOLCHAIN_IMAGE" "$repo_root/.github/workflows/build.yml" ||
    fail 'CI image digest does not match tools/toolchain.lock'

require_equal 'KOS version' "$TOOLCHAIN_KOS_VERSION" "${KOS_VERSION:-unset}"
require_equal 'KOS sub-architecture' "$TOOLCHAIN_KOS_SUBARCH" "${KOS_SUBARCH:-unset}"
require_equal 'SH-4 floating-point ABI' "$TOOLCHAIN_SH4_PRECISION" "${KOS_SH4_PRECISION:-unset}"
[[ -f "$KOS_BASE/include/kos/version.h" ]] || fail "KOS headers are missing from $KOS_BASE"
[[ -f "$KOS_BASE/lib/dreamcast/libkallisti.a" ]] || fail "KOS library is missing from $KOS_BASE"
[[ "${KOS_CFLAGS:-}" == *"$TOOLCHAIN_SH4_PRECISION"* ]] || fail "KOS compile flags do not contain $TOOLCHAIN_SH4_PRECISION"
[[ "${KOS_LDFLAGS:-}" == *"$TOOLCHAIN_SH4_PRECISION"* ]] || fail "KOS link flags do not contain $TOOLCHAIN_SH4_PRECISION"
case "$TOOLCHAIN_SH4_PRECISION" in
    -m4-single-only) conflicting_sh4_precision="-m4-single" ;;
    -m4-single) conflicting_sh4_precision="-m4-single-only" ;;
    *) fail "unsupported SH-4 floating-point ABI in lock: $TOOLCHAIN_SH4_PRECISION" ;;
esac
[[ " ${KOS_CFLAGS:-} " != *" $conflicting_sh4_precision "* ]] ||
    fail "KOS compile flags contain conflicting $conflicting_sh4_precision ABI"
[[ " ${KOS_LDFLAGS:-} " != *" $conflicting_sh4_precision "* ]] ||
    fail "KOS link flags contain conflicting $conflicting_sh4_precision ABI"

compiler_version="$("$KOS_CCPLUS" --version | sed -n '1p')"
[[ "$compiler_version" == *"$TOOLCHAIN_GCC_VERSION"* ]] ||
    fail "KOS C++ compiler is '$compiler_version'; expected GCC $TOOLCHAIN_GCC_VERSION"

binutils_version="$("$KOS_LD" --version | sed -n '1s/GNU ld (GNU Binutils) //p')"
require_equal 'GNU Binutils version' "$TOOLCHAIN_BINUTILS_VERSION" "$binutils_version"

newlib_version_file="$KOS_CC_BASE/sh-elf/include/_newlib_version.h"
[[ -r "$newlib_version_file" ]] || fail "Newlib version header is missing: $newlib_version_file"
newlib_version="$(sed -n 's/^#define _NEWLIB_VERSION "\(.*\)"/\1/p' "$newlib_version_file")"
require_equal 'Newlib version' "$TOOLCHAIN_NEWLIB_VERSION" "$newlib_version"

cmake_version="$(cmake --version | sed -n '1s/.*version //p')"
require_equal 'CMake version' "$TOOLCHAIN_CMAKE_VERSION" "$cmake_version"

make_version="$(make --version | sed -n '1s/GNU Make //p')"
require_equal 'GNU Make version' "$TOOLCHAIN_MAKE_VERSION" "$make_version"

printf 'Dreamcast toolchain matches tools/toolchain.lock\n'
printf '  image: %s (%s)\n' "$TOOLCHAIN_IMAGE" "$TOOLCHAIN_IMAGE_PLATFORM"
printf '  devcontainer and CI use the locked image\n'
printf '  KOS: %s (snapshot %s, source %s)\n' "$TOOLCHAIN_KOS_VERSION" "$TOOLCHAIN_KOS_SNAPSHOT" "$TOOLCHAIN_KOS_COMMIT"
printf '  compiler: %s\n' "$compiler_version"
printf '  Binutils: %s; Newlib: %s\n' "$binutils_version" "$newlib_version"
printf '  ABI: %s\n' "$KOS_SH4_PRECISION"
printf '  CMake: %s; GNU Make: %s\n' "$cmake_version" "$make_version"
printf '  optional packaging tool: %s\n' "$(command -v mkdcdisc || echo unavailable)"
printf '  optional hardware loader: %s\n' "$(command -v dc-tool-ip || echo unavailable)"
