# Task 10: usiprotocolhandler.cpp 分割（engine層）

## 概要

`src/engine/usiprotocolhandler.cpp`（714行）を「コマンド送信」「応答待機」「受信行解析」に分割し、600行以下にする。

## 前提条件

- Task 09（usi.cpp分割）完了推奨（同ディレクトリの分割パターンを再利用）

## 現状

- `src/engine/usiprotocolhandler.cpp`: 714行
- `src/engine/usiprotocolhandler.h`: 246行
- #include: 8
- シグナル: 12個

## 分析

メソッドのカテゴリ:

### カテゴリ1: セットアップ・初期化（~85行）
- コンストラクタ/デストラクタ、`setProcessManager()`, `setPresenter()`, `setGameController()`
- `initializeEngine()`（75〜118行、~43行）: USI初期化シーケンス全体
- `loadEngineOptions()`（119〜157行）: QSettings読み込み

### カテゴリ2: コマンド送信（~180行、163〜344行）
- `sendCommand()` — 低レベル送信
- `sendUsi()`, `sendIsReady()`, `sendUsiNewGame()`, `sendPosition()` — 基本コマンド
- `beginMainSearch()` — 検索開始
- `sendGo()`, `sendGoPonder()`, `sendGoMate()`, `sendGoDepth()`, `sendGoNodes()`, `sendGoMovetime()`, `sendGoSearchmoves()`, `sendGoSearchmovesDepth()`, `sendGoSearchmovesMovetime()` — go系コマンド9種
- `sendStop()`, `sendPonderHit()`, `sendGameOver()`, `sendQuit()` — 終了系
- `sendSetOption()`, `sendRaw()` — 設定・生コマンド

### カテゴリ3: 応答待機（~125行、351〜475行）
- `waitForResponseFlag()` — 汎用待機
- `waitForUsiOk()`, `waitForReadyOk()` — 初期化待機
- `waitForBestMove()`（381〜411行）: ポーリングループ
- `waitForBestMoveWithGrace()` — 猶予付き待機
- `keepWaitingForBestMove()` — 継続待機
- `waitForStopOrPonderhit()` — stop/ponderhit待機
- `shouldAbortWait()` — 中断判定

### カテゴリ4: 受信行解析（~150行、482〜631行）
- `onDataReceived()`（482〜542行）: メインディスパッチャ（~60行）
- `handleBestMoveLine()`（544〜606行）: bestmove解析（~63行）
- `handleCheckmateLine()`（608〜631行）: checkmate解析

### カテゴリ5: 座標変換・操作管理（~40行）
- `parseMoveCoordinates()`, `convertHumanMoveToUsi()`, `rankToAlphabet()`, `alphabetToRank()`
- `beginOperationContext()`, `cancelCurrentOperation()`

## 実施内容

### Step 1: 応答待機の分離

カテゴリ3はブロッキング処理で他と明確に異なる:

1. `src/engine/usiprotocolhandler_wait.cpp` を作成
2. カテゴリ3のメソッド（~125行）を移動

### Step 2: コマンド送信の分離（必要に応じて）

Step 1 で 600行以下になるか確認:
- 714 - 125 = ~589行 → 600行以下達成
- 達成できない場合はカテゴリ2も分離

### Step 3: ビルドとテスト

1. CMakeLists.txt に新規ファイルを追加
2. `tst_structural_kpi.cpp` の `knownLargeFiles()` を更新
3. ビルド確認
4. 全テスト通過確認

## 完了条件

- `usiprotocolhandler.cpp` が 600行以下
- 分割後の各ファイルが 600行以下
- ビルド成功
- 全テスト通過

## 注意事項

- `UsiProtocolHandler` は QObject 派生のため **実装ファイル分割** を採用
- `.h` は変更なし
- `waitForBestMove()` 系のポーリングループは `QCoreApplication::processEvents()` を使用しておりスレッド安全性に注意
- `onDataReceived()` のディスパッチロジックは本体に残す（受信の入口として一覧性が重要）
