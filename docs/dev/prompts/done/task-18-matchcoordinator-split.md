# Task 18: matchcoordinator.cpp の分割

## フェーズ

Phase 4（中長期）- P0-4 対応

## 背景

`src/game/matchcoordinator.cpp` は 1444 行あり、対局状態遷移と UI 通知の責務が混在している。

## 実施内容

1. `src/game/matchcoordinator.h` と `matchcoordinator.cpp` を読み、責務を分析する
2. 既に抽出済みのクラスとの関係を確認する：
   - `GameEndHandler` - 終局処理（抽出済み）
   - `GameStartOrchestrator` - 対局開始（抽出済み）
   - `GameSessionOrchestrator` - ライフサイクル管理（抽出済み）
3. 残存する責務を以下に分類し、さらなる抽出を検討する：
   - **対局状態遷移**: 手番管理、指し手処理、状態更新
   - **UI 通知**: シグナル発行、表示更新コールバック
   - **エンジン連携**: USI エンジンとの通信制御
4. 抽出可能な責務を新クラスに移動する
5. ビルド確認: `cmake --build build`

## 完了条件

- `matchcoordinator.cpp` が 1000 行以下
- ビルドが通る

## 注意事項

- 既に `GameEndHandler`, `GameStartOrchestrator` 等で大幅に抽出済みなので、残りの分割は慎重に
- Hooks/Refs パターンを踏襲する
- シグナル/スロットの接続が切れないよう注意
