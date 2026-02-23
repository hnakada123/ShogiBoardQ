# Task 12: MainWindow 長大メソッドの分解

`src/app/mainwindow.cpp` の長大メソッドを分解して可読性を改善してください。

## 対象メソッド

### 1. resetToInitialState()（164行）

以下の3〜4メソッドに分解:

- `resetUiState()` — UI 要素の状態リセット（ボタン、メニュー、ステータスバー）
- `resetModels()` — データモデルのクリア（棋譜モデル、思考モデル、分岐ツリー）
- `resetEngineState()` — エンジン関連の状態リセット
- `resetGameState()` — ゲーム状態変数のリセット

`resetToInitialState()` 本体は上記メソッドの呼び出しのみにする。

### 2. ensureMatchCoordinatorWiring()（117行）

シグナル接続をカテゴリ別に分離:

- `wireMatchLifecycleSignals()` — 対局開始/終了シグナル
- `wireEngineMoveSignals()` — エンジン着手シグナル
- `wireClockSignals()` — 時計関連シグナル
- `wireGameStateSignals()` — ゲーム状態変更シグナル

### 3. initializeBranchNavigationClasses()（101行）

ブランチナビゲーション初期化を専用メソッドに分割:

- `createBranchNavigationModels()` — モデルの生成
- `wireBranchNavigationSignals()` — シグナル接続

## 設計方針

- メソッド抽出のみ。新しいクラスへの移動はこの Task では行わない
- 分解後のメソッドは `private` メンバ関数として mainwindow.h に宣言を追加
- 分解後のメソッドは30〜50行以内を目安とする
- 既存の呼び出し元（外部からの呼び出し）には影響を与えない

## 制約

- CLAUDE.md のコードスタイルに準拠
- `connect()` にラムダを使わない
- 全既存テストが通ること
- 翻訳更新は不要（UI 文字列の変更なし）
