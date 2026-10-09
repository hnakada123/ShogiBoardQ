# ドキュメントの配置

- `index.html`: 製品紹介とサイトの入口。
- `guide/index.html`: 利用ガイドの目次。導入・画面設定、対局、棋譜・局面の操作、研究・解析、詰将棋、AI連携、ソースからのビルドの7分類。
- `guide/*.html`: 利用者向けの機能・操作説明。新しい説明ページはここに追加する。
- `en/index.html`: 製品紹介の英語版。
- `zh-cn/index.html`・`zh-tw/index.html`: 製品紹介の中国語版（簡体字・繁体字）。
- `zh-cn/guide/*.html`・`zh-tw/guide/*.html`: 利用ガイドの中国語版。翻訳済みのページだけを置き、
  未翻訳のページ・ガイド目次へのリンクは英語版（`../../en/guide/`）へ向けて「（英文）」と明記する。
  製品紹介ページの機能リンクは、中国語版のガイドがあればそちらへ向ける。
- `en/guide/*.html`: 利用ガイドの英語版。日本語版と同じファイル名・ページ内アンカーを使用する。
- `css/`: 共通スタイル。`home.css` は製品紹介ページ（4言語）専用。配色はアプリの既定の盤面配色に合わせ、`style.css` の `:root` で定義する。
- `images/`: 共通画像と日本語版の機能別スクリーンショット。
- `images/en/`: 英語版用スクリーンショット。Linuxの英語UIで撮影する（`multi-os/` の macOS・Windows の画面は各OSで撮影）。
- `images/zh-cn/`・`images/zh-tw/`: 中国語版のスクリーンショットとOGP画像（ガイド用は `images/zh-cn/<slug>/` など）。
- `images/home/`（各言語は `images/<言語>/home/`）: 製品紹介ページの特長欄の画像。
  メイン画面は各言語の `screenshot-main.png`、OGP画像は `og-image.png`。
- `images/icons/`: 製品紹介ページの機能アイコン。アプリのツールバーと同じ線画（`resources/images/actions/`）を使う。
- `images/pieces/showcase/`: 製品紹介ページの駒見本（`resources/images/pieces/` の王将を96pxに書き出したもの）。
- `dev/`: 開発者向け資料。
- `sitemap.xml`: 公開ページのURL一覧。

ルート直下のHTMLは製品紹介の `index.html` のみとし、機能・操作説明は `guide/` に配置する。
旧URL用の転送ページは設けない。

ページを追加・移動するときは、ガイド目次、関連ページのリンク、`sitemap.xml`、
`canonical` と `og:url` も更新する。画像・CSSは相対パスを使用し、
`guide/` 内からは `../images/`、`../css/` を参照する。

英語版は `en/` 以下に配置し、CSS・アイコン・駒画像は日本語版と共用する。
英語版用スクリーンショットは「設定」→「言語設定」→「English」を選択してGUIを再起動し、
Linuxの英語UIで撮影して `images/en/` に保存する。
`en/` からは `../images/en/`、`en/guide/` からは `../../images/en/` を参照する。
英語版のスクリーンショットを載せるページには、撮影OS（Linuxの英語UI）を明記する。
OSごとの画面を見せるクロスプラットフォームのガイド（`guide/multi-os.html`）だけは、
Linux（KDE Plasma）・macOS・Windows 11 のそれぞれで撮影した画面を4言語分載せ、
キャプションと代替テキストに撮影したOSを書く。macOS・Windows の画面は各OSの実機で撮影する。
OGP・Twitter Cardの画像も英語版用の画像を参照する。日本語版の画像は変更しない。
`dev/` は英訳対象に含めない。

中国語版のページを持つガイドは、4言語すべてのページで言語メニュー（`lang-menu`）を使い、
`hreflang` に `zh-Hans`・`zh-Hant` も登録する。中国語UIのスクリーンショットでKDEのファイル選択画面まで
中国語にするには、中国語ロケール（`zh_CN.UTF-8`・`zh_TW.UTF-8`）が必要。

製品紹介ページは日本語・英語・中国語（簡体字・繁体字）の4言語で同じ構成にする。
内容を変えるときは4ページすべてに反映し、`hreflang`（`ja`・`en`・`zh-Hans`・`zh-Hant`・`x-default`）、
`sitemap.xml`、各言語のOGP画像も揃える。製品紹介ページのスクリーンショットは、同じ棋譜・局面を
各言語のUIで撮影する（Xvfb上で `--automation` を使い、検討モード・棋譜解析・詰将棋・外観設定を撮影）。

利用者向けページを追加・更新するときは英語版にも反映し、本文だけでなく
タイトル・説明メタデータ・画像の代替テキストも翻訳する。
英語版のサイト内リンクは英語版へ向け、各ページの言語切り替えは対応する
日本語版・英語版へ向ける。`hreflang`（`ja`・`en`・`x-default`）と
`sitemap.xml` に両言語のURLを登録し、`canonical`・`og:url` は各言語のURLを指定する。
棋譜形式に固有の日本語表記を含むサンプルコードは、そのまま保持する。

更新後はリンク先とページ内アンカー、画像・CSSの参照先、PC・モバイル幅の表示を確認する。
