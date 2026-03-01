#!/bin/bash
# 主要ファイルの include 数を監視
# 閾値超過時に警告を出力

set -euo pipefail

THRESHOLD_FILE="resources/include-baseline.txt"
FAILED=false

while IFS=: read -r file limit; do
    actual=$(grep -c '#include' "$file" 2>/dev/null || echo 0)
    if [ "$actual" -gt "$limit" ]; then
        echo "::error::Include count regression in $file: $actual > $limit"
        FAILED=true
    fi
done < "$THRESHOLD_FILE"

if [ "$FAILED" = true ]; then exit 1; fi
echo "All include counts within limits"
