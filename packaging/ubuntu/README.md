# Ubuntu のパッケージ

ShogiBoardQ を apt でインストールできるパッケージ（`shogiboardq`）にするスクリプトです。
リリースタグのソースを GitHub から取得し、Ubuntu の Qt 6 でビルドします。
1つの `.deb` で Ubuntu Desktop 26.04.1 LTS と 26.10 の両方に対応します。

利用者向けの手順は [INSTALL.md](INSTALL.md) です。配布する ZIP に同梱します。

## ビルド

Docker が必要です。このフォルダで次を実行すると、`out/` に `.deb` ができます。

```bash
mkdir -p out
docker pull ubuntu:26.04
docker run --rm -v "$PWD:/packaging:ro" -v "$PWD/out:/out" ubuntu:26.04 bash /packaging/build-deb.sh
```

必ず Ubuntu 26.04 でビルドします（スクリプトはほかの版では止まります）。新しい Qt でビルドしたものは古い Qt では動かないので、26.10 でビルドすると 26.04.1 LTS には入れられません。

## 配布する ZIP

`ShogiBoardQ-ubuntu/` フォルダに次のファイルを入れて、`ShogiBoardQ-ubuntu.zip` にします。

- `shogiboardq_<版>-<リリース番号>_amd64.deb`
- `INSTALL.md`
- `build-deb.sh`
- `LICENSE`（リポジトリのルートのもの）

## 新しい版にする

`build-deb.sh` の `pkgver` を新しいリリースタグにして `pkgrel=1` に戻します。同じ版でスクリプトだけを直したときは `pkgrel` を1つ上げます。`INSTALL.md` の末尾のタグも直します。

26.04 の後の LTS に基づいて作るときは、`build-deb.sh` の Ubuntu の版の確認と、`INSTALL.md` の動作条件・依存関係のエラーの説明も直します。

## インストールされるもの

| 場所 | 内容 |
|---|---|
| `/usr/bin/ShogiBoardQ`、`/usr/bin/shogiboardq-cli` | アプリと CLI（`/usr/lib/shogiboardq/` の本体へのリンク） |
| `/usr/lib/shogiboardq/` | 本体と翻訳（日本語・英語・中国語の `.qm`）、P2P対局の通信プロセス（`shogiboardq-peer`） |
| `/usr/lib/shogiboardq/Hayanagi/hayanagi` | 通常対局用の USI エンジン Hayanagi。定跡（`book/hayanagi_book.db`）は自動で読み込む |
| `/usr/share/shogiboardq/tsumeshogi/` | 詰将棋問題集（3・5・7・9・11・13手詰、各1,000題） |
| `/usr/share/applications/shogiboardq.desktop`、`/usr/share/icons/hicolor/512x512/apps/shogiboardq.png` | メニューの項目とアイコン |
| `/usr/share/licenses/shogiboardq/` | 「バージョン情報」で表示するライセンス文書 |
| `/usr/share/doc/shogiboardq/` | Debian のパッケージの規約で置く著作権ファイルと変更履歴 |
