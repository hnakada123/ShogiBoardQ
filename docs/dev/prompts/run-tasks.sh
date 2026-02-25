#!/bin/bash
# docs/dev/prompts/run-tasks.sh
# タスクプロンプトファイルを順次 Claude Code で実行するバッチスクリプト
#
# 使い方:
#   cd docs/dev/prompts
#   ./run-tasks.sh                    # task-*.md を番号順に全て実行
#   ./run-tasks.sh task-42-*.md       # 特定のファイルだけ実行

set -euo pipefail
cd "$(dirname "$0")"
PROJECT_ROOT="$(cd ../../.. && pwd)"

if [ $# -gt 0 ]; then
    files=("$@")
else
    files=(refactor-*.md)
fi

echo "=== Claude Code Batch Runner ==="
echo "Project: $PROJECT_ROOT"
echo "Tasks: ${#files[@]} file(s)"
echo ""

for prompt_file in "${files[@]}"; do
    if [ ! -f "$prompt_file" ]; then
        echo "[SKIP] $prompt_file not found"
        continue
    fi
    echo "[START] $prompt_file"
    (cd "$PROJECT_ROOT" && claude -p "$(cat "docs/dev/prompts/$prompt_file")" \
        --dangerously-skip-permissions \
        --output-format text)
    echo "[DONE]  $prompt_file"
    echo ""
done

echo "=== All tasks completed ==="
