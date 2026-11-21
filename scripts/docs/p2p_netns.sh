#!/bin/bash
# P2P対局（「同じPC・同じLAN」）のガイド画像を、外へつながらないネットワーク名前空間の中で撮る。
#
# 名前空間には loopback と見本の LAN のアドレス（192.168.1.40）だけを置くので、招待文字列・合言葉に、この PC の
# 実際の LAN のアドレスや docker などのアドレスが写らない（監査 2026-11-21 S15。2026-11-20〜21 の画像には、撮影した
# PC のアドレスが写っていた）。Xvfb・2つのアプリ・協力ノードは、すべて名前空間の中で動かす。
#
# 使い方:
#   scripts/docs/p2p_netns.sh guide   <ja_JP|en|zh_CN|zh_TW>...   # shoot_p2p.py guide（p2p-invite.png など）
#   scripts/docs/p2p_netns.sh friends <ja_JP|en|zh_CN|zh_TW>...   # shoot_p2p.py friends（p2p-peers.png）
# 画像は $SBQ_P2P_NETNS_WORK/out-<guide|friends>/<言語>/ に出る（既定 build/docwork/p2pshoot-ns）。ガイドの画像との
# 違いを確かめてから docs/images/…/p2p-game/ に写す（プレイヤーID と使用期限の時刻は毎回変わる）。
# アプリは撮影用のビルド（build-p2p-docs。README の「インターネット越しの P2P対局の画像の撮り直し」）。
set -e
REPO=$(cd "$(dirname "$0")/../.." && pwd)
MODE=$1
shift || true
if [ "$MODE" != guide ] && [ "$MODE" != friends ] || [ $# -eq 0 ]; then
  sed -n '2,15p' "$0"
  exit 2
fi
# 名前空間の外で呼ばれたら、利用者の名前空間とネットワークの名前空間を作って入り直す
if [ -z "$SBQ_P2P_NETNS_INSIDE" ]; then
  exec env SBQ_P2P_NETNS_INSIDE=1 unshare -rn "$0" "$MODE" "$@"
fi

W=${SBQ_P2P_NETNS_WORK:-$REPO/build/docwork/p2pshoot-ns}
SAMPLE_IP=192.168.1.40
ip link set lo up
ip addr add "$SAMPLE_IP/32" dev lo
mkdir -p "$W"
export SBQ_APP=${SBQ_APP:-$REPO/build-p2p-docs/ShogiBoardQ} SBQ_WORK=$W/work SBQ_DISPLAY=${SBQ_DISPLAY:-96}

NODE=
cleanup() {
  [ -n "$NODE" ] && kill "$NODE" 2>/dev/null || true
  # 名前空間の中で起動した Xvfb を止める（ほかの Xvfb には触れない）
  for p in $(pgrep -x Xvfb); do
    if tr '\0' ' ' < "/proc/$p/cmdline" | grep -q "^Xvfb :$SBQ_DISPLAY "; then kill "$p" 2>/dev/null || true; fi
  done
}
trap cleanup EXIT

if [ "$MODE" = friends ]; then
  # 友だちの在席には、合言葉の待ち合わせに対応した協力ノードが要る
  mkdir -p "$W/node"
  chmod 700 "$W/node"
  rm -f "$W/node/node.log"
  env -i "$(dirname "$SBQ_APP")/shogiboardq-peer" --service --relay --meet --network sbq-local-1 \
      --identity "$W/node/node.key" --listen /ip4/0.0.0.0/tcp/4011 > "$W/node/node.log" 2>&1 &
  NODE=$!
  for _ in $(seq 1 40); do grep -q 'service started as' "$W/node/node.log" && break; sleep 0.25; done
  PEER=$(sed -n 's/.*service started as //p' "$W/node/node.log" | head -1)
  [ -n "$PEER" ] || { cat "$W/node/node.log" >&2; exit 1; }
fi

for lang in "$@"; do
  if [ "$MODE" = guide ]; then
    python3 "$REPO/scripts/docs/shoot_p2p.py" guide "$lang" "$W/out-guide/$lang"
  else
    python3 "$REPO/scripts/docs/shoot_p2p.py" friends "$lang" "$W/out-friends/$lang" \
        "/ip4/$SAMPLE_IP/tcp/4011/p2p/$PEER"
  fi
done
