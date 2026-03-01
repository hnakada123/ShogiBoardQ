# Task 20260301-mt: マルチスレッド化タスク一覧

以下の調査レポートに基づくマルチスレッド化タスク:
- `docs/dev/multithreading-investigation-2026-03-01.md`
- `docs/dev/multithreading-investigation-report.md`
- `docs/dev/multithreading-analysis.md`

## 実行順序

| # | 優先度 | ファイル | 概要 | 依存 |
|---|---|---|---|---|
| 01 | P0 | task20260301-mt-01-thread-foundation.md | スレッド基盤整備（CMake/値型/ガイドライン） | なし |
| 02 | P0 | task20260301-mt-02-kifu-load-async.md | 棋譜ロード/解析のバックグラウンド化 | 01 |
| 03 | P1 | task20260301-mt-03-joseki-io-async.md | 定跡ファイルI/Oのバックグラウンド化 | 01 |
| 04 | P1 | task20260301-mt-04-engine-registration-async.md | エンジン登録のバックグラウンド化 | 01 |
| 05 | P1 | task20260301-mt-05-usi-async-statemachine.md | USI待機ループの非同期ステートマシン化 | 01 |
| 06 | P0 | task20260301-mt-06-engine-worker-thread.md | エンジンI/Oワーカースレッド導入 | 05 |
| 07 | P1 | task20260301-mt-07-analysis-batch-async.md | 一括解析の非同期化 | 06 |
| 08 | P2 | task20260301-mt-08-kifu-save-export-async.md | 棋譜保存/エクスポート/画像出力のバックグラウンド化 | 01 |
| 09 | P2 | task20260301-mt-09-evalchart-optimization.md | 評価値グラフ描画最適化（バッチ更新） | なし |
| 10 | P2 | task20260301-mt-10-tsumeshogi-parallel.md | 詰将棋局面生成の並列化 | 06 |

## 推奨ロードマップ

```
Phase 1 (基盤+即効):  [01] 基盤整備 → [02] 棋譜ロード非同期化
Phase 2 (I/O分離):    [03] 定跡I/O + [04] エンジン登録（並行可）
Phase 3 (エンジン):   [05] USI非同期SM → [06] ワーカースレッド
Phase 4 (解析):       [07] 一括解析非同期化
Phase 5 (残件):       [08] 保存/出力 + [09] グラフ最適化 + [10] 詰将棋（並行可）
```

## 使い方

各タスクは `/clear` 後に以下のように実行する:

```
docs/dev/prompts/task20260301-mt-02-kifu-load-async.md を読んで実施してください。
```
