# 翻訳品質ポリシー

## 目的

- 翻訳の未完了件数を段階的に減らし、リリース品質を安定化する。
- CI で翻訳退行を検出し、未完了件数の増加を防止する。

## 現在の運用

- 基準ファイル: `resources/translations/baseline-unfinished.txt`
- CI チェック: `.github/workflows/ci.yml` の `translation-check` ジョブ
- 更新コマンド:
  - `lupdate src -ts resources/translations/ShogiBoardQ_ja_JP.ts resources/translations/ShogiBoardQ_en.ts`
  - `bash scripts/check-translation-sync.sh <base-branch>`

## ルール

- 未完了件数は「増加禁止」。増えた場合は CI を失敗させる。
- 未完了件数が減少した場合は `baseline-unfinished.txt` を同一PRで更新する。
- UI 文言を変更した PR は、翻訳更新の有無を PR テンプレートで明記する。

## 目標

- 短期: 英語翻訳の未完了件数を継続的に削減。
- 中期: 主要UI（対局開始・棋譜表示・解析）を未完了 0 件にする。
- 長期: `ShogiBoardQ_en.ts` / `ShogiBoardQ_ja_JP.ts` の未完了件数を 0 件にする。

## ベースライン更新手順

1. 翻訳を更新してビルド・テストを通す。
2. `resources/translations/baseline-unfinished.txt` を実測値に更新する。
3. PR 説明に「どの画面の翻訳を改善したか」を記載する。
