# Phase 1: PlayModePolicyService 導入 — 判定ロジックの分離

## 背景

`docs/dev/mainwindow-responsibility-delegation-plan.md` のフェーズ1。
`MainWindow` に散在するプレイモード依存の判定ロジックを `PlayModePolicyService` に集約する。
低リスクのリファクタリングであり、挙動変更なし。

## 対象メソッド

`src/app/mainwindow.cpp` / `src/app/mainwindow.h` から以下を移譲:

- `isHumanTurnNow()` — 人間の手番か判定
- `isHumanSide(ShogiGameController::Player)` — 人間側か判定
- `isHvH()` — 人間 vs 人間か判定
- `isGameActivelyInProgress()` — 対局進行中か判定

## 実装手順

1. **新規クラス作成**
   - `src/app/playmodepolicyservice.h` / `src/app/playmodepolicyservice.cpp` を作成
   - QObject 非継承（純粋ロジッククラス）
   - Deps struct パターンで依存関係を注入:
     - `PlayMode` の参照
     - `ShogiGameController` の参照
     - `MatchCoordinator` の参照（終局状態）
     - `CsaGameCoordinator` の参照（CSA手番状態）

2. **判定ロジックの移植**
   - 上記4メソッドのロジックを `PlayModePolicyService` に移動
   - `MainWindow` 側は薄いラッパー（1〜3行で委譲）に変更するか、呼び出し元を直接 service 呼び出しに変更

3. **CMakeLists.txt 更新**
   - `src/app/playmodepolicyservice.cpp` を SOURCES に追加

4. **MainWindow での初期化**
   - `MainWindowCompositionRoot` に `ensurePlayModePolicyService()` を追加
   - 適切なタイミングで `updateDeps()` を呼び出し

5. **ビルド確認**
   ```bash
   cmake -B build -S . -G Ninja && ninja -C build
   ```

## 完了条件

- `MainWindow` 内にプレイモード判定の `switch` 文が残らない
- 判定ロジックが `PlayModePolicyService` に集約されている
- ビルドが通る
- 翻訳ファイルの更新が必要な場合は `lupdate` を実行

## 注意

- `connect()` でラムダを使わない（CLAUDE.md 規約）
- Deps struct パターンで依存注入（プロジェクト慣例）
- `MainWindow` の既存の signal/slot 接続が壊れないよう注意
- 判定メソッドが signal/slot ターゲットとして使われていないか確認すること
