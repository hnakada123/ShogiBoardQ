# Task 20260301-mt-06: エンジンI/Oワーカースレッド導入（P0）

## 背景

USI対局思考（HvE/EvE/CSA）はUIスレッド上でbestmove待機まで実行しており、入力・描画・ネットワーク処理の遅延要因になっている。エンジンI/O専用のワーカースレッドを導入し、UIスレッドから分離する。

Task 05 で作成した非同期ステートマシンAPIをワーカースレッドで動作させる。

### 対象箇所（同期実行）

| ファイル | 行 | 処理 |
|---------|-----|------|
| `src/game/humanvsenginestrategy.cpp` | 218 | HvE思考 |
| `src/game/enginevsenginestrategy.cpp` | 147, 201, 260, 315 | EvE思考 |
| `src/network/csamoveprogresshandler.cpp` | 265 | CSA進行中の思考 |
| `src/network/csaenginecontroller.cpp` | 104 | CSA思考本体 |
| `src/engine/usimatchhandler.cpp` | 358, 375 | bestmove待機 |

## 作業内容

### Step 1: アーキテクチャ設計の確認

スレッド境界のシーケンス図:

```
メインスレッド(UI)                    エンジンワーカースレッド
     |                                        |
     |--- startEngine(path) ---------------->|
     |                                        |-- QProcess::start()
     |                                        |-- waitForStarted()
     |                                        |-- send "usi" → waitForUsiOk
     |                                        |-- send "isready" → waitForReadyOk
     |<-- engineReady() -----------------------|
     |                                        |
     |--- sendGo(posCmd, goCmd) ------------->|
     |                                        |-- send "position ..." + "go ..."
     |<-- infoLineReceived(info) ------------|  (thinking情報を随時転送)
     |<-- bestMoveReceived(move, ponder) -----|
     |                                        |
     |--- stopEngine() --------------------->|
     |                                        |-- send "quit"
     |                                        |-- waitForFinished()
     |<-- engineStopped() -------------------|
```

**原則**:
- `QProcess` はワーカースレッドで生成・操作する
- `ShogiGameController` / `ShogiBoard` / UIモデルはメインスレッド専用
- ワーカーへ渡すのは `position` 文字列・時刻・設定などの値オブジェクトに限定
- 結果は `Qt::QueuedConnection` のシグナルで返す

### Step 2: EngineWorker クラスの新規作成

`src/engine/engineworker.h/.cpp` を作成する。

```cpp
// engineworker.h
#pragma once
#include <QObject>

class QProcess;
class UsiProtocolHandler;
class EngineProcessManager;

class EngineWorker : public QObject
{
    Q_OBJECT
public:
    explicit EngineWorker(QObject* parent = nullptr);
    ~EngineWorker();

public slots:
    // メインスレッドから呼ばれる（QueuedConnection）
    void startEngine(const QString& enginePath, const QStringList& engineArgs);
    void stopEngine();
    void sendGo(const QString& positionCommand, const QString& goCommand,
                quint64 requestId);
    void sendStop();     // 思考中断
    void sendPonderhit();
    void sendSetOption(const QString& name, const QString& value);

signals:
    // メインスレッドへ返す
    void engineReady(const QString& engineName);
    void engineError(const QString& errorMessage);
    void engineStopped();
    void bestMoveReceived(quint64 requestId,
                          const QString& bestMove,
                          const QString& ponderMove);
    void infoLineReceived(const QString& infoLine);
    void readyForNextCommand();

private:
    QProcess* m_process = nullptr;
    UsiProtocolHandler* m_protocolHandler = nullptr;
    EngineProcessManager* m_processManager = nullptr;
};
```

**注意点**:
- `EngineWorker` は `moveToThread()` で使用するため、コンストラクタで parent を設定しない（呼び出し側で管理）
- `QProcess` はワーカースレッドの `startEngine()` 内で生成する（生成スレッドでのみ操作可能のため）

### Step 3: EngineWorker の実装

`engineworker.cpp` で以下を実装:

1. **`startEngine()`**:
   - `m_processManager = new EngineProcessManager(this);`
   - `m_protocolHandler = new UsiProtocolHandler(this);`
   - プロセス起動 → USI初期化（waitForUsiOk/waitForReadyOk はワーカースレッド内なのでOK）
   - 成功: `emit engineReady(engineName)`
   - 失敗: `emit engineError(errorMessage)`

2. **`sendGo()`**:
   - `position` コマンド送信
   - `go` コマンド送信
   - `bestmove` 受信を `UsiProtocolHandler` のシグナルで待つ
   - 受信したら `emit bestMoveReceived(requestId, bestMove, ponderMove)`

3. **`sendStop()`**:
   - `"stop\n"` を送信
   - bestmove を待って結果返却

4. **`stopEngine()`**:
   - `"quit\n"` 送信 + `waitForFinished()`
   - `emit engineStopped()`

