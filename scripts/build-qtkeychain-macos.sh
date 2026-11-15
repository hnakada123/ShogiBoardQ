#!/usr/bin/env bash
#
# macOS 用の QtKeychain を、アプリと同じ Qt でソースからビルドする。
#
# P2P対局のプレイヤー鍵を macOS のキーチェーンに置くのに使う（無ければ、プレイヤー鍵の保管で
# 「OSの鍵の保管庫」を選べない）。Homebrew の qtkeychain は Homebrew の Qt 用で、Qt Online Installer の
# Qt と混ぜられないので、公式のソースを SHA-256 で確かめて、静的ライブラリとしてビルドする
# （アプリに取り込まれるので、バンドルに同梱する動的ライブラリは増えない）。
# 詳細: docs/dev/macos-build-and-release.md 3.4節
#
# Usage:
#   ./scripts/build-qtkeychain-macos.sh [OPTIONS]
#
# Options:
#   --qt-prefix DIR           ビルドに使う Qt の場所（例: ~/Qt/6.12.0/macos）。
#                             省くと CMake が見つける Qt（CMAKE_PREFIX_PATH・PATH）
#   --deployment-target VER   最小対応 macOS バージョン（既定: 環境変数 MACOSX_DEPLOYMENT_TARGET、
#                             未設定なら CMake の既定）
#   --work-dir DIR            ソースの展開とビルドに使うフォルダ（既定: build-qtkeychain）。
#                             インストール先は <DIR>/install
#   --help                    このヘルプを表示
#
# 終わると、アプリの構成に渡すオプション（-DQt6Keychain_DIR=...）を標準出力の最後の行に書く。

set -euo pipefail

# 版を上げるときは、https://github.com/frankosterfeld/qtkeychain/releases の版と、その
# ソース（archive/refs/tags/<版>.tar.gz）の SHA-256 に替える。ライセンスの文書
# （resources/licenses/NOTICE*.md の QtKeychain の節）の著作権表示も確かめる
QTKEYCHAIN_VERSION="0.18.0"
QTKEYCHAIN_SHA256="837af59f2b950760a73eff81ccae817f7d3051183b447438dc610bb984a41691"
QTKEYCHAIN_URL="https://github.com/frankosterfeld/qtkeychain/archive/refs/tags/${QTKEYCHAIN_VERSION}.tar.gz"

OPT_QT_PREFIX=""
OPT_DEPLOYMENT_TARGET="${MACOSX_DEPLOYMENT_TARGET:-}"
OPT_WORK_DIR=""

# 進み具合は標準エラーへ書く（標準出力は最後の1行だけ）
info() { printf '\033[1;34m==>\033[0m %s\n' "$*" >&2; }
die()  { printf '\033[1;31m==> ERROR:\033[0m %s\n' "$*" >&2; exit 1; }

