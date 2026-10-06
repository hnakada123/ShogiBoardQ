"""Texts of guide/linux-build-and-release.html (ja / en / zh-cn / zh-tw)."""

REPO = "https://github.com/hnakada123/ShogiBoardQ"
QT_LICENSING = f"{REPO}/blob/main/docs/dev/qt-licensing.md"
RELEASE_YML = f"{REPO}/blob/main/.github/workflows/release.yml"

# ---------------------------------------------------------------- 言語に依存しないコマンド
ARCH = r"""
sudo pacman -S --needed base-devel cmake ninja git curl file python fuse2 imagemagick \
  qt6-base qt6-charts qt6-multimedia qt6-svg qt6-tools fcitx5-qt
"""
UBUNTU = r"""
sudo apt install build-essential cmake ninja-build git \
  qt6-base-dev qt6-charts-dev qt6-multimedia-dev qt6-tools-dev qt6-tools-dev-tools \
  qt6-l10n-tools libqt6sql6-sqlite libgl1-mesa-dev
"""
FEDORA = r"""
sudo dnf install gcc-c++ cmake ninja-build git \
  qt6-qtbase-devel qt6-qtcharts-devel qt6-qtmultimedia-devel qt6-qttools-devel \
  qt6-linguist mesa-libGL-devel
"""
QT_ONLINE = r"""
cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$HOME/Qt/6.11.2/gcc_64"
"""
CHECK = """
cmake --version
g++ --version
qmake6 --version
"""
CLONE = """
git clone --recurse-submodules https://github.com/hnakada123/ShogiBoardQ.git
cd ShogiBoardQ
"""
BUILD = """
cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/ShogiBoardQ
"""
TEST = """
cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
"""
QT_VERSION = "pacman -Q qt6-base qt6-charts qt6-multimedia qt6-svg"
QT_SHA = "curl -fsSL https://download.qt.io/archive/qt/6.7/6.7.2/single/qt-everywhere-src-6.7.2.tar.xz.sha256"
QT_PREPARE = r"""
python3 scripts/qt_licenses.py fetch --version 6.7.2 \
  --sha256 0aaea247db870193c260e8453ae692ca12abc1bd841faa1a6e6c99459968ca8a \
  --output build-container/qt-sources
python3 scripts/qt_licenses.py prepare --version 6.7.2 \
  --archive build-container/qt-sources/qt-everywhere-src-6.7.2.tar.xz \
  --sha256 0aaea247db870193c260e8453ae692ca12abc1bd841faa1a6e6c99459968ca8a \
  --source-url https://download.qt.io/archive/qt/6.7/6.7.2/single/qt-everywhere-src-6.7.2.tar.xz \
  --provenance 'Arch Linux packages from the Arch Linux Archive snapshot 2024-07-15: qt6-base 6.7.2-1; qt6-charts, qt6-multimedia, qt6-svg 6.7.2-1; qt6-wayland 6.7.2-2. Upstream Qt source plus Arch PKGBUILDs and patches.' \
  --output build-container/qt-licenses
"""
RUN = "./scripts/build-linux.sh"
CONTAINER_RUN = "./scripts/build-linux-container.sh"
DOCKER_GROUP = "sudo usermod -aG docker $USER"
UNZIP = """
unzip ShogiBoardQ-linux.zip
cd ShogiBoardQ-linux
chmod +x ShogiBoardQ-linux-x86_64.AppImage Hayanagi/hayanagi
./ShogiBoardQ-linux-x86_64.AppImage
"""
RELEASE = r"""
git tag 2026.10.06
git push origin 2026.10.06
gh release create 2026.10.06 --title "ShogiBoardQ 2026.10.06" \
  --notes-file RELEASE_NOTES.md ShogiBoardQ-linux.zip
"""
UPLOAD = "gh release upload 2026.10.06 ShogiBoardQ-linux.zip"
ERR_QT = 'Could not find a package configuration file provided by "Qt6"'
ERR_TSUME = "==> ERROR: 5手詰の問題集が複数あります: data/tsumeshogi/tsume_5ply_1000_20260926.txt data/tsumeshogi/tsume_5ply_1000_20261001.txt"
ERR_QT_NOTICES = """Qt license preparation failed: Qt version mismatch: build=6.7.2, source=6.11.2
Qt license preparation failed: Qt notices are missing. See docs/dev/qt-licensing.md; set SHOGIBOARDQ_QT_LICENSE_DIR to prepared matching notices."""
ERR_BUNDLED = """Bundled library notices failed: Bundled library notices need pacman (Arch Linux); other distributions are not supported yet
Bundled library notices failed: Cannot find the system file of lib/libQt6Core.so.6
Bundled library notices failed: No license text for bundled package fcitx5-qt (GPL)"""
ERR_FUSE_TOOL = "dlopen(): error loading libfuse.so.2"
ERR_FUSE = "AppImages require FUSE to run."
EXTRACT_RUN = "./ShogiBoardQ-linux-x86_64.AppImage --appimage-extract-and-run"
ERR_GLIBC = "version `GLIBC_2.38' not found"
ERR_LIBS = "error while loading shared libraries: libOpenGL.so.0: cannot open shared object file: No such file or directory"
LIBS_UBUNTU = "sudo apt install libegl1 libopengl0 libfontconfig1 libharfbuzz0b libwayland-client0"
ERR_DOCKER = "permission denied while trying to connect to the docker API at unix:///var/run/docker.sock"
ERR_XCB = 'qt.qpa.plugin: Could not load the Qt platform plugin "xcb"'
XCB = """
sudo apt install libxcb-cursor0 libxcb-xinerama0
sudo dnf install xcb-util-cursor xcb-util-wm xcb-util-keysyms
sudo pacman -S xcb-util-cursor xcb-util-wm xcb-util-keysyms
"""
IM = "QT_IM_MODULE=fcitx ./ShogiBoardQ-linux-x86_64.AppImage"
QM = """
./ShogiBoardQ-linux-x86_64.AppImage --appimage-extract
ls squashfs-root/usr/bin/*.qm
"""


def appimage_tree(c):
    """c: 注釈（言語ごと）"""
    return f"""
ShogiBoardQ-linux-x86_64.AppImage
└── {c['extracted']}
    ├── AppRun                          ← {c['apprun']}
    ├── shogiboardq.desktop
    ├── shogiboardq.png
    └── usr/
        ├── bin/
        │   ├── ShogiBoardQ             ← {c['exe']}
        │   ├── qt.conf                 ← {c['qtconf']}
        │   └── ShogiBoardQ_{{ja_JP,en,zh_CN,zh_TW}}.qm   ← {c['qm']}
        ├── lib/                        ← {c['lib']}
        ├── plugins/
        │   ├── platforms/libqxcb.so            ← X11
        │   ├── imageformats/libqsvg.so, libqjpeg.so, libqico.so
        │   ├── iconengines/libqsvgicon.so
        │   ├── platforminputcontexts/          ← {c['im']}
        │   ├── platformthemes/libqxdgdesktopportal.so
        │   └── sqldrivers/libqsqlite.so        ← {c['sqlite']}
        └── share/
            ├── applications/, icons/
            └── licenses/ShogiBoardQ/
                ├── NOTICE*.md, SOURCE_CODE*.md, GPL-3.0.txt, LGPL-3.0.txt
                ├── QT-NOTICES.md, qt/          ← {c['qt']}
                ├── QT-SOURCE.json, BUILD.json  ← {c['source']}
                └── THIRD-PARTY-NOTICES.md, third-party/   ← {c['third']}
"""


def zip_tree(c):
    return f"""
ShogiBoardQ-linux.zip
└── ShogiBoardQ-linux/
    ├── ShogiBoardQ-linux-x86_64.AppImage
    ├── README.md
    ├── LICENSE
    ├── Hayanagi/
    │   ├── hayanagi
    │   └── README.md
    └── data/tsumeshogi/
        ├── tsume_{{3,5,7,9,11,13}}ply_1000_20261001.txt   ← {c['puzzles']}
        └── README.md
"""


def test_code(c):
    return f"""
# {c['start']}
./ShogiBoardQ-linux-x86_64.AppImage

# {c['contents']}
./ShogiBoardQ-linux-x86_64.AppImage --appimage-extract
ls squashfs-root/usr/bin/ squashfs-root/usr/plugins/*/
ls squashfs-root/usr/share/licenses/ShogiBoardQ/THIRD-PARTY-NOTICES.md
test ! -e squashfs-root/usr/bin/hayanagi && test ! -e squashfs-root/usr/translations && echo OK

# {c['glibc']}
find squashfs-root/usr -type f \\( -name '*.so*' -o -name ShogiBoardQ \\) -exec objdump -T {{}} + 2>/dev/null \\
  | grep -o 'GLIBC_[0-9.]*' | sort -uV | tail -1

# {c['hayanagi']}
unzip -o -q ShogiBoardQ-linux.zip -d /tmp/zipcheck
printf 'usi\\nisready\\nquit\\n' | /tmp/zipcheck/ShogiBoardQ-linux/Hayanagi/hayanagi

# {c['zip']}
python3 -m zipfile -l ShogiBoardQ-linux.zip
python3 -m zipfile -t ShogiBoardQ-linux.zip
"""


ZIP_ROWS = [
    ["`ShogiBoardQ-linux-x86_64.AppImage`", "`ShogiBoardQ-linux/`"],
    ["`resources/platform/README-linux.md`", "`ShogiBoardQ-linux/README.md`"],
    ["`LICENSE`", "`ShogiBoardQ-linux/LICENSE`"],
    ["`build/Hayanagi/hayanagi`", "`ShogiBoardQ-linux/Hayanagi/hayanagi`"],
    ["`Hayanagi/README.md`", "`ShogiBoardQ-linux/Hayanagi/README.md`"],
    ["`data/tsumeshogi/tsume_{3,5,7,9,11,13}ply_*.txt`, `data/tsumeshogi/README.md`",
     "`ShogiBoardQ-linux/data/tsumeshogi/`"],
]

T = {}

