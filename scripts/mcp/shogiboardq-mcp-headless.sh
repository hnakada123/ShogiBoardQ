#!/bin/bash
# ShogiBoardQ の MCP サーバーを、Xvfb の仮想ディスプレイと専用の設定フォルダで起動する（Linux 用）。
#
# MCP サーバーが自動で起動するアプリ（ShogiBoardQ --automation）は、普段の画面に出ず、
# 普段の設定ファイルも書き換えない。MCP クライアントにはこのスクリプトを stdio サーバーとして登録する。
#   claude mcp add shogiboardq-headless \
#     -e SHOGIBOARDQ_EXECUTABLE=/path/to/ShogiBoardQ/build/ShogiBoardQ \
#     -- /path/to/ShogiBoardQ/scripts/mcp/shogiboardq-mcp-headless.sh
#
# 環境変数:
#   SHOGIBOARDQ_EXECUTABLE     ShogiBoardQ の実行ファイル（既定 このリポジトリの build/ShogiBoardQ）
#   SHOGIBOARDQ_HEADLESS_HOME  設定・キャッシュ・データの置き場所
#                              （既定 ${XDG_STATE_HOME:-~/.local/state}/shogiboardq-mcp-headless）。
#                              初回だけ普段の ShogiBoardQ.ini をコピーし、登録済みのエンジンを使えるようにする
#   SHOGIBOARDQ_HEADLESS_SCREEN  Xvfb の画面（既定 1920x1440x24）
#
# 標準出力は MCP の通信路なので、このスクリプトと Xvfb は標準出力に何も書かない。
# MCP サーバーが終了するときは、起動したアプリを閉じてから Xvfb を止める。
# Windows・macOS には Xvfb が無い。画面に出さずに使うときは、MCP サーバーの環境変数に
# QT_QPA_PLATFORM=offscreen を設定する（mcp/README.md）。
set -u

REPO=$(cd "$(dirname "$0")/../.." && pwd)
HOME_DIR=${SHOGIBOARDQ_HEADLESS_HOME:-${XDG_STATE_HOME:-$HOME/.local/state}/shogiboardq-mcp-headless}
REAL_CONFIG=${XDG_CONFIG_HOME:-$HOME/.config}
RUNTIME=/tmp/shogiboardq-mcp-headless-$(id -u)

log() { printf '%s\n' "shogiboardq-mcp-headless: $*" >&2; }

if ! command -v Xvfb >/dev/null 2>&1; then
    log "Xvfb が見つかりません（xorg-server-xvfb などを導入してください）"
    exit 1
fi

mkdir -p "$HOME_DIR/config/ShogiBoardQ" "$HOME_DIR/cache" "$HOME_DIR/data" "$HOME_DIR/state" "$RUNTIME"
chmod 700 "$RUNTIME"
if [ ! -f "$HOME_DIR/config/ShogiBoardQ/ShogiBoardQ.ini" ] && [ -f "$REAL_CONFIG/ShogiBoardQ/ShogiBoardQ.ini" ]; then
    cp "$REAL_CONFIG/ShogiBoardQ/ShogiBoardQ.ini" "$HOME_DIR/config/ShogiBoardQ/ShogiBoardQ.ini"
fi
# Breeze の色・フォントにするため、KDE の全体設定だけ写す
cp "$REAL_CONFIG/kdeglobals" "$HOME_DIR/config/kdeglobals" 2>/dev/null || true

# 空いている番号で Xvfb を起動し、準備ができて書き出された番号を読む
DISPLAY_FILE=$(mktemp "$RUNTIME/display.XXXXXX")
Xvfb -displayfd 9 -screen 0 "${SHOGIBOARDQ_HEADLESS_SCREEN:-1920x1440x24}" -nolisten tcp \
    9>"$DISPLAY_FILE" </dev/null >>"$RUNTIME/xvfb.log" 2>&1 &
XVFB_PID=$!
SERVER_PID=
cleanup() {
    [ -n "$SERVER_PID" ] && kill "$SERVER_PID" 2>/dev/null
    kill "$XVFB_PID" 2>/dev/null
    rm -f "$DISPLAY_FILE"
}
trap cleanup EXIT
trap 'exit 129' HUP
trap 'exit 130' INT
trap 'exit 143' TERM

DISPLAY_NUMBER=
for _ in $(seq 1 100); do
    DISPLAY_NUMBER=$(tr -dc '0-9' < "$DISPLAY_FILE")
    [ -n "$DISPLAY_NUMBER" ] && break
    kill -0 "$XVFB_PID" 2>/dev/null || break
    sleep 0.1
done
if [ -z "$DISPLAY_NUMBER" ]; then
    log "Xvfb を起動できませんでした（$RUNTIME/xvfb.log）"
    exit 1
fi

# Wayland を外して Qt を Xvfb（xcb）に向け、設定・キャッシュ・データ・ソケットを専用の場所にする
unset WAYLAND_DISPLAY
export DISPLAY=":$DISPLAY_NUMBER"
export QT_QPA_PLATFORM=xcb QT_QPA_PLATFORMTHEME=kde XDG_CURRENT_DESKTOP=KDE
export XDG_CONFIG_HOME="$HOME_DIR/config" XDG_CACHE_HOME="$HOME_DIR/cache"
export XDG_DATA_HOME="$HOME_DIR/data" XDG_STATE_HOME="$HOME_DIR/state" XDG_RUNTIME_DIR="$RUNTIME"
export SHOGIBOARDQ_CONFIG_HOME="$HOME_DIR/config" SHOGIBOARDQ_CACHE_HOME="$HOME_DIR/cache"
export SHOGIBOARDQ_DATA_HOME="$HOME_DIR/data"
export SHOGIBOARDQ_AUTOMATION_SOCKET="$RUNTIME/automation.sock"
# サーバーの終了時に、起動したアプリを正常に閉じてから Xvfb を止める
export SHOGIBOARDQ_QUIT_APP_ON_EXIT=1
export SHOGIBOARDQ_EXECUTABLE="${SHOGIBOARDQ_EXECUTABLE:-$REPO/build/ShogiBoardQ}"
export PYTHONPATH="$REPO/mcp${PYTHONPATH:+:$PYTHONPATH}"

# 終了の合図を受けたらすぐ後片付けできるよう、標準入力を渡して裏で動かし wait する
python3 -m shogiboardq_mcp <&0 &
SERVER_PID=$!
wait "$SERVER_PID"
STATUS=$?
SERVER_PID=
exit "$STATUS"
