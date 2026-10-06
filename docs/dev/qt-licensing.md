# Qt 文書とリリース添付の方針

## Release に添付するファイル

GitHub Release には各 OS の実行用パッケージだけを添付する。

| OS | 公開する添付ファイル |
|---|---|
| Linux | `ShogiBoardQ-linux.zip`（AppImage・問題集・Hayanagi を含む） |
| macOS | `ShogiBoardQ-<version>-macos.dmg` |
| Windows | `ShogiBoardQ-<version>-windows-x86_64.zip` |

Linux だけを公開する場合は ZIP 1個のみとする。Qt ソースアーカイブ、Qt のパッチ・
ビルド手順アーカイブ、`QT-SOURCE.json`、`SOURCE_CODE.md`、`BUILD-INFO.txt`、
アプリのソースアーカイブ、チェックサム、SBOM、CI ログを別添付しない。
アップロードするファイル名を明示し、作業フォルダ全体をアップロードしない。
GitHub が自動表示する Source code のリンクは手動添付ファイルとは別扱いとなる。

Qt ソースの取得はライセンス文書の抽出に使用するビルド準備であり、Release への
添付処理ではない。Qt の実行用ライブラリ・プラグインとライセンス文書はアプリ内に収録する。
Linux ZIP の外部ファイルには `licenses/` を作成せず、AppImage 内の文書を利用する。
Windows ZIP にも外部の `licenses/` と `Hayanagi/source/` は収録しない。
Windows ではアプリ内蔵のライセンス表示とルートの `LICENSE` を維持し、
実行用ファイル・問題集・利用案内を配布する。この構成を今後の Windows リリースにも適用する。

## ライセンス文書

ShogiBoardQ は GPL-3.0 で配布する。Qt Charts のオープンソース版は GPLv3、
Qt Base / Multimedia などは LGPLv3 または GPL。Qt 内の第三者コードには個別の
条件があるため、LGPL の本文だけでなく元の著作権表示とライセンスも保持する。

## 同梱する Qt 文書の範囲

ライセンス上の義務は配布するコードに対して生じる。そのため配布物には、同梱する Qt モジュールの
ライセンス・著作権表示・第三者コードの文書と、Qt ソース最上位の `LICENSES/`、それらの
`qt_attribution.json` が参照する文書（別モジュール内のものを含む）だけを収録する。
WebEngine・Qt 3D など配布しないモジュールの文書は入れない。`scripts/qt_licenses.py stage` の
`--modules` で対象を指定し、指定したモジュールの文書がない場合は配布物を作らない。

| 形式 | 対象モジュール |
|---|---|
| Linux AppImage | qtbase・qtcharts・qtmultimedia・qtsvg・qttranslations・qtwayland（Qt 6.9 以前は日本語入力が使う Qt Wayland Client がここにある） |
| macOS .app | qtbase・qtcharts・qtmultimedia・qtsvg・qttranslations・qtimageformats（macdeployqt が画像形式プラグインを配置するため） |

同梱するモジュールを増やしたとき（`find_package(Qt6 … COMPONENTS …)` やプラグインの追加）は、
この表と各ビルドスクリプトの `--modules` も更新する。作業用一式の整合性検査に使う `FILES.json` は
配布物に入れない。

## Qt 以外の同梱ライブラリ（Linux AppImage）

linuxdeploy は Qt のほかに、ビルド環境のライブラリ（glib・PulseAudio・OpenSSL・ICU・fcitx5-qt など）を
AppImage に同梱する。これらにもそれぞれのライセンス（MIT・BSD・Apache-2.0・LGPL など）があり、
著作権表示・ライセンス本文の添付や、LGPL のライブラリではソースの入手方法の提示が必要になる。

