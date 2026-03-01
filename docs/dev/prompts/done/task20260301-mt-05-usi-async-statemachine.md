# Task 20260301-mt-05: USI待機ループの非同期ステートマシン化（P1）

## 背景

`UsiProtocolHandler` の `waitFor*` メソッド群が `QEventLoop` / `processEvents` / `msleep` を使用して同期的に応答を待機している。ネストされたイベントループは再入・予期せぬ順序実行・CPUスパイクの原因になる。これをシグナル駆動のステートマシンに置き換える。

### 対象ファイル（全て `src/engine/usiprotocolhandler_wait.cpp`）

| 行 | メソッド | ブロッキング方法 | タイムアウト |
|----|---------|----------------|------------|
| 34 | `waitForUsiOk()` | `QEventLoop::exec()` | 5000ms |
| 40 | `waitForReadyOk()` | `QEventLoop::exec()` | 5000ms |
| 46 | `waitForBestMove()` | 10msスピンループ + `QEventLoop` | 可変 |
| 78 | `waitForBestMoveWithGrace()` | `processEvents()` + `msleep(1)` | 可変 |
| 98 | `keepWaitingForBestMove()` | 無限 `QEventLoop` | 無制限 |
| 119 | `waitForStopOrPonderhit()` | `QEventLoop::exec()` | 無制限 |

### 関連ファイル

| ファイル | 行 | 用途 |
|---------|-----|------|
| `src/engine/usi_communication.cpp` | 100, 111 | 解析モードでの同期待機呼び出し |
| `src/engine/usiprotocolhandler.cpp` | 71, 76, 105 | 初期化シーケンスの同期呼び出し |
| `src/engine/usimatchhandler.cpp` | 358, 375 | bestmove待機の呼び出し元 |
| `src/engine/engineprocessmanager.cpp` | 59, 81-85 | プロセス起動/終了の同期待機 |

## 作業内容

### Step 1: 現状の全waitメソッドとその呼び出し元を棚卸し

以下を調査し、一覧を作成する:

1. `waitForUsiOk()` の全呼び出し元
2. `waitForReadyOk()` の全呼び出し元
3. `waitForBestMove()` / `waitForBestMoveWithGrace()` / `keepWaitingForBestMove()` の全呼び出し元
4. `waitForStopOrPonderhit()` の全呼び出し元
5. `waitForStarted()` / `waitForFinished()` の全呼び出し元

各呼び出し元が「待機完了後に何をしているか」を記録する。

### Step 2: USI プロトコルのステート定義

`src/engine/usiprotocolstate.h` を新規作成する:

```cpp
#pragma once

enum class UsiProtocolState {
    Idle,           // 未起動
    Starting,       // プロセス起動中
    WaitingUsiOk,   // "usi" 送信済み、"usiok" 待ち
    WaitingReadyOk, // "isready" 送信済み、"readyok" 待ち
    Ready,          // 準備完了（go 送信可能）
    Searching,      // "go" 送信済み、"bestmove" 待ち
    Stopping,       // "stop" 送信済み、bestmove 待ち（猶予期間）
    Quitting,       // "quit" 送信済み、プロセス終了待ち
    Error           // エラー状態
};
```

### Step 3: 非同期初期化メソッドの追加

`UsiProtocolHandler` に非同期版メソッドを追加する（既存の同期版は維持）:

```cpp
// usiprotocolhandler.h に追加
signals:
    void usiOkReceived();     // 既存シグナルがあれば流用
    void readyOkReceived();   // 既存シグナルがあれば流用
    void initializationCompleted();
    void initializationFailed(const QString& reason);

public:
    // 非同期版: シグナルで結果を通知
    void beginInitialize();   // usi 送信 → usiok → isready → readyok → initializationCompleted
```

**実装**:
1. `beginInitialize()` で `"usi\n"` を送信し、状態を `WaitingUsiOk` に遷移
2. `QTimer` でタイムアウト（5秒）を設定
3. `usiok` 受信時（既存のパースロジック内）:
   - 状態を `WaitingReadyOk` に遷移
   - `"isready\n"` を送信
   - タイムアウトタイマーをリセット
4. `readyok` 受信時:
   - 状態を `Ready` に遷移
   - `initializationCompleted()` をemit