# ======================================================================= 日本語
T["ja"] = {
    "title": "Linux ビルド・リリース手順",
    "subtitle": "ShogiBoardQ を Linux でビルドし、AppImage を含む ZIP として GitHub で公開する手順",
    "description": "ShogiBoardQ を Linux でビルドし、AppImage・詰将棋問題集・Hayanagi を含む ZIP を作って GitHub Release で公開する手順。開発環境、Qt のライセンス文書の準備、AppImage・ZIP の作成、トラブルシューティングを解説します。",
    "summary": "Linux での開発環境のセットアップからビルド、AppImage・ZIP の作成、GitHub Release での公開までを解説します。",
    "badges": ["Linux", "Qt 6", "AppImage", "GitHub Release"],
    "lang_aria": "表示言語",
    "menu_aria": "メニュー",
    "nav": [("ホーム", "../index.html"), ("利用ガイド", "index.html"), ("動作環境・ビルド方法", "../index.html#build"),
            ("目次", "#contents"), ("GitHub", REPO)],
    "back": ("動作環境・ビルド方法に戻る", "../index.html#build"),
    "blocks": [
        ("p", "ShogiBoardQ を Linux でビルドし、`ShogiBoardQ-linux.zip` を作って GitHub Release で公開する手順。"
              "ZIP には AppImage（アプリ本体）、3・5・7・9・11・13手詰の詰将棋問題集（各1,000題、計6,000題）、"
              "通常対局用の USI エンジン Hayanagi を収録する。**リリースに添付するのはこの ZIP 1個だけ。**"),
        ("note", "**配布物の作成は Arch Linux だけに対応している。** AppImage に同梱するライブラリのライセンス文書を "
                 "pacman で集めるため、Arch Linux の Qt パッケージを使って作る。リリース用は、古い Arch Linux の"
                 "コンテナで作る（[5.1](#package-run)）。アプリのビルドと開発は、ほかのディストリビューションでもできる。"),
        ("h2", "contents", "目次"),
        ("toc", [("requirements", "前提条件"), ("setup", "開発環境のセットアップ"), ("build", "ソースの取得とビルド"),
                 ("qt-notices", "Qt のライセンス文書の準備"), ("package", "AppImage と ZIP の作成"),
                 ("release", "GitHub Release での公開"), ("troubleshooting", "トラブルシューティング")]),

        ("h2", "requirements", "1. 前提条件"),
        ("table", ["項目", "内容"], [
            ["OS", "x86_64 の Linux。配布物（AppImage・ZIP）の作成は Arch Linux（リリース用は Docker のコンテナ内の Arch Linux）"],
            ["コンパイラ", "C++17 に対応した GCC 9 以降、または Clang 10 以降"],
            ["CMake", "3.16 以上（Ninja を推奨）"],
            ["Qt", "6.7 以上（Widgets・Charts・Network・Concurrent・Multimedia・Sql・LinguistTools）。"
                   "配布物の作成には SVG のプラグインも必要"],
            ["Python", "Python 3（配布用のスクリプト）"],
            ["Docker", "リリース用の配布物をコンテナで作る場合"],
            ["その他", "Git、curl、file、FUSE 2（AppImage 形式のツールの実行）、ImageMagick（512px を超えるアイコンの縮小）"],
        ], "labels"),
        ("p", "配布物を作るときはネットワークに接続できる必要がある。初回に linuxdeploy と appimagetool をダウンロードするため。"),
        ("note", "**glibc について**: AppImage は、ビルドした環境と同じか、より新しい glibc のシステムでしか動かない。"
                 "最新の Arch Linux でビルドすると glibc 2.43 以降が必要になり、Ubuntu 24.04（glibc 2.39）などで起動できない。"
                 "そのためリリース用は、2024年7月15日の Arch Linux（glibc 2.39・Qt 6.7.2）に固定したコンテナでビルドする"
                 "（[5.1](#package-run)）。この AppImage は glibc 2.38 以降と GCC 12 以降の libstdc++ を必要とし、"
                 "Ubuntu 24.04 以降、Debian 13、Fedora 39 以降などで動く。"),

        ("h2", "setup", "2. 開発環境のセットアップ"),
        ("h3", "setup-arch", "2.1 Arch Linux と Docker（配布物を作る場合）"),
        ("p", "リリース用の配布物はコンテナの中でビルドするので、手元には Docker・Git・Python 3 があればよい"
              "（Qt などのビルドに使うものはコンテナに入る）。ユーザーを docker グループに入れ、ログインし直す。"),
        ("code", DOCKER_GROUP),
        ("note", "docker グループのユーザーは、docker を通じて root と同等の操作ができる。"),
        ("p", "手元の Arch Linux でビルド・開発する場合は、ビルドツールと Arch の Qt パッケージを入れる。fcitx5-qt は、"
              "日本語入力（fcitx5）用の入力プラグインを AppImage に同梱するために使う。入っていないと、このプラグインは同梱されない。"),
        ("code", ARCH),
        ("h3", "setup-other", "2.2 ほかのディストリビューション（アプリのビルドのみ）"),
        ("p", "Ubuntu・Debian や Fedora でも、アプリのビルドと開発はできる。Qt 6.7 以上が必要なので、ディストリビューションの "
              "Qt が古い場合は Qt Online Installer で Qt を入れる。"),
        ("p", "Ubuntu / Debian:"),
        ("code", UBUNTU),
        ("p", "Fedora:"),
        ("code", FEDORA),
        ("p", "Qt Online Installer で入れた Qt は、CMake の構成時に場所を指定する（例は Qt 6.11.2）。"),
        ("code", QT_ONLINE),
        ("h3", "setup-check", "2.3 インストールの確認"),
        ("code", CHECK),

        ("h2", "build", "3. ソースの取得とビルド"),
        ("p", "Hayanagi をサブモジュールとして含めてリポジトリを取得する。サブモジュールがないと、ビルドに必要なファイルが欠ける。"),
        ("code", CLONE),
        ("p", "取得済みのリポジトリでは、`git submodule update --init --recursive` で記録された版の Hayanagi を取得する。"
              "Release ビルドして起動する。"),
        ("code", BUILD),
        ("p", "テストを実行する場合:"),
        ("code", TEST),
        ("p", "ビルドだけを行うなら `./scripts/build-linux.sh --skip-appimage` も使える。"),

        ("h2", "qt-notices", "4. Qt のライセンス文書の準備"),
        ("p", "AppImage には、使った Qt と同じ版のソースから取り出したライセンス文書と、ソースの取得元の記録を収録する。"
              "配布物を作る前に一度準備し、Qt を更新したら作り直す。通常のビルドには不要。"),
        ("p", "Qt の版は、ビルドする環境で決まる。リリース用のコンテナ（[5.1](#package-run)）の Qt は 6.7.2 なので、"
              "Qt 6.7.2 のソース（qt-everywhere）を取得・検証し、文書を `build-container/qt-licenses` に取り出す。"
              "SHA-256 は、公式配布サイトでソースと同じ場所にある `.sha256` ファイルで確かめる。"),
        ("code", QT_SHA),
        ("code", QT_PREPARE),
        ("p", "`--provenance` には使った Qt の由来（Arch のパッケージの版と、Arch のパッチ・ビルド手順を含むこと）を書く。"
              "この内容は `QT-SOURCE.json` に記録され、「バージョン情報」の「ソースコードの入手方法」に表示される。"
              "手元の Arch Linux でそのままビルドする場合は、`pacman -Q qt6-base` で確かめた版の文書を `build/qt-licenses` に準備する。"),
        ("ul", [
            "`prepare` は既存の出力先を上書きしない。作り直すときは新しい出力先を指定する。",
            "既定の出力先は、ビルドディレクトリ（コンテナは `build-container`、手元のビルドは `build`）の `qt-licenses`。"
            "`--clean` でビルドディレクトリを消す場合は文書をその外に作り、環境変数 `SHOGIBOARDQ_QT_LICENSE_DIR` で指定する。",
            "取得した Qt ソースや文書を Release に別添付しない。方針の詳細は "
            f"[Qt 文書とリリース添付の方針]({QT_LICENSING}) を参照。",
        ]),

        ("h2", "package", "5. AppImage と ZIP の作成"),
        ("h3", "package-run", "5.1 スクリプトの実行"),
        ("p", "リリース用の配布物は、古い Arch Linux（2024年7月15日の Arch Linux Archive。glibc 2.39・Qt 6.7.2）の"
              "コンテナで作る。"),
        ("code", CONTAINER_RUN),
        ("ul", [
            "初回はビルド環境のイメージ（`scripts/linux-container/Dockerfile`、約3GB）を作る。Arch Linux Archive からの"
            "ダウンロードは遅く、数十分かかる。",
            "コンテナの中で、自分のユーザーとして `scripts/build-linux.sh` を実行する。ビルドディレクトリは普段の `build` と"
            "分けて `build-container` を使い、出力はリポジトリ直下に置く。",
            "当時のパッケージに署名した鍵には、その後に期限が切れたものがある。イメージでは、署名を公式の鍵束で確かめた"
            "うえで鍵の信頼度は問わない設定にしている。",
        ]),
        ("p", "手元の Arch Linux でそのまま作ることもできる。その場合の AppImage は、手元の glibc（2026年10月時点で 2.43）"
              "以降を必要とする。"),
        ("code", RUN),
        ("p", "どちらのスクリプトにも、次のオプションを付けられる。"),
        ("table", ["オプション", "説明"], [
            ["`--skip-appimage`", "AppImage・ZIP を作らず、ビルドだけを行う"],
            ["`--clean`", "ビルドディレクトリを削除してからビルドする"],
            ["`--help`", "ヘルプを表示する"],
        ], "labels"),
        ("table", ["環境変数", "説明"], [
            ["`SHOGIBOARDQ_BUILD_DIR`", "ビルドディレクトリ（既定は `build`。コンテナでは `build-container`）"],
            ["`SHOGIBOARDQ_QT_LICENSE_DIR`", "準備した Qt 文書の場所（既定はビルドディレクトリの `qt-licenses`）"],
            ["`APPIMAGE_EXTRACT_AND_RUN=1`", "FUSE を使えない環境で、AppImage 形式のツールを展開して実行する（コンテナでは常に指定）"],
            ["`DOCKER`", "コンテナ用。docker コマンド（例: `sudo docker`）"],
            ["`SHOGIBOARDQ_ARCH_SNAPSHOT`", "コンテナ用。Arch Linux Archive の日付（既定は `2024/07/15`）"],
        ], "labels"),
        ("p", "`build-linux.sh` は次の処理を行う（コンテナでは、その中で同じ処理を行う）。"),
        ("ol", [
            "前提ツールの確認（cmake・python3。ninja を推奨）",
            "CMake の構成と Release ビルド（ShogiBoardQ と Hayanagi）",
            "実行ファイル・翻訳ファイルと、3〜13手詰の問題集（各手数1ファイル）の確認",
            "linuxdeploy・Qt プラグイン・appimagetool のダウンロード（初回のみ。ビルドディレクトリに保存）",
            "AppDir の作成と AppImage の生成（実行に必要なファイルと、Qt・同梱ライブラリのライセンス文書）",
            "AppImage・問題集・Hayanagi を含む ZIP の作成",
        ]),
        ("p", "出力はリポジトリ直下の `ShogiBoardQ-linux-x86_64.AppImage` と `ShogiBoardQ-linux.zip`。"
              "作業用のディレクトリは、ビルドディレクトリの `AppDir` と `ShogiBoardQ-linux/`。"),
        ("h3", "appimage", "5.2 AppImage の中身"),
        ("p", "AppImage には、アプリの起動と動作に必要なファイルとライセンス文書だけを収録する。"),
        ("code", appimage_tree({
            "extracted": "（展開時）", "apprun": "同梱の Qt だけを使って起動する", "exe": "実行ファイル",
            "qtconf": "Qt プラグインの検索先", "qm": "アプリの翻訳（4言語）", "lib": "Qt と依存ライブラリ",
            "im": "日本語入力（compose・fcitx5・ibus）", "sqlite": "詰将棋の解答履歴・解析キャッシュ",
            "qt": "同梱する Qt モジュールの文書", "source": "Qt の取得元・ビルド時の版",
            "third": "Qt 以外の同梱ライブラリの文書"})),
        ("ul", [
            "問題集・通常対局用の Hayanagi・説明書・問題集の検証記録（`validation_*.json`）は入れない。アプリはこれらを "
            "AppImage 内から読まず、利用者もファイル選択できないため、ZIP の外部ファイルとしてだけ配布する。"
            "詰将棋対局の Hayanagi はアプリ本体に組み込まれている。",
            "linuxdeploy が配置する Qt 標準の翻訳（`usr/translations/`）は使わないので削除する。標準ダイアログの"
            "日本語・中国語訳は実行ファイルに内蔵し、アプリの翻訳は `usr/bin/` から読む。",
            "使わないプラグイン（OpenGL 連携の `xcbglintegrations`、通信暗号化の `tls`、GIF 画像）は入れない。"
            "OpenGL を使う画面部品はなく、CSA 通信対局は暗号化しない TCP で、HTTPS などの通信もしないため。",
            "Qt のライセンス文書は、同梱する Qt モジュール（qtbase・qtcharts・qtmultimedia・qtsvg・qttranslations・qtwayland）"
            "の分と、それらが参照する文書だけを入れる（`scripts/qt_licenses.py`）。WebEngine など配布しないモジュールの"
            "文書は入れない。",
            "Qt 以外の同梱ライブラリ（glib・PulseAudio・OpenSSL・fcitx5-qt など約50パッケージ）は、"
            "`scripts/bundled_licenses.py` が元のパッケージを pacman で調べ、ライセンス本文を `third-party/` に、"
            "版・ライセンス・ソースの取得先の一覧を `THIRD-PARTY-NOTICES.md` に入れる。どちらも「バージョン情報」の"
            "「Qt 内の第三者ライセンス一覧」「同梱ライブラリのライセンス一覧」で表示される。",
        ]),
        ("h3", "zip", "5.3 ZIP の中身"),
        ("p", "ZIP には次のファイルを配置する。問題集と Hayanagi は、ZIP を展開すればすぐファイル選択できる。"),
        ("table", ["元ファイル", "ZIP 内の配置先"], ZIP_ROWS),
        ("code", zip_tree({"puzzles": "6ファイル（各1,000題）"})),
        ("ul", [
            "問題集は内容を変更せずにコピーする。アプリに内蔵した監査記録とハッシュが一致するため、読み込み時に"
            "検証済みの手数と手順を再利用できる。",
            "`data/tsumeshogi/` には各手数の問題集を1ファイルだけ置く（git で追跡しているのは現行の20261001版）。"
            "同じ手数のファイルが複数あると、スクリプトはエラーで止まる。",
            "Hayanagi は `strip` してから配置する。ZIP の外部ファイルには `docs/`・`validation_*.json`・`licenses/` を"
            "入れない。ライセンス文書は AppImage 内にある。",
        ]),
        ("p", "利用者は次のように起動する。詰将棋対局の「局面集を開く…」で `data/tsumeshogi/` の問題集を選び、"
              "通常対局用のエンジン登録では `Hayanagi/hayanagi` を選ぶ。"),
        ("code", UNZIP),
        ("h3", "test", "5.4 動作確認"),
        ("code", test_code({
            "start": "起動", "contents": "展開して中身を確かめる（Hayanagi と Qt 標準の翻訳が入っていないこと）",
            "glibc": "必要な glibc の版", "hayanagi": "ZIP の Hayanagi が USI エンジンとして応答すること",
            "zip": "ZIP が 12 ファイルの構成どおりで、破損がないこと"})),
        ("p", "必要な glibc の版が、コンテナでビルドした場合は `GLIBC_2.38` 以下であることを確かめる。AppImage は、"
              "OpenGL（libEGL・libOpenGL）・fontconfig・HarfBuzz・wayland-client をシステムのものを使う（通常のデスクトップには"
              "入っている）。"),
        ("note", "Qt の lib ディレクトリを `LD_LIBRARY_PATH` に含めない環境で確かめる。含まれているとシステムの"
                 "ライブラリが使われ、同梱漏れに気づけない。できれば Ubuntu 24.04 など別の環境でも、"
                 "日本語・英語・中国語の切り替え、駒の SVG、駒音、詰将棋の履歴の保存、「バージョン情報」の"
                 "ライセンス一覧を確認する。"),

        ("h2", "release", "6. GitHub Release での公開"),
        ("p", "リリースのタグにはアプリの版（`CMakeLists.txt` の `APP_VERSION`。例: 2026.10.06）を使う。Linux の添付は "
              "`ShogiBoardQ-linux.zip` だけで、ファイル名は版によらず同じにする。"),
        ("code", RELEASE),
        ("p", "ほかの OS の配布物を先に公開している場合は、既存のリリースに追加する。"),
        ("code", UPLOAD),
        ("ul", [
            "添付するファイルは名前で指定する。作業フォルダ全体や `build/` のファイルをアップロードしない。",
            "Qt のソース、`QT-SOURCE.json`、チェックサム、SBOM などは別に添付しない。ライセンス文書とソースの入手方法は "
            "AppImage 内にある。",
            "公開後に添付ファイルの一覧を確かめる。GitHub が自動で表示する Source code のリンクは、手動の添付とは別のもの。",
        ]),
        ("p", "リリースノートには、Linux での起動方法と動作条件（必要な glibc の版など）を書く。"),
        ("code", """
### Linux での起動方法

1. ShogiBoardQ-linux.zip をダウンロードして展開する
2. chmod +x ShogiBoardQ-linux-x86_64.AppImage Hayanagi/hayanagi
3. ./ShogiBoardQ-linux-x86_64.AppImage

動作条件: x86_64 の Linux、glibc 2.38 以降（Ubuntu 24.04 以降、Debian 13、Fedora 39 以降など）
FUSE がない場合は --appimage-extract-and-run を付けて起動できます。
"""),
        ("note", f"リポジトリの [release.yml]({RELEASE_YML}) は `v` で始まるタグや手動実行で3つの OS をビルドするが、"
                 "Linux のジョブは Ubuntu で動く。同梱ライブラリの文書の収集が Ubuntu に対応するまでは Linux の配布物を"
                 "作れないため、現在は Arch Linux のコンテナで作った ZIP を手動で添付する。"),

        ("h2", "troubleshooting", "7. トラブルシューティング"),
        ("h3", "ts-qt", "Qt が見つからない"),
        ("code", ERR_QT),
        ("p", "Qt 6.7 以上と必要なモジュールが入っているか確かめる。Qt Online Installer の Qt は `-DCMAKE_PREFIX_PATH` で"
              "指定する。Qt や CMake のジェネレーターを替えたときは、新しい build ディレクトリでやり直す。"),
        ("h3", "ts-submodule", "Hayanagi のソースがない"),
        ("p", "リポジトリ直下で `git submodule update --init --recursive` を実行する。"),
        ("h3", "ts-tsume", "問題集が複数あると表示されて止まる"),
        ("code", ERR_TSUME),
        ("p", "`data/tsumeshogi/` に旧版の問題集が残っている。各手数1ファイルになるよう、使わない版を削除する。"),
        ("h3", "ts-qt-notices", "Qt の文書の検証で止まる"),
        ("code", ERR_QT_NOTICES),
        ("p", "ビルドに使った Qt と同じ版の文書を準備する（[4章](#qt-notices)）。コンテナの Qt は 6.7.2、手元の Arch Linux の"
              "Qt は `pacman -Q qt6-base` の版。失敗を無視して配布しない。"),
        ("h3", "ts-bundled", "同梱ライブラリの文書の収集で止まる"),
        ("code", ERR_BUNDLED),
        ("ul", [
            "配布物は Arch Linux で、Arch の Qt パッケージを使って作る。Qt Online Installer の Qt など、pacman の"
            "パッケージに含まれないライブラリが AppImage に入ると、元のパッケージを特定できずに止まる。",
            "`No license text for …` は、ライセンス本文が見つからないパッケージがあることを示す。古いリポジトリでは"
            "ライセンス欄が正式な形式（SPDX）でないことがあるので、`scripts/bundled_licenses.py` の `LICENSE_OVERRIDES` に"
            "上流の表記を加える。パッケージにない文書（著作権表示付きの本文など）は `scripts/license-texts/<パッケージ名>/` に置く。",
        ]),
        ("h3", "ts-docker", "docker を使えない"),
        ("code", ERR_DOCKER),
        ("p", "ユーザーを docker グループに入れてログインし直す（`sudo usermod -aG docker $USER`）。または環境変数 "
              "`DOCKER=\"sudo docker\"` を指定する。"),
        ("h3", "ts-linuxdeploy", "linuxdeploy が起動しない"),
        ("code", ERR_FUSE_TOOL),
        ("p", "`fuse2` を入れるか、`APPIMAGE_EXTRACT_AND_RUN=1 ./scripts/build-linux.sh` で実行する。"),
        ("h3", "ts-fuse", "AppImage が起動しない（FUSE）"),
        ("code", ERR_FUSE),
        ("p", "FUSE 2 を入れるか、`--appimage-extract-and-run` を付けて起動する。"),
        ("code", EXTRACT_RUN),
        ("h3", "ts-glibc", "glibc の版が足りない"),
        ("code", ERR_GLIBC),
        ("p", "起動したシステムの glibc が、ビルドした環境より古い。リリース用はコンテナでビルドする（[5.1](#package-run)）。"
              "それでも足りない場合は、glibc がより新しいディストリビューションで使う（[前提条件](#requirements)の glibc の説明を参照）。"),
        ("h3", "ts-libs", "システムのライブラリが見つからない"),
        ("code", ERR_LIBS),
        ("p", "AppImage に同梱しないライブラリ（OpenGL・fontconfig・HarfBuzz・wayland-client）がシステムに入っていない。"
              "Ubuntu / Debian の例:"),
        ("code", LIBS_UBUNTU),
        ("h3", "ts-xcb", "xcb プラットフォームプラグインを読み込めない"),
        ("code", ERR_XCB),
        ("p", "X11 関連のライブラリが足りない。Ubuntu / Debian、Fedora、Arch Linux の順に例を示す。"),
        ("code", XCB),
        ("h3", "ts-im", "日本語を入力できない"),
        ("p", "AppImage は X11 用の表示プラグインだけを同梱しているので、Wayland のデスクトップでも XWayland 経由で動く。"
              "環境変数 `QT_IM_MODULE` が未設定だと、日本語入力のプラグインが選ばれない。fcitx5 を使っている場合は "
              "`QT_IM_MODULE=fcitx`、IBus の場合は `QT_IM_MODULE=ibus` を付けて起動する。"),
        ("code", IM),
        ("h3", "ts-translations", "翻訳が読み込まれない"),
        ("p", "`.qm` ファイルが実行ファイルと同じ `usr/bin/` にあるか確かめる。"),
        ("code", QM),
    ],
}

