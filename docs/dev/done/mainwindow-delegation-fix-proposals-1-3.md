# MainWindow 1〜3 責務移譲 修正案

作成日: 2026-02-25  
対象: `src/app/mainwindow.cpp`, `src/app/mainwindow.h`

---

## 1. 目的

`MainWindow` を「UIイベント受信と委譲」に限定し、以下3領域の責務外コードを段階的に分離する。

1. 配線・DI組み立て
2. セッション制御
3. 棋譜・局面更新

---

## 2. 対象メソッド（現状）

### 2.1 配線・DI組み立て

- `buildRuntimeRefs`
- `buildMatchWiringDeps`
- `wireMatchWiringSignals`
- `initializeDialogLaunchWiring`
- `createAndWireKifuLoadCoordinator`
- `updateKifuExportDependencies`

### 2.2 セッション制御

- `resetToInitialState`
- `resetGameState`
- `startNewShogiGame`
- `onMatchGameEnded`
- `onApplyTimeControlRequested`

### 2.3 棋譜・局面更新

- `displayGameRecord`
- `onBuildPositionRequired`
- `undoLastTwoMoves`
- `ensureGameRecordModel`

---

## 3. 修正案

## 3.1 領域1: 配線・DI組み立ての分離

### 課題

- `MainWindow` が大量の依存情報を組み立てており、変更時の影響範囲が広い。
- `ensure*` と `Deps` 構築ロジックが混在しており、意図が追いにくい。

### 方針

`MainWindow` から「構築ロジック」を排除し、呼び出しのみにする。

### 新規/拡張クラス案

- `MainWindowRuntimeRefsFactory`
  - 役割: `buildRuntimeRefs` 相当を専用化。
- `MainWindowWiringAssembler`
  - 役割: `buildMatchWiringDeps` / `wireMatchWiringSignals` / `initializeDialogLaunchWiring` を集約。
- `KifuLoadCoordinatorFactory`
  - 役割: `createAndWireKifuLoadCoordinator` を集約。
- `KifuExportDepsAssembler`
  - 役割: `updateKifuExportDependencies` の依存収集を集約。

### 実装ステップ

1. `MainWindowRuntimeRefsFactory` を追加し `buildRuntimeRefs` の中身を移植。
2. `MainWindowWiringAssembler` を追加し `MatchCoordinatorWiring::Deps` 生成と signal 接続を移植。
3. `KifuLoadCoordinatorFactory` を追加し `createAndWireKifuLoadCoordinator` を置換。
4. `KifuExportDepsAssembler` で `updateKifuExportDependencies` を1行委譲化。

### 完了条件

- 上記対象メソッドの本体が「ensure + 1回の委譲呼び出し」程度になる。
- `MainWindow` から `std::bind` の塊が大幅に減る。

---

## 3.2 領域2: セッション制御の分離

### 課題

- 対局開始/終了/リセット/連続対局/時間設定が `MainWindow` に集中している。
- UIイベント処理とドメイン状態遷移が同一メソッド内に混在している。

### 方針

セッションの状態遷移を `SessionLifecycleCoordinator` に集約し、`MainWindow` は入口のみ担当する。

### 新規クラス案

- `SessionLifecycleCoordinator`
  - 役割:
    - 新規対局開始準備
    - リセットシーケンス
    - 終局処理
    - 時間設定適用
    - 連続対局への遷移判定

### 実装ステップ

1. `resetToInitialState` / `resetGameState` のロジックを `SessionLifecycleCoordinator` へ移植。
2. `startNewShogiGame` の再開判定・評価グラフ初期化・開始呼び出しを移植。
3. `onMatchGameEnded` の終局時刻反映と連続対局判定を移植。
4. `onApplyTimeControlRequested` の保存・適用・UI反映を移植。

### 完了条件

- 対象メソッドが「イベント受信 -> coordinator 呼び出し」に統一される。
- `MainWindow` 内の `PlayMode` 条件分岐がさらに削減される。

---

## 3.3 領域3: 棋譜・局面更新の分離

### 課題

- 棋譜モデル初期化、コメント配列同期、検討モード局面解決、待った時の評価値グラフ処理が混在。
- `MainWindow` 側で棋譜データ構造の細部を直接管理している。

### 方針

用途別にサービスを分離し、`MainWindow` から「モデル操作の詳細」を取り除く。

### 新規/拡張クラス案

- `GameRecordLoadService`（新規）
  - 役割: `displayGameRecord` の初期化とコメント同期を担当。
- `ConsiderationPositionService`（新規）
  - 役割: `onBuildPositionRequired` の分岐・本譜統合局面解決を担当。
- `UndoFlowService`（新規）
  - 役割: `undoLastTwoMoves` の巻き戻し後処理（評価値グラフ・ply同期）を担当。
- `GameRecordModelBuilder`（新規）
  - 役割: `ensureGameRecordModel` の bind/callback/connect 構築を担当。
- `GameRecordUpdateService`（既存拡張）
  - 役割: 1手追記だけでなく、必要な更新フックを一元化。

### 実装ステップ

1. `displayGameRecord` のデータ初期化部分を `GameRecordLoadService` へ移管。
2. `onBuildPositionRequired` を `ConsiderationPositionService` 呼び出しのみへ変更。
3. `undoLastTwoMoves` を `UndoFlowService` 呼び出しへ変更。
4. `ensureGameRecordModel` の構築処理を `GameRecordModelBuilder` へ移管。

### 完了条件

- `MainWindow` で `m_kifu.commentsByRow` 等の直接操作が最小化される。
- 棋譜関連ロジックが UI 依存なしで単体検証しやすい構造になる。

---

## 4. 推奨実施順序

1. 領域1（配線・DI組み立て）
2. 領域2（セッション制御）
3. 領域3（棋譜・局面更新）

理由: 依存注入の整理を先行することで、後続のロジック移動時の差分を小さくできる。

---

## 5. 回帰確認ポイント（手動）

1. 新規対局開始、途中局面開始、連続対局の開始と次局遷移。
2. 投了/中断/終局時の棋譜末尾・終局時刻・UI状態。
3. 棋譜読込直後の行選択、コメント表示、分岐ナビ、検討再開。
4. 「待った」実行後の盤面、手番、評価値グラフカーソル位置。
5. KIF/KI2/CSA/クリップボード出力の内容差分。

---

## 6. 成果指標

- `MainWindow` のメソッド平均行数を削減。
- `MainWindow` の `ensure*`/`Deps` 組み立てコード量を削減。
- 新規クラス単位で責務が明文化され、変更時の影響範囲を局所化できる。
