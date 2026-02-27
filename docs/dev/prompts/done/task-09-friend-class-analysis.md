# Task 09: MainWindow friend class 依存の分析と削減計画

## フェーズ

Phase 2（中期）- P0-2 対応・分析フェーズ

## 背景

`MainWindow` に `friend class` が 13 件あり、変更時の影響範囲が読み取りづらい。

## 実施内容

1. `mainwindow.h` の全 `friend class` 宣言を列挙する
2. 各 friend class がアクセスしている MainWindow の private メンバを調査する
   - どの private メンバ/メソッドに直接アクセスしているか
   - そのアクセスは `RuntimeRefs` / `Deps` 経由に置き換え可能か
3. 各 friend class について以下を判定する：
   - **削除可能**: `RuntimeRefs` / `Deps` パターンで代替可能
   - **削除困難**: 密結合が深く、大幅なリファクタが必要
   - **保留**: Qt の制約等で friend が必要
4. 削減計画を `docs/dev/friend-class-reduction-plan.md` に記録する
   - 削除可能な friend class のリストと移行方法
   - 削除順序（依存が少ないものから）
   - 目標: 13 → 7 以下

## 完了条件

- 全 13 friend class の分析が完了している
- 各 friend class の削除可否と理由が記載されている
- `docs/dev/friend-class-reduction-plan.md` が作成されている

## 注意事項

- このタスクは**分析のみ**。コード変更は行わない
