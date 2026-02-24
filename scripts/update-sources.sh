#!/bin/bash
# CMakeLists.txt のソースリストを更新するヘルパー
# 使い方: ./scripts/update-sources.sh > /tmp/sources.txt
# 出力を CMakeLists.txt にコピーする

cd "$(dirname "$0")/.." || exit 1

for dir in app core game kifu analysis engine network navigation board ui views widgets dialogs models services common; do
    echo "# src/${dir}/"
    find "src/${dir}" -name '*.cpp' -o -name '*.h' | sort
    echo ""
done