# ======================================================================= English
T["en"] = {
    "title": "Linux Build and Release",
    "subtitle": "Build ShogiBoardQ on Linux and publish it on GitHub as a ZIP containing the AppImage",
    "description": "How to build ShogiBoardQ on Linux, create a ZIP with the AppImage, tsume shogi collections, and Hayanagi, and publish it on GitHub Releases. Covers the development environment, Qt license notices, packaging, and troubleshooting.",
    "summary": "Set up a Linux development environment, build ShogiBoardQ, create the AppImage and ZIP, and publish them on GitHub Releases.",
    "badges": ["Linux", "Qt 6", "AppImage", "GitHub Releases"],
    "lang_aria": "Language",
    "menu_aria": "Menu",
    "nav": [("Home", "../index.html"), ("User Guide", "index.html"), ("Build Guides", "../index.html#build"),
            ("Contents", "#contents"), ("GitHub", REPO)],
    "back": ("Back to Build Guides", "../index.html#build"),
    "blocks": [
        ("p", "This guide builds ShogiBoardQ on Linux, creates `ShogiBoardQ-linux.zip`, and publishes it on GitHub Releases. "
              "The ZIP contains the AppImage (the application), tsume shogi collections for 3, 5, 7, 9, 11, and 13 moves to mate "
              "(1,000 puzzles each, 6,000 in total), and the Hayanagi USI engine for regular games. "
              "**This ZIP is the only Linux file attached to a release.**"),
        ("note", "**Packaging is supported only on Arch Linux.** The license notices of the libraries bundled in the AppImage "
                 "are collected with pacman, so the package is built with Arch's Qt packages. Release packages are built in an "
                 "older Arch Linux container ([5.1](#package-run)). You can build and develop the application itself on other distributions."),
        ("h2", "contents", "Contents"),
        ("toc", [("requirements", "Requirements"), ("setup", "Setting up the development environment"),
                 ("build", "Getting the source and building"), ("qt-notices", "Preparing Qt license notices"),
                 ("package", "Creating the AppImage and ZIP"), ("release", "Publishing on GitHub Releases"),
                 ("troubleshooting", "Troubleshooting")]),

        ("h2", "requirements", "1. Requirements"),
        ("table", ["Item", "Requirement"], [
            ["OS", "x86_64 Linux. Packaging (AppImage and ZIP) requires Arch Linux (for releases, Arch Linux in a Docker container)"],
            ["Compiler", "GCC 9 or later, or Clang 10 or later, with C++17 support"],
            ["CMake", "3.16 or later (Ninja recommended)"],
            ["Qt", "6.7 or later (Widgets, Charts, Network, Concurrent, Multimedia, Sql, LinguistTools). "
                   "Packaging also needs the SVG plugins"],
            ["Python", "Python 3 (packaging scripts)"],
            ["Docker", "To build release packages in the container"],
            ["Other", "Git, curl, file, FUSE 2 (to run AppImage-based tools), ImageMagick (to shrink icons larger than 512 px)"],
        ], "labels"),
        ("p", "Packaging needs network access: linuxdeploy and appimagetool are downloaded the first time."),
        ("note", "**About glibc:** an AppImage runs only on systems whose glibc is the same as or newer than that of the build "
                 "system. Built on the latest Arch Linux, it requires glibc 2.43 or later and does not start on, for example, "
                 "Ubuntu 24.04 (glibc 2.39). Release packages are therefore built in a container pinned to Arch Linux as of "
                 "2024-07-15 (glibc 2.39, Qt 6.7.2; see [5.1](#package-run)). That AppImage requires glibc 2.38 or later and "
                 "libstdc++ from GCC 12 or later, and runs on Ubuntu 24.04 or later, Debian 13, Fedora 39 or later, and similar."),

        ("h2", "setup", "2. Setting up the development environment"),
        ("h3", "setup-arch", "2.1 Arch Linux and Docker (for packaging)"),
        ("p", "Release packages are built inside the container, so the host only needs Docker, Git, and Python 3; Qt and the "
              "other build dependencies are in the container. Add your user to the docker group and log in again."),
        ("code", DOCKER_GROUP),
        ("note", "Members of the docker group can perform root-equivalent operations through docker."),
        ("p", "To build and develop on Arch Linux directly, install the build tools and Arch's Qt packages. fcitx5-qt provides the "
              "Japanese input (fcitx5) plugin that is bundled in the AppImage; without it, that plugin is left out."),
        ("code", ARCH),
        ("h3", "setup-other", "2.2 Other distributions (building the application only)"),
        ("p", "You can build and develop the application on Ubuntu, Debian, or Fedora as well. Qt 6.7 or later is required; "
              "if your distribution's Qt is older, install Qt with the Qt Online Installer."),
        ("p", "Ubuntu / Debian:"),
        ("code", UBUNTU),
        ("p", "Fedora:"),
        ("code", FEDORA),
        ("p", "For Qt installed with the Qt Online Installer, pass its location when configuring CMake (Qt 6.11.2 in this example)."),
        ("code", QT_ONLINE),
        ("h3", "setup-check", "2.3 Checking the installation"),
        ("code", CHECK),

        ("h2", "build", "3. Getting the source and building"),
        ("p", "Clone the repository together with its Hayanagi submodule. Without the submodule, files needed for the build are missing."),
        ("code", CLONE),
        ("p", "In an existing checkout, run `git submodule update --init --recursive` to fetch the recorded Hayanagi version. "
              "Then build in Release mode and start the application."),
        ("code", BUILD),
        ("p", "To run the tests:"),
        ("code", TEST),
        ("p", "For a build without packaging, you can also use `./scripts/build-linux.sh --skip-appimage`."),

        ("h2", "qt-notices", "4. Preparing Qt license notices"),
        ("p", "The AppImage includes the license notices extracted from the source of the same Qt version, together with a "
              "record of where the source comes from. Prepare them once before packaging and again after updating Qt. "
              "Ordinary builds do not need them."),
        ("p", "The Qt version depends on where you build. The release container ([5.1](#package-run)) uses Qt 6.7.2, so "
              "download and verify the Qt 6.7.2 source (qt-everywhere) and extract the notices to `build-container/qt-licenses`. "
              "Check the SHA-256 against the `.sha256` file next to the source on the official download site."),
        ("code", QT_SHA),
        ("code", QT_PREPARE),
        ("p", "In `--provenance`, describe where the Qt you use comes from, including the Arch package versions and that Arch's "
              "patches and build instructions apply. It is recorded in `QT-SOURCE.json` and shown under “Obtaining source code” "
              "in “Version Info”. To build directly on Arch Linux, prepare the notices for the version reported by "
              "`pacman -Q qt6-base` in `build/qt-licenses`."),
        ("ul", [
            "`prepare` does not overwrite an existing output directory; use a new one to recreate the notices.",
            "The default location is `qt-licenses` in the build directory (`build-container` for the container, `build` for "
            "direct builds). If you use `--clean`, which deletes the build directory, create the notices elsewhere and point "
            "to them with the `SHOGIBOARDQ_QT_LICENSE_DIR` environment variable.",
            "Do not attach the Qt source or the notices to a release separately. See the "
            f"[Qt distribution notes (Japanese)]({QT_LICENSING}) for the policy.",
        ]),

        ("h2", "package", "5. Creating the AppImage and ZIP"),
        ("h3", "package-run", "5.1 Running the script"),
        ("p", "Release packages are built in an older Arch Linux container (the Arch Linux Archive as of 2024-07-15: glibc 2.39, Qt 6.7.2)."),
        ("code", CONTAINER_RUN),
        ("ul", [
            "The first run creates the build image (`scripts/linux-container/Dockerfile`, about 3 GB). Downloads from the Arch "
            "Linux Archive are slow and take tens of minutes.",
            "Inside the container, `scripts/build-linux.sh` runs as your user. It uses `build-container` instead of the usual "
            "`build` directory and writes the outputs to the repository root.",
            "Some keys that signed packages at that time have since expired. The image verifies signatures against the "
            "official keyring but does not require the keys to be currently trusted.",
        ]),
        ("p", "You can also build directly on Arch Linux. Such an AppImage requires the host's glibc version or later "
              "(2.43 as of October 2026)."),
        ("code", RUN),
        ("p", "Both scripts accept these options:"),
        ("table", ["Option", "Description"], [
            ["`--skip-appimage`", "Build only; do not create the AppImage and ZIP"],
            ["`--clean`", "Delete the build directory before building"],
            ["`--help`", "Show help"],
        ], "labels"),
        ("table", ["Environment variable", "Description"], [
            ["`SHOGIBOARDQ_BUILD_DIR`", "Build directory (default: `build`; `build-container` in the container)"],
            ["`SHOGIBOARDQ_QT_LICENSE_DIR`", "Location of the prepared Qt notices (default: `qt-licenses` in the build directory)"],
            ["`APPIMAGE_EXTRACT_AND_RUN=1`", "Extract and run AppImage-based tools where FUSE is unavailable (always set in the container)"],
            ["`DOCKER`", "Container only: the docker command (for example `sudo docker`)"],
            ["`SHOGIBOARDQ_ARCH_SNAPSHOT`", "Container only: the Arch Linux Archive date (default: `2024/07/15`)"],
        ], "labels"),
        ("p", "`build-linux.sh` performs these steps (in the container, the same steps run inside it):"),
        ("ol", [
            "Check the required tools (cmake and python3; ninja recommended)",
            "Configure CMake and build in Release mode (ShogiBoardQ and Hayanagi)",
            "Check the executable, translation files, and the 3- to 13-move collections (one file per move count)",
            "Download linuxdeploy, its Qt plugin, and appimagetool (first time only; saved in the build directory)",
            "Create the AppDir and generate the AppImage (files needed to run, plus Qt and bundled library license notices)",
            "Create the ZIP with the AppImage, puzzle collections, and Hayanagi",
        ]),
        ("p", "The outputs are `ShogiBoardQ-linux-x86_64.AppImage` and `ShogiBoardQ-linux.zip` in the repository root. "
              "The working directories are `AppDir` and `ShogiBoardQ-linux/` in the build directory."),
        ("h3", "appimage", "5.2 Contents of the AppImage"),
        ("p", "The AppImage contains only the files needed to start and run the application, plus license notices."),
        ("code", appimage_tree({
            "extracted": "(extracted)", "apprun": "starts the app with the bundled Qt only", "exe": "executable",
            "qtconf": "Qt plugin location", "qm": "application translations (4 languages)", "lib": "Qt and dependencies",
            "im": "Japanese input (compose, fcitx5, ibus)", "sqlite": "tsume history and analysis cache",
            "qt": "notices of the bundled Qt modules", "source": "Qt source record and build version",
            "third": "notices of the other bundled libraries"})),
        ("ul", [
            "Puzzle collections, the Hayanagi engine for regular games, documentation, and validation records "
            "(`validation_*.json`) are not included. The application does not read them from inside the AppImage and users "
            "cannot select files there, so they are shipped only as files outside the AppImage in the ZIP. The Hayanagi used "
            "for tsume play is built into the application.",
            "Qt's own translations placed by linuxdeploy (`usr/translations/`) are removed. The Japanese and Chinese "
            "translations of standard dialogs are built into the executable, and the application translations are read from `usr/bin/`.",
            "Unused plugins are not bundled: OpenGL integration (`xcbglintegrations`), TLS (`tls`), and GIF images. "
            "No widget uses OpenGL, and CSA network play uses unencrypted TCP with no HTTPS or other encrypted connections.",
            "The Qt license notices cover only the bundled Qt modules (qtbase, qtcharts, qtmultimedia, qtsvg, qttranslations, "
            "and qtwayland) and the files they refer to (`scripts/qt_licenses.py`). Notices for modules that are not "
            "distributed, such as Qt WebEngine, are left out.",
            "For the other bundled libraries (about 50 packages such as glib, PulseAudio, OpenSSL, and fcitx5-qt), "
            "`scripts/bundled_licenses.py` looks up each package with pacman, copies the license texts to `third-party/`, "
            "and lists versions, licenses, and source locations in `THIRD-PARTY-NOTICES.md`. Both lists are shown in "
            "“Version Info” as “Third-party licenses in Qt” and “Bundled library licenses”.",
        ]),
        ("h3", "zip", "5.3 Contents of the ZIP"),
        ("p", "The ZIP contains the following files. After extracting it, users can select the puzzle collections and Hayanagi right away."),
        ("table", ["Source file", "Location in the ZIP"], ZIP_ROWS),
        ("code", zip_tree({"puzzles": "6 files (1,000 puzzles each)"})),
        ("ul", [
            "The puzzle collections are copied unchanged. Their hashes match the audit records built into the application, "
            "so verified move counts and solutions are reused when a collection is opened.",
            "Keep exactly one collection per move count in `data/tsumeshogi/` (the repository tracks only the current 20261001 "
            "files). The script stops with an error if two files exist for the same move count.",
            "Hayanagi is stripped before it is added. The ZIP has no `docs/`, `validation_*.json`, or `licenses/` outside the "
            "AppImage; the license notices are inside the AppImage.",
        ]),
        ("p", "Users start the application as follows. In tsume play, they choose a collection in `data/tsumeshogi/` with "
              "“Open collection…”; to register the engine for regular games, they choose `Hayanagi/hayanagi`."),
        ("code", UNZIP),
        ("h3", "test", "5.4 Testing"),
        ("code", test_code({
            "start": "Start", "contents": "Extract and check the contents (no Hayanagi and no Qt translations inside)",
            "glibc": "Required glibc version", "hayanagi": "Hayanagi in the ZIP answers as a USI engine",
            "zip": "The ZIP has the 12 expected files and is not corrupted"})),
        ("p", "For a container build, the required glibc version should be `GLIBC_2.38` or lower. The AppImage uses the "
              "system's OpenGL (libEGL, libOpenGL), fontconfig, HarfBuzz, and wayland-client, which desktop systems normally provide."),
        ("note", "Test in an environment whose `LD_LIBRARY_PATH` does not include Qt's lib directory; otherwise system libraries "
                 "are used and missing files go unnoticed. If possible, also check on another environment such as Ubuntu 24.04: "
                 "switching among Japanese, English, and Chinese, SVG pieces, piece sounds, saving tsume history, "
                 "and the license lists in “Version Info”."),

        ("h2", "release", "6. Publishing on GitHub Releases"),
        ("p", "Tag the release with the application version (`APP_VERSION` in `CMakeLists.txt`, for example 2026.10.06). "
              "Attach only `ShogiBoardQ-linux.zip` for Linux, and keep this filename the same in every release."),
        ("code", RELEASE),
        ("p", "If the packages for other operating systems are already published, add the ZIP to the existing release:"),
        ("code", UPLOAD),
        ("ul", [
            "Name each file to attach. Do not upload the whole working directory or files in `build/`.",
            "Do not attach the Qt source, `QT-SOURCE.json`, checksums, or an SBOM separately. The license notices and "
            "source information are inside the AppImage.",
            "After publishing, check the list of attached files. The Source code links that GitHub adds automatically are "
            "separate from the attached files.",
        ]),
        ("p", "In the release notes, explain how to start the Linux version and its requirements, such as the glibc version."),
        ("code", """
### Running on Linux

1. Download and extract ShogiBoardQ-linux.zip
2. chmod +x ShogiBoardQ-linux-x86_64.AppImage Hayanagi/hayanagi
3. ./ShogiBoardQ-linux-x86_64.AppImage

Requirements: x86_64 Linux with glibc 2.38 or later (Ubuntu 24.04 or later, Debian 13, Fedora 39 or later, etc.)
Without FUSE, start it with --appimage-extract-and-run.
"""),
        ("note", f"The repository's [release.yml]({RELEASE_YML}) builds all three operating systems for tags beginning with `v` "
                 "or when run manually, but its Linux job runs on Ubuntu. Until collecting the bundled library notices supports "
                 "Ubuntu, that job cannot create the Linux package, so the ZIP built in the Arch Linux container is attached manually."),

        ("h2", "troubleshooting", "7. Troubleshooting"),
        ("h3", "ts-qt", "Qt is not found"),
        ("code", ERR_QT),
        ("p", "Make sure Qt 6.7 or later and the required modules are installed. For Qt from the Qt Online Installer, pass "
              "`-DCMAKE_PREFIX_PATH`. After switching Qt or the CMake generator, start over in a new build directory."),
        ("h3", "ts-submodule", "Hayanagi sources are missing"),
        ("p", "Run `git submodule update --init --recursive` in the repository root."),
        ("h3", "ts-tsume", "The script stops because of multiple collections"),
        ("code", ERR_TSUME),
        ("p", "An older version of a collection remains in `data/tsumeshogi/` (the message says that several collections were "
              "found for one move count). Delete the unused version so that one file remains per move count."),
        ("h3", "ts-qt-notices", "Qt notice validation fails"),
        ("code", ERR_QT_NOTICES),
        ("p", "Prepare the notices for the same Qt version used for the build ([section 4](#qt-notices)): Qt 6.7.2 in the "
              "container, or the version reported by `pacman -Q qt6-base` on Arch Linux. Do not ship a package after a failed step."),
        ("h3", "ts-bundled", "Collecting bundled library notices fails"),
        ("code", ERR_BUNDLED),
        ("ul", [
            "Create the package on Arch Linux with Arch's Qt packages. If the AppImage contains libraries that do not belong "
            "to a pacman package, such as Qt from the Qt Online Installer, their package cannot be identified and the script stops.",
            "`No license text for …` means that no license text was found for a package. Older repositories may use license "
            "fields that are not SPDX expressions; add the upstream expression to `LICENSE_OVERRIDES` in "
            "`scripts/bundled_licenses.py`. Put texts that a package lacks, such as a notice with the actual copyright line, in "
            "`scripts/license-texts/<package>/`.",
        ]),
        ("h3", "ts-docker", "docker cannot be used"),
        ("code", ERR_DOCKER),
        ("p", "Add your user to the docker group and log in again (`sudo usermod -aG docker $USER`), or set "
              "`DOCKER=\"sudo docker\"`."),
        ("h3", "ts-linuxdeploy", "linuxdeploy does not start"),
        ("code", ERR_FUSE_TOOL),
        ("p", "Install `fuse2`, or run `APPIMAGE_EXTRACT_AND_RUN=1 ./scripts/build-linux.sh`."),
        ("h3", "ts-fuse", "The AppImage does not start (FUSE)"),
        ("code", ERR_FUSE),
        ("p", "Install FUSE 2, or start it with `--appimage-extract-and-run`."),
        ("code", EXTRACT_RUN),
        ("h3", "ts-glibc", "The glibc version is too old"),
        ("code", ERR_GLIBC),
        ("p", "The system's glibc is older than that of the build system. Build release packages in the container "
              "([5.1](#package-run)). If that is still too new, use a distribution with a newer glibc "
              "(see the glibc note in [Requirements](#requirements))."),
        ("h3", "ts-libs", "System libraries are missing"),
        ("code", ERR_LIBS),
        ("p", "Libraries that are not bundled in the AppImage (OpenGL, fontconfig, HarfBuzz, wayland-client) are missing "
              "on the system. Example for Ubuntu / Debian:"),
        ("code", LIBS_UBUNTU),
        ("h3", "ts-xcb", "The xcb platform plugin cannot be loaded"),
        ("code", ERR_XCB),
        ("p", "X11 libraries are missing. Examples for Ubuntu / Debian, Fedora, and Arch Linux, in that order:"),
        ("code", XCB),
        ("h3", "ts-im", "Japanese text cannot be entered"),
        ("p", "The AppImage bundles only the X11 platform plugin, so it runs through XWayland on Wayland desktops. If the "
              "`QT_IM_MODULE` environment variable is not set, no Japanese input plugin is selected. Start it with "
              "`QT_IM_MODULE=fcitx` for fcitx5, or `QT_IM_MODULE=ibus` for IBus."),
        ("code", IM),
        ("h3", "ts-translations", "Translations are not loaded"),
        ("p", "Check that the `.qm` files are in `usr/bin/`, next to the executable."),
        ("code", QM),
    ],
}

