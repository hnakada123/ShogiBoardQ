# ShogiBoardQ macOS 配布版

ZIP を展開すると、アプリ本体（`ShogiBoardQ.dmg`）、Hayanagi USI エンジン、3・5・7・9・11・13手詰
各1,000題（計6,000題）が利用できます。問題集と通常対局用 Hayanagi は、
ファイル選択画面から選べるよう DMG の外に配置しています（DMG 内には入っていません）。

## インストールと起動

1. `ShogiBoardQ.dmg` を開き、`ShogiBoardQ.app` を「アプリケーション」フォルダにドラッグします。
2. 「アプリケーション」から ShogiBoardQ を起動します。

Apple の公証を受けていないため、初回起動時に警告が表示されて開けないことがあります。
その場合は「システム設定」→「プライバシーとセキュリティ」で「このまま開く」をクリックするか、
ターミナルで次を実行してください（macOS 15 以降は、Finder の右クリック →「開く」では開けません）。

```bash
xattr -dr com.apple.quarantine /Applications/ShogiBoardQ.app
```

## 詰将棋問題集

「詰将棋対局」の「局面集を開く…」から `data/tsumeshogi/` にある
`tsume_3ply_1000_20261001.txt` などを選択してください。
各ファイル名の `3ply` ～ `13ply` が詰み手数です。
詰将棋対局の玉方にはアプリ内蔵の Hayanagi を使います。

問題集の説明は [data/tsumeshogi/README.md](data/tsumeshogi/README.md)、
詳しい操作方法はリポジトリの [詰将棋対局ガイド](https://github.com/hnakada123/ShogiBoardQ/blob/main/docs/dev/tsume-play.md) を参照してください。

## Hayanagi

通常対局用の USI エンジンとして使用する場合は、展開したフォルダを書類フォルダなど
移動しない場所に置いてから、ターミナルで隔離属性を外し、エンジン登録画面で
`Hayanagi/hayanagi` を選択してください。実行ファイル名は小文字です。

```bash
xattr -d com.apple.quarantine Hayanagi/hayanagi
```

隔離属性が付いたままだと、macOS がエンジンの起動を止めることがあります。
Hayanagi は同じフォルダの `book/hayanagi_book.db` を定跡として使います（エンジン設定の
`BookFile` を `no_book` にすると定跡を使いません）。
詳細は [Hayanagi/README.md](Hayanagi/README.md) を参照してください。

## 動作環境とソース

Apple Silicon（arm64）の macOS 26 以降向けです。Intel Mac では動作しません。
動作条件はダウンロード元のリリースノートでも確認してください。

Qt のライセンス文書は、アプリの「バージョン情報」で参照できます。
アプリ内では `ShogiBoardQ.app/Contents/Resources/licenses/` にあります。
ビルド手順はリポジトリの [macOS ビルド・リリース手順](https://github.com/hnakada123/ShogiBoardQ/blob/main/docs/dev/macos-build-and-release.md)
を参照してください。
