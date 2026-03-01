# Task 01: GameSubRegistry 3ファイル分割

## 概要

`src/app/gamesubregistry.cpp`（697行、#include 48）を3ファイルに分割し、各ファイルを450行以下に削減する。

## 前提条件

- なし（最初に着手するタスク）

## 現状

- `src/app/gamesubregistry.cpp`: 697行、#include 48
- `src/app/gamesubregistry.h`: 61行
- 22メソッドが単一ファイルに集中
- プロジェクト内で最多の #include 数（48）
- `MainWindow` の friend class として直接メンバにアクセス

## 分析

メソッドの責務は以下の3グループに分かれる:

### グループ1: GameSessionSubRegistry（セッション管理）
- `ensurePreStartCleanupHandler()` — 対局前クリーンアップ
- `ensureLiveGameSessionStarted()` — ライブセッション開始
- `ensureLiveGameSessionUpdater()` — ライブセッション更新
- `ensureGameSessionOrchestrator()` — セッションオーケストレータ（~60行、最大メソッドの一つ）
- `ensureCoreInitCoordinator()` — コア初期化
- `ensureSessionLifecycleCoordinator()` — セッションライフサイクル
- `clearGameStateFields()` — 状態クリア
- `resetEngineState()` — エンジン状態リセット

### グループ2: GameWiringSubRegistry（配線管理）
- `ensureMatchCoordinatorWiring()` — MC配線（最大メソッド）
- `buildMatchWiringDeps()` — MC配線依存構築（~85行）
- `ensureCsaGameWiring()` — CSA配線
- `ensureConsecutiveGamesController()` — 連続対局
- `ensureTurnSyncBridge()` — ターン同期ブリッジ
- `initMatchCoordinator()` — MC初期化

### グループ3: GameStateSubRegistry（状態・コントローラ管理）
- `ensureTimeController()` — 時間コントローラ
- `ensureReplayController()` — リプレイコントローラ
- `ensureGameStateController()` — 対局状態コントローラ
- `ensureGameStartCoordinator()` — 対局開始
- `ensureTurnStateSyncService()` — ターン同期サービス
- `ensureUndoFlowService()` — 手戻しサービス
- `updateTurnStatus()` — ターン状態更新

## 実施内容

### Step 1: 分割構造の設計

1. `gamesubregistry.cpp/.h` を読み込み、上記3グループの妥当性を検証
2. 各メソッド間の呼び出し依存を確認
3. #include の分配を計画（各ファイルに必要なヘッダのみ）

### Step 2: GameSessionSubRegistry の抽出

1. `src/app/gamesessionsubregistry.h/.cpp` を作成
2. グループ1のメソッドを移動
3. コンストラクタで `MainWindow&`, `MainWindowServiceRegistry*`, `MainWindowFoundationRegistry*` を受け取る（既存パターン踏襲）
4. QObject 派生とし、GameSubRegistry の子として生成

### Step 3: GameWiringSubRegistry の抽出

1. `src/app/gamewiringsubregistry.h/.cpp` を作成
2. グループ2のメソッドを移動
3. `buildMatchWiringDeps()` は private メソッドとして移動

### Step 4: GameSubRegistry のスリム化

1. 元の `GameSubRegistry` にはグループ3のメソッドのみ残す
2. 2つの新サブレジストリへのアクセサを追加: `session()`, `wiring()`
3. 外部呼び出し元（MainWindow 各所、KifuSubRegistry 等）のメソッド呼び出しパスを更新

### Step 5: ビルドとテスト

1. CMakeLists.txt に新規ファイルを追加
2. `tst_structural_kpi.cpp` の `knownLargeFiles()` から `gamesubregistry.cpp` を削除（600行以下になるため）
3. ビルド確認
4. 全テスト通過確認（特に `tst_wiring_contracts`, `tst_app_error_handling`）

## 完了条件

- `gamesubregistry.cpp` が 450行以下
- `gamesessionsubregistry.cpp` が 350行以下
- `gamewiringsubregistry.cpp` が 350行以下
- 各ファイルの #include が 25以下
- ビルド成功
- 全テスト通過

## 注意事項

- `GameSubRegistry` は `MainWindow` の friend class。新サブレジストリも friend に追加する場合は `mw_friend_classes` KPI上限（5）に注意。代わりに `GameSubRegistry` 経由のアクセスを検討する
- 3グループ間の呼び出し関係がある場合は、サブレジストリ間の参照を持たせる
- `connect()` にラムダを使わないこと
- 挙動変更なし。メソッドの移動のみ
