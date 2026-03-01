# Qtプロジェクトにおけるマルチスレッド化調査レポート

作成日: 2026-03-01  
対象: `src/` 配下（USI対局・CSA通信・棋譜I/O・定跡I/O・エンジン登録）

## 1. 結論（先に要点）

UIフリーズとイベント遅延の主要因は、**UIスレッド上での同期待機**と**重い解析/I/O処理の直列実行**です。  
優先度順の推奨は以下です。

1. USI思考待機・対局進行（HvE/EvE/CSA）を専用ワーカースレッドへ分離
2. 棋譜ロード（読込/解析/SFEN再構築）をバックグラウンド化
3. エンジン起動/初期化/終了の同期待機を非同期化（または1と同時にワーカー内へ隔離）
4. 定跡ファイルロード/保存をバックグラウンド化
5. エンジン登録処理（QProcess起動・USIオプション収集）をバックグラウンド化
6. 棋譜エクスポート/保存、詰将棋局面生成は中優先で分離

## 2. 調査サマリ（優先度一覧）

| 優先度 | 対象 | 現状の問題 | 推奨方式 |
|---|---|---|---|
| 高 | USI対局思考（HvE/EvE/CSA） | UIスレッドでbestmove待機まで実行 | `QThread` + `QObject`ワーカー（エンジンI/O専用） |
| 高 | USI待機ループ | `processEvents` / `msleep` / ローカル`QEventLoop`の待機 | 非同期state machine（タイマー+シグナル） |
| 高 | 棋譜ロード/解析 | ファイルI/O + 解析 + SFEN再構築を同期実行 | `QtConcurrent::run` + `QFutureWatcher` |
| 中 | エンジン初期化/終了 | `waitForStarted`/`waitForFinished`/`waitForUsiOk`が同期 | エンジンスレッド隔離 or 非同期起動API |
| 中 | 定跡ファイルI/O | 大きい`.db`の読書きが同期 | バックグラウンドI/O + 結果スワップ |
| 中 | エンジン登録 | 登録ダイアログ操作中に同期起動待機 | 登録ワーカー + 進捗通知 |
| 低-中 | 棋譜保存/出力・詰将棋局面生成 | 長尺データで待ち時間増加 | バックグラウンドジョブ化 |

## 3. 詳細調査と実装方針

### 3.1 高優先: USI対局思考（HvE/EvE/CSA）

### 根拠（同期実行箇所）
- HvE: `src/game/humanvsenginestrategy.cpp:218`
- EvE: `src/game/enginevsenginestrategy.cpp:147`, `:201`, `:260`, `:315`
- CSA進行: `src/network/csamoveprogresshandler.cpp:265`
- CSA思考本体: `src/network/csaenginecontroller.cpp:104`
- bestmove待機: `src/engine/usimatchhandler.cpp:358`, `:375`

### 問題
- 着手処理スロット内でエンジン応答待機まで完結しているため、入力・描画・ネットワーク処理の遅延要因になる。
- CSAでは `onMoveReceived` 系の経路（`src/network/csagamecoordinator.cpp:294`）で思考を直実行しており、受信処理自体が詰まりやすい。

### 推奨方式
- **エンジンI/O専用ワーカー**を `QThread` で常駐化。
- UIスレッドは「リクエスト送信」と「結果適用」だけ担当。
- `QProcess` / `UsiProtocolHandler` / 待機ロジックはワーカー側に集約。

### 実装イメージ
1. `EngineSearchRequest` / `EngineSearchResult` 構造体を定義（`requestId`含む）。
2. `EngineWorker : QObject` を作り、`moveToThread(engineThread)`。
3. `requestBestMove(request)` を `Qt::QueuedConnection` で呼び出し。
4. `bestMoveReady(result)` をUIへ返し、盤面反映はUI側で実施。
5. stale結果は `requestId` で破棄（局面更新/中断直後の遅延応答対策）。