`scripts/bundled_licenses.py` は AppDir 内の各ライブラリとプラグインの元のパッケージを pacman で調べ、
パッケージのライセンス本文を `third-party/` に、版・ライセンス・ソースの取得先（Arch Linux の
パッケージのソースと上流）の一覧を `THIRD-PARTY-NOTICES.md` に収録する。Qt のパッケージ（`qt6-*`）は
上記の Qt 文書で扱うので除く。パッケージが分からないファイルや本文がないパッケージがあれば配布物を作らない。
Arch のライセンス欄が SPDX 形式でないパッケージ（libasyncns・libidn2 や、古いリポジトリの多くのパッケージ）は、
`LICENSE_OVERRIDES` で上流の表記に補う。補正は（パッケージ名, 元の表記）の組で指定し、表記が変わったら当てない。
GPL 系の本文が見つからないパッケージがあれば配布物を作らない。パッケージにない文書（BSD の本文に実際の
著作権表示を付けたものなど）は `scripts/license-texts/<パッケージ名>/` に置く（例: fcitx5-qt の DBusAddons）。

現在は Arch Linux（pacman）でビルドした場合だけに対応する。リリース用は `scripts/build-linux-container.sh` で、
glibc の古い Arch Linux（Arch Linux Archive の 2024-07-15）のコンテナの中でビルドする（Qt は 6.7.2 になるので、
Qt 文書もその版で準備する）。Ubuntu など他の環境（CI を含む）で配布物を作るには、そのパッケージ管理
（dpkg と `/usr/share/doc/<パッケージ名>/copyright` など）への対応を追加する必要がある。

## 利用者向けの表示

「バージョン情報」に使用中／ビルド時の Qt バージョン、Qt の著作権表示、
GPL / LGPL 本文、対応ソースの案内を表示する。本文はリソースにも含め、オフラインで
読める。配布版では `licenses` 内の文書と `QT-SOURCE.json` / `BUILD.json` を優先する。

配布物内の保存先:

| 形式 | 保存先 |
|---|---|
| Windows ZIP | 外部 `licenses/` は収録しない。アプリ内蔵の表示とルートの `LICENSE` を利用 |
| macOS .app / DMG | `ShogiBoardQ.app/Contents/Resources/licenses/` |
| Linux AppImage | 展開した AppDir の `usr/share/licenses/ShogiBoardQ/` |
| 通常の CMake install | `<prefix>/share/licenses/ShogiBoardQ/` |

## GitHub 公式リリース

`.github/workflows/release.yml` は Qt SDK とソースを同じ **完全な版番号** に固定する。
Qt を更新するときは `QT_VERSION` と `QT_SOURCE_SHA256` の両方を公式配布サイトで
確認して更新する。ワイルドカード指定は禁止。現在のチェックサム取得元:

https://download.qt.io/archive/qt/6.7/6.7.3/single/qt-everywhere-src-6.7.3.tar.xz.sha256

1. 完全な Qt ソースアーカイブを取得し、SHA-256 を検証する。
2. アーカイブの `qtbase/.cmake.conf` でも版を確認する。
3. ライセンス、著作権表示、`qt_attribution.json` とその参照文書を原文のまま抽出する。
   この作業用の一式には使用しない Qt モジュールの文書も含む。ソースアーカイブは作業用に保持する。
4. Linux / macOS の配布処理で Qt のビルド時バージョンと一致する文書を同梱する。Windows は外部文書の配置を行わない。
   同梱するのは配布する Qt モジュールの文書だけ（下記「同梱する Qt 文書の範囲」）。
5. 製品パッケージを `release-package-*` artifact に保存する。
6. 公開ジョブは製品パッケージだけを取得し、上表の3ファイル名を明示して公開する。
   SHA256 と SBOM は CI の `release-verification` artifact に保存する。

Qt 文書の受け渡し用 `qt-license-documents` artifact と検証用 artifact は
GitHub Release の添付対象にしない。取得元は同梱する `QT-SOURCE.json` の
`source_url` と `provenance` に記録する。アプリと Hayanagi の取得方法は
`SOURCE_CODE.md` で案内する。

バージョン情報では、案内文書 `NOTICE.md` と `SOURCE_CODE.md` を UI の言語に合わせて
選択する。英語・中国語（簡体字・繁体字）は `resources/licenses/` の言語別ファイルを
使用し、ファイル名は `VersionDialog` の翻訳カタログで指定する。案内を変更するときは
各言語の文書も更新する。文書はリソースと配布物の両方に同梱し、古い配布用キャッシュにも
配置時に最新版を追加する。GPL / LGPL と Qt 内の第三者ライセンスは原文を保持する。

