#!/usr/bin/env bash
#
# Linux ビルドスクリプト for ShogiBoardQ
#
# Release ビルド → linuxdeploy → AppImage / ZIP 作成を一括実行する。
# 詳細: docs/dev/linux-build-and-release.md
#
# Usage:
#   ./scripts/build-linux.sh [OPTIONS]
#
# Options:
#   --skip-appimage   配布ファイル作成をスキップ（ビルドのみ）
#   --clean           build ディレクトリを削除してからビルド
#   --help            このヘルプを表示
#
# Prerequisites:
#   - GCC / Clang（C++17 対応）
#   - Qt 6.x (Widgets, Charts, Network, Multimedia, LinguistTools)
#   - CMake 3.16+
#   - Ninja（推奨）

set -euo pipefail

# ──────────────────────────────────────────────
# 定数
# ──────────────────────────────────────────────

APP_NAME="ShogiBoardQ"
# ビルドディレクトリ（コンテナでのビルドなど、普段の build と分けるときは SHOGIBOARDQ_BUILD_DIR で指定する）
BUILD_DIR="${SHOGIBOARDQ_BUILD_DIR:-build}"
APPDIR="${BUILD_DIR}/AppDir"
APPIMAGE_NAME="${APP_NAME}-linux-x86_64.AppImage"
PACKAGE_DIR="${BUILD_DIR}/${APP_NAME}-linux"
ZIP_NAME="${APP_NAME}-linux.zip"
HAYANAGI_EXE="${BUILD_DIR}/Hayanagi/hayanagi"
ICON_PATH="resources/icons/linux/shogiboardq.png"
DESKTOP_PATH="resources/platform/shogiboardq.desktop"

LINUXDEPLOY_URL="https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage"
LINUXDEPLOY_QT_URL="https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage"
APPIMAGETOOL_URL="https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-x86_64.AppImage"
LINUXDEPLOY="${BUILD_DIR}/linuxdeploy-x86_64.AppImage"
LINUXDEPLOY_QT="${BUILD_DIR}/linuxdeploy-plugin-qt-x86_64.AppImage"
APPIMAGETOOL="${BUILD_DIR}/appimagetool-x86_64.AppImage"

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

Linux 用の Release ビルド〜AppImage / ZIP 作成を一括実行するスクリプト。

Options:
  --skip-appimage   配布ファイル作成をスキップ（ビルドのみ）
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

REQUIRED_TOOLS=(cmake python3)
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

# 「バージョン情報」のビルド日時（__DATE__・__TIME__）は versiondialog.cpp をコンパイルした時刻。
# 差分ビルドでも今回の時刻になるように、そのオブジェクトだけ作り直させる
find "$BUILD_DIR" -name 'versiondialog.cpp.o' -delete

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

