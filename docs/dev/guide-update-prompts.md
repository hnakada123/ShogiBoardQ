# ガイド更新・新規作成の依頼方法

公開ガイド（`docs/guide/*.html`）とホームページの作業手順は、Claude Code の Skill
[.claude/skills/guide-page/SKILL.md](../../.claude/skills/guide-page/SKILL.md) にまとめてある。
4言語（日本語・英語・簡体字・繁体字）のページ作成とスクリーンショット撮影、不具合の修正、確認、コミットの決まりまで含むので、
依頼は短い一文でよい。補助スクリプトは [scripts/docs/](../../scripts/docs/README.md)。

会話が長くなったら `/clear` してから依頼する。Skill・メモリ・`scripts/docs/` は `/clear` 後も使える。

## 依頼の例

| やりたいこと | 依頼の例 |
|---|---|
| 既存ページの更新 | `menu.html をお願いします` ／ `/guide-page piece-style.html 画像がぼやけているので全部撮り直してください` |
| 新しいページ | `/guide-page 「対局」→「〇〇…」の新しいガイドページ docs/guide/xxx.html を作ってください。tsumi-search.html と相互にリンクしてください` |
| ホームの機能一覧 | `/guide-page ホームページの機能一覧に〇〇のカードを追加してください` |
| 一部だけ更新 | `/guide-page multi-os.html の Linux の箇所だけ更新してください。Windows・macOS は別途更新します` |
| 自分で撮った画像を使う | `/guide-page multi-os.html の Windows の箇所を、~/shots/windows/ の4言語の画像で更新してください` |
| 新機能の実装から | `〇〇を実装してください（目的・画面のイメージ…）。実装後、/guide-page の手順でガイドページも4言語で作ってください` |

作業が終わったら「コミットとプッシュをお願いします」と依頼する（Skill の決まりに従い、内容ごとにコミットを分けてプッシュする）。

## 残っている作業（2026-10-07 時点）

- `guide/multi-os.html` の Windows・macOS の箇所（画像はユーザーが用意する）。あわせて、日本語版の
  「Qt6によるクロスプラットフォーム」の記述（各 OS のネイティブなルック＆フィール → 実際は全 OS で Fusion スタイル、
  ビルド要件の Qt モジュール一覧と Qt 6.7 以上）を直す
- GitHub の CI ワークフローは手動で無効化されている（有効にするかは別途判断）
