#!/bin/bash
# インターネット越しの P2P対局を撮るための Docker のコンテナを動かす（ホストで実行する）。
#
#   lab.sh up        コンテナを起動し、中にネットワーク・協力ノード・Xvfb を用意する
#   lab.sh down      コンテナを止める（作業フォルダの持ち主をホストの利用者に戻す）
#   lab.sh exec ...  コンテナの中でコマンドを実行する（例: lab.sh exec /repo/scripts/docs/p2p_internet/inside.sh player ja-A ja_JP A）
#   lab.sh rpc <名前> <メソッド> [JSON]
#                    コンテナの中のアプリの自動化 API を呼ぶ
#
# アプリは撮影用のビルド（cmake -DENABLE_P2P_PLAY=ON -DENABLE_P2P_DOCS_AUTOMATION=ON。既定 build-p2p-docs）を使う。
# コンテナはホストの /usr を読み取り専用で見せるので、ホストでビルドしたアプリ・書体・ロケールがそのまま動く
# （ホストと同じ Arch Linux の基本イメージ archlinux:latest を使う）。ネットワークはコンテナの中だけにあり、
# 外とはつながらない（--network none）。
#
# 環境変数: SBQ_P2P_WORK（作業フォルダ。既定 /tmp/sbq-p2p-lab-<uid>）、SBQ_BUILD（ビルドのフォルダ名）
set -euo pipefail
REPO=$(cd "$(dirname "$0")/../../.." && pwd)
WORK=${SBQ_P2P_WORK:-/tmp/sbq-p2p-lab-$(id -u)}
NAME=sbq-p2p-lab
INSIDE=/repo/scripts/docs/p2p_internet/inside.sh

case ${1:-} in
    up)
        mkdir -p "$WORK"
        cp ~/.config/kdeglobals "$WORK/kdeglobals" 2>/dev/null || true
        docker run -d --rm --name "$NAME" --privileged --network none \
            -v /usr:/usr:ro -v /etc/fonts:/etc/fonts:ro -v /etc/localtime:/etc/localtime:ro \
            -v "$REPO:/repo:ro" -v "$WORK:/work" \
            -e SBQ_BUILD="${SBQ_BUILD:-build-p2p-docs}" \
            archlinux:latest sleep infinity > /dev/null
        docker exec "$NAME" "$INSIDE" up
        ;;
    down)
        docker exec "$NAME" chown -R "$(id -u):$(id -g)" /work 2>/dev/null || true
        docker stop -t 2 "$NAME" > /dev/null
        ;;
    exec)
        shift
        docker exec "$NAME" "$@"
        ;;
    rpc)
        shift
        docker exec "$NAME" python3 /repo/scripts/docs/rpc.py "/work/run/$1.sock" "${@:2}"
        ;;
    *)
        sed -n '2,15p' "$0"; exit 2 ;;
esac
