# Task 09: usi.cpp 分割（engine層）

## 概要

`src/engine/usi.cpp`（781行）を「送受信」「状態遷移」「コマンド解釈」に分割し、600行以下にする。

## 前提条件

- なし（engine層は比較的独立しており、他タスクと並行可能）

## 現状

- `src/engine/usi.cpp`: 781行
- `src/engine/usi.h`: 276行
- #include: 6
- シグナル: 12個
- 既に分離済みのコンポーネント: `EngineProcessManager`, `UsiProtocolHandler`, `UsiPresenter`, `UsiMatchHandler`
- Usi クラスは4コンポーネントのファサードとして機能

## 分析

メソッドのカテゴリ:

### カテゴリ1: 構築・配線（~110行）
- コンストラクタ（50〜81行）: 4コンポーネント生成
- `setupConnections()`（94〜152行）: ~60行のシグナル/スロット接続

### カテゴリ2: モデル更新スロット（~120行）
- `onSearchedMoveUpdated()`, `onSearchDepthUpdated()`, `onNodeCountUpdated()`, `onNpsUpdated()`, `onHashUsageUpdated()`, `onCommLogAppended()` — CommLogModel への転送
- `onAnalysisStopTimeout()`, `onConsiderationStopTimeout()` — タイマー処理
- `onClearThinkingInfoRequested()`, `onThinkingInfoUpdated()` — ThinkingModel更新

### カテゴリ3: 状態アクセサ・プロパティ（~125行）
- `scoreStr()`, `isResignMove()`, `isWinMove()`, `lastScoreCp()`, `pvKanjiStr()`, `setPvKanjiStr()`, `parseMoveCoordinates()`, `rankToAlphabet()`, `lastBestmoveElapsedMs()`, `setPreviousFileTo()`, `setPreviousRankTo()`, `setLastUsiMove()`, `setLogIdentity()`, `setSquelchResignLogging()`, `resetResignNotified()`, `resetWinNotified()`, `markHardTimeout()`, `clearHardTimeout()`, `isIgnoring()`, `currentEnginePath()`, `setThinkingModel()`, `setLogModel()`, `cancelCurrentOperation()`
- すべて1〜5行の薄い委譲

### カテゴリ4: エンジンライフサイクル（~60行）
- `initializeAndStartEngineCommunication()`, `changeDirectoryToEnginePath()`, `startAndInitializeEngine()`, `cleanupEngineProcessAndThread()`

### カテゴリ5: コマンド送信（~130行）
- `sendGameOverCommand()`, `sendQuitCommand()`, `sendStopCommand()`, `setConsiderationModel()`, `updateConsiderationMultiPV()`, `sendGoCommand()`, `sendRaw()`, `isEngineRunning()`, `prepareBoardDataForAnalysis()`, `setClonedBoardData()`, `setBaseSfen()`, `flushThinkingInfoBuffer()`, `requestClearThinkingInfo()`, `sendGameOverLoseAndQuitCommands()`, `sendGameOverWinAndQuitCommands()`

### カテゴリ6: 詰将棋・対局通信・解析（~150行）
- 詰将棋: `executeTsumeCommunication()`, `sendPositionAndGoMateCommands()`
- 対局: `handleHumanVsEngineCommunication()`, `handleEngineVsHumanOrEngineMatchCommunication()`
- 解析: `executeAnalysisCommunication()`(~47行), `sendAnalysisCommands()`(~43行)

## 実施内容

### Step 1: 解析・対局通信の分離

カテゴリ6が最もまとまった責務:

1. `src/engine/usi_communication.cpp` を作成（同一クラスの実装分割方式）
2. カテゴリ6のメソッド（詰将棋・対局・解析通信）を移動
3. ~150行

### Step 2: モデル更新スロットの分離

1. `src/engine/usi_slots.cpp` を作成
2. カテゴリ2のモデル更新スロットを移動
3. ~120行

### Step 3: 状態アクセサの分離

1. カテゴリ3の薄い委譲メソッド群を `usi_communication.cpp` または新規ファイルに移動
2. ファイルサイズのバランスを調整

### Step 4: ビルドとテスト

1. CMakeLists.txt に新規ファイルを追加
2. `tst_structural_kpi.cpp` の `knownLargeFiles()` を更新
3. ビルド確認
4. 全テスト通過確認

## 完了条件

- `usi.cpp` が 600行以下
- 分割後の各ファイルが 600行以下
- ビルド成功
- 全テスト通過

## 注意事項

- `Usi` は QObject 派生で12個のシグナルを持つため、クラス分割ではなく **実装ファイル分割** を採用
- `.h` は変更なし（または最小限）
- `setupConnections()` のシグナル/スロット接続は本体に残す（配線の一覧性を維持）
- `connect()` にラムダを使わないこと