# ======================================================================= 简体中文
T["zh-cn"] = {
    "title": "Linux 构建与发布",
    "subtitle": "在 Linux 上构建 ShogiBoardQ，并以包含 AppImage 的 ZIP 在 GitHub 上发布",
    "description": "介绍如何在 Linux 上构建 ShogiBoardQ，制作包含 AppImage、诘棋题集和 Hayanagi 的 ZIP，并在 GitHub Releases 上发布。内容包括开发环境、Qt 许可文档的准备、AppImage 与 ZIP 的制作以及故障排除。",
    "summary": "从搭建 Linux 开发环境、构建，到制作 AppImage 与 ZIP 并在 GitHub Releases 上发布。",
    "badges": ["Linux", "Qt 6", "AppImage", "GitHub Releases"],
    "lang_aria": "界面语言",
    "menu_aria": "菜单",
    "nav": [("首页", "../index.html"), ("使用指南（英文）", "../../en/guide/index.html"),
            ("从源代码构建", "../index.html#build"), ("目录", "#contents"), ("GitHub", REPO)],
    "back": ("返回“从源代码构建”", "../index.html#build"),
    "blocks": [
        ("p", "本文介绍如何在 Linux 上构建 ShogiBoardQ，制作 `ShogiBoardQ-linux.zip` 并在 GitHub Releases 上发布。"
              "ZIP 中包含 AppImage（应用程序本体）、3・5・7・9・11・13 手诘的诘棋题集（各 1,000 题，共 6,000 题），"
              "以及用于普通对局的 USI 引擎 Hayanagi。**发布时 Linux 只附加这一个 ZIP。**"),
        ("note", "**目前只能在 Arch Linux 上制作发布包。** AppImage 中随附的库的许可文档要通过 pacman 收集，"
                 "因此使用 Arch Linux 的 Qt 软件包制作。用于发布的发布包在较旧的 Arch Linux 容器中制作（[5.1](#package-run)）。"
                 "应用程序本身的构建和开发也可以在其他发行版上进行。"),
        ("h2", "contents", "目录"),
        ("toc", [("requirements", "前提条件"), ("setup", "搭建开发环境"), ("build", "获取源代码并构建"),
                 ("qt-notices", "准备 Qt 的许可文档"), ("package", "制作 AppImage 与 ZIP"),
                 ("release", "在 GitHub Releases 上发布"), ("troubleshooting", "故障排除")]),

        ("h2", "requirements", "1. 前提条件"),
        ("table", ["项目", "要求"], [
            ["操作系统", "x86_64 的 Linux。制作发布包（AppImage 与 ZIP）需要 Arch Linux（用于发布时为 Docker 容器中的 Arch Linux）"],
            ["编译器", "支持 C++17 的 GCC 9 及以上，或 Clang 10 及以上"],
            ["CMake", "3.16 及以上（推荐 Ninja）"],
            ["Qt", "6.7 及以上（Widgets、Charts、Network、Concurrent、Multimedia、Sql、LinguistTools）。制作发布包还需要 SVG 插件"],
            ["Python", "Python 3（发布用脚本）"],
            ["Docker", "在容器中制作用于发布的发布包时"],
            ["其他", "Git、curl、file、FUSE 2（运行 AppImage 格式的工具）、ImageMagick（缩小超过 512px 的图标）"],
        ], "labels"),
        ("p", "制作发布包时需要联网，因为首次会下载 linuxdeploy 和 appimagetool。"),
        ("note", "**关于 glibc：** AppImage 只能在 glibc 与构建环境相同或更新的系统上运行。在最新的 Arch Linux 上构建时，"
                 "需要 glibc 2.43 及以上，无法在 Ubuntu 24.04（glibc 2.39）等系统上启动。因此用于发布的发布包在固定为 "
                 "2024 年 7 月 15 日 Arch Linux（glibc 2.39、Qt 6.7.2）的容器中构建（[5.1](#package-run)）。该 AppImage 需要 "
                 "glibc 2.38 及以上和 GCC 12 及以上的 libstdc++，可在 Ubuntu 24.04 及以上、Debian 13、Fedora 39 及以上等系统上运行。"),

        ("h2", "setup", "2. 搭建开发环境"),
        ("h3", "setup-arch", "2.1 Arch Linux 与 Docker（制作发布包时）"),
        ("p", "用于发布的发布包在容器中构建，因此本机只需要 Docker、Git 和 Python 3（Qt 等构建所需的软件在容器中）。"
              "请把用户加入 docker 组并重新登录。"),
        ("code", DOCKER_GROUP),
        ("note", "docker 组的用户可以通过 docker 进行相当于 root 的操作。"),
        ("p", "直接在 Arch Linux 上构建和开发时，安装构建工具和 Arch 的 Qt 软件包。fcitx5-qt 用于把日语输入（fcitx5）的"
              "输入法插件放进 AppImage；未安装时不会随附该插件。"),
        ("code", ARCH),
        ("h3", "setup-other", "2.2 其他发行版（仅构建应用程序）"),
        ("p", "在 Ubuntu、Debian 或 Fedora 上也可以构建和开发应用程序。需要 Qt 6.7 及以上，"
              "如果发行版提供的 Qt 较旧，请使用 Qt Online Installer 安装 Qt。"),
        ("p", "Ubuntu / Debian："),
        ("code", UBUNTU),
        ("p", "Fedora："),
        ("code", FEDORA),
        ("p", "使用 Qt Online Installer 安装的 Qt 时，在配置 CMake 时指定其位置（示例为 Qt 6.11.2）。"),
        ("code", QT_ONLINE),
        ("h3", "setup-check", "2.3 确认安装"),
        ("code", CHECK),

        ("h2", "build", "3. 获取源代码并构建"),
        ("p", "获取仓库时请一并获取 Hayanagi 子模块。缺少子模块时，构建所需的文件会不完整。"),
        ("code", CLONE),
        ("p", "对于已获取的仓库，运行 `git submodule update --init --recursive` 获取所记录版本的 Hayanagi。"
              "然后以 Release 方式构建并启动。"),
        ("code", BUILD),
        ("p", "运行测试："),
        ("code", TEST),
        ("p", "只构建而不制作发布包时，也可以使用 `./scripts/build-linux.sh --skip-appimage`。"),

        ("h2", "qt-notices", "4. 准备 Qt 的许可文档"),
        ("p", "AppImage 中收录从同版本 Qt 源代码中提取的许可文档，以及源代码来源的记录。"
              "请在制作发布包之前准备一次，更新 Qt 后重新准备。普通构建不需要。"),
        ("p", "Qt 的版本取决于构建环境。用于发布的容器（[5.1](#package-run)）使用 Qt 6.7.2，因此获取并校验 Qt 6.7.2 的"
              "源代码（qt-everywhere），把文档提取到 `build-container/qt-licenses`。SHA-256 请用官方下载站点上与源代码位于"
              "同一位置的 `.sha256` 文件确认。"),
        ("code", QT_SHA),
        ("code", QT_PREPARE),
        ("p", "在 `--provenance` 中写明所用 Qt 的来源（包括 Arch 软件包的版本，以及适用 Arch 的补丁和构建步骤）。"
              "该内容会记录在 `QT-SOURCE.json` 中，并显示在“版本信息”的“获取源代码”里。直接在 Arch Linux 上构建时，"
              "请在 `build/qt-licenses` 中准备与 `pacman -Q qt6-base` 所示版本相同的文档。"),
        ("ul", [
            "`prepare` 不会覆盖已有的输出目录。重新准备时请指定新的输出目录。",
            "默认位置为构建目录（容器为 `build-container`，直接构建为 `build`）下的 `qt-licenses`。使用会删除构建目录的 "
            "`--clean` 时，请在其外部生成文档，并通过环境变量 `SHOGIBOARDQ_QT_LICENSE_DIR` 指定。",
            f"不要把获取的 Qt 源代码和文档单独附加到发布中。方针详情请参阅 [Qt 文档与发布附件方针（日文）]({QT_LICENSING})。",
        ]),

        ("h2", "package", "5. 制作 AppImage 与 ZIP"),
        ("h3", "package-run", "5.1 运行脚本"),
        ("p", "用于发布的发布包在较旧的 Arch Linux（2024 年 7 月 15 日的 Arch Linux Archive；glibc 2.39、Qt 6.7.2）容器中制作。"),
        ("code", CONTAINER_RUN),
        ("ul", [
            "首次运行时会创建构建环境镜像（`scripts/linux-container/Dockerfile`，约 3GB）。从 Arch Linux Archive 下载较慢，需要数十分钟。",
            "在容器中以自己的用户运行 `scripts/build-linux.sh`。构建目录与平时的 `build` 分开，使用 `build-container`，"
            "输出放在仓库根目录。",
            "为当时的软件包签名的密钥中，有些后来已过期。镜像中用官方密钥环验证签名，但不要求密钥当前受信任。",
        ]),
        ("p", "也可以直接在 Arch Linux 上制作。这样制作的 AppImage 需要本机的 glibc 版本及以上（2026 年 10 月时为 2.43）。"),
        ("code", RUN),
        ("p", "两个脚本都可以使用以下选项："),
        ("table", ["选项", "说明"], [
            ["`--skip-appimage`", "只构建，不制作 AppImage 与 ZIP"],
            ["`--clean`", "删除构建目录后再构建"],
            ["`--help`", "显示帮助"],
        ], "labels"),
        ("table", ["环境变量", "说明"], [
            ["`SHOGIBOARDQ_BUILD_DIR`", "构建目录（默认为 `build`；容器中为 `build-container`）"],
            ["`SHOGIBOARDQ_QT_LICENSE_DIR`", "已准备的 Qt 文档的位置（默认为构建目录下的 `qt-licenses`）"],
            ["`APPIMAGE_EXTRACT_AND_RUN=1`", "在无法使用 FUSE 的环境中，解压并运行 AppImage 格式的工具（容器中始终指定）"],
            ["`DOCKER`", "仅用于容器。docker 命令（例如 `sudo docker`）"],
            ["`SHOGIBOARDQ_ARCH_SNAPSHOT`", "仅用于容器。Arch Linux Archive 的日期（默认为 `2024/07/15`）"],
        ], "labels"),
        ("p", "`build-linux.sh` 依次执行以下处理（在容器中，于容器内执行同样的处理）："),
        ("ol", [
            "检查所需工具（cmake、python3；推荐 ninja）",
            "配置 CMake 并以 Release 方式构建（ShogiBoardQ 与 Hayanagi）",
            "检查可执行文件、翻译文件，以及 3〜13 手诘的题集（每种手数各 1 个文件）",
            "下载 linuxdeploy、其 Qt 插件和 appimagetool（仅首次；保存在构建目录中）",
            "创建 AppDir 并生成 AppImage（运行所需的文件，以及 Qt 与随附库的许可文档）",
            "制作包含 AppImage、题集和 Hayanagi 的 ZIP",
        ]),
        ("p", "输出为仓库根目录下的 `ShogiBoardQ-linux-x86_64.AppImage` 和 `ShogiBoardQ-linux.zip`。"
              "工作目录为构建目录下的 `AppDir` 和 `ShogiBoardQ-linux/`。"),
        ("h3", "appimage", "5.2 AppImage 的内容"),
        ("p", "AppImage 中只收录启动和运行应用程序所需的文件以及许可文档。"),
        ("code", appimage_tree({
            "extracted": "（解压后）", "apprun": "只使用随附的 Qt 启动", "exe": "可执行文件",
            "qtconf": "Qt 插件的位置", "qm": "应用程序翻译（4 种语言）", "lib": "Qt 及其依赖库",
            "im": "日语输入（compose、fcitx5、ibus）", "sqlite": "诘棋作答记录与分析缓存",
            "qt": "随附的 Qt 模块的文档", "source": "Qt 的来源与构建时版本",
            "third": "Qt 以外随附库的文档"})),
        ("ul", [
            "不收录题集、普通对局用的 Hayanagi、说明文档和题集的校验记录（`validation_*.json`）。应用程序不会从 AppImage "
            "内部读取它们，用户也无法在其中选择文件，因此只作为 ZIP 中 AppImage 之外的文件发布。诘棋练习用的 Hayanagi "
            "已内置于应用程序中。",
            "删除 linuxdeploy 放入的 Qt 标准翻译（`usr/translations/`）。标准对话框的日语和中文翻译已内置于可执行文件，"
            "应用程序的翻译从 `usr/bin/` 读取。",
            "不收录用不到的插件：OpenGL 集成（`xcbglintegrations`）、通信加密（`tls`）和 GIF 图像。没有使用 OpenGL 的界面部件，"
            "CSA 联网对局使用不加密的 TCP，也不进行 HTTPS 等通信。",
            "Qt 的许可文档只收录随附的 Qt 模块（qtbase、qtcharts、qtmultimedia、qtsvg、qttranslations、qtwayland）"
            "及其引用的文档（`scripts/qt_licenses.py`）。不收录 WebEngine 等未发布模块的文档。",
            "对于 Qt 以外的随附库（glib、PulseAudio、OpenSSL、fcitx5-qt 等约 50 个软件包），`scripts/bundled_licenses.py` "
            "通过 pacman 查找所属软件包，把许可原文放入 `third-party/`，并在 `THIRD-PARTY-NOTICES.md` 中列出版本、许可和"
            "源代码获取位置。两者分别显示在“版本信息”的“Qt 中的第三方许可证”和“随附库的许可证”中。",
        ]),
        ("h3", "zip", "5.3 ZIP 的内容"),
        ("p", "ZIP 中放入以下文件。解压 ZIP 后即可选择题集和 Hayanagi。"),
        ("table", ["源文件", "在 ZIP 中的位置"], ZIP_ROWS),
        ("code", zip_tree({"puzzles": "6 个文件（各 1,000 题）"})),
        ("ul", [
            "题集按原样复制。其哈希值与应用程序内置的审核记录一致，因此读取时可以复用已验证的手数和手顺。",
            "`data/tsumeshogi/` 中每种手数只放 1 个题集文件（仓库只跟踪当前的 20261001 版）。"
            "同一手数有多个文件时，脚本会报错并停止。",
            "Hayanagi 经 `strip` 后放入。ZIP 中 AppImage 之外不放 `docs/`、`validation_*.json` 和 `licenses/`，"
            "许可文档位于 AppImage 内。",
        ]),
        ("p", "用户按以下方式启动。在诘棋练习的“打开题集…”中选择 `data/tsumeshogi/` 中的题集；"
              "登记普通对局用的引擎时选择 `Hayanagi/hayanagi`。"),
        ("code", UNZIP),
        ("h3", "test", "5.4 运行确认"),
        ("code", test_code({
            "start": "启动", "contents": "解压并确认内容（不含 Hayanagi 和 Qt 标准翻译）",
            "glibc": "所需的 glibc 版本", "hayanagi": "ZIP 中的 Hayanagi 能作为 USI 引擎响应",
            "zip": "ZIP 由预期的 12 个文件组成且没有损坏"})),
        ("p", "在容器中构建时，所需的 glibc 版本应为 `GLIBC_2.38` 及以下。AppImage 使用系统中的 OpenGL（libEGL、libOpenGL）、"
              "fontconfig、HarfBuzz 和 wayland-client（普通桌面环境中已安装）。"),
        ("note", "请在 `LD_LIBRARY_PATH` 不包含 Qt 的 lib 目录的环境中确认，否则会使用系统中的库，发现不了遗漏的文件。"
                 "如有可能，也请在 Ubuntu 24.04 等其他环境中确认日语、英语、中文的切换、SVG 棋子、棋子音效、"
                 "诘棋记录的保存，以及“版本信息”中的许可证列表。"),

        ("h2", "release", "6. 在 GitHub Releases 上发布"),
        ("p", "发布的标签使用应用程序的版本（`CMakeLists.txt` 中的 `APP_VERSION`，例如 2026.10.06）。"
              "Linux 只附加 `ShogiBoardQ-linux.zip`，文件名不随版本变化。"),
        ("code", RELEASE),
        ("p", "如果已经先发布了其他操作系统的发布包，则添加到现有的发布中："),
        ("code", UPLOAD),
        ("ul", [
            "按文件名指定要附加的文件。不要上传整个工作目录或 `build/` 中的文件。",
            "不要单独附加 Qt 源代码、`QT-SOURCE.json`、校验和或 SBOM 等。许可文档和源代码的获取方法位于 AppImage 内。",
            "发布后请确认附件列表。GitHub 自动显示的 Source code 链接与手动附加的文件是分开的。",
        ]),
        ("p", "在发布说明中写明 Linux 版的启动方法和运行条件（所需的 glibc 版本等）。"),
        ("code", """
### 在 Linux 上启动

1. 下载并解压 ShogiBoardQ-linux.zip
2. chmod +x ShogiBoardQ-linux-x86_64.AppImage Hayanagi/hayanagi
3. ./ShogiBoardQ-linux-x86_64.AppImage

运行条件：x86_64 的 Linux，glibc 2.38 及以上（Ubuntu 24.04 及以上、Debian 13、Fedora 39 及以上等）
没有 FUSE 时，可加上 --appimage-extract-and-run 启动。
"""),
        ("note", f"仓库中的 [release.yml]({RELEASE_YML}) 会在推送以 `v` 开头的标签或手动运行时构建三种操作系统，"
                 "但其 Linux 任务运行在 Ubuntu 上。在随附库许可文档的收集支持 Ubuntu 之前，该任务无法制作 Linux 发布包，"
                 "因此目前手动附加在 Arch Linux 容器中制作的 ZIP。"),

        ("h2", "troubleshooting", "7. 故障排除"),
        ("h3", "ts-qt", "找不到 Qt"),
        ("code", ERR_QT),
        ("p", "确认已安装 Qt 6.7 及以上和所需模块。使用 Qt Online Installer 的 Qt 时，用 `-DCMAKE_PREFIX_PATH` 指定。"
              "更换 Qt 或 CMake 生成器后，请在新的 build 目录中重新开始。"),
        ("h3", "ts-submodule", "缺少 Hayanagi 的源代码"),
        ("p", "在仓库根目录运行 `git submodule update --init --recursive`。"),
        ("h3", "ts-tsume", "因存在多个题集而停止"),
        ("code", ERR_TSUME),
        ("p", "`data/tsumeshogi/` 中残留了旧版题集（消息表示同一手数找到了多个题集）。请删除不用的版本，"
              "使每种手数只剩 1 个文件。"),
        ("h3", "ts-qt-notices", "Qt 文档的校验失败"),
        ("code", ERR_QT_NOTICES),
        ("p", "请准备与构建所用 Qt 同版本的文档（[第 4 节](#qt-notices)）：容器中为 Qt 6.7.2，直接在 Arch Linux 上构建时为 "
              "`pacman -Q qt6-base` 所示的版本。不要忽略失败而继续发布。"),
        ("h3", "ts-bundled", "收集随附库的文档时停止"),
        ("code", ERR_BUNDLED),
        ("ul", [
            "请在 Arch Linux 上使用 Arch 的 Qt 软件包制作发布包。如果 AppImage 中含有不属于 pacman 软件包的库"
            "（例如通过 Qt Online Installer 安装的 Qt），就无法确定其所属软件包，脚本会停止。",
            "`No license text for …` 表示有软件包找不到许可原文。较旧的软件仓库中，许可字段可能不是 SPDX 表达式，"
            "请在 `scripts/bundled_licenses.py` 的 `LICENSE_OVERRIDES` 中加入上游的写法。软件包中没有的文档"
            "（例如带有实际版权声明的原文）放在 `scripts/license-texts/<软件包名>/` 中。",
        ]),
        ("h3", "ts-docker", "无法使用 docker"),
        ("code", ERR_DOCKER),
        ("p", "把用户加入 docker 组并重新登录（`sudo usermod -aG docker $USER`），或指定环境变量 `DOCKER=\"sudo docker\"`。"),
        ("h3", "ts-linuxdeploy", "linuxdeploy 无法启动"),
        ("code", ERR_FUSE_TOOL),
        ("p", "安装 `fuse2`，或用 `APPIMAGE_EXTRACT_AND_RUN=1 ./scripts/build-linux.sh` 运行。"),
        ("h3", "ts-fuse", "AppImage 无法启动（FUSE）"),
        ("code", ERR_FUSE),
        ("p", "安装 FUSE 2，或加上 `--appimage-extract-and-run` 启动。"),
        ("code", EXTRACT_RUN),
        ("h3", "ts-glibc", "glibc 版本不足"),
        ("code", ERR_GLIBC),
        ("p", "运行系统的 glibc 比构建环境旧。用于发布的发布包请在容器中构建（[5.1](#package-run)）。仍然不足时，请在 glibc "
              "更新的发行版上使用（参阅[前提条件](#requirements)中关于 glibc 的说明）。"),
        ("h3", "ts-libs", "找不到系统库"),
        ("code", ERR_LIBS),
        ("p", "系统中缺少 AppImage 不随附的库（OpenGL、fontconfig、HarfBuzz、wayland-client）。Ubuntu / Debian 的示例："),
        ("code", LIBS_UBUNTU),
        ("h3", "ts-xcb", "无法加载 xcb 平台插件"),
        ("code", ERR_XCB),
        ("p", "缺少 X11 相关的库。以下依次为 Ubuntu / Debian、Fedora、Arch Linux 的示例："),
        ("code", XCB),
        ("h3", "ts-im", "无法输入日语"),
        ("p", "AppImage 只随附 X11 平台插件，因此在 Wayland 桌面上也会通过 XWayland 运行。未设置环境变量 `QT_IM_MODULE` 时，"
              "不会选用日语输入法插件。使用 fcitx5 时请加上 `QT_IM_MODULE=fcitx`，使用 IBus 时请加上 `QT_IM_MODULE=ibus` 启动。"),
        ("code", IM),
        ("h3", "ts-translations", "无法加载翻译"),
        ("p", "确认 `.qm` 文件位于可执行文件所在的 `usr/bin/` 中。"),
        ("code", QM),
    ],
}

