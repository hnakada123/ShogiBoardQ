# Task 32: MatchCoordinator 検討モード関連の分離

## Workstream B-2: MatchCoordinator 責務分割

## 目的

`MatchCoordinator` から検討モード関連の処理を専用コンポーネントへ分離する。

## 背景

- `MatchCoordinator` が司令塔として肥大化（2,598 行）
- 検討モード関連の処理（`startAnalysis`, `updateConsiderationPosition`, 再開処理）が `MatchCoordinator` 内に混在している
- 検討モードの責務を分離することで、`MatchCoordinator` の見通しが改善される

## 対象ファイル

- `src/game/matchcoordinator.h`
- `src/game/matchcoordinator.cpp`
- `src/game/` 配下に新規コンポーネントを追加

## 実装内容

1. 検討モード関連のメソッドを特定する:
   - `startAnalysis` 関連
   - `updateConsiderationPosition` 関連
   - 検討モードの再開処理
   - その他、検討モード固有の状態管理

2. 専用コンポーネント（例: `ConsiderationModeHandler` / `ConsiderationModeService`）を作成

3. `MatchCoordinator` から検討モード関連のメソッドと状態を移動

4. `MatchCoordinator` からは委譲呼び出しのみとする

## 制約

- 既存挙動を変更しない
- Qt signal/slot はメンバ関数ポインタ構文を使う
- 新しい lambda connect を追加しない
- 既存の `ConsiderationFlowController`（`src/analysis/`）との関係を整理すること

## 受け入れ条件

- 検討モード関連の処理が `MatchCoordinator` から分離されている
- `MatchCoordinator` の行数が削減されている
- 検討モードの開始・更新・再開が正常に動作する

## 検証コマンド

```bash
cmake -B build -S .
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure -j4
```

## 出力

- 変更ファイル一覧
- 新規作成クラスの責務説明
- 回帰リスク
- 残課題
