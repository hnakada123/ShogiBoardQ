#!/bin/bash
# 棋譜ナビゲーション自動テストスクリプト
# 使用方法: ./scripts/run_navigation_tests.sh

set -e

cd "$(dirname "$0")/.."
REPO_ROOT="$(pwd)"

# ビルド確認
if [ ! -f "build/ShogiBoardQ" ]; then
    echo "Error: build/ShogiBoardQ not found. Run 'cmake --build build' first."
    exit 1
fi

# テストKIFファイル確認
if [ ! -f "test_branch.kif" ]; then
    echo "Error: test_branch.kif not found in repository root."
    exit 1
fi

# 絶対パスを設定
KIF_FILE="$REPO_ROOT/test_branch.kif"

echo "=== 棋譜ナビゲーション自動テスト ==="
echo ""

PASS_COUNT=0
FAIL_COUNT=0

run_test() {
    # ログファイルをクリアしてからテスト実行
    > debug.log
    "$@" 2>&1 > /dev/null
}

check_result() {
    local test_name="$1"
    local expected="$2"
    local pattern="$3"

    local actual=$(grep -E "$pattern" debug.log | tail -1)

    if echo "$actual" | grep -q "$expected"; then
        echo "  [PASS] $test_name"
        PASS_COUNT=$((PASS_COUNT + 1))
    else
        echo "  [FAIL] $test_name"
        echo "    Expected to contain: $expected"
        echo "    Actual: $actual"
        FAIL_COUNT=$((FAIL_COUNT + 1))
    fi
}

# テスト1: 本譜の全行クリック
echo "--- テスト1: 本譜の全行クリック ---"
for row in 0 1 2 3 4 5 6 7; do
    run_test ./build/ShogiBoardQ --test-mode \
        --load-kif "$KIF_FILE" \
        --click-kifu-row $row \
        --dump-state-after 7000
    check_result "Row $row: line=0" "line= 0" "highlighting branch tree"
done

# テスト2: 分岐選択
echo ""
echo "--- テスト2: 分岐選択（６六歩） ---"
run_test ./build/ShogiBoardQ --test-mode \
    --load-kif "$KIF_FILE" \
    --navigate-to 3 \
    --click-branch 1 \
    --dump-state-after 4000
check_result "currentLineIndex: 1" "currentLineIndex: 1" "currentLineIndex:"
check_result "displayText: ６六歩" "６六歩" "currentNode displayText:"

# テスト3: 分岐内ナビゲーション（戻る→進む）
echo ""
echo "--- テスト3: 分岐内ナビゲーション ---"
run_test ./build/ShogiBoardQ --test-mode \
    --load-kif "$KIF_FILE" \
    --navigate-to 3 \
    --click-branch 1 \
    --click-prev 1 \
    --click-next 1 \
    --dump-state-after 5000
check_result "分岐維持: lineIndex=1" "currentLineIndex: 1" "currentLineIndex:"
check_result "分岐維持: ６六歩" "６六歩" "currentNode displayText:"

# テスト4: サブブランチ（７七角）選択
echo ""
echo "--- テスト4: サブブランチ選択（７七角） ---"
run_test ./build/ShogiBoardQ --test-mode \
    --load-kif "$KIF_FILE" \
    --navigate-to 3 \
    --click-branch 1 \
    --click-next 2 \
    --click-branch2 1 \
    --dump-state-after 5000
check_result "currentLineIndex: 2" "currentLineIndex: 2" "currentLineIndex:"
check_result "displayText: ７七角" "７七角" "currentNode displayText:"

# テスト5: サブブランチ内ナビゲーション
echo ""
echo "--- テスト5: サブブランチ内ナビゲーション ---"
run_test ./build/ShogiBoardQ --test-mode \
    --load-kif "$KIF_FILE" \
    --navigate-to 3 \
    --click-branch 1 \
    --click-next 2 \
    --click-branch2 1 \
    --click-prev2 1 \
    --click-next3 1 \
    --dump-state-after 7000
check_result "サブブランチ維持: lineIndex=2" "currentLineIndex: 2" "currentLineIndex:"
check_result "サブブランチ維持: ７七角" "７七角" "currentNode displayText:"

# テスト6: 分岐選択後の棋譜欄クリック
echo ""
echo "--- テスト6: 分岐選択後の棋譜欄クリック ---"
run_test ./build/ShogiBoardQ --test-mode \
    --load-kif "$KIF_FILE" \
    --navigate-to 3 \
    --click-branch 1 \
    --click-kifu-row 4 \
    --dump-state-after 6000
check_result "分岐ツリーハイライト: line=1" "line= 1" "highlighting branch tree"
check_result "currentLineIndex: 1" "currentLineIndex: 1" "currentLineIndex:"

# 結果サマリー
echo ""
echo "=== テスト結果 ==="
echo "PASS: $PASS_COUNT"
echo "FAIL: $FAIL_COUNT"

if [ $FAIL_COUNT -eq 0 ]; then
    echo ""
    echo "All tests passed!"
    exit 0
else
    echo ""
    echo "Some tests failed."
    exit 1
fi
