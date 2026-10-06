#!/usr/bin/env bash
#
# 古い Arch Linux のコンテナで Linux 版（AppImage / ZIP）を作る。
#
# 最新の Arch でビルドすると AppImage が新しい glibc を必要とし、Ubuntu 24.04 などで起動できない。
# このスクリプトは scripts/linux-container/Dockerfile の環境（2024-07-15 の Arch: glibc 2.39・Qt 6.7.2）で
# scripts/build-linux.sh を実行する。ビルドディレクトリは普段の build と分けて build-container を使い、
# 出力（ShogiBoardQ-linux-x86_64.AppImage / ShogiBoardQ-linux.zip）はリポジトリ直下に置く。
# 詳細: docs/dev/linux-build-and-release.md
#
# Usage:
#   ./scripts/build-linux-container.sh [build-linux.sh のオプション]
#
# 事前準備:
#   - docker を使えること（docker グループに入っているか、DOCKER="sudo docker" を指定する）
#   - コンテナの Qt（6.7.2）と同じ版の Qt 文書を build-container/qt-licenses に準備する
#     （docs/dev/linux-build-and-release.md の「Qt のライセンス文書の準備」）
#
# 環境変数:
#   DOCKER                      docker コマンド（既定 docker）
#   SHOGIBOARDQ_ARCH_SNAPSHOT   Arch Linux Archive の日付（既定 2024/07/15）
#   SHOGIBOARDQ_ARCH_IMAGE      元にするイメージ（既定 archlinux:base-devel-20240714.0.246936）

set -euo pipefail

SNAPSHOT="${SHOGIBOARDQ_ARCH_SNAPSHOT:-2024/07/15}"
BASE_IMAGE="${SHOGIBOARDQ_ARCH_IMAGE:-archlinux:base-devel-20240714.0.246936}"
IMAGE="shogiboardq-build:arch-${SNAPSHOT//\//}"
BUILD_DIR="build-container"
read -r -a DOCKER_CMD <<< "${DOCKER:-docker}"

info() { printf '\033[1;34m==>\033[0m %s\n' "$*"; }
die()  { printf '\033[1;31m==> ERROR:\033[0m %s\n' "$*" >&2; exit 1; }

cd "$(dirname "$0")/.."
"${DOCKER_CMD[@]}" info >/dev/null 2>&1 \
    || die "docker を使えません。docker グループに入るか、DOCKER=\"sudo docker\" を指定してください。"
[[ -f "$BUILD_DIR/qt-licenses/QT-SOURCE.json" ]] \
    || die "$BUILD_DIR/qt-licenses に Qt 文書がありません。コンテナの Qt と同じ版の文書を準備してください。"

if ! "${DOCKER_CMD[@]}" image inspect "$IMAGE" >/dev/null 2>&1; then
    info "ビルド環境のイメージを作成中: $IMAGE（Arch Linux Archive $SNAPSHOT）"
    "${DOCKER_CMD[@]}" build -t "$IMAGE" --build-arg BASE_IMAGE="$BASE_IMAGE" --build-arg SNAPSHOT="$SNAPSHOT" \
        scripts/linux-container
fi

# 自分のユーザーで実行し、作られるファイルの所有者をそろえる。FUSE は使えないので AppImage 形式の
# ツールは展開して実行する。
# イメージの時間帯は UTC なので、手元の時間帯を渡してビルド日時を手元と同じ時刻で記録する。
TZ_ARGS=()
[[ -e /etc/localtime ]] && TZ_ARGS=(-v /etc/localtime:/etc/localtime:ro)
info "コンテナでビルド中..."
"${DOCKER_CMD[@]}" run --rm --user "$(id -u):$(id -g)" \
    -e HOME=/tmp/home -e SHOGIBOARDQ_BUILD_DIR="$BUILD_DIR" -e APPIMAGE_EXTRACT_AND_RUN=1 \
    "${TZ_ARGS[@]}" -v "$PWD:/src" -w /src "$IMAGE" \
    bash -c 'mkdir -p "$HOME" && exec ./scripts/build-linux.sh "$@"' build-linux "$@"
