# Task 20260301-mt-04: エンジン登録のバックグラウンド化（P1）

## 背景

エンジン登録ダイアログでエンジン追加時に、`QProcess` の同期起動待機（`waitForStarted`）と USI オプション収集（`waitForUsiOk`）がメインスレッドで実行され、ダイアログが応答不能になる。

### 対象ファイル

| ファイル | 行 | メソッド | ブロッキング方法 |
|---------|-----|---------|----------------|
| `src/dialogs/engineregistrationdialog.cpp` | 122 | UIイベントから同期登録開始 | 直接呼び出し |
| `src/dialogs/engineregistrationhandler.cpp` | 136 | 同期起動待機 | `waitForStarted()` |
| `src/dialogs/engineregistrationhandler.cpp` | 110 | 終了待機 | `waitForFinished(3000)` |

## 作業内容

### Step 1: 現状把握

以下のファイルを読み、エンジン登録フローを理解する:

- `src/dialogs/engineregistrationdialog.h/.cpp` — ダイアログUI
- `src/dialogs/engineregistrationhandler.h/.cpp` — 登録処理本体
- エンジンオプション収集の流れ（`usi` 送信 → `usiok` 待ち → オプション解析）

確認ポイント:
- `EngineRegistrationHandler` が QObject 派生か否か
- QProcess の生成・所有関係
- 登録完了後のデータ（エンジン名、オプション一覧）の型

### Step 2: EngineRegistrationWorker の新規作成

`src/dialogs/engineregistrationworker.h/.cpp` を作成する。

```cpp
// engineregistrationworker.h
#pragma once
#include <QObject>
#include <QThread>

class EngineRegistrationWorker : public QObject
{
    Q_OBJECT
public:
    explicit EngineRegistrationWorker(QObject* parent = nullptr);

public slots:
    void registerEngine(const QString& enginePath);

signals:
    // 成功時: エンジン名 + オプション一覧を返す
    void engineRegistered(const QString& engineName,
                          const QList<UsiOptionItem>& options);

    // 失敗時: エラーメッセージを返す
    void registrationFailed(const QString& errorMessage);

    // 進捗通知
    void progressUpdated(const QString& status);
};
```

**注意**: `UsiOptionItem` の型を確認し、値コピー可能であることを検証する。

### Step 3: ワーカースレッドの導入

`EngineRegistrationDialog` にワーカースレッドを導入する:

```cpp
// メンバ変数
QThread* m_workerThread = nullptr;
EngineRegistrationWorker* m_worker = nullptr;
```

**初期化**:
```cpp
m_workerThread = new QThread(this);
m_worker = new EngineRegistrationWorker();  // parent なし（moveToThread のため）
m_worker->moveToThread(m_workerThread);

connect(m_worker, &EngineRegistrationWorker::engineRegistered,
        this, &EngineRegistrationDialog::onEngineRegistered);
connect(m_worker, &EngineRegistrationWorker::registrationFailed,
        this, &EngineRegistrationDialog::onRegistrationFailed);
connect(m_worker, &EngineRegistrationWorker::progressUpdated,
        this, &EngineRegistrationDialog::onProgressUpdated);
connect(m_workerThread, &QThread::finished,
        m_worker, &QObject::deleteLater);

m_workerThread->start();
```

**終了処理**（デストラクタまたは closeEvent）:
```cpp
m_workerThread->quit();
m_workerThread->wait();
```

### Step 4: ワーカー内でのエンジン起動・オプション取得

`EngineRegistrationWorker::registerEngine()` の実装:

1. `QProcess` をワーカースレッド内で生成（`new QProcess()`、parent なし）
2. `process->start(enginePath)` で起動
3. `process->waitForStarted(5000)` — ワーカースレッド内なのでUIはブロックしない
4. `"usi\n"` を送信
5. `readyReadStandardOutput` を読み取り、`usiok` を待機
6. オプション行（`option name ...`）を解析
7. `"quit\n"` を送信、`waitForFinished(3000)`
8. 結果を `engineRegistered()` シグナルで通知
9. タイムアウト時は `registrationFailed()` で通知

### Step 5: ダイアログ側のUI更新

- 登録ボタン押下時に `QMetaObject::invokeMethod(m_worker, "registerEngine", ...)` で非同期呼び出し
  - または `connect(registerButton, &clicked, ...)` で `m_worker->registerEngine()` を `QueuedConnection` で呼ぶ
- 登録中は:
  - 「登録中...」プログレスバーまたはメッセージを表示
  - 登録/キャンセルボタンの状態を適切に管理
- 成功時: オプション一覧をダイアログに表示
- 失敗時: エラーメッセージを表示

### Step 6: タイムアウトとキャンセル対応

- `usiok` が5秒以内に来ない場合はタイムアウトエラー
- ユーザーがキャンセルした場合:
  - `QProcess::kill()` でプロセスを強制終了
  - ワーカーの処理を中断（`std::atomic_bool` フラグ）

### Step 7: CMakeLists.txt 更新

新規ファイル `engineregistrationworker.h/.cpp` を `CMakeLists.txt` のソースリストに追加する。

### Step 8: ビルド・テスト確認

```bash
cmake --build build
cd build && ctest --output-on-failure
```

**手動テスト項目**:
1. エンジン登録ダイアログでエンジンを追加できること
2. 登録中にダイアログが応答すること（移動/リサイズ可能）
3. 存在しないパスを指定した場合のエラー表示
4. 登録をキャンセルできること
5. タイムアウト時のエラー表示

## 完了条件

- [ ] `EngineRegistrationWorker` が作成されている
- [ ] エンジン登録がワーカースレッドで実行される
- [ ] 登録中にダイアログが応答する
- [ ] タイムアウトとキャンセルが機能する
- [ ] 全テスト pass
