#!/usr/bin/env bash
#
# Linux ビルドスクリプト for ShogiBoardQ
#
# Release ビルド → linuxdeploy → AppImage 作成を一括実行する。
# 詳細: docs/dev/linux-build-and-release.md
#
# Usage:
#   ./scripts/build-linux.sh [OPTIONS]
#
# Options:
#   --skip-appimage   AppImage 作成をスキップ（AppDir のみ）
#   --clean           build ディレクトリを削除してからビルド
#   --help            このヘルプを表示
#
# Prerequisites:
#   - GCC / Clang（C++17 対応）
#   - Qt 6.x (Widgets, Charts, Network, LinguistTools)
#   - CMake 3.16+
#   - Ninja（推奨）

set -euo pipefail

# ──────────────────────────────────────────────
# 定数
# ──────────────────────────────────────────────

APP_NAME="ShogiBoardQ"
BUILD_DIR="build"
APPDIR="${BUILD_DIR}/AppDir"
APPIMAGE_NAME="${APP_NAME}-x86_64.AppImage"
ICON_PATH="resources/icons/shogiboardq.png"
DESKTOP_PATH="resources/platform/shogiboardq.desktop"

LINUXDEPLOY_URL="https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage"
LINUXDEPLOY_QT_URL="https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage"
LINUXDEPLOY="${BUILD_DIR}/linuxdeploy-x86_64.AppImage"
LINUXDEPLOY_QT="${BUILD_DIR}/linuxdeploy-plugin-qt-x86_64.AppImage"

# ──────────────────────────────────────────────
# デフォルトオプション
# ──────────────────────────────────────────────

OPT_SKIP_APPIMAGE=false
OPT_CLEAN=false

# ──────────────────────────────────────────────
# ヘルパー関数
# ──────────────────────────────────────────────

info()  { printf '\033[1;34m==>\033[0m %s\n' "$*"; }
warn()  { printf '\033[1;33m==> WARNING:\033[0m %s\n' "$*"; }
error() { printf '\033[1;31m==> ERROR:\033[0m %s\n' "$*" >&2; }
die()   { error "$*"; exit 1; }

usage() {
    cat <<'EOF'
Usage: ./scripts/build-linux.sh [OPTIONS]

Linux 用の Release ビルド〜AppImage 作成を一括実行するスクリプト。

Options:
  --skip-appimage   AppImage 作成をスキップ（ビルドのみ）
  --clean           build ディレクトリを削除してからビルド
  --help            このヘルプを表示

Examples:
  # 通常ビルド + AppImage 作成
  ./scripts/build-linux.sh

  # クリーンビルド、AppImage なし
  ./scripts/build-linux.sh --clean --skip-appimage
EOF
}

# ──────────────────────────────────────────────
# 引数パース
# ──────────────────────────────────────────────

while [[ $# -gt 0 ]]; do
    case "$1" in
        --skip-appimage) OPT_SKIP_APPIMAGE=true ;;
        --clean)         OPT_CLEAN=true ;;
        --help)          usage; exit 0 ;;
        *)               die "Unknown option: $1 (--help でヘルプを表示)" ;;
    esac
    shift
done

# ──────────────────────────────────────────────
# Step 1: 前提チェック
# ──────────────────────────────────────────────

info "前提ツールを確認中..."

REQUIRED_TOOLS=(cmake)
MISSING_TOOLS=()
for tool in "${REQUIRED_TOOLS[@]}"; do
    if ! command -v "$tool" &>/dev/null; then
        MISSING_TOOLS+=("$tool")
    fi
done