### 注意点
- `ShogiGameController` / `ShogiBoard` / 各UIモデルはUIスレッド専用のまま維持。
- ワーカーへ渡すのは `position` 文字列・時刻・設定などの**値オブジェクト**に限定。
- `QProcess` は生成したスレッドでイベントループを回す（途中でスレッド移動しない）。

### 3.2 高優先: USI待機ループの非同期化

### 根拠
- `src/engine/usiprotocolhandler_wait.cpp:58`（while待機）
- `src/engine/usiprotocolhandler_wait.cpp:87`（`processEvents` + `msleep`）
- `src/engine/usiprotocolhandler_wait.cpp:102`（無期限待機）
- 解析モードでも同期待機を使用: `src/engine/usi_communication.cpp:100`, `:111`

### 問題
- ネストしたイベントループ/ポーリング待機は、再入・予期せぬ順序実行・CPUスパイクの原因になりやすい。

### 推奨方式
- `waitFor...` をUI経路から排除し、**シグナル駆動state machine**へ置換。
- `go`送信時に `QTimer` を開始し、`bestmove`受信 or timeoutで完了シグナルを返す。

### 実装ステップ
1. `beginSearch(requestId, timeoutMs)` を追加。
2. `bestmoveReceived` で `searchFinished(requestId, result)` をemit。
3. timeout時は `sendStop()` → 猶予タイマー → `searchTimeout` をemit。
4. 既存 `waitForBestMove*` は互換用途に限定（新経路では不使用）。

## 3.3 高優先: 棋譜ロード（読込/解析/SFEN再構築）

### 根拠
- ロード入口（UI直下）: `src/kifu/kifufilecontroller.cpp:56`
- 同期ロード本体: `src/kifu/kifuloadcoordinator.cpp:152`
- 解析→SFEN構築: `src/kifu/kifuloadcoordinator.cpp:190`, `:206`, `:220`
- 適用処理の重い再構築: `src/kifu/kifuapplyservice.cpp:102`, `:117`, `:129`, `:194`
- ファイル読込（全読み）: `src/kifu/kifreader.cpp:116`
- 例: KIF変換で複数フェーズ解析: `src/kifu/formats/kiftosfenconverter.cpp:202`
- SFEN再構築ループ: `src/board/sfenpositiontracer.cpp:332`, `:357`

### 問題
- 解析とデータ再構築が1本の同期処理で、ファイルサイズや分岐数に比例してUI停止時間が増える。

### 推奨方式
- 解析フェーズを `QtConcurrent::run` で別スレッド化。
- UIスレッドでは `applyParsedResult` などのUI適用のみ実施。

### 実装ステップ
1. `KifuLoadJobInput/Output` を定義（`initialSfen`, `teaiLabel`, `KifParseResult`, `infoItems`, `warn`）。
2. `parseFunc` / `detectSfenFunc` / `extractGameInfoFunc` をジョブ側で実行。
3. 完了通知を `QFutureWatcher` で受け、UIスレッドで `m_applyService->applyParsedResult(...)`。
4. ロード世代番号（generation）で古い結果を破棄。
5. 任意でキャンセルフラグ（`std::atomic_bool`）を導入。

### 注意点
- `QWidget` / `QAbstractItemModel` へのアクセスはワーカー禁止。
- `KifParseResult` は値で受け渡し（shared mutable state回避）。

## 3.4 中優先: エンジン起動・初期化・終了の同期待機

### 根拠
- `waitForStarted`: `src/engine/engineprocessmanager.cpp:59`
- `waitForFinished`: `src/engine/engineprocessmanager.cpp:81`, `:83`, `:85`
- 同期初期化シーケンス: `src/engine/usi.cpp:310` → `src/engine/usiprotocolhandler.cpp:71`
- `waitForUsiOk`/`waitForReadyOk`: `src/engine/usiprotocolhandler.cpp:76`, `:105`

