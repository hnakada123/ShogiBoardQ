#!/usr/bin/env bash
#
# macOS ビルドスクリプト for ShogiBoardQ
#
# Release ビルド → macdeployqt → DMG 作成を一括実行する。
# 詳細: docs/dev/macos-build-and-release.md
#
# Usage:
#   ./scripts/build-macos.sh [OPTIONS]
#
# Options:
#   --universal   Universal Binary (arm64 + x86_64) をビルド
#   --skip-dmg    DMG 作成をスキップ（.app のみ）
#   --clean       build ディレクトリを削除してからビルド
#   --help        このヘルプを表示

set -euo pipefail

# ──────────────────────────────────────────────
# 定数
# ──────────────────────────────────────────────

APP_NAME="ShogiBoardQ"
BUILD_DIR="build"
DMG_NAME="${APP_NAME}.dmg"
APP_BUNDLE="${BUILD_DIR}/${APP_NAME}.app"
ICON_PATH="resources/icons/shogiboardq.icns"

# ──────────────────────────────────────────────
# デフォルトオプション
# ──────────────────────────────────────────────

OPT_UNIVERSAL=false
OPT_SKIP_DMG=false
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
Usage: ./scripts/build-macos.sh [OPTIONS]

macOS 用の Release ビルド〜DMG 作成を一括実行するスクリプト。

Options:
  --universal   Universal Binary (arm64 + x86_64) をビルド
  --skip-dmg    DMG 作成をスキップ（.app バンドルのみ生成）
  --clean       build ディレクトリを削除してからビルド
  --help        このヘルプを表示

Examples:
  # 通常ビルド + DMG 作成
  ./scripts/build-macos.sh

  # Universal Binary でビルド
  ./scripts/build-macos.sh --universal

  # クリーンビルド、DMG なし
  ./scripts/build-macos.sh --clean --skip-dmg
EOF
}

# ──────────────────────────────────────────────
# 引数パース
# ──────────────────────────────────────────────

while [[ $# -gt 0 ]]; do
    case "$1" in
        --universal) OPT_UNIVERSAL=true ;;
        --skip-dmg)  OPT_SKIP_DMG=true ;;
        --clean)     OPT_CLEAN=true ;;
        --help)      usage; exit 0 ;;
        *)           die "Unknown option: $1 (--help でヘルプを表示)" ;;
    esac
    shift
done

# ──────────────────────────────────────────────
# Step 1: 前提チェック
# ──────────────────────────────────────────────

info "前提ツールを確認中..."

REQUIRED_TOOLS=(cmake ninja macdeployqt)
if [[ "$OPT_SKIP_DMG" = false ]]; then
    REQUIRED_TOOLS+=(create-dmg)
fi

MISSING_TOOLS=()
for tool in "${REQUIRED_TOOLS[@]}"; do
    if ! command -v "$tool" &>/dev/null; then
        MISSING_TOOLS+=("$tool")
    fi
done

if [[ ${#MISSING_TOOLS[@]} -gt 0 ]]; then
    die "以下のツールが見つかりません: ${MISSING_TOOLS[*]}
  brew install cmake ninja create-dmg
  Qt の macdeployqt は PATH に含まれている必要があります。"
fi

info "cmake:       $(cmake --version | head -1)"
info "ninja:       $(ninja --version)"
info "macdeployqt: $(command -v macdeployqt)"
if [[ "$OPT_SKIP_DMG" = false ]]; then
    info "create-dmg:  $(command -v create-dmg)"
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

info "CMake Configure (Release, Ninja)..."

CMAKE_ARGS=(
    -B "$BUILD_DIR"
    -S .
    -G Ninja
    -DCMAKE_BUILD_TYPE=Release
)

if [[ "$OPT_UNIVERSAL" = true ]]; then
    info "Universal Binary モード: arm64 + x86_64"
    CMAKE_ARGS+=("-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64")
fi

cmake "${CMAKE_ARGS[@]}"

# ──────────────────────────────────────────────
# Step 5: ビルド
# ──────────────────────────────────────────────

info "ビルド中..."
ninja -C "$BUILD_DIR"

# ──────────────────────────────────────────────
# Step 6: ビルド成果物の確認
# ──────────────────────────────────────────────

info "ビルド成果物を確認中..."

if [[ ! -d "$APP_BUNDLE" ]]; then
    die "${APP_BUNDLE} が見つかりません。ビルドに失敗した可能性があります。"
fi

# 翻訳ファイルの確認
QM_COUNT=$(find "$APP_BUNDLE" -name "*.qm" 2>/dev/null | wc -l | tr -d ' ')
if [[ "$QM_COUNT" -eq 0 ]]; then
    warn ".qm 翻訳ファイルがバンドル内に見つかりません。"
    warn "翻訳が正しくビルドされているか確認してください。"
else
    info "翻訳ファイル: ${QM_COUNT} 個の .qm ファイルを検出"
fi

# ──────────────────────────────────────────────
# Step 7: macdeployqt
# ──────────────────────────────────────────────

info "macdeployqt でフレームワークをバンドル中..."
macdeployqt "$APP_BUNDLE" -verbose=2

# ──────────────────────────────────────────────
# Step 8: macdeployqt 後の検証
# ──────────────────────────────────────────────

info "バンドルを検証中..."

# Frameworks ディレクトリの確認
if [[ ! -d "$APP_BUNDLE/Contents/Frameworks" ]]; then
    die "Frameworks ディレクトリが存在しません。macdeployqt が失敗した可能性があります。"
fi

# PlugIns ディレクトリの確認
if [[ ! -d "$APP_BUNDLE/Contents/PlugIns" ]]; then
    warn "PlugIns ディレクトリが存在しません。"
fi

# 外部パスの残存チェック
EXTERNAL_DEPS=$(otool -L "$APP_BUNDLE/Contents/MacOS/$APP_NAME" 2>/dev/null \
    | grep -E '(/usr/local/|/opt/homebrew/|/Users/)' \
    | grep -v '@' || true)

if [[ -n "$EXTERNAL_DEPS" ]]; then
    warn "外部パスへの依存が残っています:"
    echo "$EXTERNAL_DEPS"
    warn "配布用バンドルとして不完全な可能性があります。"
fi

info "Frameworks: $(ls "$APP_BUNDLE/Contents/Frameworks/" | wc -l | tr -d ' ') 個"
info "PlugIns:    $(find "$APP_BUNDLE/Contents/PlugIns" -type f 2>/dev/null | wc -l | tr -d ' ') 個"

# ──────────────────────────────────────────────
# Step 9: DMG 作成
# ──────────────────────────────────────────────

if [[ "$OPT_SKIP_DMG" = true ]]; then
    info "DMG 作成をスキップしました。"
    info "出力: $APP_BUNDLE"
    exit 0
fi

info "DMG を作成中..."

# 既存の DMG を削除
if [[ -f "$DMG_NAME" ]]; then
    info "既存の ${DMG_NAME} を削除中..."
    rm -f "$DMG_NAME"
fi

create-dmg \
    --volname "$APP_NAME" \
    --volicon "$ICON_PATH" \
    --window-pos 200 120 \
    --window-size 600 400 \
    --icon-size 100 \
    --icon "${APP_NAME}.app" 150 190 \
    --hide-extension "${APP_NAME}.app" \
    --app-drop-link 450 190 \
    "$DMG_NAME" \
    "$APP_BUNDLE"

# ──────────────────────────────────────────────
# 完了
# ──────────────────────────────────────────────

info "ビルド完了!"
info "アプリバンドル: $APP_BUNDLE"
info "DMG ファイル:   $DMG_NAME"
