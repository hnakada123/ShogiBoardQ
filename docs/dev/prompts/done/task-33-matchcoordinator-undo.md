# Task 33: MatchCoordinator UNDO 処理の整理

## Workstream B-3: MatchCoordinator 責務分割

## 目的

`MatchCoordinator` の UNDO 処理（`UndoRefs` / `UndoHooks`）の責務境界を明確化し、再利用可能な部分を抽出する。

## 背景

- `MatchCoordinator` 内の UNDO 処理が複雑で責務が不明確
- `UndoRefs` / `UndoHooks` の役割分担が曖昧
- 処理の再利用可能部分が混在している

## 対象ファイル

- `src/game/matchcoordinator.h`
- `src/game/matchcoordinator.cpp`
- 必要に応じて `src/game/` 配下に新規ヘルパを追加

## 実装内容

1. 現在の UNDO 処理の全体像を把握する:
   - `UndoRefs` の定義と使用箇所
   - `UndoHooks` の定義と使用箇所
   - UNDO 処理のフロー全体

2. `UndoRefs` / `UndoHooks` の責務境界を明確化する:
   - 参照（Refs）: どのオブジェクトを参照しているか
   - フック（Hooks）: どの処理をコールバックしているか

3. 再利用可能部分を抽出する:
   - UNDO のコアロジック
   - UI 更新部分
   - 状態復元部分

4. 必要に応じて専用クラスまたはヘルパ関数に分離する

## 制約

- 既存挙動を変更しない
- Qt signal/slot はメンバ関数ポインタ構文を使う
- 新しい lambda connect を追加しない
- 待った（UNDO）機能の動作を維持する

## 受け入れ条件

- `UndoRefs` / `UndoHooks` の責務境界が明確になっている
- 再利用可能部分が適切に抽出されている
- UNDO 機能が正常に動作する

## 検証コマンド

```bash
cmake -B build -S .
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure -j4
```

## 出力

- 変更ファイル一覧
- UNDO 処理の分解構造の説明
- 回帰リスク
- 残課題
