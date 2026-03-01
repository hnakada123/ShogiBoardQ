# マルチスレッド化分析レポート

本ドキュメントでは、ShogiBoardQ のソースコードを調査し、マルチスレッド化すべき箇所・方法・優先度を整理する。

## 現状の概要

ShogiBoardQ は **すべての処理がメインスレッド（GUIスレッド）** で実行されている。真のマルチスレッド（`QThread`, `QRunnable`, `QtConcurrent`, `std::thread`）は一切使用されていない。

現在の非同期パターン:
- `QEventLoop` によるブロッキング待機（エンジン通信）
- `QTimer::singleShot(0, ...)` によるイベントループへの遅延実行
- `QTcpSocket` のシグナル/スロットによる非同期ネットワーク I/O（CSAクライアント）

同期プリミティブ（`QMutex`, `std::mutex` 等）は存在しない。

---

## 優先度1: エンジン通信のマルチスレッド化（影響度: 最高）

### 対象箇所

| ファイル | メソッド | ブロッキング方法 | タイムアウト |
|---------|---------|----------------|------------|
| `src/engine/usiprotocolhandler_wait.cpp:34` | `waitForUsiOk()` | `QEventLoop::exec()` | 5000ms |
| `src/engine/usiprotocolhandler_wait.cpp:40` | `waitForReadyOk()` | `QEventLoop::exec()` | 5000ms |
| `src/engine/usiprotocolhandler_wait.cpp:46` | `waitForBestMove()` | 10msスピンループ + `QEventLoop` | 可変 |
| `src/engine/usiprotocolhandler_wait.cpp:78` | `waitForBestMoveWithGrace()` | `processEvents()` + `msleep(1)` | 可変 |
| `src/engine/usiprotocolhandler_wait.cpp:98` | `keepWaitingForBestMove()` | 無限 `QEventLoop` | 無制限 |
| `src/engine/usiprotocolhandler_wait.cpp:119` | `waitForStopOrPonderhit()` | `QEventLoop::exec()` | 無制限 |
| `src/engine/engineprocessmanager.cpp:59` | `startProcess()` 内 `waitForStarted()` | `QProcess::waitForStarted()` | OS依存 |
| `src/engine/engineprocessmanager.cpp:81-85` | `stopProcess()` 内 `waitForFinished()` | `QProcess::waitForFinished()` | 1000-3000ms / 無制限 |

### 問題点

- エンジン初期化時に `waitForStarted()` → `waitForUsiOk()` → `waitForReadyOk()` と連続ブロッキングが発生し、合計 **6〜10秒** UIが応答不能になる可能性がある
- 対局中の `waitForBestMove()` は `QEventLoop::AllEvents` でイベント処理するが、ネストされたイベントループは再入問題（シグナルの再帰呼び出し）のリスクがある
- `keepWaitingForBestMove()` は無期限待機で、中断手段がシグナルのみ

### 推奨方法: QThread によるエンジンワーカースレッド

```
メインスレッド                    エンジンワーカースレッド
     |                                   |
     |--- startEngine(path) ------------>|
     |                                   |-- QProcess::start()
     |                                   |-- waitForStarted()
     |                                   |-- send "usi"
     |                                   |-- waitForUsiOk()
     |                                   |-- send "isready"
     |                                   |-- waitForReadyOk()
     |<-- engineReady() ----------------|
     |                                   |
     |--- sendGo(position, options) ---->|
     |                                   |-- send "position ... go ..."
     |<-- infoLineReceived(info) --------|  (thinking情報を随時転送)
     |<-- bestMoveReceived(move) --------|
     |                                   |
     |--- stopEngine() ---------------->|
     |                                   |-- send "quit"
     |                                   |-- waitForFinished()
     |<-- engineStopped() --------------|
```

**実装方針:**

