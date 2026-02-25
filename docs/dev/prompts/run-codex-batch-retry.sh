#!/usr/bin/env bash
# docs/dev/prompts/run-codex-batch-retry.sh
#
# 直前バッチで失敗したジョブだけを再実行するスクリプトです。
# `run-codex-batch.sh` が出力した failed_jobs.txt を入力に使います。
#
# 使い方:
#   1) 先に通常バッチを実行する
#        bash docs/dev/prompts/run-codex-batch.sh "docs/dev/prompts/task-*.md" "batch_results/tasks"
#   2) 失敗分のみ再実行する
#        bash docs/dev/prompts/run-codex-batch-retry.sh "batch_results/tasks/failed_jobs.txt" "batch_results/tasks-retry"
#
# 引数:
#   第1引数: 失敗ジョブ一覧ファイル（省略時: batch_results/failed_jobs.txt）
#   第2引数: 結果出力ディレクトリ（省略時: batch_results/retry）
#
# 出力:
#   <結果ディレクトリ>/<連番>-<プロンプト名>.final.txt    最終応答
#   <結果ディレクトリ>/<連番>-<プロンプト名>.events.jsonl 実行イベント
#   <結果ディレクトリ>/job_status.tsv                    実行結果一覧（OK/FAIL）
#   <結果ディレクトリ>/failed_jobs.txt                   再実行後も失敗した一覧
#
# 注意:
#   - failed_jobs.txt が空の場合は何も実行せず終了します。
#   - 同じパスが重複していても1回だけ実行します。

set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
RUN_CWD="$(pwd)"

FAILED_LIST_FILE="${1:-batch_results/failed_jobs.txt}"
RESULT_DIR="${2:-batch_results/retry}"
STATUS_FILE="${RESULT_DIR}/job_status.tsv"
RETRY_FAILED_FILE="${RESULT_DIR}/failed_jobs.txt"

if ! command -v codex >/dev/null 2>&1; then
    echo "Error: codex コマンドが見つかりません。" >&2
    exit 1
fi

if [ ! -f "${FAILED_LIST_FILE}" ]; then
    echo "失敗ジョブ一覧が見つかりません: ${FAILED_LIST_FILE}" >&2
    exit 1
fi

declare -A seen_paths
declare -a prompt_files

while IFS= read -r raw_line; do
    line="${raw_line%$'\r'}"
    if [[ -z "${line//[[:space:]]/}" ]]; then
        continue
    fi
    if [[ "${line}" =~ ^[[:space:]]*# ]]; then
        continue
    fi

    if [[ "${line}" = /* ]]; then
        prompt_path="${line}"
    elif [ -f "${RUN_CWD}/${line}" ]; then
        prompt_path="${RUN_CWD}/${line}"
    else
        prompt_path="${PROJECT_ROOT}/${line}"
    fi

    if [[ -z "${seen_paths["${prompt_path}"]+x}" ]]; then
        seen_paths["${prompt_path}"]=1
        prompt_files+=("${prompt_path}")
    fi
done < "${FAILED_LIST_FILE}"

if [ "${#prompt_files[@]}" -eq 0 ]; then
    echo "再実行対象の失敗ジョブはありません。"
    exit 0
fi

mkdir -p "${RESULT_DIR}"
: > "${RETRY_FAILED_FILE}"
printf "status\tprompt_file\tfinal_file\tevents_file\n" > "${STATUS_FILE}"

echo "=== Codex Retry Runner ==="
echo "Project root : ${PROJECT_ROOT}"
echo "Failed list  : ${FAILED_LIST_FILE}"
echo "Result dir   : ${RESULT_DIR}"
echo "Retry jobs   : ${#prompt_files[@]}"
echo

ok_count=0
ng_count=0
index=1

for prompt_file in "${prompt_files[@]}"; do
    job_id="$(printf "%03d" "${index}")"
    base_name="$(basename "${prompt_file}")"
    name_without_ext="${base_name%.*}"
    final_file="${RESULT_DIR}/${job_id}-${name_without_ext}.final.txt"
    events_file="${RESULT_DIR}/${job_id}-${name_without_ext}.events.jsonl"

    if [ ! -f "${prompt_file}" ]; then
        echo "[FAIL]  ${prompt_file} (file not found)" >&2
        printf "FAIL\t%s\t%s\t%s\n" "${prompt_file}" "${final_file}" "${events_file}" >> "${STATUS_FILE}"
        printf "%s\n" "${prompt_file}" >> "${RETRY_FAILED_FILE}"
        ng_count=$((ng_count + 1))
        index=$((index + 1))
        echo
        continue
    fi

    echo "[START] ${prompt_file}"
    if codex exec --full-auto -C "${PROJECT_ROOT}" --json -o "${final_file}" - < "${prompt_file}" > "${events_file}"; then
        echo "[OK]    ${prompt_file}"
        printf "OK\t%s\t%s\t%s\n" "${prompt_file}" "${final_file}" "${events_file}" >> "${STATUS_FILE}"
        ok_count=$((ok_count + 1))
    else
        echo "[FAIL]  ${prompt_file}" >&2
        printf "FAIL\t%s\t%s\t%s\n" "${prompt_file}" "${final_file}" "${events_file}" >> "${STATUS_FILE}"
        printf "%s\n" "${prompt_file}" >> "${RETRY_FAILED_FILE}"
        ng_count=$((ng_count + 1))
    fi

    index=$((index + 1))
    echo
done

echo "=== Completed ==="
echo "Success: ${ok_count}"
echo "Failed : ${ng_count}"
echo "Status : ${STATUS_FILE}"
echo "Still failed list: ${RETRY_FAILED_FILE}"

if [ "${ng_count}" -gt 0 ]; then
    exit 1
fi
