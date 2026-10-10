# Arch Linux のパッケージ

ShogiBoardQ を pacman でインストールできるパッケージ（`shogiboardq`）にする PKGBUILD です。
リリースタグのソースを GitHub から取得し、システムの Qt 6 でビルドします。
AppImage 版と違い、Qt はシステムのものを使い、`pacman -Syu` で更新される Qt に合わせて動きます。

## インストール

`base-devel` と `git` が必要です。ビルドに使う Qt の開発用パッケージなどは `makepkg -s` が入れます。

```bash
sudo pacman -S --needed base-devel git
cd packaging/arch
makepkg -si
```

`makepkg` はこのフォルダにソース（`ShogiBoardQ/`・`Hayanagi/`・`src/`）とパッケージ
（`shogiboardq-<版>-<リリース番号>-x86_64.pkg.tar.zst`）を作ります。できたパッケージは
`sudo pacman -U shogiboardq-*.pkg.tar.zst` でほかの Arch Linux にも入れられます。

起動はアプリケーションメニューの「ShogiBoardQ」か、`ShogiBoardQ` コマンドです。

## インストールされるもの

| 場所 | 内容 |
|---|---|
| `/usr/bin/ShogiBoardQ`、`/usr/bin/shogiboardq-cli` | アプリと CLI（`/usr/lib/shogiboardq/` の本体へのリンク） |
| `/usr/lib/shogiboardq/` | 本体と翻訳（日本語・英語・中国語の `.qm`） |
| `/usr/lib/shogiboardq/Hayanagi/hayanagi` | 通常対局用の USI エンジン Hayanagi。「設定」→「エンジン設定…」で登録する。定跡（`book/hayanagi_book.db`）は自動で読み込む |
| `/usr/share/shogiboardq/tsumeshogi/` | 詰将棋問題集（3・5・7・9・11・13手詰、各1,000題）。「対局」→「詰将棋対局…」の「局面集を開く…」から選ぶ |
| `/usr/share/applications/shogiboardq.desktop`、`/usr/share/icons/hicolor/512x512/apps/shogiboardq.png` | メニューの項目とアイコン |
| `/usr/share/licenses/shogiboardq/` | 「バージョン情報」で表示するライセンス文書 |

設定は `~/.config/ShogiBoardQ/ShogiBoardQ.ini` に保存します（AppImage 版と同じ場所なので、設定を引き継げます）。

### メニューに出ないとき

以前に手動で登録した `~/.local/share/applications/shogiboardq.desktop` があると、パッケージの項目より
優先されます（古い実行ファイルを指したままだと起動もできません）。このファイルを削除してください。
KDE の「メニューを編集」で ShogiBoardQ を隠したことがある場合は、`~/.config/menus/applications-kmenuedit.menu`
に `shogiboardq.desktop` の `<Exclude>` や `.hidden` の項目が残るので、メニューの編集で表示し直します。
直したあと `kbuildsycoca6` を実行すると、メニューにすぐ反映されます。

## 新しい版にする

`PKGBUILD` の `pkgver` を新しいリリースタグ（例: `2026.10.11`）にし、`pkgrel=1` に戻してから
`makepkg -si` を実行します。同じ版で PKGBUILD だけを直したときは `pkgrel` を1つ上げます。

## アンインストール

```bash
sudo pacman -R shogiboardq
```

設定ファイル（`~/.config/ShogiBoardQ/`）とデータ（`~/.local/share/ShogiBoardQ/`）は残ります。
