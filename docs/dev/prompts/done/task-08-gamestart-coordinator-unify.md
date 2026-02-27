# Task 08: GameStartCoordinator 二重参照の統一

## フェーズ

Phase 1（短期）- P1-3 対応

## 背景

`MainWindow` に `m_gameStart` と `m_gameStartCoordinator` という同型（`GameStartCoordinator*`）のメンバが 2 つ併存している。参照先の意味差分がコードから直感しにくい。

MEMORY.md の記録:
> **Two GameStartCoordinator instances**: `m_gameStartCoordinator` (内部配線用, MCW管理) and `m_gameStart` (メニュー操作用, MCW::ensureMenuGameStartCoordinator管理). 両方ともMatchCoordinatorWiringが生成・配線

## 実施内容

1. `m_gameStart` と `m_gameStartCoordinator` の全使用箇所を調査する
   - `mainwindow.h` での宣言
   - `mainwindowserviceregistry.cpp`（または分割後の Registry）での生成
   - 全参照箇所（`m_gameStart->`, `m_gameStartCoordinator->`）
2. 両者の責務の違いを明確化する：
   - 同一概念であれば一本化（名前を統一し、片方を削除）
   - 別概念であれば、型エイリアスかラッパーで区別するか、命名で意図を明確にする
3. 統一/整理を実施する
4. ビルド確認: `cmake --build build`

## 完了条件

- `GameStartCoordinator` の参照が名前で意図を判別可能
- 不要な重複がなくなっている
- ビルドが通る

## 注意事項

- MEMORY.md に記録された「内部配線用」と「メニュー操作用」の使い分けが本当に必要かを検証する
- もし統一できない正当な理由がある場合は、コメントでその理由を明記する
