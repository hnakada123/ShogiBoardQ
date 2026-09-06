# 公開ドキュメントの表示・リンク・ビルド確認（2026-09-06）

英語ページとLinuxの英語スクリーンショットを追加したコミット
`fe9f1c73bdf34c451623ae3e911c59b7ba32601f` を対象に再確認した。
以下の確認はすべて成功した。公開ページ・画像・アプリケーションソースの追加修正は不要だった。

## 前回の記録と今回の確認

前回のブラウザー確認結果は `/tmp/shogiboardq-browser-results.json` に残っており、
失敗一覧は `[]` だった。ただし、このファイルには対象コミットや成功したページの一覧がなく、
確認結果をまとめた文書もリポジトリに保存されていなかった。

今回は対象コミット、実行日時、コマンドの終了コード、各ページ・画面幅での判定結果を保存した。
リンク・ビルド確認は2026-09-06 12:41 JST、ブラウザー確認は同日13:05 JSTに実行した。

## 結果

| 確認項目 | 対象・方法 | 結果 |
|---|---|---|
| ページ表示 | 日本語21ページ・英語21ページをLinuxのChromium 151.0.7922.173で表示 | 42ページすべて成功 |
| 画面幅 | 1,440 / 1,024 / 768 / 390 / 320 px、高さ1,000 px | 210通りすべて成功 |
| 画像の読み込み | 各ページの画像を読み込み・デコードし、欠落を検出 | 欠落0件 |
| 横方向の表示 | ページ全体の横スクロール、ナビゲーションのはみ出し、言語切り替えの表示位置 | 問題0件 |
| モバイルメニュー | 幅390 pxで全ページのメニューを開閉し、表示と幅を確認 | 42ページすべて成功 |
| ローカル参照 | HTML内のリンク、ページ内アンカー、画像・CSS、当サイトのメタデータURLをファイルと照合 | 1,075件、参照切れ0件 |
| 言語切り替え | 対応する日本語・英語ページへの往復、英語版内のリンク、アンカーの一致 | 21組すべて成功 |
| メタデータ | `lang`、タイトル、説明、`canonical`、`og:url`、`hreflang` | 問題0件 |
| サイトマップ | 両言語の公開URLと代替言語URLを照合 | 42件、重複・欠落なし |
| 英語画像 | 本文とOGP・Twitter Cardの画像参照を確認 | 英語スクリーンショット113枚、欠落・未使用画像なし |
| 撮影OSの注記 | スクリーンショットを掲載する英語20ページ | 全ページにLinuxと明記 |
| ビルド | `cmake --build build -j2` | 終了コード0、`ninja: no work to do.` |
| 差分の空白 | `git diff --check` | 終了コード0 |

英語ページが日本語版と共用する画像は、言語に依存しない駒のSVG 14種類とアプリアイコンのみ。
Windows・macOSの説明でもLinuxの画面を共通例として使用し、本文・キャプション・代替テキストに撮影OSを記載している。

## 実行結果の抜粋

リンク確認:

```text
PASS: 42 pages, 1075 local references, 21 reciprocal language pairs, 42 sitemap entries, matching content structure and anchors.
```

ブラウザー確認:

```json
{
  "pages": 42,
  "viewports": 5,
  "failures": []
}
```

ビルド:

```text
ninja: no work to do.
```

## 確認方法と範囲

静的な参照確認にはPythonとBeautiful Soupを使用した。
ブラウザー確認ではChromiumのDevTools Protocolを使用し、各ページの読み込み完了とURLの一致を確認してから判定した。
ビルドは既存の `build/` を使う増分ビルドであり、クリーンビルドやCTestの再実行は含まない。

表示確認はローカルの `docs/` を `file://` で読み込んで実施した。
GitHub Pagesへのデプロイ完了、外部サイトの応答、Chromium以外のブラウザーでの表示は今回の確認範囲に含まない。

詳細な実行ログと使用した確認スクリプトは、ローカルの
`build/docs-validation/2026-09-06/` に保存した。このディレクトリはGit管理対象外。

- `checks.json`、`links.log`、`diff.log`、`build.log`: 対象コミット、実行日時、各コマンドの終了コードと出力。
- `browser-run.json`、`browser.log`、`browser-results.json`: ブラウザーの実行情報と210通りの判定結果。
- `assets.json`: 英語画像113枚と共用画像の参照確認結果。
- `previous-browser-failures.json`: 前回の失敗一覧の写し。
- `shogiboardq_check_docs.py`、`shogiboardq_browser_check.mjs`: 今回使用した確認スクリプト。

リポジトリのルートで実行した確認コマンド:

```bash
python3 build/docs-validation/2026-09-06/shogiboardq_check_docs.py
node build/docs-validation/2026-09-06/shogiboardq_browser_check.mjs
cmake --build build -j2
git diff --check
```

ブラウザー確認スクリプトは、デバッグポート9226で起動した専用のローカルChromiumを使用した。
