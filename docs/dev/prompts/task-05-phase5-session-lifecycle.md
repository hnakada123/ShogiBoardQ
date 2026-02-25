# Phase 5: SessionLifecycleCoordinator 導入 — セッション開始・終了の分離

## 背景

`docs/dev/mainwindow-responsibility-delegation-plan.md` のフェーズ5。
対局セッションの開始・終了・連続対局の制御ロジックを `SessionLifecycleCoordinator` に集約する。
高リスクのため、慎重に進めること。

## 対象メソッド

`src/app/mainwindow.cpp` / `src/app/mainwindow.h` から以下を移譲:

- `startNewShogiGame()` — 新規対局開始前の準備
- `resetToInitialState()` — 初期状態へのリセット
- `resetGameState()` — ゲーム状態のリセット
- `onMatchGameEnded()` — 終局時処理
- `onApplyTimeControlRequested()` — 時間設定の適用

## 実装手順

1. **事前調査（重要）**
   - 上記5メソッドの実装を詳細に読み、以下を特定:
     - 依存する MainWindow メンバ変数（特に `m_state.*` 系）
     - 呼び出す他の ensure* メソッド
     - connect() でのスロットターゲット使用
   - `MainWindowGameStartService` / `MainWindowResetService` との関係を確認
   - `GameStartCoordinator` / `GameEndHandler` との責務境界を整理

2. **新規クラス作成**
   - `src/app/sessionlifecyclecoordinator.h` / `src/app/sessionlifecyclecoordinator.cpp` を作成
   - Deps struct + Hooks パターン（GameEndHandler の前例に従う）:
     - `MatchCoordinator` の参照
     - `ShogiGameController` の参照
     - `MainWindowResetService` の参照
     - `MainWindowGameStartService` の参照
     - GUI操作用コールバック（Hooks）

3. **ロジックの移植**
   - 各メソッドの本体ロジックを移動
   - MainWindow 側は薄い委譲ラッパーに変更
   - リセットシーケンスの順序を厳守

4. **CMakeLists.txt 更新**

5. **ビルド確認**
   ```bash
   cmake -B build -S . -G Ninja && ninja -C build
   ```

## 完了条件

- 開始/終了/リセットシーケンスが `SessionLifecycleCoordinator` に集約
- `MainWindow` には薄い委譲ラッパーのみ残る
- ビルドが通る

## 注意

- **高リスク**: セッション制御はアプリケーション全体に影響する
- `deleteLater` と再配線の競合に注意（計画書 9. リスク表）
- 既存の `MainWindowGameStartService` / `MainWindowResetService` との責務を明確に分ける
  - 既存サービスは「個別操作の実行」、新クラスは「シーケンス制御」
- 手動テスト必須（全テストシナリオ）:
  1. 新規対局が正しく開始される
  2. 終局後に正しく処理される
  3. 連続対局が機能する
  4. リセット後に初期状態に戻る
  5. 時間設定の変更が反映される
