# スレッド安全ガイドライン

ShogiBoardQ のマルチスレッド化における設計方針と実装ルールを定める。

## 1. スレッドモデル

```
メインスレッド (GUI)
├── QWidget / QGraphicsScene / QAbstractItemModel 操作
├── シグナル/スロット配線
└── ユーザー操作の受付

ワーカースレッド (QtConcurrent / QThread)
├── エンジンI/O（USIプロセス通信）
├── ファイルI/O（棋譜ロード/保存、定跡ファイル）
└── CPU集約処理（一括解析、詰将棋探索）
```

- ワーカースレッドはメインスレッドから起動し、結果をシグナルで返す
- ワーカースレッド同士の直接通信は行わない

## 2. UIアクセスルール

以下のクラスは**メインスレッドでのみ**操作すること:

- `QWidget` およびそのサブクラス
- `QAbstractItemModel` およびそのサブクラス
- `QGraphicsItem` / `QGraphicsScene`
- `QAction`, `QMenu`, `QToolBar`

ワーカースレッドからUIを更新する場合は、シグナル/スロット（`Qt::QueuedConnection`）を使用する。

## 3. QProcess ルール

`QProcess` は**生成したスレッドでのみ**操作すること。

- `QProcess` を別スレッドに `moveToThread()` してはならない
- エンジンI/Oワーカースレッドでは `QProcess` をそのスレッド内で生成・操作する
- メインスレッドで生成した `QProcess` をワーカーから操作してはならない

## 4. スレッド間通信

### 推奨パターン

1. **シグナル/スロット** (`Qt::QueuedConnection`)
   - スレッド境界をまたぐ場合は `Qt::QueuedConnection` を明示する
   - 引数は値渡し（暗黙的共有型を含む）

2. **QtConcurrent + QFutureWatcher**
   - 単発の非同期処理に使用
   - `QFutureWatcher::finished` シグナルでメインスレッドに結果を返す

### 禁止パターン

- `QMetaObject::invokeMethod` でのスレッド間呼び出し（デバッグ困難）
- 共有ポインタ経由でのミュータブル状態共有
- `std::thread` の直接使用（Qt のイベントループと統合できない）

## 5. データ受け渡し

ワーカースレッドには**値オブジェクト（コピー）のみ**渡す。

```cpp
// Good: 値コピーを渡す
struct KifuLoadRequest {
    QString filePath;
    JobGeneration generation;
    CancelFlag cancelFlag;
};

// Bad: ポインタや参照を渡す
void loadInWorker(GameRecordModel* model);  // 禁止
```

- `QString`, `QList`, `QMap` 等の暗黙的共有型はコピーコストが低い
- 結果もシグナル経由で値として返す
- 共有ミュータブル状態は禁止

## 6. stale結果の破棄

非同期処理の結果が古くなった場合に備え、`JobGeneration` を使用する。

```cpp
// リクエスト時に世代番号をインクリメント
++m_currentGeneration;
auto gen = m_currentGeneration;

// 結果受信時に世代番号を照合
void onResultReady(JobGeneration gen, const Result& result)
{
    if (gen != m_currentGeneration)
        return;  // stale結果を破棄
    // 最新の結果を処理
}
```

## 7. キャンセル

ワーカースレッド内で定期的にキャンセルフラグをチェックする。

```cpp
CancelFlag cancel = makeCancelFlag();

// ワーカー内
for (int i = 0; i < total; ++i) {
    if (cancel->load())
        return;  // キャンセルされた
    // 処理を続行
}

// キャンセル要求
cancel->store(true);
```

- `CancelFlag` は `std::shared_ptr<std::atomic_bool>` のエイリアス
- ワーカーとリクエスト側で共有する

## 8. connect() ルール

スレッド間接続でも**ラムダ禁止**（CLAUDE.md 準拠）。

```cpp
// Good: メンバ関数ポインタ + 明示的接続タイプ
connect(worker, &Worker::resultReady,
        this, &Controller::onResultReady,
        Qt::QueuedConnection);

// Bad: ラムダ使用
connect(worker, &Worker::resultReady,
        [this](const Result& r) { ... });
```

## 9. 既存パターンとの整合

マルチスレッド化しても以下の既存パターンを維持する:

- **`ensure*()` 遅延初期化**: メインスレッドで実行（ワーカーから呼ばない）
- **Deps/Hooks/Refs パターン**: 依存注入構造体は変更なし
- **所有権ルール**: QObject は parent ownership、非QObject は `std::unique_ptr`

ワーカーで使用するオブジェクトは、ワーカー起動前にメインスレッドで準備する。