1. `EngineWorkerThread` クラスを新規作成（`QThread` 継承、または `QObject` を `moveToThread()`）
2. `QProcess`, `EngineProcessManager`, `UsiProtocolHandler` をワーカースレッドに移動
3. メインスレッドとの通信は `queued connection` のシグナル/スロットで行う
4. `waitFor*()` メソッド群はワーカースレッド内でのみ呼び出されるため、GUIをブロックしない
5. `Usi` クラスをファサードとして維持し、スレッド境界を隠蔽

**注意点:**
- `QProcess` は生成されたスレッドで使用する必要がある（Qt の制約）
- `infoLineReceived` シグナルの頻度が高いため、`ThinkingInfoPresenter` のバッファリング（現在50ms）をワーカースレッド側に移動することを検討
- エンジンは最大2つ同時に動作するため、各エンジンに独立したワーカースレッドを割り当てる

---

## 優先度2: 棋譜ファイル読み込みのマルチスレッド化（影響度: 高）

### 対象箇所

| ファイル | メソッド | 処理内容 |
|---------|---------|---------|
| `src/kifu/kifreader.cpp:116` | `readLinesAuto()` | `f.readAll()` + エンコーディング検出 |
| `src/kifu/formats/csatosfenconverter.cpp:27` | `readAllLinesDetectEncoding()` | `f.readAll()` + 文字コード検出 |
| `src/kifu/formats/jkftosfenconverter.cpp:229` | `loadJsonFile()` | `file.readAll()` + JSON パース |
| `src/kifu/formats/kiftosfenconverter.cpp:151` | `convertFile()` | 行ごとの正規表現パース |
| `src/kifu/kifuloadcoordinator.cpp:152-242` | `loadKifuCommon()` | ファイル読み込み→パース→SFEN生成→ツリー構築 |
| `src/kifu/kifuapplyservice.cpp:43-255` | `applyParsedResult()` | SFEN再構築 + 指し手オブジェクト生成 |
| `src/board/sfenpositiontracer.cpp:332-355` | `buildSfenRecord()` | 全手順のSFEN文字列生成 |
| `src/board/sfenpositiontracer.cpp:357-411` | `buildGameMoves()` | ShogiMove オブジェクト生成 |
| `src/kifu/kifubranchtreebuilder.cpp:19-38` | `buildFromKifParseResult()` | 分岐ツリー構築 |

### 問題点

- 棋譜読み込みは「ファイル読み込み → エンコーディング検出 → フォーマットパース → SFEN変換 → ツリー構築」の全工程がメインスレッドで同期実行される
- 200手以上の棋譜や分岐の多い棋譜では、**500ms〜3秒** のUI凍結が発生する
- `readAll()` はファイル全体をメモリに読み込むため、大容量ファイルでは特に遅い

### 推奨方法: QtConcurrent::run + QFutureWatcher

```cpp
// kifufilecontroller.cpp での実装イメージ

#include <QtConcurrent>
#include <QFutureWatcher>

struct KifuLoadResult {
    bool success;
    KifuParseResult parseResult;
    QList<KifGameInfoItem> gameInfo;
    QString errorMessage;
};

void KifuFileController::loadKifuAsync(const QString& filePath)
{
    auto* watcher = new QFutureWatcher<KifuLoadResult>(this);
    connect(watcher, &QFutureWatcher<KifuLoadResult>::finished,
            this, &KifuFileController::onKifuLoadFinished);

    // プログレスダイアログ表示
    showLoadingIndicator();

    QFuture<KifuLoadResult> future = QtConcurrent::run([filePath]() {
        KifuLoadResult result;
        // ファイル読み込み + パース（ワーカースレッドで実行）
        // ... 既存の loadKifuCommon() のパース部分 ...
        return result;
    });

    watcher->setFuture(future);
}

void KifuFileController::onKifuLoadFinished()
{
    auto* watcher = qobject_cast<QFutureWatcher<KifuLoadResult>*>(sender());
    KifuLoadResult result = watcher->result();
    watcher->deleteLater();

    hideLoadingIndicator();

    if (result.success) {
        // UIモデルへの適用はメインスレッドで実行
        m_applyService->applyParsedResult(result.parseResult);
        m_applyService->populateGameInfo(result.gameInfo);
    }
}
```

