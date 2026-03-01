#!/bin/bash
# git diff で tr() を含む行が変更されている場合、
# .ts ファイルも同時に変更されているか確認

set -euo pipefail

BASE_BRANCH="${1:-origin/main}"

# tr() を含む変更があるか
TR_CHANGES=$(git diff "$BASE_BRANCH" -- 'src/**/*.cpp' 'src/**/*.h' | grep -c 'tr(' || true)

if [ "$TR_CHANGES" -gt 0 ]; then
    # .ts ファイルの変更があるか
    TS_CHANGES=$(git diff --name-only "$BASE_BRANCH" -- 'resources/translations/*.ts' | wc -l)
    if [ "$TS_CHANGES" -eq 0 ]; then
        echo "::warning::tr() strings were modified but no .ts files were updated. Run: cmake --build build --target update_translations"
    fi
fi
