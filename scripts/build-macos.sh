#!/usr/bin/env bash
#
# macOS ビルドスクリプト for ShogiBoardQ
#
# Release ビルド → macdeployqt → コード署名 → DMG 作成（→ 公証）→ 配布 ZIP 作成を一括実行する。
# ZIP（ShogiBoardQ-macos.zip）には DMG・通常対局用 Hayanagi（定跡を含む）・詰将棋問題集を入れる。
# Hayanagi と問題集は DMG（アプリバンドル）には入れない。
# 詳細: docs/dev/macos-build-and-release.md
#
# Usage:
#   ./scripts/build-macos.sh [OPTIONS]
#
# Options:
#   --universal               Universal Binary (arm64 + x86_64) をビルド
#   --deployment-target VER   最小対応 macOS バージョン（既定: 26.0）
#   --sign-identity ID        コード署名 ID（既定: "-" = アドホック署名）
#   --notarize                DMG を公証・ステープルしてから ZIP を作る（Developer ID と
#                             環境変数 APPLE_ID / APPLE_TEAM_ID / APPLE_APP_PASSWORD が必要）
#   --skip-dmg                DMG と ZIP の作成をスキップ（.app のみ）
#   --skip-qt-licenses        Qt ライセンス文書の追加をスキップ（ビルド同梱の簡易文書のみ）
#   --clean                   build ディレクトリを削除してからビルド
#   --help                    このヘルプを表示

set -euo pipefail

# ──────────────────────────────────────────────
# 定数
# ──────────────────────────────────────────────

APP_NAME="ShogiBoardQ"
BUILD_DIR="build"
DMG_NAME="${APP_NAME}.dmg"
PACKAGE_DIR="${BUILD_DIR}/${APP_NAME}-macos"
ZIP_NAME="${APP_NAME}-macos.zip"
HAYANAGI_EXE="${BUILD_DIR}/Hayanagi/hayanagi"
# Hayanagi の定跡（ビルド時に hayanagi の横の book/ にコピーされる）
HAYANAGI_BOOK="${BUILD_DIR}/Hayanagi/book/hayanagi_book.db"
APP_BUNDLE="${BUILD_DIR}/${APP_NAME}.app"
ICON_PATH="resources/icons/shogiboardq.icns"
DEFAULT_DEPLOYMENT_TARGET="26.0"

# ──────────────────────────────────────────────
# デフォルトオプション
# ──────────────────────────────────────────────

OPT_UNIVERSAL=false
OPT_SKIP_DMG=false
OPT_SKIP_QT_LICENSES=false
OPT_CLEAN=false
OPT_DEPLOYMENT_TARGET="${MACOSX_DEPLOYMENT_TARGET:-$DEFAULT_DEPLOYMENT_TARGET}"
OPT_SIGN_IDENTITY="-"
OPT_NOTARIZE=false

# ──────────────────────────────────────────────
# ヘルパー関数
# ──────────────────────────────────────────────

info()  { printf '\033[1;34m==>\033[0m %s\n' "$*"; }
warn()  { printf '\033[1;33m==> WARNING:\033[0m %s\n' "$*"; }
error() { printf '\033[1;31m==> ERROR:\033[0m %s\n' "$*" >&2; }
die()   { error "$*"; exit 1; }

# "26" と "26.0" を同一視するため、末尾の ".0" を取り除く
normalize_version() {
    local v="$1"
    while [[ "$v" == *.0 ]]; do v="${v%.0}"; done
    printf '%s' "$v"
}

