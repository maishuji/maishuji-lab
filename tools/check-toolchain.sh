#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=toolchain.lock
source "$script_dir/toolchain.lock"

fail() {
    printf 'Dreamcast toolchain check failed: %s\n' "$1" >&2
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

require_equal 'KOS version' "$TOOLCHAIN_KOS_VERSION" "${KOS_VERSION:-unset}"
require_equal 'KOS sub-architecture' "$TOOLCHAIN_KOS_SUBARCH" "${KOS_SUBARCH:-unset}"
require_equal 'SH-4 floating-point ABI' "$TOOLCHAIN_SH4_PRECISION" "${KOS_SH4_PRECISION:-unset}"
[[ -f "$KOS_BASE/include/kos/version.h" ]] || fail "KOS headers are missing from $KOS_BASE"
[[ -f "$KOS_BASE/lib/dreamcast/libkallisti.a" ]] || fail "KOS library is missing from $KOS_BASE"
[[ "${KOS_CFLAGS:-}" == *"$TOOLCHAIN_SH4_PRECISION"* ]] || fail "KOS compile flags do not contain $TOOLCHAIN_SH4_PRECISION"
[[ "${KOS_LDFLAGS:-}" == *"$TOOLCHAIN_SH4_PRECISION"* ]] || fail "KOS link flags do not contain $TOOLCHAIN_SH4_PRECISION"

compiler_version="$("$KOS_CCPLUS" --version | sed -n '1p')"
[[ "$compiler_version" == *"$TOOLCHAIN_GCC_VERSION"* ]] ||
    fail "KOS C++ compiler is '$compiler_version'; expected GCC $TOOLCHAIN_GCC_VERSION"

cmake_version="$(cmake --version | sed -n '1s/.*version //p')"
require_equal 'CMake version' "$TOOLCHAIN_CMAKE_VERSION" "$cmake_version"

make_version="$(make --version | sed -n '1s/GNU Make //p')"
require_equal 'GNU Make version' "$TOOLCHAIN_MAKE_VERSION" "$make_version"

printf 'Dreamcast toolchain matches tools/toolchain.lock\n'
printf '  image: %s (%s)\n' "$TOOLCHAIN_IMAGE" "$TOOLCHAIN_IMAGE_PLATFORM"
printf '  KOS: %s (source commit locked by the image digest)\n' "$TOOLCHAIN_KOS_VERSION"
printf '  compiler: %s\n' "$compiler_version"
printf '  ABI: %s\n' "$KOS_SH4_PRECISION"
printf '  CMake: %s; GNU Make: %s\n' "$cmake_version" "$make_version"
printf '  optional packaging tool: %s\n' "$(command -v mkdcdisc || echo unavailable)"
printf '  optional hardware loader: %s\n' "$(command -v dc-tool-ip || echo unavailable)"
