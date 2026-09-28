# ソースコードの入手 / Obtaining source code

ShogiBoardQ のソースとビルドスクリプトは次のリポジトリから取得できます。
Hayanagi はサブモジュールとして記録した版を取得してください。
Release の添付ファイルは実行用パッケージのみです。

Obtain ShogiBoardQ sources and build scripts from the repository below, including
the recorded Hayanagi submodule revision. Release attachments contain only runnable packages.

https://github.com/hnakada123/ShogiBoardQ

    git clone --recurse-submodules https://github.com/hnakada123/ShogiBoardQ.git
    cd ShogiBoardQ
    git checkout <release-tag-or-commit>
    git submodule update --init --recursive

GitHub が自動生成する Source code ZIP には Hayanagi の中身がないため、再ビルドには
上記の方法でサブモジュールも取得してください。

GitHub's automatically generated source ZIP omits Hayanagi's contents; initialize
the submodule as shown above when rebuilding.

配布物の `licenses/QT-SOURCE.json` に Qt の正確なバージョン、ソースファイル名、
SHA-256、取得元とビルド元の説明を記録します。`licenses/BUILD.json` はアプリの
ビルドに使用した Qt のバージョンを記録します。

`licenses/QT-SOURCE.json` identifies the exact Qt version, source archive,
SHA-256, origin and source provenance. `licenses/BUILD.json` records the Qt
version used to build the application.

Qt ソースは `QT-SOURCE.json` の `source_url` に記録された取得元を参照してください。
OS パッケージや改変版を使用した場合は `provenance` の説明にあるパッチ・ビルド手順も
確認してください。Linux ではこれらの文書は AppImage 内の
`usr/share/licenses/ShogiBoardQ/` にあり、ZIP の外部に `licenses/` は配置しません。

For Qt sources, use the `source_url` recorded in `QT-SOURCE.json`. For downstream
builds, also consult `provenance` for the supplier's patches and build instructions.
On Linux these documents reside inside the AppImage at `usr/share/licenses/ShogiBoardQ/`;
the outer ZIP has no `licenses/` directory.

## 再ビルド / Rebuilding

ソースを取得し、対応する Qt と CMake、C++17 コンパイラを用意します。
Qt のビルド方法は Qt ソース内の README とプラットフォーム別説明にあります。

Obtain the sources and install/build the corresponding Qt, CMake and a C++17
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