**実装方針:**

1. パース処理（ファイル読み込み〜SFEN生成〜分岐ツリー構築）をワーカースレッドに移動
2. UIモデルへの適用（`QTableWidget` 更新、ツリービュー更新）はメインスレッドで実行
3. `QFutureWatcher::finished` シグナルでメインスレッドに結果を返す
4. 読み込み中はプログレスインジケータを表示
5. パース結果を `KifuLoadResult` 構造体にまとめて返す

**注意点:**
- パース処理内で `QWidget` や `QAbstractItemModel` を操作してはならない
- `KifuApplyService::applyParsedResult()` は UIモデル操作を含むため、メインスレッドで呼ぶ
- `SfenPositionTracer::buildSfenRecord()` と `buildGameMoves()` は static メソッドなので、スレッドセーフに呼べる

---

## 優先度3: 棋譜解析（一括解析）のマルチスレッド化（影響度: 高）

### 対象箇所

| ファイル | メソッド | 処理内容 |
|---------|---------|---------|
| `src/analysis/analysiscoordinator.cpp:44-70` | `startAnalyzeRange()` | 解析範囲のループ実行 |
| `src/engine/usi_communication.cpp:66-113` | `executeAnalysisCommunication()` | `waitForBestMove()` / `keepWaitingForBestMove()` |
| `src/analysis/tsumesearchflowcontroller.cpp:21-52` | `runWithDialog()` | 詰み探索の同期実行 |

### 問題点

- 棋譜の一括解析は「各局面でエンジンに `go` を送り、`bestmove` を待つ」を繰り返す
- 100手の棋譜を30秒/手で解析すると約50分間UIが凍結する
- `executeAnalysisCommunication()` 内の `keepWaitingForBestMove()` は無期限ブロッキング
- 進捗表示やキャンセルが適切に機能しない

### 推奨方法: エンジンワーカースレッド上での非同期解析ループ

優先度1のエンジンワーカースレッド化が前提。ワーカースレッド内で解析ループを実行し、各局面の結果をシグナルでメインスレッドに通知する。

```
メインスレッド                    エンジンワーカースレッド
     |                                   |
     |--- startRangeAnalysis(range) ---->|
     |                                   |-- for each ply in range:
     |<-- analysisProgress(ply, total) --|    send "position" + "go"
     |<-- positionAnalyzed(ply, result) -|    waitForBestMove()
     |                                   |
     |--- cancelAnalysis() ------------>|  (キャンセル要求)
     |                                   |-- send "stop"
     |<-- analysisFinished() -----------|
```

**実装方針:**

1. 解析ループ全体をワーカースレッドで実行
2. 各局面の解析完了時に `positionAnalyzed(int ply, AnalysisResult result)` シグナルを発行
3. 進捗通知 `analysisProgress(int current, int total)` でプログレスバーを更新
4. キャンセルは `QAtomicInt` フラグで安全に通知
5. `QProgressDialog` でキャンセルボタン付きの進捗表示

---

## 優先度4: 棋譜ファイル書き込みのバックグラウンド化（影響度: 中）

### 対象箇所

| ファイル | メソッド | 処理内容 |
|---------|---------|---------|
| `src/kifu/kifuioservice.cpp:77-135` | `writeKifuFile()` | 行ごとの同期書き込み + Shift-JIS エンコーディング |

### 問題点

- 大量の手数（10,000行以上）の棋譜を保存する場合、行ごとの `file.write()` が **2〜5秒** ブロックする
- 対局中の自動保存でUIが一瞬固まる可能性がある

### 推奨方法: QtConcurrent::run による非同期書き込み

```cpp
QFuture<bool> KifuIoService::writeKifuFileAsync(
    const QString& filePath,
    const QStringList& kifuLines,
    bool useShiftJis)
{
    return QtConcurrent::run([filePath, kifuLines, useShiftJis]() -> bool {
        // 既存の writeKifuFile() ロジックをそのまま実行
        // QFile, QStringEncoder はスレッドセーフ
        return writeKifuFileImpl(filePath, kifuLines, useShiftJis);
    });
}
```

