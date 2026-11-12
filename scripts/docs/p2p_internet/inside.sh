#!/bin/bash
# コンテナの中で動かす（root）。インターネット越しの P2P対局を撮るための仮想のネットワークを作る。
#
#   inside.sh up                 ネットワーク・協力ノード・Xvfb を用意する
#   inside.sh player <名前> <言語> <A|B> [W H]
#                                プレイヤーのアプリを家庭LAN A・B の中で --automation 付きで起動する
#   inside.sh nat <A|B> <easy|strict>
#                                ルーターの NAT を切り替える（strict は接続ごとにポートを変え、ホールパンチングが通らない）
#
# 構成（「インターネット」の 192.88.99.0/24 は、6to4 のために予約され今は使われない範囲。アプリは文書用の範囲
# 203.0.113.0/24 などを宛先に使わないので、グローバル扱いになるこの範囲を使う。コンテナの外へは出ない）:
#
#   協力ノード 192.88.99.1 ─┬─ ルーターA 192.88.99.11（NAT）─ 家庭LAN A 192.168.10.0/24 ─ プレイヤーA .10
#                           └─ ルーターB 192.88.99.12（NAT）─ 家庭LAN B 192.168.20.0/24 ─ プレイヤーB .10
#
# 環境: /repo（リポジトリ。読み取り専用）、/work（作業フォルダ）、SBQ_BUILD（既定 build-p2p-docs）
set -euo pipefail
BUILD=/repo/${SBQ_BUILD:-build-p2p-docs}
WORK=/work
NODE_PORT=4001

wan_of() { case $1 in A) echo 192.88.99.11 ;; B) echo 192.88.99.12 ;; esac; }
lan_of() { case $1 in A) echo 10 ;; B) echo 20 ;; esac; }

up() {
    ldconfig 2>/dev/null || true
    mkdir -p "$WORK/run" "$WORK/shots"
    # 自動化 API のソケットの置き場所は、アプリ（root）だけが書ける場所にする（lab.sh down が持ち主を戻すので取り直す）
    chown root:root "$WORK/run"
    chmod 700 "$WORK/run"

    # 「インターネット」のスイッチ（コンテナのネットワーク名前空間に置く）
    ip link add br0 type bridge
    ip link set br0 up

    # 協力ノード
    ip netns add node
    ip link add n-node type veth peer name eth0 netns node
    ip link set n-node master br0 up
    ip -n node addr add 192.88.99.1/24 dev eth0
    ip -n node link set eth0 up
    ip -n node link set lo up

    for side in A B; do
        local wan lan
        wan=$(wan_of $side)
        lan=$(lan_of $side)
        # ルーター: wan はインターネット側、lan は家庭LAN側
        ip netns add "r$side"
        ip link add "n-r$side" type veth peer name wan netns "r$side"
        ip link set "n-r$side" master br0 up
        ip -n "r$side" addr add "$wan/24" dev wan
        ip -n "r$side" link set wan up
        ip -n "r$side" link set lo up
        ip netns exec "r$side" sysctl -qw net.ipv4.ip_forward=1
        # プレイヤーの PC
        ip netns add "p$side"
        ip link add lan netns "r$side" type veth peer name eth0 netns "p$side"
        ip -n "r$side" addr add "192.168.$lan.1/24" dev lan
        ip -n "r$side" link set lan up
        ip -n "p$side" addr add "192.168.$lan.10/24" dev eth0
        ip -n "p$side" link set eth0 up
        ip -n "p$side" link set lo up
        ip -n "p$side" route add default via "192.168.$lan.1"
        # 家庭のルーター: 外から始まる接続は受け付けない。記録する前（宛先の変換の前）に捨てる。記録すると、
        # ホールパンチングで相手の最初の包みが先に届いたとき、その記録が変換表を埋め、こちらから出る包みの
        # 送り元のポートが変わってしまう（Linux の変換表の衝突。家庭のルーターはふつう黙って捨てる）
        ip netns exec "r$side" nft -f - <<EOF
table ip filter {
    chain prerouting {
        type filter hook prerouting priority -150; policy accept;
        iifname "wan" ct state new drop
    }
    chain forward {
        type filter hook forward priority 0; policy drop;
        ct state established,related accept
        iifname "lan" accept
    }
}
EOF
        nat "$side" easy
    done

    # 協力ノード（中継つき）
    ip netns exec node "$BUILD/shogiboardq-peer" --service --relay --network sbq-limited-1 \
        --identity "$WORK/node.key" \
        --listen "/ip4/0.0.0.0/tcp/$NODE_PORT" --listen "/ip4/0.0.0.0/udp/$NODE_PORT/quic-v1" \
        > "$WORK/node.log" 2>&1 &
    for _ in $(seq 1 40); do grep -q 'service started as' "$WORK/node.log" && break; sleep 0.25; done
    local peer
    peer=$(sed -n 's/.*service started as //p' "$WORK/node.log" | head -1)
    [ -n "$peer" ] || { cat "$WORK/node.log" >&2; exit 1; }
    # TCP と QUIC の宛先（QUIC は待受のソケットから接続するので、ルーターが送り元のポートを保てばホールパンチングが通る）
    printf '/ip4/192.88.99.1/udp/%s/quic-v1/p2p/%s\n/ip4/192.88.99.1/tcp/%s/p2p/%s\n' \
        "$NODE_PORT" "$peer" "$NODE_PORT" "$peer" > "$WORK/node.addr"
    cat "$WORK/node.addr"

    Xvfb :99 -screen 0 "${SBQ_SCREEN:-1920x1440x24}" -nolisten tcp > "$WORK/xvfb.log" 2>&1 &
    for _ in $(seq 1 40); do [ -S /tmp/.X11-unix/X99 ] && break; sleep 0.25; done
}

