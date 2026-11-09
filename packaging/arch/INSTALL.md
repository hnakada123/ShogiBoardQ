# ShogiBoardQ Arch Linux パッケージのインストール手順

Arch Linux の pacman でインストールできる ShogiBoardQ のパッケージです。アプリ本体に加えて、CLI（`shogiboardq-cli`）、
USI エンジン Hayanagi とその定跡、詰将棋問題集（3・5・7・9・11・13手詰、各1,000題）が入ります。

Qt はシステムのものを使います。AppImage 版のように Qt を同梱していないので、Qt は `pacman -Syu` で更新されます。

## 動作条件

- x86_64 の Arch Linux
- システムが最新の状態であること（Qt 6.12 以降が必要です）

## インストール

次のコマンドで ZIP を展開し、システムを更新してからパッケージを入れます。`bsdtar` は pacman と一緒に入っているコマンドです。

```bash
bsdtar -xf ShogiBoardQ-arch-linux.zip
cd ShogiBoardQ-arch-linux
sudo pacman -Syu
sudo pacman -U ./shogiboardq-*.pkg.tar.zst
```

`qt6-multimedia-backend` や `jack` について「どれを入れるか」を聞かれたら、そのまま Enter キーを押して既定のものを選んでください。

パッケージには署名をしていません。ダウンロードしたファイルは、リリースノートに書いてある ZIP の SHA-256 と比べて確かめられます。

```bash
sha256sum ShogiBoardQ-arch-linux.zip
```

## 起動

アプリケーションメニューの「ShogiBoardQ」か、端末の `ShogiBoardQ` コマンドで起動します。

設定は `~/.config/ShogiBoardQ/ShogiBoardQ.ini` に保存します。AppImage 版と同じ場所なので、設定はそのまま引き継がれます。

## 一緒に入るもの

| 場所 | 内容・使い方 |
|---|---|
| `/usr/bin/ShogiBoardQ`、`/usr/bin/shogiboardq-cli` | アプリと CLI（`/usr/lib/shogiboardq/` の本体へのリンク） |
| `/usr/lib/shogiboardq/Hayanagi/hayanagi` | Hayanagi。通常対局に使うときは、「設定」→「エンジン設定…」でこのファイルを登録します。定跡（`book/hayanagi_book.db`）は自動で読み込みます |
| `/usr/share/shogiboardq/tsumeshogi/` | 詰将棋問題集。「対局」→「詰将棋対局…」の「局面集を開く…」から選びます |
| `/usr/share/licenses/shogiboardq/` | ライセンス文書。「バージョン情報」からも読めます |

## 日本語の表示と入力

日本語の文字が四角（□）で表示されるときは、日本語のフォントを入れてください。

```bash
sudo pacman -S noto-fonts-cjk
```

fcitx5 で日本語を入力できないときは、Qt 用のプラグインを入れて、アプリを起動し直してください。

```bash
sudo pacman -S fcitx5-qt
```

## 新しい版にする

新しい版の ZIP を展開し、インストールと同じコマンドを実行します。設定とデータは引き継がれます。

## アンインストール

```bash
sudo pacman -R shogiboardq
```

設定ファイル（`~/.config/ShogiBoardQ/`）とデータ（`~/.local/share/ShogiBoardQ/`）は残ります。

## うまく動かないとき

### 起動すると `version 'Qt_6.12' not found` などのエラーが出る

手元の Qt がビルドに使った版より古いときのエラーです。まずシステムを更新してください。

```bash
sudo pacman -Syu
```

更新しても直らないときは、同じフォルダの `PKGBUILD` で、手元の Qt に合わせてビルドし直せます。GitHub からリリースタグのソースを取得します。

```bash
sudo pacman -S --needed base-devel git
makepkg -sif
```

`-f` を付けないと、ビルドせずに同じフォルダにある配布のパッケージをそのまま入れ直します。

### アプリケーションメニューに出ない

以前に手動で作った `~/.local/share/applications/shogiboardq.desktop` があると、パッケージの項目より優先されます。このファイルを削除してください。KDE の「メニューを編集」で ShogiBoardQ を隠したことがある場合は、メニューの編集で表示し直します。直した後に `kbuildsycoca6` を実行すると、メニューにすぐ反映されます。

## ソースコードとライセンス

ShogiBoardQ は GPL-3.0 で配布しています（`LICENSE`）。このパッケージはリリースタグ `2026.11.09` のソースからビルドしました。

<https://github.com/hnakada123/ShogiBoardQ/tree/2026.11.09>
