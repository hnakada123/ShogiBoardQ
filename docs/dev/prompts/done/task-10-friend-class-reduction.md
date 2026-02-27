# Task 10: MainWindow friend class 削減（実装）

## フェーズ

Phase 2（中期）- P0-2 対応・実装フェーズ

## 背景

Task 09 の分析に基づき、friend class 依存を段階的に削減する。

## 実施内容

1. `docs/dev/friend-class-reduction-plan.md` を読み、削減計画を確認する
2. 「削除可能」と判定された friend class について、順番に以下を実施する：
   - 該当クラスが直接アクセスしている MainWindow の private メンバを特定
   - `RuntimeRefs` / `Deps` 構造体に必要なメンバを追加
   - 該当クラスのコンストラクタ/メソッドを修正し、Deps 経由でアクセスするよう変更
   - `mainwindow.h` から `friend class` 宣言を削除
3. 各削除後にビルド確認: `cmake --build build`
4. KPI 確認: `friend class` 数が 7 以下になっているか

## 完了条件

- `friend class` 数が 13 → 7 以下
- ビルドが通る
- 既存の動作が変わらない

## 前提

- Task 09 の分析が完了していること

## 注意事項

- 一度に全部やらず、1 friend class ずつ削除→ビルド確認のサイクルで進める
- 削除困難なものは無理に削除せず、計画を更新する
