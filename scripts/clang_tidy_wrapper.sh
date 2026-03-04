#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 1 ]]; then
    echo "usage: clang_tidy_wrapper.sh <clang-tidy> [args...]" >&2
    exit 2
fi

clang_tidy_bin="$1"
shift

filtered_args=()
in_compile_args=0

for arg in "$@"; do
    if [[ "$arg" == "--" ]]; then
        in_compile_args=1
        filtered_args+=("$arg")
        continue
    fi

    # 一部 GCC 環境で自動付与されるフラグは clang-tidy 側で未対応。
    if [[ $in_compile_args -eq 1 && "$arg" == "-mno-direct-extern-access" ]]; then
        continue
    fi

    filtered_args+=("$arg")
done

exec "$clang_tidy_bin" "${filtered_args[@]}"
