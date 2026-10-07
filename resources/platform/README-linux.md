# ShogiBoardQ Linux 配布版

ZIP を展開すると、アプリ本体、Hayanagi USI エンジン、3・5・7・9・11・13手詰
各1,000題（計6,000題）が利用できます。問題集と通常対局用 Hayanagi は、
ファイル選択画面から選べるよう AppImage の外に配置しています（AppImage 内には入っていません）。

## 起動

```bash
chmod +x ShogiBoardQ-linux-x86_64.AppImage Hayanagi/hayanagi
./ShogiBoardQ-linux-x86_64.AppImage
```

FUSE を利用できない環境では、次のように起動します。

```bash
./ShogiBoardQ-linux-x86_64.AppImage --appimage-extract-and-run
```

## 詰将棋問題集

「詰将棋対局」の「局面集を開く…」から `data/tsumeshogi/` にある
`tsume_3ply_1000_20261001.txt` などを選択してください。
各ファイル名の `3ply` ～ `13ply` が詰み手数です。
詰将棋対局の玉方にはアプリ内蔵の Hayanagi を使います。

問題集の説明は [data/tsumeshogi/README.md](data/tsumeshogi/README.md)、
詳しい操作方法はリポジトリの [詰将棋対局ガイド](https://github.com/hnakada123/ShogiBoardQ/blob/main/docs/dev/tsume-play.md) を参照してください。

## Hayanagi

通常対局用の USI エンジンとして使用する場合は、エンジン登録画面で
`Hayanagi/hayanagi` を選択してください。実行ファイル名は小文字です。
Hayanagi は同じフォルダの `book/hayanagi_book.db` を定跡として使います（エンジン設定の
`BookFile` を `no_book` にすると定跡を使いません）。
詳細は [Hayanagi/README.md](Hayanagi/README.md) を参照してください。

## 動作環境とソース

x86_64 Linux の X11 / XWayland 環境向けです。公式の配布版は glibc 2.38 以降
（Ubuntu 24.04 以降、Debian 13、Fedora 39 以降など）で動作します。OpenGL・fontconfig・HarfBuzz は
システムのものを使います（通常のデスクトップには入っています）。動作条件はダウンロード元の
リリースノートでも確認してください。

Qt と同梱ライブラリのライセンス文書は、アプリの「バージョン情報」で参照できます。
AppImage を展開した場合は `squashfs-root/usr/share/licenses/ShogiBoardQ/` にあります。
ビルド手順はリポジトリの [Linux ビルド・リリース手順](https://github.com/hnakada123/ShogiBoardQ/blob/main/docs/dev/linux-build-and-release.md)
を参照してください。
