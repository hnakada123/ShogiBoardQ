# スレッド安全ガイドライン

ShogiBoardQ のマルチスレッド化における設計方針と実装ルールを定める。

## 1. スレッドモデル

```
メインスレッド (GUI)
├── QWidget / QGraphicsScene / QAbstractItemModel 操作
├── シグナル/スロット配線・ユーザー操作の受付
└── QProcessによる非同期USI通信（started / readyRead / finished とタイマー）

ワーカースレッド (QtConcurrent / QThread)
├── 棋譜ファイルの読み込み・形式解析・SFEN列と指し手の構築
├── 既存の定跡処理・外部詰将棋エンジン通信
└── CPU集約処理（詰将棋探索・候補生成）
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
- 共有ポインタ経由でのミュータブル状態共有（キャンセル用atomicを除く）
- `std::thread` の直接使用（Qt のイベントループと統合できない）

## 5. データ受け渡し

ワーカースレッドには**値オブジェクト（コピー）のみ**渡す。

```cpp
// Good: 値（入力の文字列・キャンセルフラグ）を渡し、結果も値（KifuLoadResult）で受け取る
// （KifuLoadCoordinator::startLoad）
m_loadWatcher->setFuture(QtConcurrent::run([input, text, cancel]() {
    return text ? KifuLoadParser::parseText(input, cancel)
                : KifuLoadParser::parseFile(input, std::nullopt, cancel);
}));

// Bad: ポインタや参照を渡す
void loadInWorker(GameRecordModel* model);  // 禁止
```

- `QString`, `QList`, `QMap` 等の暗黙的共有型はコピーコストが低い
- 結果もシグナル経由で値として返す
- 通常の共有ミュータブル状態は禁止。キャンセルフラグと探索スレッド予算のatomicのみ例外とする。

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

- **部品の生成**: `MainWindowComposer` が起動時にメインスレッドで全部作る（[composition-root.md](composition-root.md)）。ワーカーから部品を作らない
- **Deps/Hooks/Refs パターン**: 依存注入構造体は変更なし
- **所有権ルール**: QObject は parent ownership、非QObject は `std::unique_ptr`

ワーカーで使用するオブジェクトは、ワーカー起動前にメインスレッドで準備する。

## 10. 棋譜読み込みとUSIの非同期経路

- GUIのファイル選択・貼り付けは `KifuLoadCoordinator::loadFileAsync/loadTextAsync` を使う。
- `KifuLoadParser` は入力とキャンセルフラグだけを受け取り、値（`KifuLoadResult`）として結果を返す。分岐ツリーの作り直しは `KifuLoadCoordinator` がGUIスレッドで行い（`KifuBranchTreeBuilder`）、棋譜欄・盤などの表示はツリーの変更を受けて各部品がGUIスレッドで合わせる。
- 読み込みの置換・中止では旧watcherを切り離し、古い結果を反映しない。貼り付けたテキストは一時ファイルを作らず、テキストのまま解析する。変換器内部の解析中は強制停止せず、処理の境界でキャンセルを確認する。
- 通常のGUI対局・解析とCLI解析ジョブは非同期USI初期化を使う。`usiok` と `readyok` はそれぞれタイマーで監視し、初期化完了まで探索要求を保留する。
- 対局の着手は `matchMoveReady` で適用する。先読み不一致時は `stop` の応答を回収してから新しい `go` を送る。即指しの `stop` と要求のキャンセルは区別する。
- 終了は `quit → terminate → kill` をタイマーで進める（`EngineProcessManager::stopProcessAsync()`。呼び出し元を破棄した後もイベント駆動で完了させる）。応答を待ってイベントループを止める待機型APIは使用しない。

## 11. 内蔵詰将棋探索のスレッド予算

Hayanagiの既存の並列探索を使い、OpenMPは追加しない。
`TsumeThreadBudget` は同時実行中の探索間で追加スレッドを共有する。追加分の上限は
`clamp(idealThreadCount / 2, 1, 4) - 1` 本で、各探索の呼出元ワーカー1本は別に存在する。
候補選別は外部エンジンとの同時実行を考慮して1探索あたり最大2本、局面評価・解答対局は最大4本とする。
外部USIエンジン自身のThreads設定は変更しない。

### 動作確認と計測

`tests/tst_background_tasks.cpp` で6形式の読み込み、古い結果の破棄、遅延したUSI初期化、
先読み切り替え、キャンセル、異常終了、探索結果・手数の一致を検証する。
GUIテストでは読み込み中のウィンドウ破棄、即指し、CSA、連続対局も確認する。

2026-09-29の開発環境で、同じHayanagiに対し `Hayanagi/tests/bench_tsume.py` を各2回実行した参考値
（最小時間、1スレッド対4スレッド）:

| 局面 | 1スレッド | 4スレッド |
|---|---:|---:|
| mate5-2 | 4.6 ms | 3.4 ms |
| defense6 | 3.8 ms | 2.7 ms |
| nomate5 | 211.6 ms | 76.4 ms |

8局面すべてで判定と手数が一致した。非常に短い探索では起動費用により遅くなる場合もあり、
全局面での高速化を保証するものではない。GUI全体の性能とは別の探索単体の計測である。
