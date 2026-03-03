#!/usr/bin/env bash
set -euo pipefail

build_dir="build"
summary_file="docs/dev/test-summary.md"
check_only=false

usage() {
  cat <<'EOF'
Usage: scripts/update-test-summary.sh [--build-dir DIR] [--summary FILE] [--check]

  --build-dir DIR  CTest build directory (default: build)
  --summary FILE   Output markdown path (default: docs/dev/test-summary.md)
  --check          Check mode (exit 1 when summary is outdated)
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --build-dir)
      build_dir="${2:-}"
      shift 2
      ;;
    --summary)
      summary_file="${2:-}"
      shift 2
      ;;
    --check)
      check_only=true
      shift
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "Unknown option: $1" >&2
      usage
      exit 2
      ;;
  esac
done

if [[ ! -d "$build_dir" ]]; then
  echo "Build directory not found: $build_dir" >&2
  exit 1
fi

ctest_output="$(ctest --test-dir "$build_dir" -N)"

mapfile -t tests < <(
  printf '%s\n' "$ctest_output" \
    | sed -nE 's/^[[:space:]]*Test[[:space:]]*#([0-9]+):[[:space:]]*(.+)$/\1|\2/p'
)

count="${#tests[@]}"

tmp_file="$(mktemp)"
cleanup() {
  rm -f "$tmp_file"
}
trap cleanup EXIT

{
  echo "# ShogiBoardQ テストサマリー"
  echo
  echo "> このファイルは \`scripts/update-test-summary.sh\` で生成します。"
  echo
  echo "- CTest ケース数: ${count}"
  echo "- 取得コマンド: \`ctest --test-dir ${build_dir} -N\`"
  echo
  echo "## テスト一覧"
  echo
  if (( count == 0 )); then
    echo "_テストが見つかりませんでした。_"
  else
    for entry in "${tests[@]}"; do
      idx="${entry%%|*}"
      name="${entry#*|}"
      echo "${idx}. \`${name}\`"
    done
  fi
} > "$tmp_file"

if $check_only; then
  if ! diff -u "$summary_file" "$tmp_file" > /tmp/test-summary.diff; then
    echo "Test summary is outdated: $summary_file" >&2
    cat /tmp/test-summary.diff >&2
    exit 1
  fi
  echo "Test summary is up to date: $summary_file"
  exit 0
fi

mkdir -p "$(dirname "$summary_file")"
mv "$tmp_file" "$summary_file"
trap - EXIT

echo "Updated $summary_file (${count} tests)."