5. **`infoLineReceived` の転送**:
   - `UsiProtocolHandler` の `infoLineReceived` を `EngineWorker::infoLineReceived` に接続
   - `ThinkingInfoPresenter` のバッファリング（50ms）はメインスレッド側に維持

### Step 4: Usi クラスをファサードとして改修

`src/engine/usi.h/.cpp` を改修し、スレッド境界を隠蔽する:

```cpp
// usi.h に追加
private:
    QThread* m_workerThread = nullptr;
    EngineWorker* m_worker = nullptr;

public:
    // 非同期API（内部でワーカースレッドに委譲）
    void startEngineAsync(const QString& path);
    void sendGoAsync(const QString& posCmd, const QString& goCmd, quint64 requestId);
    void stopEngineAsync();

signals:
    void asyncEngineReady(const QString& engineName);
    void asyncBestMoveReceived(quint64 requestId, const QString& bestMove,
                                const QString& ponderMove);
    void asyncEngineError(const QString& errorMessage);
    void asyncEngineStopped();
```

**実装**:
1. `startEngineAsync()` で `QThread` を作成し、`EngineWorker` を `moveToThread()`
2. ワーカーのシグナルを `Usi` クラスのシグナルに転送（signal-to-signal forwarding）
3. `stopEngineAsync()` で `quit()` + `wait()` + cleanup

### Step 5: Strategy クラスの非同期対応

`HumanVsEngineStrategy`, `EngineVsEngineStrategy` を非同期APIに移行する:

1. **HvE** (`src/game/humanvsenginestrategy.cpp`):
   - エンジン着手リクエスト: `sendGoAsync()` で送信
   - `asyncBestMoveReceived` シグナルを受けて盤面に適用
   - 同期的な `waitForBestMove()` 呼び出しを削除

2. **EvE** (`src/game/enginevsenginestrategy.cpp`):
   - 各エンジンの `sendGoAsync()` → `asyncBestMoveReceived` → 盤面適用 → 次のターン
   - `QTimer::singleShot(0, ...)` でのターン回転は維持

3. **CSA** (`src/network/csaenginecontroller.cpp`, `csamoveprogresshandler.cpp`):
   - 受信した相手手を適用後、`sendGoAsync()` で自分の思考開始
   - `asyncBestMoveReceived` で着手 + CSAサーバーへ送信

### Step 6: エンジン2台の独立スレッド化

エンジンは最大2台同時に動作する。各 `Usi` インスタンスが独立した `QThread` + `EngineWorker` を持つ。

- `m_usi1` と `m_usi2` がそれぞれ独自の `m_workerThread` を管理
- 相互のロック不要（データ共有なし）

### Step 7: stale 結果の破棄

`requestId` を用いて、以下のケースで古い結果を無視する:

1. 局面が更新された後に前の局面の結果が届いた場合
2. 対局が中断された後に結果が届いた場合
3. 新しいリクエストが送信された後に古い結果が届いた場合

```cpp
void HumanVsEngineStrategy::onBestMoveReceived(quint64 requestId,
                                                 const QString& bestMove,
                                                 const QString& ponderMove)
{
    if (requestId != m_currentRequestId) {
        return;  // stale結果は無視
    }
    // 着手を適用...
}
```

### Step 8: CMakeLists.txt 更新

新規ファイルを追加:
- `src/engine/engineworker.h`
- `src/engine/engineworker.cpp`

### Step 9: ビルド・テスト確認

```bash
cmake --build build
cd build && ctest --output-on-failure
```

**手動テスト項目**:
1. HvE対局でエンジンが思考し、着手すること
2. EvE対局が進行すること
3. 対局中にUI操作（ウィンドウ移動、メニュー操作）が可能なこと
4. エンジン起動失敗時にエラーが表示されること
5. 対局中断でエンジンが正常停止すること
6. CSA対局が機能すること（可能であれば）
7. 2台のエンジンが同時に動作すること（EvE）

## 注意事項

- `QProcess` は生成したスレッドで使用する必要がある（Qt の制約）
- `infoLineReceived` の頻度が高いため、パフォーマンスを確認する。必要に応じてバッファリングをワーカー側に移動
- 既存の同期API（`waitFor*`）はワーカースレッド内では引き続き使用可能
- `ShogiBoard` への書き込みはメインスレッド限定を厳守

## 完了条件

- [ ] `EngineWorker` クラスが作成されている
- [ ] `Usi` クラスがファサードとしてスレッド境界を隠蔽している
- [ ] エンジンI/Oがワーカースレッドで実行される
- [ ] HvE/EvE対局が非同期で動作する
- [ ] stale結果の破棄が `requestId` で機能する
- [ ] エンジン2台が独立スレッドで動作する
- [ ] 対局中にUIが応答する
- [ ] 全テスト pass
