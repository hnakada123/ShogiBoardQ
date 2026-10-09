#!/bin/bash
# usage: kwin_capture.sh <name> <ja_JP|en|zh_CN|zh_TW> <script> [script args...]
#
# ウィンドウ枠（KDE の Breeze 装飾）付きの画像を撮るための実行環境。
# 入れ子の KWin（仮想画面）を使い捨ての D-Bus 上で起動し、ShogiBoardQ を Wayland クライアントとして
# --automation 付きで起動してから <script> を実行する。<script> には環境変数 SBQ_SOCK（自動化ソケット）が渡る。
# 枠付きの撮影は <script> の中で `spectacle -b -n -a -S -o out.png`（アクティブウィンドウ、影なし）を実行する。
# -S を付けないと周囲に半透明の影が入り、ガイドのページでは画像の外側が白い余白に見える。
#
# 隔離（2026-10-06 の教訓）:
#   - 普段のディスプレイ（DISPLAY・WAYLAND_DISPLAY）と入力メソッドの変数は、D-Bus を起動する前に外す。
#     外さないと、使い捨て D-Bus から起動したポータルや fcitx5 が普段の画面につながり、終了後も残る
#   - KWin の設定は作業フォルダに写し、kwinrc の InputMethod を消す
#   - 終了時に、使い捨て D-Bus につながったまま残ったプロセスを止める
# 環境変数は launch.sh と同じ（SBQ_WORK・SBQ_SOCKET_DIR・SBQ_APP・SBQ_ENGINES・SBQ_EXTRA_INI）。
# 画面の大きさは SBQ_KWIN_SIZE（既定 1700x1300）、ウィンドウの大きさは SBQ_WINDOW（既定 1300x980）。
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
REPO=$(cd "$HERE/../.." && pwd)
if [ "${SBQ_IN_KWIN_SESSION:-0}" != 1 ]; then
  [ $# -lt 3 ] && { sed -n '2,16p' "$0"; exit 2; }
  exec env -u DISPLAY -u WAYLAND_DISPLAY -u QT_IM_MODULE -u GTK_IM_MODULE -u XMODIFIERS -u SDL_IM_MODULE \
    QT_NO_XDG_DESKTOP_PORTAL=1 SBQ_IN_KWIN_SESSION=1 dbus-run-session -- "$0" "$@"
fi

NAME=$1; LANGUAGE_SETTING=$2; shift 2
WORK=${SBQ_WORK:-/tmp/sbq-shots-$(id -u)}
SOCKDIR=${SBQ_SOCKET_DIR:-/tmp/sbq-$(id -u)}
APP=${SBQ_APP:-$REPO/build/ShogiBoardQ}
SIZE=${SBQ_KWIN_SIZE:-1700x1300}
WINDOW=${SBQ_WINDOW:-1300x980}
C=$WORK/kcfg-$NAME
rm -rf "$C" "$WORK/kcache-$NAME" "$WORK/kdata-$NAME" "$WORK/kstate-$NAME"
mkdir -p "$C/ShogiBoardQ" "$SOCKDIR"
cp ~/.config/kdeglobals "$C/" 2>/dev/null || true
cp ~/.config/breezerc "$C/" 2>/dev/null || true
grep -v -i '^InputMethod' ~/.config/kwinrc > "$C/kwinrc" 2>/dev/null || true
{
  printf '[%%General]\nlanguage=%s\nlastKifuDirectory=%s\n\n[SizeRelated]\nmainWindowSize=@Size(%s %s)\n' \
    "$LANGUAGE_SETTING" "$HOME" "${WINDOW%x*}" "${WINDOW#*x}"
  if [ "${SBQ_ENGINES:-0}" = 1 ]; then
    printf '\n[Engines]\n1\\author=hnakada123\n1\\name=Hayanagi\n1\\path=%s\nsize=1\n' "$REPO/build/Hayanagi/hayanagi"
  fi
  if [ -n "${SBQ_EXTRA_INI:-}" ]; then printf '\n%s\n' "$SBQ_EXTRA_INI"; fi
} > "$C/ShogiBoardQ/ShogiBoardQ.ini"
unset QT_IM_MODULE GTK_IM_MODULE XMODIFIERS SDL_IM_MODULE
export QT_NO_XDG_DESKTOP_PORTAL=1
export XDG_CONFIG_HOME=$C XDG_CACHE_HOME=$WORK/kcache-$NAME XDG_DATA_HOME=$WORK/kdata-$NAME XDG_STATE_HOME=$WORK/kstate-$NAME
SOCKNAME=sbq-kwin-$NAME
kwin_wayland --virtual --width "${SIZE%x*}" --height "${SIZE#*x}" --socket "$SOCKNAME" > "$WORK/kwin-$NAME.log" 2>&1 &
KWIN=$!
for _ in $(seq 1 60); do [ -S "$XDG_RUNTIME_DIR/$SOCKNAME" ] && break; sleep 0.25; done
export WAYLAND_DISPLAY=$SOCKNAME QT_QPA_PLATFORM=wayland
case $LANGUAGE_SETTING in
  en) LOC=en_US.UTF-8 ;;
  zh_CN|zh_TW)
    LOC=$LANGUAGE_SETTING.UTF-8; export LANGUAGE=$LANGUAGE_SETTING
    [ -d "$WORK/locale/$LOC" ] || { mkdir -p "$WORK/locale"; localedef -i "$LANGUAGE_SETTING" -f UTF-8 "$WORK/locale/$LOC" 2>/dev/null || true; }
    export LOCPATH=$WORK/locale:/usr/lib/locale ;;
  *) LOC=ja_JP.UTF-8 ;;
esac
export SBQ_SOCK=$SOCKDIR/$NAME.sock
rm -f "$SBQ_SOCK"
(cd "$HOME" && LANG=$LOC LC_ALL=$LOC "$APP" --automation --automation-socket "$SBQ_SOCK" > "$WORK/kapp-$NAME.log" 2>&1) &
for _ in $(seq 1 60); do [ -S "$SBQ_SOCK" ] && break; sleep 0.5; done
sleep 3

status=0
"$@" || status=$?

python3 "$HERE/rpc.py" "$SBQ_SOCK" app.quit >/dev/null 2>&1
sleep 2
kill "$KWIN" 2>/dev/null
wait "$KWIN" 2>/dev/null
# この使い捨て D-Bus から起動されたポータルなどを残さない
for pid in $(pgrep -u "$(id -u)"); do
  [ "$pid" = $$ ] && continue
  { tr '\0' '\n' < "/proc/$pid/environ"; } 2>/dev/null | grep -qxF "DBUS_SESSION_BUS_ADDRESS=$DBUS_SESSION_BUS_ADDRESS" \
    && kill "$pid" 2>/dev/null
done
exit $status