**注意点:**
- 書き込み中に同じファイルへの読み込みが発生しないよう、ファイルロック（`QLockFile`）を検討
- 書き込み完了のコールバックで UI に通知

---

## 優先度5: 盤面画像エクスポートのバックグラウンド化（影響度: 中）

### 対象箇所

| ファイル | メソッド | 処理内容 |
|---------|---------|---------|
| `src/board/boardimageexporter.cpp:34-90` | `saveImage()` | `grab()` + `QImageWriter::write()` |
| `src/board/boardimageexporter.cpp:29-32` | `copyToClipboard()` | `grab()` + clipboard 設定 |

### 問題点

- `QWidget::grab()` はメインスレッドでのみ呼び出せる（Qt の制約）
- 画像エンコーディング（特に PNG 圧縮）が CPU 集約的で、高解像度では **1〜3秒** かかる

### 推奨方法: grab() はメインスレッド、エンコーディングはワーカースレッド

```cpp
void BoardImageExporter::saveImageAsync(QWidget* boardWidget,
                                         const QString& path,
                                         const QString& format)
{
    // Step 1: メインスレッドで画像キャプチャ（高速）
    QImage img = boardWidget->grab().toImage();

    // Step 2: エンコーディング + 書き込みをワーカースレッドで実行
    QtConcurrent::run([img, path, format]() {
        QImageWriter writer(path, format.toLatin1());
        writer.write(img);
    });
}
```

---

## 優先度6: 評価値グラフ描画の最適化（影響度: 中）

### 対象箇所

| ファイル | メソッド | 処理内容 |
|---------|---------|---------|
| `src/widgets/evaluationchartwidget.cpp` | 各 `appendScore*()` / `update()` | Qt Charts の CPU 描画 |

### 問題点

- Qt Charts はアンチエイリアシング有効時に CPU レンダリングが重い
- 300手以上の対局で解析結果を逐次追加すると、フレームレートが低下する
- 各 `appendScore()` 呼び出しごとに再描画が発生

### 推奨方法: バッチ更新 + 描画スロットリング

マルチスレッド化ではなく、メインスレッド内での最適化が効果的:

```cpp
// バッファに蓄積し、一定間隔でまとめて描画
void EvaluationChartWidget::appendScoreBuffered(int ply, double score)
{
    m_pendingScores.append({ply, score});

    if (!m_flushTimer->isActive()) {
        m_flushTimer->start(100); // 100ms間隔でフラッシュ
    }
}

void EvaluationChartWidget::flushPendingScores()
{
    m_chartView->setUpdatesEnabled(false);  // 描画抑制
    for (const auto& [ply, score] : std::as_const(m_pendingScores)) {
        m_series->append(ply, score);
    }
    m_chartView->setUpdatesEnabled(true);   // 一括再描画
    m_pendingScores.clear();
}
```

---

## 優先度7: エンジン登録時のファイル操作（影響度: 低）

### 対象箇所

| ファイル | メソッド | 処理内容 |
|---------|---------|---------|
| `src/dialogs/engineregistrationhandler.cpp:136` | エンジン登録 | `waitForStarted()` + オプション取得 |
| `src/dialogs/engineregistrationhandler.cpp:110` | エンジン終了 | `waitForFinished(3000)` |

### 問題点

- エンジン登録ダイアログでのエンジン起動・設定取得がメインスレッドをブロック
- 大きなエンジンファイルのコピー時にもブロック可能

### 推奨方法

優先度1のエンジンワーカースレッドが実現すれば、登録処理も同じ仕組みで非同期化できる。

---

## 実装順序とスレッド安全性

### 推奨実装順序

