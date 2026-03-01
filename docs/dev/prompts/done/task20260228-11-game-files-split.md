# Task 11: game層大型ファイル分割

## 概要

`src/game/` 配下の大型ファイルを分割し、600行以下に削減する。

## 前提条件

- Task 07（matchcoordinator分割）完了推奨（同ディレクトリのリファクタリングパターンを再利用）

## 現状

対象ファイル:
- `src/game/gamestartcoordinator.cpp`: 727行（ISSUE-060）
- `src/game/shogigamecontroller.cpp`: 652行（ISSUE-060）

関連（分割済み / 進行中）:
- `src/game/matchcoordinator.cpp`: 789行 → Task 07 で対応
- `src/game/gameendhandler.cpp`: 472行（抽出済み）
- `src/game/gamestartorchestrator.cpp`: 414行（抽出済み）

## 実施内容

### Step 1: gamestartcoordinator.cpp の分割

1. ファイルを読み込み、責務を分析する
2. `GameStartCoordinator` の責務を分類:
   - **対局設定の構築** — ゲームモード・エンジン選択・時間設定の組み立て
   - **対局開始フロー** — 実際の開始処理（エンジン起動、盤面初期化等）
   - **UI連携** — ダイアログ表示、ステータス更新
3. 適切なクラスに分割:
   - `src/game/gamestartoptionsbuilder.h/.cpp` — 対局設定構築ロジック
   - または責務に応じた別のクラス
4. 元ファイルを 600行以下に削減

### Step 2: shogigamecontroller.cpp の分割

1. ファイルを読み込み、責務を分析する
2. `ShogiGameController` の責務を分類:
   - **指し手検証** — 合法手チェック（MoveValidator との連携）
   - **ゲーム進行** — 手番管理、ゲーム状態遷移
   - **棋譜連携** — 指し手の記録
3. 適切なクラスに分割
4. 元ファイルを 600行以下に削減

### Step 3: KPI更新

1. `tst_structural_kpi.cpp` の `knownLargeFiles()` で上限値を更新
2. CMakeLists.txt に新規ファイルを追加

## 完了条件

- 2ファイルが 600行以下
- 新規ファイルがそれぞれ 600行以下
- ビルド成功
- 全テスト通過

## 注意事項

- `GameStartCoordinator` には2つのインスタンスがある（`m_gameStartCoordinator` と `m_gameStart`）。両方の参照を正しく更新すること（MEMORY.md 参照）
- `GameStartOrchestrator` の `buildStartOptions` は static メソッドであることに注意
- Refs/Hooks パターンを踏襲すること
- `connect()` にラムダを使わないこと
