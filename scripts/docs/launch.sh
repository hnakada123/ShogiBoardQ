#!/bin/bash
# usage: launch.sh <name> <ja_JP|en|zh_CN|zh_TW> <display> [W H]
#
# ガイド用スクリーンショットのために、ShogiBoardQ を Xvfb 上で --automation 付きで起動する。
# 設定・キャッシュ・データ・状態はすべて作業フォルダに隔離し、普段の設定には書き込まない。
# 自動操作は rpc.py <name> <method> [json] で行う（ソケットは $SBQ_SOCKET_DIR/<name>.sock）。
#
# 環境変数:
#   SBQ_WORK        作業フォルダ（既定 /tmp/sbq-shots-<uid>）。cfg-<name> などを作る
#   SBQ_SOCKET_DIR  ソケットの置き場所（既定 /tmp/sbq-<uid>。Unix ソケットのパスは約100バイトまで）
#   SBQ_APP         ShogiBoardQ の実行ファイル（既定 このリポジトリの build/ShogiBoardQ）
#   SBQ_SCREEN      Xvfb の画面（既定 1920x1440x24。高解像度の盤面は 2560x1800x24）
#   SBQ_ENGINES=1   ini に Hayanagi（build/Hayanagi/hayanagi、名前は "Hayanagi"）を登録する
#   SBQ_EXTRA_INI   ini の末尾に追記する文字列（例: $'[SizeRelated]\nsquareSize=100'）
#   SBQ_FRESH=1     既存の cfg-<name> を消してから作る
set -e
REPO=$(cd "$(dirname "$0")/../.." && pwd)
NAME=$1; LANGUAGE_SETTING=$2; DISP=$3; W=${4:-1300}; H=${5:-980}
if [ -z "$DISP" ]; then sed -n '2,20p' "$0"; exit 2; fi
WORK=${SBQ_WORK:-/tmp/sbq-shots-$(id -u)}
SOCKDIR=${SBQ_SOCKET_DIR:-/tmp/sbq-$(id -u)}
APP=${SBQ_APP:-$REPO/build/ShogiBoardQ}
CFG=$WORK/cfg-$NAME
[ "${SBQ_FRESH:-0}" = 1 ] && rm -rf "$CFG" "$WORK/cache-$NAME" "$WORK/data-$NAME" "$WORK/state-$NAME"
mkdir -p "$CFG/ShogiBoardQ" "$WORK/cache-$NAME" "$WORK/data-$NAME" "$WORK/state-$NAME" "$SOCKDIR"
chmod 700 "$SOCKDIR"
# Breeze の色・フォントにするため KDE の全体設定だけ写す
cp ~/.config/kdeglobals "$CFG/kdeglobals" 2>/dev/null || true
if [ ! -f "$CFG/ShogiBoardQ/ShogiBoardQ.ini" ]; then
  {
    # QSettings の INI では General グループを [%General] と書く（[General] だと言語が効かない）
    printf '[%%General]\nlanguage=%s\nlastKifuDirectory=%s\n\n[SizeRelated]\nmainWindowSize=@Size(%s %s)\n' \
      "$LANGUAGE_SETTING" "$HOME" "$W" "$H"
    if [ "${SBQ_ENGINES:-0}" = 1 ]; then
      printf '\n[Engines]\n1\\author=hnakada123\n1\\name=Hayanagi\n1\\path=%s\nsize=1\n' "$REPO/build/Hayanagi/hayanagi"
    fi
    if [ -n "$SBQ_EXTRA_INI" ]; then printf '\n%s\n' "$SBQ_EXTRA_INI"; fi
  } > "$CFG/ShogiBoardQ/ShogiBoardQ.ini"
fi

if ! pgrep -f "^Xvfb :$DISP " >/dev/null; then
  Xvfb ":$DISP" -screen 0 "${SBQ_SCREEN:-1920x1440x24}" -nolisten tcp >/dev/null 2>&1 &
  sleep 1
fi

LOCPATH_ENV=()
case $LANGUAGE_SETTING in
  en) LOC=en_US.UTF-8 ;;
  zh_CN|zh_TW)
    # 中国語のロケールが無い環境向けに、作業フォルダへ作る（root 不要）。KDE のファイル画面まで中国語になる
    LOC=$LANGUAGE_SETTING.UTF-8
    if [ ! -d "$WORK/locale/$LOC" ]; then
      mkdir -p "$WORK/locale"
      localedef -i "$LANGUAGE_SETTING" -f UTF-8 "$WORK/locale/$LOC" 2>/dev/null || true
    fi
    # LOCPATH を付けると既定の locale-archive を読まなくなるので、中国語のときだけ付ける
    LOCPATH_ENV=(LOCPATH="$WORK/locale:/usr/lib/locale" LANGUAGE="$LANGUAGE_SETTING") ;;
  *) LOC=ja_JP.UTF-8 ;;
esac

rm -f "$SOCKDIR/$NAME.sock"
cd "$HOME"
env DISPLAY=":$DISP" QT_QPA_PLATFORM=xcb QT_QPA_PLATFORMTHEME=kde XDG_CURRENT_DESKTOP=KDE \
  XDG_CONFIG_HOME="$CFG" XDG_CACHE_HOME="$WORK/cache-$NAME" XDG_DATA_HOME="$WORK/data-$NAME" \
  SHOGIBOARDQ_CONFIG_HOME="$CFG" SHOGIBOARDQ_CACHE_HOME="$WORK/cache-$NAME" SHOGIBOARDQ_DATA_HOME="$WORK/data-$NAME" \
  XDG_STATE_HOME="$WORK/state-$NAME" XDG_RUNTIME_DIR="$SOCKDIR" LANG="$LOC" LC_ALL="$LOC" "${LOCPATH_ENV[@]}" \
  nohup "$APP" --automation --automation-socket "$SOCKDIR/$NAME.sock" > "$WORK/app-$NAME.log" 2>&1 &
echo "pid $!"
for _ in $(seq 1 60); do [ -S "$SOCKDIR/$NAME.sock" ] && break; sleep 0.5; done
[ -S "$SOCKDIR/$NAME.sock" ] || { echo "automation socket did not appear; see $WORK/app-$NAME.log" >&2; exit 1; }