# easy: 送り元のポートをなるべく保つ（家庭のルーターに多い。ホールパンチングが通る）
# strict: 接続ごとにポートを変える（ホールパンチングが通らず、中継のまま）
nat() {
    local side=$1 kind=$2 flag=""
    [ "$kind" = strict ] && flag="fully-random"
    ip netns exec "r$side" nft delete table ip nat 2>/dev/null || true
    ip netns exec "r$side" nft -f - <<EOF
table ip nat {
    chain post {
        type nat hook postrouting priority 100;
        oifname "wan" masquerade $flag
    }
}
EOF
    ip netns exec "r$side" conntrack -F 2>/dev/null || true
}

player() {
    local name=$1 language=$2 side=$3 w=${4:-1300} h=${5:-980}
    local cfg=$WORK/cfg-$name loc locpath=()
    mkdir -p "$cfg/ShogiBoardQ" "$WORK/cache-$name" "$WORK/data-$name" "$WORK/state-$name"
    [ -f "$WORK/kdeglobals" ] && cp "$WORK/kdeglobals" "$cfg/kdeglobals"
    if [ ! -f "$cfg/ShogiBoardQ/ShogiBoardQ.ini" ]; then
        # P2P対局のウィンドウはガイドのほかの画像と同じ大きさ（900x790）
        printf '[%%General]\nlanguage=%s\n\n[SizeRelated]\nmainWindowSize=@Size(%s %s)\np2pWindowSize=@Size(900 790)\n' "$language" "$w" "$h" \
            > "$cfg/ShogiBoardQ/ShogiBoardQ.ini"
    fi
    case $language in
        en) loc=en_US.UTF-8 ;;
        zh_CN|zh_TW)
            loc=$language.UTF-8
            if [ ! -d "$WORK/locale/$loc" ]; then
                mkdir -p "$WORK/locale"
                localedef -i "$language" -f UTF-8 "$WORK/locale/$loc" 2>/dev/null || true
            fi
            locpath=(LOCPATH="$WORK/locale:/usr/lib/locale" LANGUAGE="$language") ;;
        *) loc=ja_JP.UTF-8 ;;
    esac
    rm -f "$WORK/run/$name.sock"
    ip netns exec "p$side" env -i PATH=/usr/bin HOME="$WORK/home-$name" \
        DISPLAY=:99 QT_QPA_PLATFORM=xcb QT_QPA_PLATFORMTHEME=kde XDG_CURRENT_DESKTOP=KDE \
        XDG_CONFIG_HOME="$cfg" XDG_CACHE_HOME="$WORK/cache-$name" XDG_DATA_HOME="$WORK/data-$name" \
        SHOGIBOARDQ_CONFIG_HOME="$cfg" SHOGIBOARDQ_CACHE_HOME="$WORK/cache-$name" SHOGIBOARDQ_DATA_HOME="$WORK/data-$name" \
        XDG_STATE_HOME="$WORK/state-$name" XDG_RUNTIME_DIR="$WORK/run" LANG="$loc" LC_ALL="$loc" "${locpath[@]}" \
        "$BUILD/ShogiBoardQ" --automation --automation-socket "$WORK/run/$name.sock" > "$WORK/app-$name.log" 2>&1 &
    for _ in $(seq 1 60); do [ -S "$WORK/run/$name.sock" ] && break; sleep 0.5; done
    [ -S "$WORK/run/$name.sock" ] || { cat "$WORK/app-$name.log" >&2; exit 1; }
    echo "$WORK/run/$name.sock"
}

case ${1:-} in
    up) up ;;
    player) shift; player "$@" ;;
    nat) shift; nat "$@" ;;
    *) sed -n '2,16p' "$0"; exit 2 ;;
esac
