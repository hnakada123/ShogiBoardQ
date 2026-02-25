#!/usr/bin/env bash
# docs/dev/prompt/run-codex-batch.sh
#
# 複数のプロンプトファイルを Codex で順次実行するバッチスクリプトです。
# 各ファイルは `codex exec` の新規セッションで実行されます（/new 相当）。
#
# 使い方:
#   1) プロンプトを用意する（例: prompts/001.md, prompts/002.md）
#   2) プロジェクトルートで実行する
#        bash docs/dev/prompt/run-codex-batch.sh
#
# 引数:
#   第1引数: プロンプトのグロブ（省略時: prompts/*.md）
#   第2引数: 結果出力ディレクトリ（省略時: batch_results）
#
# 例:
#   bash docs/dev/prompt/run-codex-batch.sh "docs/dev/prompts/task-*.md" "batch_results/tasks"
#
# 出力:
#   <結果ディレクトリ>/<プロンプト名>.final.txt    最終応答
#   <結果ディレクトリ>/<プロンプト名>.events.jsonl 実行イベント
#   <結果ディレクトリ>/job_status.tsv             実行結果一覧（OK/FAIL）
#   <結果ディレクトリ>/failed_jobs.txt            失敗したプロンプトの一覧
#
# 注意:
#   - 途中で失敗したプロンプトがあっても他の処理は継続します。
#   - `codex` コマンドが利用可能である必要があります。

set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
RUN_CWD="$(pwd)"

PROMPT_GLOB="${1:-prompts/*.md}"
RESULT_DIR="${2:-batch_results}"
STATUS_FILE="${RESULT_DIR}/job_status.tsv"
FAILED_JOBS_FILE="${RESULT_DIR}/failed_jobs.txt"

if ! command -v codex >/dev/null 2>&1; then
    echo "Error: codex コマンドが見つかりません。" >&2
    exit 1
fi

mkdir -p "${RESULT_DIR}"
: > "${FAILED_JOBS_FILE}"
printf "status\tprompt_file\tfinal_file\tevents_file\n" > "${STATUS_FILE}"

mapfile -t PROMPT_FILES < <(compgen -G "${PROMPT_GLOB}" || true)
if [ "${#PROMPT_FILES[@]}" -eq 0 ]; then
    echo "プロンプトファイルが見つかりません: ${PROMPT_GLOB}" >&2
    exit 1
fi

echo "=== Codex Batch Runner ==="
echo "Project root : ${PROJECT_ROOT}"
echo "Prompt glob  : ${PROMPT_GLOB}"
echo "Result dir   : ${RESULT_DIR}"
echo "Total prompts: ${#PROMPT_FILES[@]}"
echo

ok_count=0
ng_count=0

for prompt_file in "${PROMPT_FILES[@]}"; do
    if [[ "${prompt_file}" = /* ]]; then
        prompt_path="${prompt_file}"
    else
        prompt_path="${RUN_CWD}/${prompt_file}"
    fi

    base_name="$(basename "${prompt_path}")"
    name_without_ext="${base_name%.*}"
    final_file="${RESULT_DIR}/${name_without_ext}.final.txt"
    events_file="${RESULT_DIR}/${name_without_ext}.events.jsonl"

    echo "[START] ${prompt_path}"
    if codex exec --full-auto -C "${PROJECT_ROOT}" --json -o "${final_file}" - < "${prompt_path}" > "${events_file}"; then
        echo "[OK]    ${prompt_path}"
        printf "OK\t%s\t%s\t%s\n" "${prompt_path}" "${final_file}" "${events_file}" >> "${STATUS_FILE}"
        ok_count=$((ok_count + 1))
    else
        echo "[FAIL]  ${prompt_path}" >&2
        printf "FAIL\t%s\t%s\t%s\n" "${prompt_path}" "${final_file}" "${events_file}" >> "${STATUS_FILE}"
        printf "%s\n" "${prompt_path}" >> "${FAILED_JOBS_FILE}"
        ng_count=$((ng_count + 1))
    fi
    echo
done

echo "=== Completed ==="
echo "Success: ${ok_count}"
echo "Failed : ${ng_count}"
echo "Status : ${STATUS_FILE}"
echo "Failed list: ${FAILED_JOBS_FILE}"

if [ "${ng_count}" -gt 0 ]; then
    exit 1
fi