while [[ $# -gt 0 ]]; do
    case "$1" in
        --qt-prefix)
            [[ $# -ge 2 ]] || die "--qt-prefix には Qt の場所を指定してください"
            OPT_QT_PREFIX="$2"
            shift
            ;;
        --deployment-target)
            [[ $# -ge 2 ]] || die "--deployment-target にはバージョンを指定してください"
            OPT_DEPLOYMENT_TARGET="$2"
            shift
            ;;
        --work-dir)
            [[ $# -ge 2 ]] || die "--work-dir にはフォルダを指定してください"
            OPT_WORK_DIR="$2"
            shift
            ;;
        --help)
            sed -n '3,23p' "$0" | sed 's/^# \{0,1\}//'
            exit 0
            ;;
        *) die "Unknown option: $1 (--help でヘルプを表示)" ;;
    esac
    shift
done

[[ "$(uname -s)" == "Darwin" ]] || die "このスクリプトは macOS 用です。"
for tool in cmake ninja curl shasum tar; do
    command -v "$tool" &>/dev/null || die "${tool} が見つかりません。"
done

PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
if [[ -z "$OPT_WORK_DIR" ]]; then
    OPT_WORK_DIR="${PROJECT_ROOT}/build-qtkeychain"
fi
mkdir -p "$OPT_WORK_DIR"
WORK_DIR="$(cd "$OPT_WORK_DIR" && pwd)"
if [[ -n "$OPT_QT_PREFIX" ]]; then
    [[ -d "$OPT_QT_PREFIX/lib/cmake/Qt6" ]] || die "Qt が見つかりません: ${OPT_QT_PREFIX}/lib/cmake/Qt6"
    OPT_QT_PREFIX="$(cd "$OPT_QT_PREFIX" && pwd)"
fi

ARCHIVE="${WORK_DIR}/qtkeychain-${QTKEYCHAIN_VERSION}.tar.gz"
SOURCE_DIR="${WORK_DIR}/qtkeychain-${QTKEYCHAIN_VERSION}"
BUILD_DIR="${WORK_DIR}/build"
INSTALL_DIR="${WORK_DIR}/install"
CONFIG_DIR="${INSTALL_DIR}/lib/cmake/Qt6Keychain"
STAMP_FILE="${INSTALL_DIR}/.sbq-stamp"
# 同じ条件でビルド済みなら作り直さない
STAMP="qtkeychain=${QTKEYCHAIN_VERSION} sha256=${QTKEYCHAIN_SHA256} qt=${OPT_QT_PREFIX:-default} target=${OPT_DEPLOYMENT_TARGET:-default} arch=$(uname -m)"

if [[ -f "$STAMP_FILE" && "$(cat "$STAMP_FILE")" == "$STAMP" && -f "${CONFIG_DIR}/Qt6KeychainConfig.cmake" ]]; then
    info "QtKeychain ${QTKEYCHAIN_VERSION} はビルド済みです: ${INSTALL_DIR}"
else
    checksum_ok() {
        [[ -f "$ARCHIVE" ]] && [[ "$(shasum -a 256 "$ARCHIVE" | awk '{ print $1 }')" == "$QTKEYCHAIN_SHA256" ]]
    }
    if ! checksum_ok; then
        info "QtKeychain ${QTKEYCHAIN_VERSION} のソースを取得中..."
        curl -fsSL --retry 5 -o "$ARCHIVE" "$QTKEYCHAIN_URL"
        checksum_ok || die "QtKeychain のソースの SHA-256 が合いません: ${ARCHIVE}"
    fi

    rm -rf "$SOURCE_DIR" "$BUILD_DIR" "$INSTALL_DIR"
    tar -xzf "$ARCHIVE" -C "$WORK_DIR"
    [[ -f "${SOURCE_DIR}/CMakeLists.txt" ]] || die "QtKeychain のソースを展開できません: ${SOURCE_DIR}"

    info "QtKeychain ${QTKEYCHAIN_VERSION} をビルド中（静的ライブラリ）..."
    CMAKE_ARGS=(
        -B "$BUILD_DIR"
        -S "$SOURCE_DIR"
        -G Ninja
        -DCMAKE_BUILD_TYPE=Release
        "-DCMAKE_INSTALL_PREFIX=${INSTALL_DIR}"
        -DBUILD_SHARED_LIBS=OFF
        -DBUILD_TRANSLATIONS=OFF
        -DBUILD_TEST_APPLICATION=OFF
        -DBUILD_TESTING=OFF
    )
    if [[ -n "$OPT_QT_PREFIX" ]]; then
        CMAKE_ARGS+=("-DCMAKE_PREFIX_PATH=${OPT_QT_PREFIX}")
    fi
    if [[ -n "$OPT_DEPLOYMENT_TARGET" ]]; then
        CMAKE_ARGS+=("-DCMAKE_OSX_DEPLOYMENT_TARGET=${OPT_DEPLOYMENT_TARGET}")
    fi
    cmake "${CMAKE_ARGS[@]}" >&2
    cmake --build "$BUILD_DIR" >&2
    cmake --install "$BUILD_DIR" >&2
    [[ -f "${CONFIG_DIR}/Qt6KeychainConfig.cmake" ]] \
        || die "QtKeychain の CMake の設定ファイルがありません: ${CONFIG_DIR}"
    printf '%s\n' "$STAMP" > "$STAMP_FILE"
    info "QtKeychain ${QTKEYCHAIN_VERSION}: ${INSTALL_DIR}"
fi

info "アプリの構成に次のオプションを付けてください:"
printf '%s\n' "-DQt6Keychain_DIR=${CONFIG_DIR}"