5. タイムアウト時:
   - `initializationFailed("Timeout waiting for usiok/readyok")` をemit

### Step 4: 非同期思考リクエストの追加

```cpp
// usiprotocolhandler.h に追加

// 思考リクエスト構造体
struct EngineSearchRequest {
    quint64 requestId = 0;
    QString positionCommand;   // "position startpos moves ..."
    QString goCommand;         // "go btime ... wtime ..."
    int timeoutMs = 0;         // 0 = 無制限
};

struct EngineSearchResult {
    quint64 requestId = 0;
    QString bestMove;
    QString ponderMove;
    bool timeout = false;
    bool cancelled = false;
};

signals:
    void searchFinished(const EngineSearchResult& result);

public:
    void beginSearch(const EngineSearchRequest& request);
    void cancelSearch();  // "stop" を送信
```

**実装**:
1. `beginSearch()` で `position` + `go` を送信し、状態を `Searching` に遷移
2. `QTimer` でタイムアウトを設定（`timeoutMs > 0` の場合）
3. `bestmove` 受信時:
   - `EngineSearchResult` を構築
   - `searchFinished(result)` をemit
   - 状態を `Ready` に遷移
4. `cancelSearch()`:
   - `"stop\n"` を送信
   - 状態を `Stopping` に遷移
   - 猶予タイマー（3秒）後もbestmoveが来なければ `searchFinished` (cancelled=true) をemit
5. タイムアウト時:
   - `cancelSearch()` を呼び出し

### Step 5: 既存の同期待機メソッドに deprecation マークを追加

```cpp
// 既存メソッドは維持するが、新コードでは使用しないことを明示
// [[deprecated("Use beginInitialize() instead")]]
bool waitForUsiOk(int timeoutMs = 5000);
```

**注意**: 実際に `[[deprecated]]` を付けるとビルド警告が大量に出るため、コメントで示すだけでもよい。移行完了後に削除する。

### Step 6: UsiMatchHandler / usi_communication の移行

既存の同期呼び出し元を非同期版に段階的に移行する:

1. **UsiMatchHandler** (`src/engine/usimatchhandler.cpp`):
   - `waitForBestMove*()` 呼び出しを `beginSearch()` に置き換え
   - `searchFinished` シグナルを受けて次のターンに進む
   - **この段階では全面移行せず、新API の動作確認を優先する**

2. **usi_communication** (`src/engine/usi_communication.cpp`):
   - 解析モードの同期待機を非同期版に移行
   - 解析コーディネーターとの連携を調整

### Step 7: EngineProcessManager の非同期化

`src/engine/engineprocessmanager.cpp` の `waitForStarted()` / `waitForFinished()` を非同期化:

```cpp
signals:
    void processStarted();
    void processStartFailed(const QString& error);
    void processStopped();

public:
    void startProcessAsync(const QString& path);
    void stopProcessAsync();
```

### Step 8: ビルド・テスト確認

```bash
cmake --build build
cd build && ctest --output-on-failure
```

- 既存の `waitFor*` テストが pass すること（互換維持）
- 新しい非同期APIのユニットテストを追加:
  - `tst_usiprotocolhandler.cpp` に `beginInitialize` / `beginSearch` のテスト

**手動テスト項目**:
1. エンジン起動・初期化が完了すること
2. 対局でエンジンが思考し、着手すること
3. エンジン停止が正常に完了すること
4. タイムアウト時にエラーが通知されること

## 注意事項

- この段階ではまだメインスレッドで実行される。ワーカースレッドへの移動は Task 06 で行う
- 既存の同期APIを急に削除しない。呼び出し元を全て移行してから削除する
- `infoLineReceived` シグナルの頻度が高い（`ThinkingInfoPresenter` で50msバッファリング済み）ため、パフォーマンスへの影響を確認する

## 完了条件

- [ ] `UsiProtocolState` 列挙型が定義されている
- [ ] `beginInitialize()` が実装され、シグナルで結果を通知する
- [ ] `beginSearch()` / `cancelSearch()` が実装されている
- [ ] `EngineProcessManager` に非同期版メソッドがある
- [ ] 既存の同期APIとの互換性が維持されている
- [ ] 全テスト pass