```
Phase 1: エンジンワーカースレッド化（優先度1+3）
  ├── EngineWorkerThread クラス新規作成
  ├── QProcess + UsiProtocolHandler をワーカーに移動
  ├── Usi クラスのインターフェースを非同期化
  └── 一括解析をワーカースレッド内ループに変更

Phase 2: 棋譜読み込み非同期化（優先度2）
  ├── KifuLoadResult 構造体定義
  ├── パース処理を QtConcurrent::run で実行
  └── UI適用をメインスレッドのコールバックに分離

Phase 3: ファイル書き込み・画像エクスポート（優先度4+5）
  ├── 書き込みの QtConcurrent::run 化
  └── 画像エンコーディングのバックグラウンド化

Phase 4: 描画最適化（優先度6）
  └── バッチ更新 + スロットリング実装
```

### スレッド安全性の考慮事項

現在のコードベースにはスレッド安全性の仕組みが一切ないため、マルチスレッド化時は以下に注意が必要:

1. **QWidget 操作はメインスレッドのみ**: `QTableWidget`, `QTreeView`, `QChartView` 等の操作はメインスレッドで行う
2. **QProcess はオーナースレッドで操作**: ワーカースレッドで生成した `QProcess` はそのスレッドでのみ操作する
3. **シグナル/スロットの接続タイプ**: スレッド間通信では `Qt::QueuedConnection` を使用する（`connect()` はスレッドが異なる場合に自動で `QueuedConnection` を選択する）
4. **共有データの保護**: ゲーム状態（`ShogiBoard`, `TurnManager`, `GameRecordModel`）へのアクセスがスレッド間で競合する場合は `QMutex` で保護する
5. **エンジン2台の独立性**: 各エンジンが独立したワーカースレッドを持てば、相互のロック不要

### 既存パターンとの整合性

- **Deps/Hooks/Refs パターン**: ワーカースレッドからの結果通知は Hooks のコールバックとして実装可能
- **ensure* 遅延初期化**: ワーカースレッドの生成も `ensure*()` パターンに従う
- **connect() でのラムダ禁止**: スレッド間接続でもメンバ関数ポインタ構文を使用する

---

## 参考: 現在使用されている非同期パターン一覧

### QTimer::singleShot による遅延実行（18箇所）

| ファイル | 遅延 | 用途 |
|---------|-----|------|
| `engineprocessmanager.cpp:384` | 0ms | stdout の分割読み込みスケジュール |
| `thinkinginfopresenter.cpp:88` | 50ms | thinking情報のバッファフラッシュ |
| `enginevsenginestrategy.cpp:243,357,438` | 0ms | EvE対局のターン回転 |
| `analysissessionhandler.cpp:425` | 0ms | 検討モードの遅延再開 |
| `engineanalysispresenter.cpp:168` | 500ms | 列幅の遅延復元 |
| `evaluationgraphcontroller.cpp:119,130` | 50ms | bestmove後のグラフ再描画 |
| `boardsetupcontroller.cpp:332` | 0ms | 分岐ツリーの遅延リフレッシュ |
| `kifunavigationcoordinator.cpp:250` | 0ms | 分岐ナビゲーションガード解除 |
| `recordpane.cpp:247,288` | 0ms | 行選択・シグナル接続の遅延実行 |
| `analysisresultspresenter.cpp:356` | 0ms | 完了メッセージの遅延表示 |
| `consecutivegamescontroller.cpp:114` | 100ms | 連続対局の次局開始 |
| `sfencollectiondialog.cpp:61` | 0ms | ウィンドウサイズ調整 |
| `pvboarddialog.cpp:58` | 0ms | ウィンドウサイズ調整 |
| `josekiwindow.cpp:515` | 2000ms | ステータス表示の復元 |

### QEventLoop によるブロッキング待機（6箇所）

すべて `src/engine/usiprotocolhandler_wait.cpp` に集中:
- `waitForResponseFlag()` - 汎用フラグ待機
- `waitForUsiOk()` - usiok 待機
- `waitForReadyOk()` - readyok 待機
- `waitForBestMove()` - bestmove 待機（10msスピンループ）
- `waitForBestMoveWithGrace()` - bestmove 待機（猶予期間付き）
- `keepWaitingForBestMove()` / `waitForStopOrPonderhit()` - 無期限待機
