# ShogiBoardQ Ubuntu パッケージのインストール手順

Ubuntu の apt でインストールできる ShogiBoardQ のパッケージです。アプリ本体に加えて、CLI（`shogiboardq-cli`）、
USI エンジン Hayanagi とその定跡、詰将棋問題集（3・5・7・9・11・13手詰、各1,000題）が入ります。

Qt は Ubuntu のものを使います。AppImage 版のように Qt を同梱していないので、Qt は Ubuntu の更新で新しくなります。

## 動作条件

- x86_64（amd64）の Ubuntu Desktop 26.04.1 LTS または 26.10

同じパッケージを、どちらの Ubuntu にも入れられます。

## インストール

次のコマンドで ZIP を展開し、パッケージを入れます。ファイル名の前の `./` は省略しないでください（省略すると、apt はファイルではなくパッケージの名前として探します）。

```bash
unzip ShogiBoardQ-ubuntu.zip
cd ShogiBoardQ-ubuntu
sudo apt update
sudo apt install ./shogiboardq_*.deb
```

パッケージには署名をしていません。ダウンロードしたファイルは、リリースノートに書いてある ZIP の SHA-256 と比べて確かめられます。

```bash
sha256sum ShogiBoardQ-ubuntu.zip
```

## 起動

アプリの一覧の「ShogiBoardQ」か、端末の `ShogiBoardQ` コマンドで起動します。

設定は `~/.config/ShogiBoardQ/ShogiBoardQ.ini` に保存します。AppImage 版と同じ場所なので、設定はそのまま引き継がれます。

## 一緒に入るもの

| 場所 | 内容・使い方 |
|---|---|
| `/usr/bin/ShogiBoardQ`、`/usr/bin/shogiboardq-cli` | アプリと CLI（`/usr/lib/shogiboardq/` の本体へのリンク） |
| `/usr/lib/shogiboardq/Hayanagi/hayanagi` | Hayanagi。通常対局に使うときは、「設定」→「エンジン設定…」でこのファイルを登録します。定跡（`book/hayanagi_book.db`）は自動で読み込みます |
| `/usr/share/shogiboardq/tsumeshogi/` | 詰将棋問題集。「対局」→「詰将棋対局…」の「局面集を開く…」から選びます |
| `/usr/share/licenses/shogiboardq/` | ライセンス文書。「バージョン情報」からも読めます |

## 日本語の表示と入力

Ubuntu の IBus と Mozc で日本語を入力できます。Mozc が入っていないときは、次のコマンドで入れてください。

```bash
sudo apt install ibus-mozc
```

日本語の文字が四角（□）で表示されるときは、日本語のフォントを入れてください。

```bash
sudo apt install fonts-noto-cjk
```

fcitx5 で日本語を入力できないときは、Qt 用のプラグインを入れて、アプリを起動し直してください。

```bash
sudo apt install fcitx5-frontend-qt6
```

## 新しい版にする

新しい版の ZIP を展開し、インストールと同じコマンドを実行します。設定とデータは引き継がれます。

## アンインストール

```bash
sudo apt remove shogiboardq
```

設定ファイル（`~/.config/ShogiBoardQ/`）とデータ（`~/.local/share/ShogiBoardQ/`）は残ります。一緒に入った Qt などのパッケージも消すときは、続けて `sudo apt autoremove` を実行してください。

## うまく動かないとき

### インストールすると `libqt6core6t64 (>= 6.10.2)` などの依存関係のエラーになる

Ubuntu の Qt がこのパッケージより古いときのエラーです。Ubuntu 24.04 LTS など 26.04 より前の Ubuntu には入れられません。AppImage 版（`ShogiBoardQ-linux.zip`）を使ってください。

26.04.1 LTS・26.10 でこのエラーになるときは、`sudo apt update` と `sudo apt upgrade` でシステムを更新してから、もう一度インストールしてください。

### アプリの一覧に出ない

以前に手動で作った `~/.local/share/applications/shogiboardq.desktop` があると、パッケージの項目より優先されます。このファイルを削除してください。

## ソースコードとライセンス

ShogiBoardQ は GPL-3.0 で配布しています（`LICENSE`）。このパッケージはリリースタグ `2026.11.09` のソースから、同じフォルダの `build-deb.sh` でビルドしました。

<https://github.com/hnakada123/ShogiBoardQ/tree/2026.11.09>