if [[ ${#MISSING_TOOLS[@]} -gt 0 ]]; then
    die "以下のツールが見つかりません: ${MISSING_TOOLS[*]}"
fi

info "cmake: $(cmake --version | head -1)"

# Ninja チェック（推奨）
if command -v ninja &>/dev/null; then
    USE_NINJA=true
    info "ninja: $(ninja --version)"
else
    USE_NINJA=false
    warn "ninja が見つかりません。make を使用します。"
    warn "高速ビルドには ninja の使用を推奨します。"
fi

# ──────────────────────────────────────────────
# Step 2: プロジェクトルートへ移動
# ──────────────────────────────────────────────

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$PROJECT_ROOT"
info "プロジェクトルート: $PROJECT_ROOT"

# ──────────────────────────────────────────────
# Step 3: クリーン（オプション）
# ──────────────────────────────────────────────

if [[ "$OPT_CLEAN" = true ]]; then
    info "build ディレクトリを削除中..."
    rm -rf "$BUILD_DIR"
fi

# ──────────────────────────────────────────────
# Step 4: CMake Configure
# ──────────────────────────────────────────────

info "CMake Configure (Release)..."

CMAKE_ARGS=(
    -B "$BUILD_DIR"
    -S .
    -DCMAKE_BUILD_TYPE=Release
    -DCMAKE_INSTALL_PREFIX=/usr
)

if [[ "$USE_NINJA" = true ]]; then
    CMAKE_ARGS+=(-G Ninja)
fi

cmake "${CMAKE_ARGS[@]}"

# ──────────────────────────────────────────────
# Step 5: ビルド
# ──────────────────────────────────────────────

info "ビルド中..."

if [[ "$USE_NINJA" = true ]]; then
    ninja -C "$BUILD_DIR"
else
    cmake --build "$BUILD_DIR" --config Release -- -j"$(nproc)"
fi

# ──────────────────────────────────────────────
# Step 6: ビルド成果物の確認
# ──────────────────────────────────────────────

info "ビルド成果物を確認中..."

EXE_PATH="${BUILD_DIR}/${APP_NAME}"
if [[ ! -f "$EXE_PATH" ]]; then
    die "${EXE_PATH} が見つかりません。ビルドに失敗した可能性があります。"
fi

info "実行ファイル: $EXE_PATH"

# 翻訳ファイルの確認
QM_COUNT=$(find "$BUILD_DIR" -maxdepth 1 -name "*.qm" 2>/dev/null | wc -l | tr -d ' ')
if [[ "$QM_COUNT" -eq 0 ]]; then
    warn ".qm 翻訳ファイルが見つかりません。"
else
    info "翻訳ファイル: ${QM_COUNT} 個の .qm ファイルを検出"
fi

if [[ "$OPT_SKIP_APPIMAGE" = true ]]; then
    info "AppImage 作成をスキップしました。"
    info "出力: $EXE_PATH"
    exit 0
fi

# ──────────────────────────────────────────────
# Step 7: linuxdeploy のダウンロード
# ──────────────────────────────────────────────

info "linuxdeploy を準備中..."

download_if_missing() {
    local url="$1"
    local dest="$2"
    if [[ ! -f "$dest" ]]; then
        info "ダウンロード中: $(basename "$dest")"
        curl -fSL -o "$dest" "$url"
        chmod +x "$dest"
    else
        info "キャッシュ済み: $(basename "$dest")"
    fi
}

download_if_missing "$LINUXDEPLOY_URL" "$LINUXDEPLOY"
download_if_missing "$LINUXDEPLOY_QT_URL" "$LINUXDEPLOY_QT"

# ──────────────────────────────────────────────
# Step 8: AppDir 作成 + linuxdeploy 実行
# ──────────────────────────────────────────────

info "AppImage を作成中..."

# AppDir をクリーン
rm -rf "$APPDIR"

# linuxdeploy で AppImage を作成
# QMAKE 環境変数で Qt のパスを指定（Qt Online Installer の場合）
export EXTRA_QT_PLUGINS="svg;iconengines"
export OUTPUT="$APPIMAGE_NAME"

# .qm ファイルを実行ファイルと同じ場所にデプロイするため、
# まず AppDir/usr/bin/ に手動コピーしてから linuxdeploy を実行
mkdir -p "${APPDIR}/usr/bin"
cp "$EXE_PATH" "${APPDIR}/usr/bin/"

# .qm ファイルをコピー
for qm in "$BUILD_DIR"/*.qm; do
    if [[ -f "$qm" ]]; then
        cp "$qm" "${APPDIR}/usr/bin/"
    fi
done

"$LINUXDEPLOY" \
    --appdir "$APPDIR" \
    --executable "${APPDIR}/usr/bin/${APP_NAME}" \
    --desktop-file "$DESKTOP_PATH" \
    --icon-file "$ICON_PATH" \
    --plugin qt \
    --output appimage

# ──────────────────────────────────────────────
# Step 9: 検証
# ──────────────────────────────────────────────

if [[ ! -f "$APPIMAGE_NAME" ]]; then
    die "AppImage の作成に失敗しました。"
fi

APPIMAGE_SIZE=$(du -h "$APPIMAGE_NAME" | cut -f1)
info "AppImage サイズ: $APPIMAGE_SIZE"

# ──────────────────────────────────────────────
# 完了
# ──────────────────────────────────────────────

info "ビルド完了!"
info "実行ファイル: $EXE_PATH"
info "AppImage:     $APPIMAGE_NAME"