usage() {
    cat <<'EOF'
Usage: ./scripts/build-macos.sh [OPTIONS]

macOS 用の Release ビルド〜DMG 作成を一括実行するスクリプト。

Options:
  --universal               Universal Binary (arm64 + x86_64) をビルド
  --deployment-target VER   最小対応 macOS バージョン
                            （既定: 環境変数 MACOSX_DEPLOYMENT_TARGET、未設定なら 26.0）
  --sign-identity ID        コード署名 ID（既定: "-" = アドホック署名）
                            Developer ID を指定すると Hardened Runtime を有効にして署名し、
                            DMG にも署名する
  --notarize                DMG を Apple の公証に提出し、公証結果をステープルしてから ZIP を作る
                            --sign-identity で Developer ID を指定し、環境変数 APPLE_ID・
                            APPLE_TEAM_ID・APPLE_APP_PASSWORD（App 用パスワード）を設定しておく
  --skip-dmg                DMG と ZIP の作成をスキップ（.app バンドルのみ生成）
  --skip-qt-licenses        Qt のライセンス文書と対応ソース情報の追加をスキップ
                            （ビルド時に同梱される簡易文書のみ。配布用は通常付けない）
  --clean                   build ディレクトリを削除してからビルド
  --help                    このヘルプを表示

Examples:
  # 通常ビルド + DMG 作成
  ./scripts/build-macos.sh

  # Universal Binary でビルド
  ./scripts/build-macos.sh --universal

  # クリーンビルド、DMG なし
  ./scripts/build-macos.sh --clean --skip-dmg

  # Developer ID で署名
  ./scripts/build-macos.sh --sign-identity "Developer ID Application: Your Name (TEAMID)"

  # Developer ID で署名し、DMG を公証してから ZIP を作る
  export APPLE_ID="your@email.com" APPLE_TEAM_ID="TEAMID" APPLE_APP_PASSWORD="app-specific-password"
  ./scripts/build-macos.sh --sign-identity "Developer ID Application: Your Name (TEAMID)" --notarize
EOF
}

# ──────────────────────────────────────────────
# 引数パース
# ──────────────────────────────────────────────

while [[ $# -gt 0 ]]; do
    case "$1" in
        --universal) OPT_UNIVERSAL=true ;;
        --deployment-target)
            [[ $# -ge 2 ]] || die "--deployment-target にはバージョンを指定してください"
            OPT_DEPLOYMENT_TARGET="$2"
            shift
            ;;
        --sign-identity)
            [[ $# -ge 2 ]] || die "--sign-identity には署名 ID を指定してください"
            OPT_SIGN_IDENTITY="$2"
            shift
            ;;
        --notarize)  OPT_NOTARIZE=true ;;
        --skip-dmg)  OPT_SKIP_DMG=true ;;
        --skip-qt-licenses) OPT_SKIP_QT_LICENSES=true ;;
        --clean)     OPT_CLEAN=true ;;
        --help)      usage; exit 0 ;;
        *)           die "Unknown option: $1 (--help でヘルプを表示)" ;;
    esac
    shift
done

# 公証はビルドの後なので、条件が揃っていないときはビルドの前に止める。
if [[ "$OPT_NOTARIZE" = true ]]; then
    [[ "$OPT_SIGN_IDENTITY" != "-" ]] \
        || die "--notarize には --sign-identity で Developer ID を指定してください（アドホック署名は公証できません）"
    [[ "$OPT_SKIP_DMG" = false ]] || die "--notarize と --skip-dmg は同時に指定できません"
    for var in APPLE_ID APPLE_TEAM_ID APPLE_APP_PASSWORD; do
        [[ -n "${!var:-}" ]] || die "--notarize には環境変数 ${var} が必要です"
    done
fi

# ──────────────────────────────────────────────
# Step 1: 前提チェック
# ──────────────────────────────────────────────

info "前提ツールを確認中..."

REQUIRED_TOOLS=(cmake ninja macdeployqt codesign vtool python3)
if [[ "$OPT_SKIP_DMG" = false ]]; then
    REQUIRED_TOOLS+=(create-dmg)
fi
if [[ "$OPT_NOTARIZE" = true ]]; then
    REQUIRED_TOOLS+=(xcrun)
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
info "最小 macOS:  ${OPT_DEPLOYMENT_TARGET}"
if [[ "$OPT_SIGN_IDENTITY" = "-" ]]; then
    info "署名:        アドホック署名"
else
    info "署名:        ${OPT_SIGN_IDENTITY}"
fi
if [[ "$OPT_SKIP_DMG" = false ]]; then
    info "create-dmg:  $(command -v create-dmg)"
fi
if [[ "$OPT_NOTARIZE" = true ]]; then
    info "公証:        する（チーム: ${APPLE_TEAM_ID}）"
fi