CI は未改変の公式 Qt SDK を使用する。独自のパッチやビルド手順を使う場合は、
後述の手動配布と同様に、使用した内容と取得元を記録する。

## 手動で配布物を作る場合

以下の文書抽出・配置は Linux / macOS 向け。Windows の ZIP 作成では実行しない。
文書抽出には Python 3 が必要。実際に使用した Qt の対応ソース、変更箇所、ビルド手順を準備する。
版番号が同じでも、OS パッケージのパッチ済み Qt と公式の未改変 Qt は同一ではない。
バージョン一致の自動確認は必要条件であり、由来の確認の代わりにはならない。

未改変の公式 Qt 6.7.3 SDK を使用する場合の例:

```bash
python3 scripts/qt_licenses.py fetch --version 6.7.3 \
  --sha256 a3f1d257cbb14c6536585ffccf7c203ce7017418e1a0c2ed7c316c20c729c801 \
  --output build/qt-sources
python3 scripts/qt_licenses.py prepare --version 6.7.3 \
  --archive build/qt-sources/qt-everywhere-src-6.7.3.tar.xz \
  --sha256 a3f1d257cbb14c6536585ffccf7c203ce7017418e1a0c2ed7c316c20c729c801 \
  --source-url https://download.qt.io/archive/qt/6.7/6.7.3/single/qt-everywhere-src-6.7.3.tar.xz \
  --provenance 'Official Qt 6.7.3 SDK, unmodified; build scripts in source archive' \
  --output build/qt-licenses
```

その後、対象 OS の `scripts/build-*` を実行する。`--clean` は build を消すため、文書をその外に生成し、
`SHOGIBOARDQ_QT_LICENSE_DIR` 環境変数で指定する。

独自 Qt の場合は、`qtbase/.cmake.conf` を含む完全な対応ソースを tar 形式で用意し、
`prepare` にそのファイルと取得元・パッチ・ビルド方法の説明を渡す。ライセンス文書が
不足する場合は補ってから再実行する。取得元は `QT-SOURCE.json` に正確に反映する。
ここで準備した Qt ソースや文書を Release の別添付ファイルにはしない。

`prepare` は既存の出力先を上書きしない。再作成時は新しい出力先を指定する。
`stage` は文書のハッシュ、Qt の版、共有ライブラリ構成を検証する。失敗を無視して
配布しない。`stage` はリポジトリの最新のソース案内を使用するため、以前に準備した
Qt 文書を使っても旧方針の「Release の別添付から取得する」という案内は引き継がない。
通常の開発ビルドはネットワークや Qt ソースを必要としない。

## 改変版 Qt で実行する

アプリの全ソースとビルドスクリプトを提供し、互換 Qt の差し替えやデバッグを
妨げる追加条件は設けない。静的 Qt の配布はこのスクリプトの対象外とする。

- Linux: `./ShogiBoardQ-*.AppImage --appimage-extract` で展開し、`squashfs-root`
  内の Qt を差し替えて `AppRun` から実行できる。
- Windows: ZIP を展開し、対応する Qt DLL とプラグインを差し替えるか、
  `CMAKE_PREFIX_PATH` で改変 Qt を指定してビルドし直す。
- macOS: .app 内の Frameworks / PlugIns を差し替えるかビルドし直し、
  `codesign --force --deep --sign - ShogiBoardQ.app` でローカル用に再署名する。
  公式配布者の秘密鍵は必要ない。

## 確認範囲

この仕組みは Qt と Qt ソースに収録された第三者コードを扱う。Qt SQL / QSQLITE も文書抽出の対象に含む。
Linux AppImage で配布ツールが OS から追加する Qt 外の共有ライブラリと外付けプラグインは、
`scripts/bundled_licenses.py` が扱う（上記「Qt 以外の同梱ライブラリ」）。macOS・Windows で追加される
Qt 外のライブラリや MSVC ランタイム等については、配布元の条件と対応ソースの要否を別途確認する。

参照:
- https://www.qt.io/development/open-source-lgpl-obligations
- https://doc.qt.io/archives/qt-6.7/qtcharts-index.html
- https://doc.qt.io/qt-6/licenses-used-in-qt.html
