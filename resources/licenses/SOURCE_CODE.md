# ソースコードの入手 / Obtaining source code

公式配布版のソースコードは、バイナリと同じ GitHub Release の Assets から
無償でダウンロードできます。Qt のソースは大きいため別の添付ファイルです。

Source archives are available at no charge in the Assets of the same GitHub
Release as the binaries. Qt sources are a separate download because of their size.

https://github.com/hnakada123/ShogiBoardQ/releases

- `ShogiBoardQ-<tag>-source.tar.gz`: アプリ本体、Hayanagi、ビルドスクリプト。
  Application sources, Hayanagi, and build scripts.
- `qt-everywhere-src-<version>.tar.xz`: その配布版で使用する Qt のソース。
  The Qt sources corresponding to that release.
- `SHA256SUMS.txt`: ダウンロードしたファイルの検証用ハッシュ。
  Checksums for the downloads.

配布物の `licenses/QT-SOURCE.json` に Qt の正確なバージョン、ソースファイル名、
SHA-256、取得元とビルド元の説明を記録します。`licenses/BUILD.json` はアプリの
ビルドに使用した Qt のバージョンを記録します。

`licenses/QT-SOURCE.json` identifies the exact Qt version, source archive,
SHA-256, origin and source provenance. `licenses/BUILD.json` records the Qt
version used to build the application.

## 再ビルド / Rebuilding

ソースアーカイブを展開し、対応する Qt と CMake、C++17 コンパイラを用意します。
Qt のビルド方法は Qt ソース内の README とプラットフォーム別説明にあります。

Extract the sources and install/build the corresponding Qt, CMake and a C++17
compiler. Qt's own README and platform instructions explain how to build Qt.

    cmake -B build -S . -DCMAKE_PREFIX_PATH=/path/to/your/Qt
    cmake --build build

配布・再署名の手順はアプリのソース内の以下の文書を参照してください。
See these documents in the application sources for packaging and re-signing:

- `docs/dev/linux-build-and-release.md`
- `docs/dev/macos-build-and-release.md`
- `docs/dev/windows-build-and-release.md`
- `docs/dev/qt-licensing.md`

Windows は実行ファイル横の Qt DLL と各プラグインディレクトリ、macOS は .app 内の
Contents/Frameworks と Contents/PlugIns、Linux AppImage は展開した AppDir 内の
usr/lib と usr/plugins に共有ライブラリを配置します。互換性のある改変版を使用
できます。macOS で変更した .app は再署名してください。開発者の秘密鍵は不要で、
ローカル実行用にはアドホック署名を利用できます。

Shared Qt libraries reside beside the executable and in plugin directories on
Windows, in Contents/Frameworks and Contents/PlugIns on macOS, and in usr/lib
and usr/plugins of the extracted AppDir on Linux. Interface-compatible modified
versions can be used. A modified macOS app must be re-signed; a local ad-hoc
signature can be used without the developer's private signing key.

## 自分でビルドした場合 / Local or downstream builds

公式配布版以外では、上記 Release の Qt と一致するとは限りません。
OS のパッケージを使用した場合は、その配布元の対応ソース・パッチ・ビルド手順を
取得してください。再配布する方は、使用した版に対応するソースと文書を用意し、
バイナリを公開している間はソースも入手できる状態を維持してください。

Local/downstream builds may use a different Qt. For distribution packages,
obtain the matching sources, patches and build instructions from that
distribution. Redistributors must provide sources and notices matching their
actual binaries and keep the source downloads available while distributing them.
