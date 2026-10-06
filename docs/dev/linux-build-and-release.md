# Linux ビルド・リリース手順

<!-- scripts/docs/build_guide/gen_linux_build.py で生成。本文は texts_linux_build.py を編集する。 -->

公開ページ: https://hnakada123.github.io/ShogiBoardQ/guide/linux-build-and-release.html

ShogiBoardQ を Linux でビルドし、`ShogiBoardQ-linux.zip` を作って GitHub Release で公開する手順。ZIP には AppImage（アプリ本体）、3・5・7・9・11・13手詰の詰将棋問題集（各1,000題、計6,000題）、通常対局用の USI エンジン Hayanagi を収録する。**リリースに添付するのはこの ZIP 1個だけ。**

> **配布物の作成は Arch Linux だけに対応している。** AppImage に同梱するライブラリのライセンス文書を pacman で集めるため、Arch Linux の Qt パッケージを使って作る。リリース用は、古い Arch Linux のコンテナで作る（[5.1](#package-run)）。アプリのビルドと開発は、ほかのディストリビューションでもできる。

<a id="contents"></a>

## 目次

1. [前提条件](#requirements)
2. [開発環境のセットアップ](#setup)
3. [ソースの取得とビルド](#build)
4. [Qt のライセンス文書の準備](#qt-notices)
5. [AppImage と ZIP の作成](#package)
6. [GitHub Release での公開](#release)
7. [トラブルシューティング](#troubleshooting)

<a id="requirements"></a>

## 1. 前提条件

| 項目 | 内容 |
|---|---|
| OS | x86_64 の Linux。配布物（AppImage・ZIP）の作成は Arch Linux（リリース用は Docker のコンテナ内の Arch Linux） |
| コンパイラ | C++17 に対応した GCC 9 以降、または Clang 10 以降 |
| CMake | 3.16 以上（Ninja を推奨） |
| Qt | 6.7 以上（Widgets・Charts・Network・Concurrent・Multimedia・Sql・LinguistTools）。配布物の作成には SVG のプラグインも必要 |
| Python | Python 3（配布用のスクリプト） |
| Docker | リリース用の配布物をコンテナで作る場合 |
| その他 | Git、curl、file、FUSE 2（AppImage 形式のツールの実行）、ImageMagick（512px を超えるアイコンの縮小） |

配布物を作るときはネットワークに接続できる必要がある。初回に linuxdeploy と appimagetool をダウンロードするため。

> **glibc について**: AppImage は、ビルドした環境と同じか、より新しい glibc のシステムでしか動かない。最新の Arch Linux でビルドすると glibc 2.43 以降が必要になり、Ubuntu 24.04（glibc 2.39）などで起動できない。そのためリリース用は、2024年7月15日の Arch Linux（glibc 2.39・Qt 6.7.2）に固定したコンテナでビルドする（[5.1](#package-run)）。この AppImage は glibc 2.38 以降と GCC 12 以降の libstdc++ を必要とし、Ubuntu 24.04 以降、Debian 13、Fedora 39 以降などで動く。

<a id="setup"></a>

## 2. 開発環境のセットアップ

<a id="setup-arch"></a>

### 2.1 Arch Linux と Docker（配布物を作る場合）

リリース用の配布物はコンテナの中でビルドするので、手元には Docker・Git・Python 3 があればよい（Qt などのビルドに使うものはコンテナに入る）。ユーザーを docker グループに入れ、ログインし直す。

```
sudo usermod -aG docker $USER
```

> docker グループのユーザーは、docker を通じて root と同等の操作ができる。

手元の Arch Linux でビルド・開発する場合は、ビルドツールと Arch の Qt パッケージを入れる。fcitx5-qt は、日本語入力（fcitx5）用の入力プラグインを AppImage に同梱するために使う。入っていないと、このプラグインは同梱されない。

```
sudo pacman -S --needed base-devel cmake ninja git curl file python fuse2 imagemagick \
  qt6-base qt6-charts qt6-multimedia qt6-svg qt6-tools fcitx5-qt
```

<a id="setup-other"></a>

### 2.2 ほかのディストリビューション（アプリのビルドのみ）

Ubuntu・Debian や Fedora でも、アプリのビルドと開発はできる。Qt 6.7 以上が必要なので、ディストリビューションの Qt が古い場合は Qt Online Installer で Qt を入れる。

Ubuntu / Debian:

```
sudo apt install build-essential cmake ninja-build git \
  qt6-base-dev qt6-charts-dev qt6-multimedia-dev qt6-tools-dev qt6-tools-dev-tools \
  qt6-l10n-tools libqt6sql6-sqlite libgl1-mesa-dev
```

Fedora:

```
sudo dnf install gcc-c++ cmake ninja-build git \
  qt6-qtbase-devel qt6-qtcharts-devel qt6-qtmultimedia-devel qt6-qttools-devel \
  qt6-linguist mesa-libGL-devel
```

Qt Online Installer で入れた Qt は、CMake の構成時に場所を指定する（例は Qt 6.11.2）。

```
cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$HOME/Qt/6.11.2/gcc_64"
```

<a id="setup-check"></a>

### 2.3 インストールの確認

```
cmake --version
g++ --version
qmake6 --version
```

<a id="build"></a>

## 3. ソースの取得とビルド

Hayanagi をサブモジュールとして含めてリポジトリを取得する。サブモジュールがないと、ビルドに必要なファイルが欠ける。

```
git clone --recurse-submodules https://github.com/hnakada123/ShogiBoardQ.git
cd ShogiBoardQ
```

取得済みのリポジトリでは、`git submodule update --init --recursive` で記録された版の Hayanagi を取得する。Release ビルドして起動する。

```
cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/ShogiBoardQ
```

テストを実行する場合:

```
cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

ビルドだけを行うなら `./scripts/build-linux.sh --skip-appimage` も使える。

<a id="qt-notices"></a>

## 4. Qt のライセンス文書の準備

AppImage には、使った Qt と同じ版のソースから取り出したライセンス文書と、ソースの取得元の記録を収録する。配布物を作る前に一度準備し、Qt を更新したら作り直す。通常のビルドには不要。

Qt の版は、ビルドする環境で決まる。リリース用のコンテナ（[5.1](#package-run)）の Qt は 6.7.2 なので、Qt 6.7.2 のソース（qt-everywhere）を取得・検証し、文書を `build-container/qt-licenses` に取り出す。SHA-256 は、公式配布サイトでソースと同じ場所にある `.sha256` ファイルで確かめる。

```
curl -fsSL https://download.qt.io/archive/qt/6.7/6.7.2/single/qt-everywhere-src-6.7.2.tar.xz.sha256
```

```
python3 scripts/qt_licenses.py fetch --version 6.7.2 \
  --sha256 0aaea247db870193c260e8453ae692ca12abc1bd841faa1a6e6c99459968ca8a \
  --output build-container/qt-sources
python3 scripts/qt_licenses.py prepare --version 6.7.2 \
  --archive build-container/qt-sources/qt-everywhere-src-6.7.2.tar.xz \
  --sha256 0aaea247db870193c260e8453ae692ca12abc1bd841faa1a6e6c99459968ca8a \
  --source-url https://download.qt.io/archive/qt/6.7/6.7.2/single/qt-everywhere-src-6.7.2.tar.xz \
  --provenance 'Arch Linux packages from the Arch Linux Archive snapshot 2024-07-15: qt6-base 6.7.2-1; qt6-charts, qt6-multimedia, qt6-svg 6.7.2-1; qt6-wayland 6.7.2-2. Upstream Qt source plus Arch PKGBUILDs and patches.' \
  --output build-container/qt-licenses
```

`--provenance` には使った Qt の由来（Arch のパッケージの版と、Arch のパッチ・ビルド手順を含むこと）を書く。この内容は `QT-SOURCE.json` に記録され、「バージョン情報」の「ソースコードの入手方法」に表示される。手元の Arch Linux でそのままビルドする場合は、`pacman -Q qt6-base` で確かめた版の文書を `build/qt-licenses` に準備する。

- `prepare` は既存の出力先を上書きしない。作り直すときは新しい出力先を指定する。
- 既定の出力先は、ビルドディレクトリ（コンテナは `build-container`、手元のビルドは `build`）の `qt-licenses`。`--clean` でビルドディレクトリを消す場合は文書をその外に作り、環境変数 `SHOGIBOARDQ_QT_LICENSE_DIR` で指定する。
- 取得した Qt ソースや文書を Release に別添付しない。方針の詳細は [Qt 文書とリリース添付の方針](https://github.com/hnakada123/ShogiBoardQ/blob/main/docs/dev/qt-licensing.md) を参照。

<a id="package"></a>

## 5. AppImage と ZIP の作成

<a id="package-run"></a>

### 5.1 スクリプトの実行

リリース用の配布物は、古い Arch Linux（2024年7月15日の Arch Linux Archive。glibc 2.39・Qt 6.7.2）のコンテナで作る。

```
./scripts/build-linux-container.sh
```

- 初回はビルド環境のイメージ（`scripts/linux-container/Dockerfile`、約3GB）を作る。Arch Linux Archive からのダウンロードは遅く、数十分かかる。
- コンテナの中で、自分のユーザーとして `scripts/build-linux.sh` を実行する。ビルドディレクトリは普段の `build` と分けて `build-container` を使い、出力はリポジトリ直下に置く。
- 当時のパッケージに署名した鍵には、その後に期限が切れたものがある。イメージでは、署名を公式の鍵束で確かめたうえで鍵の信頼度は問わない設定にしている。

手元の Arch Linux でそのまま作ることもできる。その場合の AppImage は、手元の glibc（2026年10月時点で 2.43）以降を必要とする。

```
./scripts/build-linux.sh
```

どちらのスクリプトにも、次のオプションを付けられる。

| オプション | 説明 |
|---|---|
| `--skip-appimage` | AppImage・ZIP を作らず、ビルドだけを行う |
| `--clean` | ビルドディレクトリを削除してからビルドする |
| `--help` | ヘルプを表示する |

| 環境変数 | 説明 |
|---|---|
| `SHOGIBOARDQ_BUILD_DIR` | ビルドディレクトリ（既定は `build`。コンテナでは `build-container`） |
| `SHOGIBOARDQ_QT_LICENSE_DIR` | 準備した Qt 文書の場所（既定はビルドディレクトリの `qt-licenses`） |
| `APPIMAGE_EXTRACT_AND_RUN=1` | FUSE を使えない環境で、AppImage 形式のツールを展開して実行する（コンテナでは常に指定） |
| `DOCKER` | コンテナ用。docker コマンド（例: `sudo docker`） |
| `SHOGIBOARDQ_ARCH_SNAPSHOT` | コンテナ用。Arch Linux Archive の日付（既定は `2024/07/15`） |

`build-linux.sh` は次の処理を行う（コンテナでは、その中で同じ処理を行う）。

1. 前提ツールの確認（cmake・python3。ninja を推奨）
2. CMake の構成と Release ビルド（ShogiBoardQ と Hayanagi）
3. 実行ファイル・翻訳ファイルと、3〜13手詰の問題集（各手数1ファイル）の確認
4. linuxdeploy・Qt プラグイン・appimagetool のダウンロード（初回のみ。ビルドディレクトリに保存）
5. AppDir の作成と AppImage の生成（実行に必要なファイルと、Qt・同梱ライブラリのライセンス文書）
6. AppImage・問題集・Hayanagi を含む ZIP の作成

出力はリポジトリ直下の `ShogiBoardQ-linux-x86_64.AppImage` と `ShogiBoardQ-linux.zip`。作業用のディレクトリは、ビルドディレクトリの `AppDir` と `ShogiBoardQ-linux/`。

<a id="appimage"></a>

### 5.2 AppImage の中身

AppImage には、アプリの起動と動作に必要なファイルとライセンス文書だけを収録する。

```
ShogiBoardQ-linux-x86_64.AppImage
└── （展開時）
    ├── AppRun                          ← 同梱の Qt だけを使って起動する
    ├── shogiboardq.desktop
    ├── shogiboardq.png
    └── usr/
        ├── bin/
        │   ├── ShogiBoardQ             ← 実行ファイル
        │   ├── qt.conf                 ← Qt プラグインの検索先
        │   └── ShogiBoardQ_{ja_JP,en,zh_CN,zh_TW}.qm   ← アプリの翻訳（4言語）
        ├── lib/                        ← Qt と依存ライブラリ
        ├── plugins/
        │   ├── platforms/libqxcb.so            ← X11
        │   ├── imageformats/libqsvg.so, libqjpeg.so, libqico.so
        │   ├── iconengines/libqsvgicon.so
        │   ├── platforminputcontexts/          ← 日本語入力（compose・fcitx5・ibus）
        │   ├── platformthemes/libqxdgdesktopportal.so
        │   └── sqldrivers/libqsqlite.so        ← 詰将棋の解答履歴・解析キャッシュ
        └── share/
            ├── applications/, icons/
            └── licenses/ShogiBoardQ/
                ├── NOTICE*.md, SOURCE_CODE*.md, GPL-3.0.txt, LGPL-3.0.txt
                ├── QT-NOTICES.md, qt/          ← 同梱する Qt モジュールの文書
                ├── QT-SOURCE.json, BUILD.json  ← Qt の取得元・ビルド時の版
                └── THIRD-PARTY-NOTICES.md, third-party/   ← Qt 以外の同梱ライブラリの文書
```

- 問題集・通常対局用の Hayanagi・説明書・問題集の検証記録（`validation_*.json`）は入れない。アプリはこれらを AppImage 内から読まず、利用者もファイル選択できないため、ZIP の外部ファイルとしてだけ配布する。詰将棋対局の Hayanagi はアプリ本体に組み込まれている。
- linuxdeploy が配置する Qt 標準の翻訳（`usr/translations/`）は使わないので削除する。標準ダイアログの日本語・中国語訳は実行ファイルに内蔵し、アプリの翻訳は `usr/bin/` から読む。
- 使わないプラグイン（OpenGL 連携の `xcbglintegrations`、通信暗号化の `tls`、GIF 画像）は入れない。OpenGL を使う画面部品はなく、CSA 通信対局は暗号化しない TCP で、HTTPS などの通信もしないため。
- Qt のライセンス文書は、同梱する Qt モジュール（qtbase・qtcharts・qtmultimedia・qtsvg・qttranslations・qtwayland）の分と、それらが参照する文書だけを入れる（`scripts/qt_licenses.py`）。WebEngine など配布しないモジュールの文書は入れない。
- Qt 以外の同梱ライブラリ（glib・PulseAudio・OpenSSL・fcitx5-qt など約50パッケージ）は、`scripts/bundled_licenses.py` が元のパッケージを pacman で調べ、ライセンス本文を `third-party/` に、版・ライセンス・ソースの取得先の一覧を `THIRD-PARTY-NOTICES.md` に入れる。どちらも「バージョン情報」の「Qt 内の第三者ライセンス一覧」「同梱ライブラリのライセンス一覧」で表示される。

<a id="zip"></a>

### 5.3 ZIP の中身

ZIP には次のファイルを配置する。問題集と Hayanagi は、ZIP を展開すればすぐファイル選択できる。

| 元ファイル | ZIP 内の配置先 |
|---|---|
| `ShogiBoardQ-linux-x86_64.AppImage` | `ShogiBoardQ-linux/` |
| `resources/platform/README-linux.md` | `ShogiBoardQ-linux/README.md` |
| `LICENSE` | `ShogiBoardQ-linux/LICENSE` |
| `build/Hayanagi/hayanagi` | `ShogiBoardQ-linux/Hayanagi/hayanagi` |
| `Hayanagi/README.md` | `ShogiBoardQ-linux/Hayanagi/README.md` |
| `data/tsumeshogi/tsume_{3,5,7,9,11,13}ply_*.txt`, `data/tsumeshogi/README.md` | `ShogiBoardQ-linux/data/tsumeshogi/` |

```
ShogiBoardQ-linux.zip
└── ShogiBoardQ-linux/
    ├── ShogiBoardQ-linux-x86_64.AppImage
    ├── README.md
    ├── LICENSE
    ├── Hayanagi/
    │   ├── hayanagi
    │   └── README.md
    └── data/tsumeshogi/
        ├── tsume_{3,5,7,9,11,13}ply_1000_20261001.txt   ← 6ファイル（各1,000題）
        └── README.md
```

- 問題集は内容を変更せずにコピーする。アプリに内蔵した監査記録とハッシュが一致するため、読み込み時に検証済みの手数と手順を再利用できる。
- `data/tsumeshogi/` には各手数の問題集を1ファイルだけ置く（git で追跡しているのは現行の20261001版）。同じ手数のファイルが複数あると、スクリプトはエラーで止まる。
- Hayanagi は `strip` してから配置する。ZIP の外部ファイルには `docs/`・`validation_*.json`・`licenses/` を入れない。ライセンス文書は AppImage 内にある。

利用者は次のように起動する。詰将棋対局の「局面集を開く…」で `data/tsumeshogi/` の問題集を選び、通常対局用のエンジン登録では `Hayanagi/hayanagi` を選ぶ。

```
unzip ShogiBoardQ-linux.zip
cd ShogiBoardQ-linux
chmod +x ShogiBoardQ-linux-x86_64.AppImage Hayanagi/hayanagi
./ShogiBoardQ-linux-x86_64.AppImage
```

<a id="test"></a>

### 5.4 動作確認

```
# 起動
./ShogiBoardQ-linux-x86_64.AppImage

# 展開して中身を確かめる（Hayanagi と Qt 標準の翻訳が入っていないこと）
./ShogiBoardQ-linux-x86_64.AppImage --appimage-extract
ls squashfs-root/usr/bin/ squashfs-root/usr/plugins/*/
ls squashfs-root/usr/share/licenses/ShogiBoardQ/THIRD-PARTY-NOTICES.md
test ! -e squashfs-root/usr/bin/hayanagi && test ! -e squashfs-root/usr/translations && echo OK

# 必要な glibc の版
find squashfs-root/usr -type f \( -name '*.so*' -o -name ShogiBoardQ \) -exec objdump -T {} + 2>/dev/null \
  | grep -o 'GLIBC_[0-9.]*' | sort -uV | tail -1

# ZIP の Hayanagi が USI エンジンとして応答すること
unzip -o -q ShogiBoardQ-linux.zip -d /tmp/zipcheck
printf 'usi\nisready\nquit\n' | /tmp/zipcheck/ShogiBoardQ-linux/Hayanagi/hayanagi

# ZIP が 12 ファイルの構成どおりで、破損がないこと
python3 -m zipfile -l ShogiBoardQ-linux.zip
python3 -m zipfile -t ShogiBoardQ-linux.zip
```

必要な glibc の版が、コンテナでビルドした場合は `GLIBC_2.38` 以下であることを確かめる。AppImage は、OpenGL（libEGL・libOpenGL）・fontconfig・HarfBuzz・wayland-client をシステムのものを使う（通常のデスクトップには入っている）。

> Qt の lib ディレクトリを `LD_LIBRARY_PATH` に含めない環境で確かめる。含まれているとシステムのライブラリが使われ、同梱漏れに気づけない。できれば Ubuntu 24.04 など別の環境でも、日本語・英語・中国語の切り替え、駒の SVG、駒音、詰将棋の履歴の保存、「バージョン情報」のライセンス一覧を確認する。

<a id="release"></a>

## 6. GitHub Release での公開

リリースのタグにはアプリの版（`CMakeLists.txt` の `APP_VERSION`。例: 2026.10.06）を使う。Linux の添付は `ShogiBoardQ-linux.zip` だけで、ファイル名は版によらず同じにする。

```
git tag 2026.10.06
git push origin 2026.10.06
gh release create 2026.10.06 --title "ShogiBoardQ 2026.10.06" \
  --notes-file RELEASE_NOTES.md ShogiBoardQ-linux.zip
```

ほかの OS の配布物を先に公開している場合は、既存のリリースに追加する。

```
gh release upload 2026.10.06 ShogiBoardQ-linux.zip
```

- 添付するファイルは名前で指定する。作業フォルダ全体や `build/` のファイルをアップロードしない。
- Qt のソース、`QT-SOURCE.json`、チェックサム、SBOM などは別に添付しない。ライセンス文書とソースの入手方法は AppImage 内にある。
- 公開後に添付ファイルの一覧を確かめる。GitHub が自動で表示する Source code のリンクは、手動の添付とは別のもの。

リリースノートには、Linux での起動方法と動作条件（必要な glibc の版など）を書く。

```
### Linux での起動方法

1. ShogiBoardQ-linux.zip をダウンロードして展開する
2. chmod +x ShogiBoardQ-linux-x86_64.AppImage Hayanagi/hayanagi
3. ./ShogiBoardQ-linux-x86_64.AppImage

動作条件: x86_64 の Linux、glibc 2.38 以降（Ubuntu 24.04 以降、Debian 13、Fedora 39 以降など）
FUSE がない場合は --appimage-extract-and-run を付けて起動できます。
```

> リポジトリの [release.yml](https://github.com/hnakada123/ShogiBoardQ/blob/main/.github/workflows/release.yml) は `v` で始まるタグや手動実行で3つの OS をビルドするが、Linux のジョブは Ubuntu で動く。同梱ライブラリの文書の収集が Ubuntu に対応するまでは Linux の配布物を作れないため、現在は Arch Linux のコンテナで作った ZIP を手動で添付する。

<a id="troubleshooting"></a>

## 7. トラブルシューティング

<a id="ts-qt"></a>

### Qt が見つからない

```
Could not find a package configuration file provided by "Qt6"
```

Qt 6.7 以上と必要なモジュールが入っているか確かめる。Qt Online Installer の Qt は `-DCMAKE_PREFIX_PATH` で指定する。Qt や CMake のジェネレーターを替えたときは、新しい build ディレクトリでやり直す。

<a id="ts-submodule"></a>

### Hayanagi のソースがない

リポジトリ直下で `git submodule update --init --recursive` を実行する。

<a id="ts-tsume"></a>

### 問題集が複数あると表示されて止まる

```
==> ERROR: 5手詰の問題集が複数あります: data/tsumeshogi/tsume_5ply_1000_20260926.txt data/tsumeshogi/tsume_5ply_1000_20261001.txt
```

`data/tsumeshogi/` に旧版の問題集が残っている。各手数1ファイルになるよう、使わない版を削除する。

<a id="ts-qt-notices"></a>

### Qt の文書の検証で止まる

```
Qt license preparation failed: Qt version mismatch: build=6.7.2, source=6.11.2
Qt license preparation failed: Qt notices are missing. See docs/dev/qt-licensing.md; set SHOGIBOARDQ_QT_LICENSE_DIR to prepared matching notices.
```

ビルドに使った Qt と同じ版の文書を準備する（[4章](#qt-notices)）。コンテナの Qt は 6.7.2、手元の Arch Linux のQt は `pacman -Q qt6-base` の版。失敗を無視して配布しない。

<a id="ts-bundled"></a>

### 同梱ライブラリの文書の収集で止まる

```
Bundled library notices failed: Bundled library notices need pacman (Arch Linux); other distributions are not supported yet
Bundled library notices failed: Cannot find the system file of lib/libQt6Core.so.6
Bundled library notices failed: No license text for bundled package fcitx5-qt (GPL)
```

- 配布物は Arch Linux で、Arch の Qt パッケージを使って作る。Qt Online Installer の Qt など、pacman のパッケージに含まれないライブラリが AppImage に入ると、元のパッケージを特定できずに止まる。
- `No license text for …` は、ライセンス本文が見つからないパッケージがあることを示す。古いリポジトリではライセンス欄が正式な形式（SPDX）でないことがあるので、`scripts/bundled_licenses.py` の `LICENSE_OVERRIDES` に上流の表記を加える。パッケージにない文書（著作権表示付きの本文など）は `scripts/license-texts/<パッケージ名>/` に置く。

<a id="ts-docker"></a>

### docker を使えない

```
permission denied while trying to connect to the docker API at unix:///var/run/docker.sock
```

ユーザーを docker グループに入れてログインし直す（`sudo usermod -aG docker $USER`）。または環境変数 `DOCKER="sudo docker"` を指定する。

<a id="ts-linuxdeploy"></a>

### linuxdeploy が起動しない

```
dlopen(): error loading libfuse.so.2
```

`fuse2` を入れるか、`APPIMAGE_EXTRACT_AND_RUN=1 ./scripts/build-linux.sh` で実行する。

<a id="ts-fuse"></a>

### AppImage が起動しない（FUSE）

```
AppImages require FUSE to run.
```

FUSE 2 を入れるか、`--appimage-extract-and-run` を付けて起動する。

```
./ShogiBoardQ-linux-x86_64.AppImage --appimage-extract-and-run
```

<a id="ts-glibc"></a>

### glibc の版が足りない

```
version `GLIBC_2.38' not found
```

起動したシステムの glibc が、ビルドした環境より古い。リリース用はコンテナでビルドする（[5.1](#package-run)）。それでも足りない場合は、glibc がより新しいディストリビューションで使う（[前提条件](#requirements)の glibc の説明を参照）。

<a id="ts-libs"></a>

### システムのライブラリが見つからない

```
error while loading shared libraries: libOpenGL.so.0: cannot open shared object file: No such file or directory
```

AppImage に同梱しないライブラリ（OpenGL・fontconfig・HarfBuzz・wayland-client）がシステムに入っていない。Ubuntu / Debian の例:

```
sudo apt install libegl1 libopengl0 libfontconfig1 libharfbuzz0b libwayland-client0
```

<a id="ts-xcb"></a>

### xcb プラットフォームプラグインを読み込めない

```
qt.qpa.plugin: Could not load the Qt platform plugin "xcb"
```

X11 関連のライブラリが足りない。Ubuntu / Debian、Fedora、Arch Linux の順に例を示す。

```
sudo apt install libxcb-cursor0 libxcb-xinerama0
sudo dnf install xcb-util-cursor xcb-util-wm xcb-util-keysyms
sudo pacman -S xcb-util-cursor xcb-util-wm xcb-util-keysyms
```

<a id="ts-im"></a>

### 日本語を入力できない

AppImage は X11 用の表示プラグインだけを同梱しているので、Wayland のデスクトップでも XWayland 経由で動く。環境変数 `QT_IM_MODULE` が未設定だと、日本語入力のプラグインが選ばれない。fcitx5 を使っている場合は `QT_IM_MODULE=fcitx`、IBus の場合は `QT_IM_MODULE=ibus` を付けて起動する。

```
QT_IM_MODULE=fcitx ./ShogiBoardQ-linux-x86_64.AppImage
```

<a id="ts-translations"></a>

### 翻訳が読み込まれない

`.qm` ファイルが実行ファイルと同じ `usr/bin/` にあるか確かめる。

```
./ShogiBoardQ-linux-x86_64.AppImage --appimage-extract
ls squashfs-root/usr/bin/*.qm
```
