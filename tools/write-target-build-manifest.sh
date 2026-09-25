#!/usr/bin/env bash
set -euo pipefail

if (($# > 2)); then
    echo "Usage: $0 [build-directory] [build-type]" >&2
    exit 2
fi

build_dir=build-dreamcast
build_type=unknown
if (($# >= 1)); then
    build_dir=$1
fi
if (($# >= 2)); then
    build_type=$2
fi

[[ -d "$build_dir" ]] || {
    echo "Build directory not found: $build_dir" >&2
    exit 2
}

elfs=(
    "$build_dir/maishuji-pvr-smoke.elf"
    "$build_dir/maishuji-hello-pvr.elf"
    "$build_dir/maishuji-colored-primitives.elf"
    "$build_dir/maishuji-textured-quad.elf"
)
maps=(
    "$build_dir/maishuji-pvr-smoke.map"
    "$build_dir/maishuji-hello-pvr.map"
    "$build_dir/maishuji-colored-primitives.map"
    "$build_dir/maishuji-textured-quad.map"
)
for elf in "${elfs[@]}"; do
    [[ -f "$elf" ]] || {
        echo "Expected ELF is missing: $elf" >&2
        exit 1
    }
done
for map in "${maps[@]}"; do
    [[ -f "$map" ]] || {
        echo "Expected linker map is missing: $map" >&2
        exit 1
    }
done

source_commit=$(git rev-parse --verify HEAD 2>/dev/null || echo unknown)
working_tree_status=$(git status --short 2>/dev/null || true)
manifest="$build_dir/target-build-manifest.txt"
size_summary="$build_dir/target-size.txt"
lock_copy="$build_dir/target-toolchain.lock"

{
    echo "maishuji-lab target build manifest"
    echo "source_commit: $source_commit"
    echo "build_type: $build_type"
    echo "toolchain_lock: tools/toolchain.lock"
    echo
    echo "verified_toolchain:"
    ./tools/with-kos.sh ./tools/check-toolchain.sh
    echo
    echo "working_tree_status:"
    if [[ -n "$working_tree_status" ]]; then
        printf '%s\n' "$working_tree_status"
    else
        echo clean
    fi
    echo
    echo "elf_sizes:"
    ./tools/with-kos.sh sh-elf-size "${elfs[@]}"
    echo
    echo "linker_maps:"
    for map in "${maps[@]}"; do
        echo "present: $map"
    done
} > "$manifest"

{
    echo "maishuji-lab ELF size summary"
    echo "source_commit: $source_commit"
    echo "build_type: $build_type"
    echo
    ./tools/with-kos.sh sh-elf-size "${elfs[@]}"
} > "$size_summary"

cp -- tools/toolchain.lock "$lock_copy"