# 外部 USI エンジンと問題集も毎回 ZIP に同梱する。欠落した配布物は作らない。
[[ -x "$HAYANAGI_EXE" ]] || die "Hayanagi が見つかりません: $HAYANAGI_EXE"
TSUME_FILES=()
for plies in 3 5 7 9 11 13; do
    collections=(data/tsumeshogi/tsume_"${plies}"ply_*.txt)
    [[ -f "${collections[0]}" ]] || die "${plies}手詰の問題集が見つかりません。"
    # 旧版などが残っていると ZIP に混ざるため、各手数1ファイルに限る。
    [[ ${#collections[@]} -eq 1 ]] || die "${plies}手詰の問題集が複数あります: ${collections[*]}"
    TSUME_FILES+=("${collections[0]}")
done

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
download_if_missing "$APPIMAGETOOL_URL" "$APPIMAGETOOL"

# ──────────────────────────────────────────────
# Step 8: AppDir 作成 + linuxdeploy 実行
# ──────────────────────────────────────────────

info "AppImage を作成中..."

# AppDir をクリーン
rm -rf "$APPDIR"

# linuxdeploy で AppImage を作成
# Qt6 の qmake を明示指定（Qt5 が混入するのを防ぐ）
QMAKE6="$(command -v qmake6 2>/dev/null || command -v qmake-qt6 2>/dev/null || true)"
if [[ -z "$QMAKE6" ]]; then
    die "qmake6 が見つかりません。qt6-base-dev をインストールしてください。"
fi
info "qmake: $QMAKE6"

# Step 8a: フィルタ済み Qt プラグインディレクトリを作成
# KDE 画像フォーマットプラグイン (kimg_*) 等は依存ライブラリが不足している
# 場合があり、linuxdeploy-plugin-qt がエラー終了する。
# 問題のあるプラグインを除外したディレクトリを作成し、qmake ラッパー経由で
# linuxdeploy-plugin-qt に参照させる。
info "フィルタ済み Qt プラグインディレクトリを作成中..."
QT_PLUGINS_SRC="$("$QMAKE6" -query QT_INSTALL_PLUGINS)"
FILTERED_PLUGINS="$(pwd)/${BUILD_DIR}/qt-plugins-filtered"
rm -rf "$FILTERED_PLUGINS"

# 必要なプラグインのみをホワイトリストで選択
link_plugins() {
    local category="$1"
    shift
    local src_dir="$QT_PLUGINS_SRC/$category"
    [[ -d "$src_dir" ]] || return 0
    mkdir -p "$FILTERED_PLUGINS/$category"
    for name in "$@"; do
        [[ -f "$src_dir/$name" ]] && ln -s "$src_dir/$name" "$FILTERED_PLUGINS/$category/"
    done
}

# platforms: X11 のみ
link_plugins platforms libqxcb.so

# xcbglintegrations（OpenGL 統合）は同梱しない。OpenGL を使う画面部品（QOpenGLWidget 等）がないため。

# imageformats: 駒・盤の SVG、盤面画像の JPEG 保存、ウィンドウアイコンの ICO
link_plugins imageformats libqjpeg.so libqsvg.so libqico.so

# iconengines: SVG アイコン
link_plugins iconengines libqsvgicon.so

# platforminputcontexts: 日本語入力（compose, fcitx5, ibus）
link_plugins platforminputcontexts \
    libcomposeplatforminputcontextplugin.so \
    libfcitx5platforminputcontextplugin.so \
    libibusplatforminputcontextplugin.so

# platformthemes: デスクトップ統合
link_plugins platformthemes libqxdgdesktopportal.so

# tls は同梱しない。CSA 通信対局は暗号化しない TCP で、HTTPS 等の通信もしないため。

# 詰将棋の解答履歴・解析キャッシュ
link_plugins sqldrivers libqsqlite.so

# multimedia: 駒音は QSoundEffect（libQt6Multimedia 本体）だけで再生できるため、
# ffmpeg/gstreamer バックエンドプラグインは同梱しない（数十MBの依存を回避）。
# libQt6Multimedia.so.6 は実行ファイルの依存として linuxdeploy が自動で同梱する。

# qmake ラッパーを作成（QT_INSTALL_PLUGINS をフィルタ済みパスに差し替え）
# linuxdeploy-plugin-qt が確実に見つけられるよう絶対パスを使用
QMAKE_WRAPPER="$(pwd)/${BUILD_DIR}/qmake-wrapper"
cat > "$QMAKE_WRAPPER" <<WRAPPER_EOF
#!/bin/bash
if [[ "\$1" == "-query" && -z "\$2" ]]; then
    "$QMAKE6" -query | sed 's|^QT_INSTALL_PLUGINS:.*|QT_INSTALL_PLUGINS:${FILTERED_PLUGINS}|'
elif [[ "\$1" == "-query" && "\$2" == "QT_INSTALL_PLUGINS" ]]; then
    echo "${FILTERED_PLUGINS}"
else
    exec "$QMAKE6" "\$@"
fi
WRAPPER_EOF
chmod +x "$QMAKE_WRAPPER"
export QMAKE="$QMAKE_WRAPPER"

export EXTRA_QT_PLUGINS="svg;iconengines;sqldrivers"

# linuxdeploy 内蔵の strip は古く、最新の .relr.dyn セクションを
# 認識できない場合がある。linuxdeploy の strip をスキップし、
# 後からシステムの strip を手動適用する。
export NO_STRIP=true

# アイコンが 512x512 を超える場合、linuxdeploy が拒否するため
# 512x512 にリサイズしたコピーを使用する
ICON_DEPLOY="$ICON_PATH"
if command -v magick &>/dev/null; then
    ICON_SIZE=$(magick identify -format '%w' "$ICON_PATH" 2>/dev/null || true)
    if [[ -n "$ICON_SIZE" ]] && [[ "$ICON_SIZE" -gt 512 ]]; then
        ICON_DEPLOY="${BUILD_DIR}/shogiboardq.png"
        info "アイコンを 512x512 にリサイズ中..."
        magick "$ICON_PATH" -resize 512x512 "$ICON_DEPLOY"
    fi
elif command -v convert &>/dev/null; then
    ICON_SIZE=$(identify -format '%w' "$ICON_PATH" 2>/dev/null || true)
    if [[ -n "$ICON_SIZE" ]] && [[ "$ICON_SIZE" -gt 512 ]]; then
        ICON_DEPLOY="${BUILD_DIR}/shogiboardq.png"
        info "アイコンを 512x512 にリサイズ中..."
        convert "$ICON_PATH" -resize 512x512 "$ICON_DEPLOY"
    fi
else
    warn "ImageMagick が見つかりません。アイコンサイズの調整をスキップします。"
fi

# .qm ファイルを実行ファイルと同じ場所にデプロイするため、
# まず AppDir/usr/bin/ に手動コピーしてから linuxdeploy を実行
# Qt プラグインは遅延コピー前に翻訳リンクを作るため、配置先も先に作る。
# AppImage にはアプリの実行に必要なファイルだけを入れる。問題集・Hayanagi・説明書は
# AppImage 内からはファイル選択できないため、ZIP の外部ファイルとしてのみ配布する。
mkdir -p "${APPDIR}/usr/bin" "${APPDIR}/usr/translations"
cp "$EXE_PATH" "${APPDIR}/usr/bin/"

# .qm ファイルをコピー
for qm in "$BUILD_DIR"/*.qm; do
    if [[ -f "$qm" ]]; then
        cp "$qm" "${APPDIR}/usr/bin/"
    fi
done

# Step 8b: linuxdeploy で AppDir を作成（AppImage 生成はまだしない）
info "AppDir を構築中..."
"$LINUXDEPLOY" \
    --appdir "$APPDIR" \
    --executable "${APPDIR}/usr/bin/${APP_NAME}" \
    --desktop-file "$DESKTOP_PATH" \
    --icon-file "$ICON_DEPLOY" \
    --plugin qt

# Step 8c: 不要ライブラリを削除
# ホストシステムに存在するライブラリや、不要プラグインが引き込んだ
# 依存ライブラリを除去して AppImage サイズを削減する
info "不要ライブラリを削除中..."
APPDIR_LIB="${APPDIR}/usr/lib"
BEFORE_SIZE=$(du -sm "$APPDIR_LIB" | cut -f1)

# ICU データは同梱の libicuuc と同じバージョンが必要なため保持する。
# fcitx5 の入力プラグインが参照する Qt Wayland / Fcitx5 / wayland-cursor
# ライブラリも、日本語入力がホストの Qt インストールに依存しないよう保持する。

# 除外したプラグインの専用依存ライブラリ
# (libqmng, libqtiff, libqjp2, libqwebp, libqtga, libqicns 等を除外したため)
rm -f "$APPDIR_LIB"/libmng.so* "$APPDIR_LIB"/liblcms2.so*
rm -f "$APPDIR_LIB"/libtiff.so* "$APPDIR_LIB"/libjbig.so* "$APPDIR_LIB"/liblzma.so*
rm -f "$APPDIR_LIB"/libdeflate.so*
rm -f "$APPDIR_LIB"/libjasper.so*
rm -f "$APPDIR_LIB"/libwebp.so* "$APPDIR_LIB"/libwebpdemux.so* "$APPDIR_LIB"/libwebpmux.so*
rm -f "$APPDIR_LIB"/libsharpyuv.so*

AFTER_SIZE=$(du -sm "$APPDIR_LIB" | cut -f1)
info "ライブラリ削減: ${BEFORE_SIZE}MB → ${AFTER_SIZE}MB（$((BEFORE_SIZE - AFTER_SIZE))MB 削減）"

# linuxdeploy-plugin-qt が配置する Qt 標準の翻訳（全言語の qtbase_*.qm / qtmultimedia_*.qm）
# は使わない。標準ダイアログの訳は実行ファイルのリソースに内蔵し、アプリの訳は usr/bin から読む。
rm -rf "${APPDIR}/usr/translations"

# Step 8d: システムの strip で AppDir 内のバイナリをストリップ
info "AppDir 内のバイナリをストリップ中..."
find "$APPDIR" -type f \( -name '*.so*' -o -executable \) -print0 | \
    while IFS= read -r -d '' file; do
        if file "$file" | grep -q 'ELF'; then
            strip --strip-unneeded "$file" 2>/dev/null || true
        fi
    done

# Step 8e: AppRun をラッパースクリプトに置き換え
# 同梱の Qt とホスト側の Qt プラグインが混在しないよう、検索先を固定する。
info "AppRun ラッパースクリプトを作成中..."
rm -f "$APPDIR/AppRun"
cat > "$APPDIR/AppRun" <<'APPRUN_EOF'
#!/bin/bash
HERE="$(dirname "$(readlink -f "$0")")"

# アプリは Fusion スタイルを使用するため、ホストのテーマは不要。
# Qt プラグインは同梱のものを使用し、別バージョンの Qt の読み込みを防ぐ。
export QT_PLUGIN_PATH="${HERE}/usr/plugins"
export QT_QPA_PLATFORM_PLUGIN_PATH="${HERE}/usr/plugins/platforms"

exec "${HERE}/usr/bin/ShogiBoardQ" "$@"
APPRUN_EOF
chmod +x "$APPDIR/AppRun"

# Step 8f: appimagetool で AppImage を生成
# linuxdeploy --output appimage は依存ライブラリを再デプロイしてしまうため、
# クリーンアップ済みの AppDir をそのままパッケージングする appimagetool を使用
info "Qt・同梱ライブラリのライセンスと対応ソース情報を同梱中..."
[[ -f "${APPDIR}/usr/plugins/sqldrivers/libqsqlite.so" ]] || die "SQLite ドライバーが配布物にありません。"
# Qt の文書は同梱するモジュールの分だけ入れる（WebEngine など使わないモジュールの分は入れない）。
# qtwayland: Qt 6.9 以前は日本語入力（fcitx5）が使う Qt Wayland Client がこのモジュールにある。
# qttranslations: 標準ダイアログの訳（qtbase_*.qm）を実行ファイルに内蔵している。
python3 scripts/qt_licenses.py stage --build-dir "$BUILD_DIR" \
    --notices "${SHOGIBOARDQ_QT_LICENSE_DIR:-${BUILD_DIR}/qt-licenses}" \
    --destination "${APPDIR}/usr/share/licenses/ShogiBoardQ" \
    --modules qtbase qtcharts qtmultimedia qtsvg qttranslations qtwayland
# Qt 以外の同梱ライブラリ（Arch Linux のパッケージ）のライセンス本文と一覧。
# 「バージョン情報」の「同梱ライブラリのライセンス一覧」で表示する。
python3 scripts/bundled_licenses.py --appdir "$APPDIR" --qt-plugins "$QT_PLUGINS_SRC" \
    --destination "${APPDIR}/usr/share/licenses/ShogiBoardQ"

info "AppImage を生成中..."
"$APPIMAGETOOL" "$APPDIR" "$APPIMAGE_NAME"

# ──────────────────────────────────────────────
# Step 9: 検証
# ──────────────────────────────────────────────

if [[ ! -f "$APPIMAGE_NAME" ]]; then
    die "AppImage の作成に失敗しました。"
fi

APPIMAGE_SIZE=$(du -h "$APPIMAGE_NAME" | cut -f1)
info "AppImage サイズ: $APPIMAGE_SIZE"

# 問題集と通常対局用エンジンを、ZIP 展開後すぐファイル選択できるよう外にも配置する。
info "AppImage・問題集・Hayanagi を含む ZIP を作成中..."
rm -rf "$PACKAGE_DIR"
mkdir -p "$PACKAGE_DIR/data/tsumeshogi" "$PACKAGE_DIR/Hayanagi"
cp "$APPIMAGE_NAME" "$PACKAGE_DIR/"
cp "${TSUME_FILES[@]}" data/tsumeshogi/README.md "$PACKAGE_DIR/data/tsumeshogi/"
cp Hayanagi/README.md "$PACKAGE_DIR/Hayanagi/"
install -m755 "$HAYANAGI_EXE" "$PACKAGE_DIR/Hayanagi/hayanagi"
strip --strip-unneeded "$PACKAGE_DIR/Hayanagi/hayanagi"
cp resources/platform/README-linux.md "$PACKAGE_DIR/README.md"
cp LICENSE "$PACKAGE_DIR/"
# docs、検証 JSON、licenses は ZIP の外部ファイルに含めない。
python3 - "$PACKAGE_DIR" "$ZIP_NAME" <<'PYTHON_EOF'
from pathlib import Path
import sys
import zipfile

package = Path(sys.argv[1])
with zipfile.ZipFile(sys.argv[2], "w", compression=zipfile.ZIP_DEFLATED) as archive:
    for path in sorted(package.rglob("*")):
        if path.is_file():
            archive.write(path, path.relative_to(package.parent))
PYTHON_EOF

# ──────────────────────────────────────────────
# 完了
# ──────────────────────────────────────────────

info "ビルド完了!"
info "実行ファイル: $EXE_PATH"
info "AppImage:     $APPIMAGE_NAME"
info "ZIP:          $ZIP_NAME"
