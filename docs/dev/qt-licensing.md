# Qt のライセンスと対応ソースの配布

ShogiBoardQ は GPL-3.0 で配布する。Qt Charts のオープンソース版は GPLv3、
Qt Base / Multimedia などは LGPLv3 または GPL。Qt 内の第三者コードには個別の
条件があるため、LGPL の本文だけでなく元の著作権表示とライセンスも保持する。

## 利用者向けの表示

「バージョン情報」に使用中／ビルド時の Qt バージョン、Qt の著作権表示、
GPL / LGPL 本文、対応ソースの案内を表示する。本文はリソースにも含め、オフラインで
読める。配布版では `licenses` 内の文書と `QT-SOURCE.json` / `BUILD.json` を優先する。

配布物内の保存先:

| 形式 | 保存先 |
|---|---|
| Windows ZIP | `licenses/` |
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
   使用しない Qt モジュールの文書も含む。Qt の全ソースも配布する。
4. 全 OS の配布処理で Qt のビルド時バージョンと一致する文書を同梱する。
5. アプリ本体と Hayanagi サブモジュールを含むソースアーカイブを作る。
   GitHub が自動生成する Source code ZIP にはサブモジュールの中身がないため、代用しない。
6. Qt ソース、アプリソース、ソース案内、チェックサムをバイナリと同じ Release に公開する。
   ソースがない場合は公開ジョブを失敗させる。

Qt ソースは約 900 MB の別ダウンロードであり、実行用 ZIP / DMG / AppImage に
詰め込まない。CI の一時 artifact やキャッシュの保持期限には依存せず、GitHub
Release の恒久的な添付ファイルにする。バイナリを公開している間は対応ソースも
公開し続ける。Qt の一般的なトップページへのリンクだけで代用しない。

この方式は未改変の公式 Qt SDK を前提とする。Qt のビルドスクリプトはソースに含まれる。
独自のパッチやビルド手順を使う場合は、後述の手動配布と同様にそれらも保存・提供する。

## 手動で配布物を作る場合

Python 3 が必要。実際に使用した Qt の対応ソース、変更箇所、ビルド手順を準備する。
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

その後、各 OS の `scripts/build-*` を実行する。Windows では `python3` を `python`
に置き換える。`--clean` / `-Clean` は build を消すため、文書をその外に生成し、
`SHOGIBOARDQ_QT_LICENSE_DIR` 環境変数で指定する。

独自 Qt の場合は、`qtbase/.cmake.conf` を含む完全な対応ソースを tar 形式で用意し、
`prepare` にそのファイルと取得元・パッチ・ビルド方法の説明を渡す。ライセンス文書が
不足する場合は補ってから再実行する。配布先は `QT-SOURCE.json` の説明と
`SOURCE_CODE.md` に正確に反映し、バイナリとともに対応ソースを公開する。

`prepare` は既存の出力先を上書きしない。再作成時は新しい出力先を指定する。
`stage` は文書のハッシュ、Qt の版、共有ライブラリ構成を検証する。失敗を無視して
配布しない。通常の開発ビルドはネットワークや Qt ソースを必要としないが、その出力を
配布用スクリプトを通さずに再配布する場合には配布者が対応文書とソースを用意する。

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

この仕組みは Qt と Qt ソースに収録された第三者コードを扱う。配布ツールが OS から
追加する Qt 外の共有ライブラリ、外付けプラグイン、MSVC ランタイム等については、
配布元の条件と対応ソースの要否も別途確認する。Qt SQL / QSQLITE を将来追加しても、
今回の完全な Qt ソースと文書抽出の対象に含まれる。SQLite の利用機能自体は未追加。

参照:
- https://www.qt.io/development/open-source-lgpl-obligations
- https://doc.qt.io/archives/qt-6.7/qtcharts-index.html
- https://doc.qt.io/qt-6/licenses-used-in-qt.html