# ======================================================================= 繁體中文
T["zh-tw"] = {
    "title": "Linux 建置與發行",
    "subtitle": "在 Linux 上建置 ShogiBoardQ，並以包含 AppImage 的 ZIP 在 GitHub 上發行",
    "description": "說明如何在 Linux 上建置 ShogiBoardQ，製作包含 AppImage、詰棋題集和 Hayanagi 的 ZIP，並在 GitHub Releases 上發行。內容包括開發環境、Qt 授權文件的準備、AppImage 與 ZIP 的製作以及疑難排解。",
    "summary": "從建立 Linux 開發環境、建置，到製作 AppImage 與 ZIP 並在 GitHub Releases 上發行。",
    "badges": ["Linux", "Qt 6", "AppImage", "GitHub Releases"],
    "lang_aria": "介面語言",
    "menu_aria": "選單",
    "nav": [("首頁", "../index.html"), ("使用指南（英文）", "../../en/guide/index.html"),
            ("從原始碼建置", "../index.html#build"), ("目錄", "#contents"), ("GitHub", REPO)],
    "back": ("返回「從原始碼建置」", "../index.html#build"),
    "blocks": [
        ("p", "本文說明如何在 Linux 上建置 ShogiBoardQ，製作 `ShogiBoardQ-linux.zip` 並在 GitHub Releases 上發行。"
              "ZIP 中包含 AppImage（應用程式本體）、3・5・7・9・11・13 手詰的詰棋題集（各 1,000 題，共 6,000 題），"
              "以及用於一般對局的 USI 引擎 Hayanagi。**發行時 Linux 只附加這一個 ZIP。**"),
        ("note", "**目前只能在 Arch Linux 上製作發行套件。** AppImage 中隨附的程式庫的授權文件要透過 pacman 收集，"
                 "因此使用 Arch Linux 的 Qt 套件製作。用於發行的發行套件在較舊的 Arch Linux 容器中製作（[5.1](#package-run)）。"
                 "應用程式本身的建置與開發也可以在其他發行版上進行。"),
        ("h2", "contents", "目錄"),
        ("toc", [("requirements", "前提條件"), ("setup", "建立開發環境"), ("build", "取得原始碼並建置"),
                 ("qt-notices", "準備 Qt 的授權文件"), ("package", "製作 AppImage 與 ZIP"),
                 ("release", "在 GitHub Releases 上發行"), ("troubleshooting", "疑難排解")]),

        ("h2", "requirements", "1. 前提條件"),
        ("table", ["項目", "需求"], [
            ["作業系統", "x86_64 的 Linux。製作發行套件（AppImage 與 ZIP）需要 Arch Linux（用於發行時為 Docker 容器中的 Arch Linux）"],
            ["編譯器", "支援 C++17 的 GCC 9 以上，或 Clang 10 以上"],
            ["CMake", "3.16 以上（建議使用 Ninja）"],
            ["Qt", "6.7 以上（Widgets、Charts、Network、Concurrent、Multimedia、Sql、LinguistTools）。製作發行套件還需要 SVG 外掛程式"],
            ["Python", "Python 3（發行用指令碼）"],
            ["Docker", "在容器中製作用於發行的發行套件時"],
            ["其他", "Git、curl、file、FUSE 2（執行 AppImage 格式的工具）、ImageMagick（縮小超過 512px 的圖示）"],
        ], "labels"),
        ("p", "製作發行套件時需要連上網路，因為第一次會下載 linuxdeploy 和 appimagetool。"),
        ("note", "**關於 glibc：** AppImage 只能在 glibc 與建置環境相同或更新的系統上執行。在最新的 Arch Linux 上建置時，"
                 "需要 glibc 2.43 以上，無法在 Ubuntu 24.04（glibc 2.39）等系統上啟動。因此用於發行的發行套件在固定為 "
                 "2024 年 7 月 15 日 Arch Linux（glibc 2.39、Qt 6.7.2）的容器中建置（[5.1](#package-run)）。該 AppImage 需要 "
                 "glibc 2.38 以上和 GCC 12 以上的 libstdc++，可在 Ubuntu 24.04 以上、Debian 13、Fedora 39 以上等系統上執行。"),

        ("h2", "setup", "2. 建立開發環境"),
        ("h3", "setup-arch", "2.1 Arch Linux 與 Docker（製作發行套件時）"),
        ("p", "用於發行的發行套件在容器中建置，因此本機只需要 Docker、Git 和 Python 3（Qt 等建置所需的軟體在容器中）。"
              "請將使用者加入 docker 群組並重新登入。"),
        ("code", DOCKER_GROUP),
        ("note", "docker 群組的使用者可以透過 docker 進行相當於 root 的操作。"),
        ("p", "直接在 Arch Linux 上建置與開發時，安裝建置工具和 Arch 的 Qt 套件。fcitx5-qt 用於將日文輸入（fcitx5）的"
              "輸入法外掛程式放進 AppImage；未安裝時不會隨附該外掛程式。"),
        ("code", ARCH),
        ("h3", "setup-other", "2.2 其他發行版（僅建置應用程式）"),
        ("p", "在 Ubuntu、Debian 或 Fedora 上也可以建置與開發應用程式。需要 Qt 6.7 以上，"
              "若發行版提供的 Qt 較舊，請使用 Qt Online Installer 安裝 Qt。"),
        ("p", "Ubuntu / Debian："),
        ("code", UBUNTU),
        ("p", "Fedora："),
        ("code", FEDORA),
        ("p", "使用 Qt Online Installer 安裝的 Qt 時，在設定 CMake 時指定其位置（範例為 Qt 6.11.2）。"),
        ("code", QT_ONLINE),
        ("h3", "setup-check", "2.3 確認安裝"),
        ("code", CHECK),

        ("h2", "build", "3. 取得原始碼並建置"),
        ("p", "取得儲存庫時請一併取得 Hayanagi 子模組。缺少子模組時，建置所需的檔案會不完整。"),
        ("code", CLONE),
        ("p", "對於已取得的儲存庫，執行 `git submodule update --init --recursive` 取得所記錄版本的 Hayanagi。"
              "接著以 Release 方式建置並啟動。"),
        ("code", BUILD),
        ("p", "執行測試："),
        ("code", TEST),
        ("p", "只建置而不製作發行套件時，也可以使用 `./scripts/build-linux.sh --skip-appimage`。"),

        ("h2", "qt-notices", "4. 準備 Qt 的授權文件"),
        ("p", "AppImage 中收錄從同版本 Qt 原始碼中擷取的授權文件，以及原始碼來源的紀錄。"
              "請在製作發行套件之前準備一次，更新 Qt 後重新準備。一般建置不需要。"),
        ("p", "Qt 的版本取決於建置環境。用於發行的容器（[5.1](#package-run)）使用 Qt 6.7.2，因此取得並驗證 Qt 6.7.2 的"
              "原始碼（qt-everywhere），將文件擷取到 `build-container/qt-licenses`。SHA-256 請用官方下載網站上與原始碼位於"
              "同一位置的 `.sha256` 檔案確認。"),
        ("code", QT_SHA),
        ("code", QT_PREPARE),
        ("p", "在 `--provenance` 中寫明所用 Qt 的來源（包括 Arch 套件的版本，以及適用 Arch 的修補程式和建置步驟）。"
              "該內容會記錄在 `QT-SOURCE.json` 中，並顯示在「版本資訊」的「取得原始碼」裡。直接在 Arch Linux 上建置時，"
              "請在 `build/qt-licenses` 中準備與 `pacman -Q qt6-base` 所示版本相同的文件。"),
        ("ul", [
            "`prepare` 不會覆寫既有的輸出目錄。重新準備時請指定新的輸出目錄。",
            "預設位置為建置目錄（容器為 `build-container`，直接建置為 `build`）下的 `qt-licenses`。使用會刪除建置目錄的 "
            "`--clean` 時，請在其外部產生文件，並透過環境變數 `SHOGIBOARDQ_QT_LICENSE_DIR` 指定。",
            f"不要將取得的 Qt 原始碼和文件單獨附加到發行中。方針詳情請參閱 [Qt 文件與發行附件方針（日文）]({QT_LICENSING})。",
        ]),

        ("h2", "package", "5. 製作 AppImage 與 ZIP"),
        ("h3", "package-run", "5.1 執行指令碼"),
        ("p", "用於發行的發行套件在較舊的 Arch Linux（2024 年 7 月 15 日的 Arch Linux Archive；glibc 2.39、Qt 6.7.2）容器中製作。"),
        ("code", CONTAINER_RUN),
        ("ul", [
            "第一次執行時會建立建置環境映像（`scripts/linux-container/Dockerfile`，約 3GB）。從 Arch Linux Archive 下載較慢，需要數十分鐘。",
            "在容器中以自己的使用者執行 `scripts/build-linux.sh`。建置目錄與平時的 `build` 分開，使用 `build-container`，"
            "輸出放在儲存庫根目錄。",
            "為當時的套件簽章的金鑰中，有些後來已過期。映像中以官方金鑰圈驗證簽章，但不要求金鑰目前受信任。",
        ]),
        ("p", "也可以直接在 Arch Linux 上製作。這樣製作的 AppImage 需要本機的 glibc 版本以上（2026 年 10 月時為 2.43）。"),
        ("code", RUN),
        ("p", "兩個指令碼都可以使用以下選項："),
        ("table", ["選項", "說明"], [
            ["`--skip-appimage`", "只建置，不製作 AppImage 與 ZIP"],
            ["`--clean`", "刪除建置目錄後再建置"],
            ["`--help`", "顯示說明"],
        ], "labels"),
        ("table", ["環境變數", "說明"], [
            ["`SHOGIBOARDQ_BUILD_DIR`", "建置目錄（預設為 `build`；容器中為 `build-container`）"],
            ["`SHOGIBOARDQ_QT_LICENSE_DIR`", "已準備的 Qt 文件的位置（預設為建置目錄下的 `qt-licenses`）"],
            ["`APPIMAGE_EXTRACT_AND_RUN=1`", "在無法使用 FUSE 的環境中，解壓縮並執行 AppImage 格式的工具（容器中一律指定）"],
            ["`DOCKER`", "僅用於容器。docker 指令（例如 `sudo docker`）"],
            ["`SHOGIBOARDQ_ARCH_SNAPSHOT`", "僅用於容器。Arch Linux Archive 的日期（預設為 `2024/07/15`）"],
        ], "labels"),
        ("p", "`build-linux.sh` 依序執行以下處理（在容器中，於容器內執行同樣的處理）："),
        ("ol", [
            "檢查所需工具（cmake、python3；建議使用 ninja）",
            "設定 CMake 並以 Release 方式建置（ShogiBoardQ 與 Hayanagi）",
            "檢查執行檔、翻譯檔，以及 3〜13 手詰的題集（每種手數各 1 個檔案）",
            "下載 linuxdeploy、其 Qt 外掛程式和 appimagetool（僅第一次；儲存在建置目錄中）",
            "建立 AppDir 並產生 AppImage（執行所需的檔案，以及 Qt 與隨附程式庫的授權文件）",
            "製作包含 AppImage、題集和 Hayanagi 的 ZIP",
        ]),
        ("p", "輸出為儲存庫根目錄下的 `ShogiBoardQ-linux-x86_64.AppImage` 和 `ShogiBoardQ-linux.zip`。"
              "工作目錄為建置目錄下的 `AppDir` 和 `ShogiBoardQ-linux/`。"),
        ("h3", "appimage", "5.2 AppImage 的內容"),
        ("p", "AppImage 中只收錄啟動與執行應用程式所需的檔案以及授權文件。"),
        ("code", appimage_tree({
            "extracted": "（解壓縮後）", "apprun": "只使用隨附的 Qt 啟動", "exe": "執行檔",
            "qtconf": "Qt 外掛程式的位置", "qm": "應用程式翻譯（4 種語言）", "lib": "Qt 及其相依程式庫",
            "im": "日文輸入（compose、fcitx5、ibus）", "sqlite": "詰棋作答紀錄與分析快取",
            "qt": "隨附的 Qt 模組的文件", "source": "Qt 的來源與建置時版本",
            "third": "Qt 以外隨附程式庫的文件"})),
        ("ul", [
            "不收錄題集、一般對局用的 Hayanagi、說明文件和題集的驗證紀錄（`validation_*.json`）。應用程式不會從 AppImage "
            "內部讀取它們，使用者也無法在其中選擇檔案，因此只作為 ZIP 中 AppImage 之外的檔案發行。練習詰棋用的 Hayanagi "
            "已內建於應用程式中。",
            "刪除 linuxdeploy 放入的 Qt 標準翻譯（`usr/translations/`）。標準對話方塊的日文與中文翻譯已內建於執行檔，"
            "應用程式的翻譯從 `usr/bin/` 讀取。",
            "不收錄用不到的外掛程式：OpenGL 整合（`xcbglintegrations`）、通訊加密（`tls`）和 GIF 圖片。沒有使用 OpenGL 的介面元件，"
            "CSA 網路對局使用不加密的 TCP，也不進行 HTTPS 等通訊。",
            "Qt 的授權文件只收錄隨附的 Qt 模組（qtbase、qtcharts、qtmultimedia、qtsvg、qttranslations、qtwayland）"
            "及其參照的文件（`scripts/qt_licenses.py`）。不收錄 WebEngine 等未發行模組的文件。",
            "對於 Qt 以外的隨附程式庫（glib、PulseAudio、OpenSSL、fcitx5-qt 等約 50 個套件），`scripts/bundled_licenses.py` "
            "透過 pacman 查找所屬套件，將授權原文放入 `third-party/`，並在 `THIRD-PARTY-NOTICES.md` 中列出版本、授權和"
            "原始碼取得位置。兩者分別顯示在「版本資訊」的「Qt 中的第三方授權」和「隨附程式庫的授權」中。",
        ]),
        ("h3", "zip", "5.3 ZIP 的內容"),
        ("p", "ZIP 中放入以下檔案。解壓縮 ZIP 後即可選擇題集和 Hayanagi。"),
        ("table", ["來源檔案", "在 ZIP 中的位置"], ZIP_ROWS),
        ("code", zip_tree({"puzzles": "6 個檔案（各 1,000 題）"})),
        ("ul", [
            "題集原樣複製。其雜湊值與應用程式內建的稽核紀錄一致，因此讀取時可以沿用已驗證的手數和手順。",
            "`data/tsumeshogi/` 中每種手數只放 1 個題集檔案（儲存庫只追蹤目前的 20261001 版）。"
            "同一手數有多個檔案時，指令碼會報錯並停止。",
            "Hayanagi 經 `strip` 後放入。ZIP 中 AppImage 之外不放 `docs/`、`validation_*.json` 和 `licenses/`，"
            "授權文件位於 AppImage 內。",
        ]),
        ("p", "使用者依以下方式啟動。在練習詰棋的「開啟題集…」中選擇 `data/tsumeshogi/` 中的題集；"
              "登錄一般對局用的引擎時選擇 `Hayanagi/hayanagi`。"),
        ("code", UNZIP),
        ("h3", "test", "5.4 執行確認"),
        ("code", test_code({
            "start": "啟動", "contents": "解壓縮並確認內容（不含 Hayanagi 和 Qt 標準翻譯）",
            "glibc": "所需的 glibc 版本", "hayanagi": "ZIP 中的 Hayanagi 能作為 USI 引擎回應",
            "zip": "ZIP 由預期的 12 個檔案組成且沒有損毀"})),
        ("p", "在容器中建置時，所需的 glibc 版本應為 `GLIBC_2.38` 以下。AppImage 使用系統中的 OpenGL（libEGL、libOpenGL）、"
              "fontconfig、HarfBuzz 和 wayland-client（一般桌面環境中已安裝）。"),
        ("note", "請在 `LD_LIBRARY_PATH` 不包含 Qt 的 lib 目錄的環境中確認，否則會使用系統中的程式庫，無法發現遺漏的檔案。"
                 "如有可能，也請在 Ubuntu 24.04 等其他環境中確認日文、英文、中文的切換、SVG 棋子、棋子音效、"
                 "詰棋紀錄的儲存，以及「版本資訊」中的授權清單。"),

        ("h2", "release", "6. 在 GitHub Releases 上發行"),
        ("p", "發行的標籤使用應用程式的版本（`CMakeLists.txt` 中的 `APP_VERSION`，例如 2026.10.06）。"
              "Linux 只附加 `ShogiBoardQ-linux.zip`，檔名不隨版本改變。"),
        ("code", RELEASE),
        ("p", "如果已經先發行了其他作業系統的發行套件，則新增到既有的發行中："),
        ("code", UPLOAD),
        ("ul", [
            "以檔名指定要附加的檔案。不要上傳整個工作目錄或 `build/` 中的檔案。",
            "不要單獨附加 Qt 原始碼、`QT-SOURCE.json`、檢查碼或 SBOM 等。授權文件和原始碼的取得方式位於 AppImage 內。",
            "發行後請確認附件清單。GitHub 自動顯示的 Source code 連結與手動附加的檔案是分開的。",
        ]),
        ("p", "在發行說明中寫明 Linux 版的啟動方式和執行條件（所需的 glibc 版本等）。"),
        ("code", """
### 在 Linux 上啟動

1. 下載並解壓縮 ShogiBoardQ-linux.zip
2. chmod +x ShogiBoardQ-linux-x86_64.AppImage Hayanagi/hayanagi
3. ./ShogiBoardQ-linux-x86_64.AppImage

執行條件：x86_64 的 Linux，glibc 2.38 以上（Ubuntu 24.04 以上、Debian 13、Fedora 39 以上等）
沒有 FUSE 時，可加上 --appimage-extract-and-run 啟動。
"""),
        ("note", f"儲存庫中的 [release.yml]({RELEASE_YML}) 會在推送以 `v` 開頭的標籤或手動執行時建置三種作業系統，"
                 "但其 Linux 工作在 Ubuntu 上執行。在隨附程式庫授權文件的收集支援 Ubuntu 之前，該工作無法製作 Linux 發行套件，"
                 "因此目前以手動方式附加在 Arch Linux 容器中製作的 ZIP。"),

        ("h2", "troubleshooting", "7. 疑難排解"),
        ("h3", "ts-qt", "找不到 Qt"),
        ("code", ERR_QT),
        ("p", "確認已安裝 Qt 6.7 以上和所需模組。使用 Qt Online Installer 的 Qt 時，以 `-DCMAKE_PREFIX_PATH` 指定。"
              "更換 Qt 或 CMake 產生器後，請在新的 build 目錄中重新開始。"),
        ("h3", "ts-submodule", "缺少 Hayanagi 的原始碼"),
        ("p", "在儲存庫根目錄執行 `git submodule update --init --recursive`。"),
        ("h3", "ts-tsume", "因存在多個題集而停止"),
        ("code", ERR_TSUME),
        ("p", "`data/tsumeshogi/` 中殘留了舊版題集（訊息表示同一手數找到了多個題集）。請刪除不用的版本，"
              "使每種手數只剩 1 個檔案。"),
        ("h3", "ts-qt-notices", "Qt 文件的驗證失敗"),
        ("code", ERR_QT_NOTICES),
        ("p", "請準備與建置所用 Qt 同版本的文件（[第 4 節](#qt-notices)）：容器中為 Qt 6.7.2，直接在 Arch Linux 上建置時為 "
              "`pacman -Q qt6-base` 所示的版本。不要忽略失敗而繼續發行。"),
        ("h3", "ts-bundled", "收集隨附程式庫的文件時停止"),
        ("code", ERR_BUNDLED),
        ("ul", [
            "請在 Arch Linux 上使用 Arch 的 Qt 套件製作發行套件。若 AppImage 中含有不屬於 pacman 套件的程式庫"
            "（例如透過 Qt Online Installer 安裝的 Qt），就無法確定其所屬套件，指令碼會停止。",
            "`No license text for …` 表示有套件找不到授權原文。較舊的套件庫中，授權欄位可能不是 SPDX 運算式，"
            "請在 `scripts/bundled_licenses.py` 的 `LICENSE_OVERRIDES` 中加入上游的寫法。套件中沒有的文件"
            "（例如附有實際著作權聲明的原文）放在 `scripts/license-texts/<套件名稱>/` 中。",
        ]),
        ("h3", "ts-docker", "無法使用 docker"),
        ("code", ERR_DOCKER),
        ("p", "將使用者加入 docker 群組並重新登入（`sudo usermod -aG docker $USER`），或指定環境變數 `DOCKER=\"sudo docker\"`。"),
        ("h3", "ts-linuxdeploy", "linuxdeploy 無法啟動"),
        ("code", ERR_FUSE_TOOL),
        ("p", "安裝 `fuse2`，或以 `APPIMAGE_EXTRACT_AND_RUN=1 ./scripts/build-linux.sh` 執行。"),
        ("h3", "ts-fuse", "AppImage 無法啟動（FUSE）"),
        ("code", ERR_FUSE),
        ("p", "安裝 FUSE 2，或加上 `--appimage-extract-and-run` 啟動。"),
        ("code", EXTRACT_RUN),
        ("h3", "ts-glibc", "glibc 版本不足"),
        ("code", ERR_GLIBC),
        ("p", "執行系統的 glibc 比建置環境舊。用於發行的發行套件請在容器中建置（[5.1](#package-run)）。仍然不足時，請在 glibc "
              "較新的發行版上使用（參閱[前提條件](#requirements)中關於 glibc 的說明）。"),
        ("h3", "ts-libs", "找不到系統程式庫"),
        ("code", ERR_LIBS),
        ("p", "系統中缺少 AppImage 不隨附的程式庫（OpenGL、fontconfig、HarfBuzz、wayland-client）。Ubuntu / Debian 的範例："),
        ("code", LIBS_UBUNTU),
        ("h3", "ts-xcb", "無法載入 xcb 平台外掛程式"),
        ("code", ERR_XCB),
        ("p", "缺少 X11 相關的程式庫。以下依序為 Ubuntu / Debian、Fedora、Arch Linux 的範例："),
        ("code", XCB),
        ("h3", "ts-im", "無法輸入日文"),
        ("p", "AppImage 只隨附 X11 平台外掛程式，因此在 Wayland 桌面上也會透過 XWayland 執行。未設定環境變數 `QT_IM_MODULE` 時，"
              "不會選用日文輸入法外掛程式。使用 fcitx5 時請加上 `QT_IM_MODULE=fcitx`，使用 IBus 時請加上 `QT_IM_MODULE=ibus` 啟動。"),
        ("code", IM),
        ("h3", "ts-translations", "無法載入翻譯"),
        ("p", "確認 `.qm` 檔案位於執行檔所在的 `usr/bin/` 中。"),
        ("code", QM),
    ],
}
