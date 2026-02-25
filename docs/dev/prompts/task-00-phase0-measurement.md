# Phase 0: 計測と安全柵整備

## 背景

`docs/dev/mainwindow-responsibility-delegation-plan.md` に基づき、`MainWindow` の責務外コードを段階的に移譲する。
本フェーズはその準備として、現状の計測とテストシナリオの文書化を行う。

## タスク

1. **MainWindow メソッド一覧と責務分類の固定**
   - `src/app/mainwindow.h` を読み、全メソッドを以下の分類でリスト化する:
     - (A) UIイベント受信・委譲（MainWindowに残す）
     - (B) プレイモード判定ロジック → Phase 1 で移譲
     - (C) 手番同期ロジック → Phase 2 で移譲
     - (D) 棋譜追記・ライブ更新ロジック → Phase 3 で移譲
     - (E) SFEN適用・分岐同期ロジック → Phase 4 で移譲
     - (F) セッション制御ロジック → Phase 5 で移譲
     - (G) 配線組み立てロジック → Phase 6 で移譲
     - (H) UI構築ロジック → Phase 7 で移譲
   - 結果を `docs/dev/mainwindow-method-classification.md` に出力する

2. **主要フローの手動テストシナリオを文書化**
   - 以下のテストシナリオを `docs/dev/manual-test-scenarios.md` に記述:
     1. 新規対局開始（人間 vs 人間、人間 vs エンジン）
     2. 指し手反映と棋譜追記
     3. 棋譜行選択での盤面同期
     4. 分岐選択での盤面同期
     5. 終局処理と連続対局
     6. リセット後の初期状態復帰
     7. 評価グラフ・コメント連携
   - 各シナリオには「操作手順」「期待される結果」「重点確認ポイント」を含める

3. **ベースライン数値の記録**
   - `mainwindow.cpp` の行数、メソッド数を計測し、テストシナリオ文書の冒頭に記録する

## 成果物

- `docs/dev/mainwindow-method-classification.md`
- `docs/dev/manual-test-scenarios.md`

## 注意

- コードの変更は行わない（調査・文書化のみ）
- CLAUDE.md のコーディング規約に従う
