#!/bin/bash
# usage: kwin_x_capture.sh <name> <ja_JP|en|zh_CN|zh_TW> <script> [args...]
# kwin_capture.sh の Xwayland 版: ShogiBoardQ を xcb で起動し、x11ctl でウィンドウを動かせるようにする
# （Wayland のクライアントは自分の位置を決められないため、浮動ドックを並べる画像に使う）。
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
REPO=$(cd "$HERE/../.." && pwd)
if [ "${SBQ_IN_KWIN_SESSION:-0}" != 1 ]; then
  [ $# -lt 3 ] && { sed -n '2,5p' "$0"; exit 2; }
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
  if [ -n "${SBQ_EXTRA_INI:-}" ]; then printf '\n%s\n' "$SBQ_EXTRA_INI"; fi
} > "$C/ShogiBoardQ/ShogiBoardQ.ini"
unset QT_IM_MODULE GTK_IM_MODULE XMODIFIERS SDL_IM_MODULE
export QT_NO_XDG_DESKTOP_PORTAL=1
export XDG_CONFIG_HOME=$C XDG_CACHE_HOME=$WORK/kcache-$NAME XDG_DATA_HOME=$WORK/kdata-$NAME XDG_STATE_HOME=$WORK/kstate-$NAME
export SHOGIBOARDQ_CONFIG_HOME=$C SHOGIBOARDQ_CACHE_HOME=$WORK/kcache-$NAME SHOGIBOARDQ_DATA_HOME=$WORK/kdata-$NAME
case $LANGUAGE_SETTING in
  en) LOC=en_US.UTF-8 ;;
  zh_CN|zh_TW)
    LOC=$LANGUAGE_SETTING.UTF-8; export LANGUAGE=$LANGUAGE_SETTING
    [ -d "$WORK/locale/$LOC" ] || { mkdir -p "$WORK/locale"; localedef -i "$LANGUAGE_SETTING" -f UTF-8 "$WORK/locale/$LOC" 2>/dev/null || true; }
    export LOCPATH=$WORK/locale:/usr/lib/locale ;;
  *) LOC=ja_JP.UTF-8 ;;
esac
export LANG=$LOC LC_ALL=$LOC
export SBQ_SOCK=$SOCKDIR/$NAME.sock
rm -f "$SBQ_SOCK"
SESSION=$WORK/ksession-$NAME.sh
{
  echo '#!/bin/bash'
  echo 'cd "$HOME"'
  printf 'QT_QPA_PLATFORM=xcb QT_QPA_PLATFORMTHEME=kde XDG_CURRENT_DESKTOP=KDE %q --automation --automation-socket %q > %q 2>&1 &\n' "$APP" "$SBQ_SOCK" "$WORK/kapp-$NAME.log"
  echo 'for _ in $(seq 1 60); do [ -S "$SBQ_SOCK" ] && break; sleep 0.5; done; sleep 3'
  printf 'cd %q\n' "$REPO"
  printf '%q ' "$@"; echo
  printf 'python3 %q "$SBQ_SOCK" app.quit >/dev/null 2>&1; sleep 2\n' "$HERE/rpc.py"
} > "$SESSION"
chmod +x "$SESSION"
SOCKNAME=sbq-kwin-$NAME
kwin_wayland --virtual --width "${SIZE%x*}" --height "${SIZE#*x}" --socket "$SOCKNAME" --xwayland --exit-with-session "$SESSION" > "$WORK/kwin-$NAME.log" 2>&1
status=$?
for pid in $(pgrep -u "$(id -u)"); do
  [ "$pid" = $$ ] && continue
  { tr '\0' '\n' < "/proc/$pid/environ"; } 2>/dev/null | grep -qxF "DBUS_SESSION_BUS_ADDRESS=$DBUS_SESSION_BUS_ADDRESS" \
    && kill "$pid" 2>/dev/null
done
exit $status
