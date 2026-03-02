# Task 12: QThread::msleep ビジーウェイト修正（P2 / §7.1）

## 概要

`src/engine/usiprotocolhandler_wait.cpp` で `QThread::msleep(1)` によるポーリングループが使われている。メインスレッドで実行された場合の UI フリーズリスクを排除する。

## 現状

`waitForBestMoveWithGrace()` 内:
```cpp
// ループ内で:
QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
QThread::msleep(1);
// m_bestMoveReceived をポーリング
```

## 手順

### Step 1: コンテキスト調査

1. `usiprotocolhandler_wait.cpp` 全体を読み、ポーリングループの構造を理解する
2. `m_bestMoveReceived` がどのスレッドから設定されるか確認する
3. このメソッドがどのスレッドから呼ばれるか（メインスレッド or ワーカースレッド）確認する
4. ループの終了条件（タイムアウト等）を確認する

### Step 2: 修正方針の検討

**方針A: QEventLoop + シグナル待機**
```cpp
QEventLoop loop;
connect(this, &UsiProtocolHandler::bestMoveReceived, &loop, &QEventLoop::quit);
QTimer::singleShot(timeout, &loop, &QEventLoop::quit);
loop.exec();
```

**方針B: 既存の processEvents を活かしつつ改善**
- `processEvents` が既にあるため、UI フリーズは回避されている可能性がある
- `msleep(1)` の代わりに `processEvents` のタイムアウトを調整

**方針C: QWaitCondition（ワーカースレッドの場合）**
- メインスレッドでない場合は `QWaitCondition` が適切

### Step 3: 実装

1. Step 1 の調査結果に基づいて最適な方針を選択する
2. 選択した方針で実装する
3. タイムアウト処理を保持する

### Step 4: ビルド・テスト

1. `cmake --build build` でビルドが通ることを確認
2. テストを実行して全パスを確認
3. エンジン連携の動作テスト（手動）を検討する

## 注意事項

- `processEvents` が既にある場合、UI フリーズの実害は限定的かもしれない（調査結果次第）
- エンジン通信のタイミングに依存する処理のため、変更による副作用に注意
- threading-guidelines.md に従うこと