### 問題
- エンジン起動失敗時や終了時にUIブロックが発生しうる。

### 推奨方式
- 3.1のエンジンワーカースレッドへ統合するのが最小リスク。
- 代替案として `startProcessAsync` / `initializeEngineAsync` のイベント駆動化。

### 実装ポイント
- 起動状態を `Idle -> Starting -> Handshaking -> Ready` で管理。
- 各遷移をシグナル/タイマーで駆動し、同期waitを廃止。

## 3.5 中優先: 定跡ファイル（.db）ロード/保存

### 根拠
- UI操作から直接I/O: `src/dialogs/josekiwindow.cpp:95`, `:110`, `:168`, `:190`
- 読み込みループ: `src/dialogs/josekirepository.cpp:148`, `:174`
- 保存ループ: `src/dialogs/josekirepository.cpp:336`, `:353`, `:376`

### 問題
- 大規模定跡ファイルで読み込み/保存中の操作感が悪化する。

### 推奨方式
- `QtConcurrent` で `loadFromFile`/`saveToFile` 相当をワーカー化。
- 読み込み完了時にUIスレッドでリポジトリを入れ替え（swap）する。

### 実装ポイント
- 読み込み中はボタン無効化 + プログレス表示。
- エラー文字列は完了時にUIへ返す。

## 3.6 中優先: エンジン登録処理

### 根拠
- UIイベント直後に同期登録開始: `src/dialogs/engineregistrationdialog.cpp:122`
- 同期起動待機: `src/dialogs/engineregistrationhandler.cpp:136`
- 終了待機: `src/dialogs/engineregistrationhandler.cpp:110`

### 問題
- エンジン追加時にダイアログ応答が止まりやすい。

### 推奨方式
- エンジン登録専用ワーカー（`QThread`）を導入。
- `QProcess` のreadyRead駆動は維持しつつ、UIと分離。

### 実装ポイント
- `engineRegistered` / `errorOccurred` をQueuedでUIへ戻す。
- タイムアウト（`usiok`未達）を明示して中断可能にする。

## 3.7 低-中優先: 棋譜保存/エクスポート・詰将棋局面生成

### 根拠
- 保存前の複数形式生成: `src/kifu/kifuexportcontroller.cpp:167`-`:172`
- 実ファイル書込ループ: `src/kifu/kifuioservice.cpp:112`, `:119`
- 詰将棋局面生成の多重ループ: `src/analysis/tsumeshogipositiongenerator.cpp:13`, `:17`, `:40`, `:62`
- 局面合法性チェック: `src/analysis/tsumeshogipositiongenerator.cpp:275`

### 推奨方式
- 保存はジョブ化（生成+書込をバックグラウンド）。
- 局面生成は必要に応じて生成専用ワーカーへ移管。

## 4. 導入順（実施しやすい順）

1. 棋譜ロードのバックグラウンド化（UI境界が明確で分離しやすい）
2. 定跡I/Oとエンジン登録のバックグラウンド化
3. USI待機ループの非同期state machine化
4. エンジンI/Oワーカー導入（HvE/EvE/CSAを順次移行）
5. 保存/エクスポート/詰将棋生成のジョブ化

## 5. 非機能要件と検証観点

- UI応答性: 長処理中もウィンドウ移動・タブ切替・キャンセル操作が100ms超で詰まらない
- 安全性: staleレスポンス無視（`requestId`）、停止時のクラッシュ無し
- スレッド安全: UIモデル更新はメインスレッド限定
- 回帰: 既存31テスト + 対局開始/棋譜読込/CSA対局の手動シナリオで確認

## 6. 補足（スレッド化しない方がよい領域）

- `ShogiView`、`QAbstractItemModel`、`QWidget`操作
- `ShogiGameController` / `ShogiBoard` の直接操作（現状は単一スレッド前提）

これらはワーカーから直接触らず、**値オブジェクトをシグナルで渡してUI側で適用**する設計を維持するのが安全です。