# ──────────────────────────────────────────────
# Step 2: プロジェクトルートへ移動
# ──────────────────────────────────────────────

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$PROJECT_ROOT"
info "プロジェクトルート: $PROJECT_ROOT"

# ZIP に入れる詰将棋問題集。旧版などが残っていると ZIP に混ざるため、各手数1ファイルに限る。
TSUME_FILES=()
if [[ "$OPT_SKIP_DMG" = false ]]; then
    for plies in 3 5 7 9 11 13; do
        collections=(data/tsumeshogi/tsume_"${plies}"ply_*.txt)
        [[ -f "${collections[0]}" ]] || die "${plies}手詰の問題集が見つかりません。"
        [[ ${#collections[@]} -eq 1 ]] || die "${plies}手詰の問題集が複数あります: ${collections[*]}"
        TSUME_FILES+=("${collections[0]}")
    done
fi

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
    "-DCMAKE_OSX_DEPLOYMENT_TARGET=${OPT_DEPLOYMENT_TARGET}"
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

# 最小対応 macOS バージョンの確認（ビルドホストの OS バージョンになっていないか）
BINARY_MINOS=$(vtool -show-build "$APP_BUNDLE/Contents/MacOS/$APP_NAME" 2>/dev/null \
    | awk '/minos/ { print $2; exit }')
if [[ -z "$BINARY_MINOS" \
      || "$(normalize_version "$BINARY_MINOS")" != "$(normalize_version "$OPT_DEPLOYMENT_TARGET")" ]]; then
    die "実行ファイルの最小 macOS が ${BINARY_MINOS:-不明} です（期待値: ${OPT_DEPLOYMENT_TARGET}）。"
fi
info "最小 macOS: ${BINARY_MINOS}"

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
# Step 7.5: 未使用の Qt 部品を削除（最小構成）
# ──────────────────────────────────────────────

# macdeployqt は Qt にあるプラグインを種類ごとにすべて配置し、仮想キーボードの
# プラグイン経由で Qt Quick / QML まで取り込む。ShogiBoardQ が使わないものは削除する。
#   - 仮想キーボード・QML・Qt Quick: 使用しない
#   - TLS・ネットワーク情報: CSA 通信は平文 TCP のみ
#   - FFmpeg バックエンド: 駒音は macOS 標準の darwin バックエンドで再生できる
#   - SQL ドライバー: SQLite のみ使用
#   - 画像形式: アイコン用の SVG / ICO と、盤面画像出力の JPEG / TIFF / WebP のみ残す
info "未使用の Qt 部品を削除中..."
CONTENTS="$APP_BUNDLE/Contents"
rm -rf \
    "$CONTENTS/PlugIns/platforminputcontexts" \
    "$CONTENTS/PlugIns/tls" \
    "$CONTENTS/PlugIns/networkinformation" \
    "$CONTENTS/PlugIns/multimedia/libffmpegmediaplugin.dylib" \
    "$CONTENTS/Frameworks/"libav*.dylib \
    "$CONTENTS/Frameworks/"libsw*.dylib \
    "$CONTENTS/Frameworks/"QtQml*.framework \
    "$CONTENTS/Frameworks/QtQuick.framework" \
    "$CONTENTS/Frameworks/"QtVirtualKeyboard*.framework
find "$CONTENTS/PlugIns/sqldrivers" -name '*.dylib' ! -name 'libqsqlite.dylib' -delete
for plugin in gif wbmp macheif icns tga macjp2; do
    rm -f "$CONTENTS/PlugIns/imageformats/libq${plugin}.dylib"
done

# 削除後も全バイナリの依存先がバンドル内に揃っていることを確認する。
MISSING_DEPS=$(find "$CONTENTS" -type f \( -name '*.dylib' -o -path '*/Versions/A/Qt*' \
        -o -path "*/MacOS/$APP_NAME" \) -print0 \
    | xargs -0 otool -L 2>/dev/null \
    | awk '$1 ~ /^@rpath\/Qt/ { sub(/^@rpath\//, "", $1); sub(/\/.*/, "", $1); print $1 }' \
    | sort -u \
    | while read -r fw; do [[ -d "$CONTENTS/Frameworks/$fw" ]] || echo "$fw"; done)
[[ -z "$MISSING_DEPS" ]] || die "削除したフレームワークがまだ参照されています: $MISSING_DEPS"

# Universal Binary でなければ、Qt の x86_64 部分を取り除いて arm64 のみにする。
if [[ "$OPT_UNIVERSAL" = false ]]; then
    info "Qt のバイナリを arm64 のみに縮小中..."
    while IFS= read -r -d '' bin; do
        if lipo -archs "$bin" 2>/dev/null | grep -q x86_64; then
            lipo "$bin" -thin arm64 -output "$bin.thin"
            chmod "$(stat -f %Lp "$bin")" "$bin.thin"
            mv "$bin.thin" "$bin"
        fi
    done < <(find "$CONTENTS" -type f \( -perm +111 -o -name '*.dylib' \) -print0)
fi

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
# Step 9: コード署名
# ──────────────────────────────────────────────

[[ -f "$APP_BUNDLE/Contents/PlugIns/sqldrivers/libqsqlite.dylib" ]] || die "SQLite ドライバーが配布物にありません。"
if [[ "$OPT_SKIP_QT_LICENSES" = true ]]; then
    warn "Qt ライセンス文書の追加をスキップしました（Contents/MacOS/licenses の簡易文書のみ）。"
else
    info "Qt ライセンスと対応ソース情報を同梱中..."
    # Qt の文書は同梱するモジュールの分だけ入れる。
    # 盤面画像出力用に qtimageformats の TIFF / WebP を残すため、そのモジュールの文書も入れる。
    python3 scripts/qt_licenses.py stage --build-dir "$BUILD_DIR" \
        --notices "${SHOGIBOARDQ_QT_LICENSE_DIR:-build/qt-licenses}" \
        --destination "$APP_BUNDLE/Contents/Resources/licenses" \
        --modules qtbase qtcharts qtmultimedia qtsvg qttranslations qtimageformats
    # 通常ビルド用の簡易文書より配布用の完全な文書を優先する。
    rm -rf "$APP_BUNDLE/Contents/MacOS/licenses"
fi

# macdeployqt がバイナリを書き換えるため、バンドル全体を署名し直す。
# 署名しないとリンカ署名のみの状態になり、厳格な検証に通らない。
info "バンドルにコード署名中..."

CODESIGN_ARGS=(--force --deep --sign "$OPT_SIGN_IDENTITY")
if [[ "$OPT_SIGN_IDENTITY" != "-" ]]; then
    CODESIGN_ARGS+=(--options runtime --timestamp)
fi

codesign "${CODESIGN_ARGS[@]}" "$APP_BUNDLE"
codesign --verify --deep --strict --verbose=2 "$APP_BUNDLE" \
    || die "コード署名の検証に失敗しました。"

# ──────────────────────────────────────────────
# Step 10: DMG 作成
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

# Developer ID のときは DMG にも署名する（アドホック署名のときは今までどおり署名しない）。
if [[ "$OPT_SIGN_IDENTITY" != "-" ]]; then
    info "DMG にコード署名中..."
    codesign --force --sign "$OPT_SIGN_IDENTITY" --timestamp "$DMG_NAME"
    codesign --verify --strict "$DMG_NAME" || die "DMG のコード署名の検証に失敗しました。"
fi

# ──────────────────────────────────────────────
# Step 10.5: 公証（--notarize）
# ──────────────────────────────────────────────

# ZIP にはステープル済みの DMG を入れるため、ZIP を作る前に公証する。
if [[ "$OPT_NOTARIZE" = true ]]; then
    info "DMG を公証に提出中（数分かかります）..."
    # 却下されたときも ID と状態を表示できるよう、終了コードではなく JSON の status で判定する。
    NOTARY_JSON=$(xcrun notarytool submit "$DMG_NAME" \
        --apple-id "$APPLE_ID" --team-id "$APPLE_TEAM_ID" --password "$APPLE_APP_PASSWORD" \
        --wait --output-format json) || true
    notary_field() {
        python3 -c 'import json, sys
try:
    print(json.loads(sys.argv[1]).get(sys.argv[2], ""))
except ValueError:
    print("")' "$NOTARY_JSON" "$1"
    }
    NOTARY_ID=$(notary_field id)
    NOTARY_STATUS=$(notary_field status)
    if [[ "$NOTARY_STATUS" != "Accepted" ]]; then
        [[ -z "$NOTARY_JSON" ]] || printf '%s\n' "$NOTARY_JSON" >&2
        die "公証が通りませんでした（status: ${NOTARY_STATUS:-不明}）。
  理由は xcrun notarytool log ${NOTARY_ID:-<ID>} --apple-id ... --team-id ... --password ... で確認してください。"
    fi
    info "公証済み: ${NOTARY_ID}"
    xcrun stapler staple "$DMG_NAME"
    xcrun stapler validate "$DMG_NAME" || die "DMG のステープルの検証に失敗しました。"
fi

# ──────────────────────────────────────────────
# Step 11: 配布 ZIP 作成
# ──────────────────────────────────────────────

# 問題集と通常対局用エンジンは、ZIP 展開後すぐファイル選択できるよう DMG の外に配置する。
info "DMG・問題集・Hayanagi を含む ZIP を作成中..."
[[ -x "$HAYANAGI_EXE" ]] || die "Hayanagi が見つかりません: $HAYANAGI_EXE"
[[ -f "$HAYANAGI_BOOK" ]] || die "Hayanagi の定跡が見つかりません: $HAYANAGI_BOOK"
HAYANAGI_MINOS=$(vtool -show-build "$HAYANAGI_EXE" 2>/dev/null | awk '/minos/ { print $2; exit }')
if [[ "$(normalize_version "${HAYANAGI_MINOS:-0}")" != "$(normalize_version "$OPT_DEPLOYMENT_TARGET")" ]]; then
    die "Hayanagi の最小 macOS が ${HAYANAGI_MINOS:-不明} です（期待値: ${OPT_DEPLOYMENT_TARGET}）。"
fi
rm -rf "$PACKAGE_DIR" "$ZIP_NAME"
mkdir -p "$PACKAGE_DIR/data/tsumeshogi" "$PACKAGE_DIR/Hayanagi/book"
cp "$DMG_NAME" "$PACKAGE_DIR/"
cp "${TSUME_FILES[@]}" data/tsumeshogi/README.md "$PACKAGE_DIR/data/tsumeshogi/"
cp Hayanagi/README.md "$PACKAGE_DIR/Hayanagi/"
install -m755 "$HAYANAGI_EXE" "$PACKAGE_DIR/Hayanagi/hayanagi"
# strip で署名が外れるため、アプリと同じ ID で署名し直す。
strip -x "$PACKAGE_DIR/Hayanagi/hayanagi"
HAYANAGI_SIGN_ARGS=(--force --sign "$OPT_SIGN_IDENTITY")
if [[ "$OPT_SIGN_IDENTITY" != "-" ]]; then
    HAYANAGI_SIGN_ARGS+=(--options runtime --timestamp)
fi
codesign "${HAYANAGI_SIGN_ARGS[@]}" "$PACKAGE_DIR/Hayanagi/hayanagi"
codesign --verify --strict "$PACKAGE_DIR/Hayanagi/hayanagi" \
    || die "Hayanagi のコード署名の検証に失敗しました。"
# Hayanagi は作業ディレクトリ（ShogiBoardQ はエンジンのあるディレクトリにする）の book/ から定跡を読む。
cp "$HAYANAGI_BOOK" "$PACKAGE_DIR/Hayanagi/book/"
cp resources/platform/README-macos.md "$PACKAGE_DIR/README.md"
cp LICENSE "$PACKAGE_DIR/"
# 拡張属性や ._ ファイルを入れない（-X）。
(cd "$BUILD_DIR" && zip -qrX "$PROJECT_ROOT/$ZIP_NAME" "$(basename "$PACKAGE_DIR")")

# ──────────────────────────────────────────────
# 完了
# ──────────────────────────────────────────────

info "ビルド完了!"
info "アプリバンドル: $APP_BUNDLE"
info "DMG ファイル:   $DMG_NAME"
info "ZIP ファイル:   $ZIP_NAME"
