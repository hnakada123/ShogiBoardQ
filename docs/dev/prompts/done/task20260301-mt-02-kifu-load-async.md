# Task 20260301-mt-02: 棋譜ロード/解析のバックグラウンド化（P0）

## 背景

棋譜読み込みは「ファイル読み込み → エンコーディング検出 → フォーマットパース → SFEN変換 → ツリー構築」の全工程がメインスレッドで同期実行されている。200手以上の棋譜や分岐の多い棋譜では 500ms〜3秒のUI凍結が発生する。UI境界が明確で、最も分離しやすい箇所。

### 対象ファイル

| ファイル | メソッド | 処理内容 |
|---------|---------|---------|
| `src/kifu/kifufilecontroller.cpp:56` | ロード入口 | UI直下からの呼び出し |
| `src/kifu/kifuloadcoordinator.cpp:152` | `loadKifuCommon()` | ファイル読込→パース→SFEN生成→ツリー構築 |
| `src/kifu/kifuloadcoordinator.cpp:190,206,220` | パース+SFEN構築 | 解析フェーズ |
| `src/kifu/kifuapplyservice.cpp:102,117,129,194` | `applyParsedResult()` | SFEN再構築+指し手生成 |
| `src/board/sfenpositiontracer.cpp:332,357` | `buildSfenRecord()/buildGameMoves()` | 全手順SFEN文字列生成 |
| `src/kifu/kifreader.cpp:116` | `readLinesAuto()` | `f.readAll()` + エンコーディング検出 |
| `src/kifu/formats/kiftosfenconverter.cpp:202` | KIF変換 | 複数フェーズ解析 |

## 作業内容

### Step 1: KifuLoadResult 構造体の定義

`src/kifu/kifuloadresult.h` を新規作成する。パース結果をスレッド間で値渡しするための構造体。

```cpp
#pragma once

#include <QString>
#include <QList>
#include "kifparseresult.h"      // 既存の型
#include "kifgameinfoitem.h"     // 既存の型（あれば）

struct KifuLoadResult {
    bool success = false;
    QString errorMessage;
    QString warningMessage;

    // パース結果（値コピー）
    KifParseResult parseResult;
    QList<KifGameInfoItem> gameInfoItems;

    // メタ情報
    QString filePath;
    QString formatLabel;       // "KIF", "CSA", "JKF" etc.
    QString initialSfen;
    QString teaiLabel;

    // stale結果破棄用
    quint64 generation = 0;
};
```

**注意**: `KifParseResult` と依存型の内容を確認し、QObject を含まない値型であることを検証すること。QObject を含む場合はコピー可能なサブセットを抽出する。

### Step 2: KifuLoadCoordinator にパース専用メソッドを抽出

`src/kifu/kifuloadcoordinator.cpp` の `loadKifuCommon()` を2分割する:

1. **`parseKifuFile()`** — ファイル読込 + パース + SFEN構築（ワーカースレッド実行可能）
   - UI/Model 操作を一切含まないこと
   - 戻り値は `KifuLoadResult`
   - `static` またはフリー関数として実装可能か検討

2. **`applyKifuLoadResult()`** — UIモデル適用（メインスレッド限定）
   - `KifuApplyService::applyParsedResult()` の呼び出し
   - `QTableWidget`, `QTreeView` 等の更新
   - エラー/警告ダイアログの表示

**分割の境界を確認するチェックリスト**:
- `parseKifuFile()` 内に `QWidget` / `QAbstractItemModel` アクセスがないこと
- `parseKifuFile()` 内に `emit` がないこと（シグナル発行はメインスレッドで）
- `parseKifuFile()` 内に `tr()` は使用可能（QString を返すだけなので）

### Step 3: KifuFileController に非同期ロードを実装

`src/kifu/kifufilecontroller.cpp` を修正する。

1. `#include <QtConcurrent>` と `#include <QFutureWatcher>` を追加

2. メンバ変数を追加:
   ```cpp
   quint64 m_loadGeneration = 0;
   QFutureWatcher<KifuLoadResult>* m_loadWatcher = nullptr;
   ```

3. 非同期ロードメソッドを実装:
   ```cpp
   void KifuFileController::loadKifuFileAsync(const QString& filePath)
   {
       ++m_loadGeneration;
       const quint64 gen = m_loadGeneration;

       // 前回のロードが進行中なら待機（稀だが安全のため）
       if (m_loadWatcher && m_loadWatcher->isRunning()) {
           // キャンセルまたは完了待ち
       }

       if (!m_loadWatcher) {
           m_loadWatcher = new QFutureWatcher<KifuLoadResult>(this);
           connect(m_loadWatcher, &QFutureWatcher<KifuLoadResult>::finished,
                   this, &KifuFileController::onKifuLoadFinished);
       }

       // プログレス表示（ステータスバーまたはカーソル変更）
       emit loadStarted();

       QFuture<KifuLoadResult> future = QtConcurrent::run(
           &KifuLoadCoordinator::parseKifuFile,
           m_coordinator, filePath, gen);
       m_loadWatcher->setFuture(future);
   }
   ```

4. 完了ハンドラ:
   ```cpp
   void KifuFileController::onKifuLoadFinished()
   {
       KifuLoadResult result = m_loadWatcher->result();

       // stale結果チェック
       if (result.generation != m_loadGeneration) {
           return;  // 古い結果は破棄
       }

       emit loadFinished();

       if (result.success) {
           m_coordinator->applyKifuLoadResult(result);
       } else {
           // エラー表示
           emit loadError(result.errorMessage);
       }
   }
   ```

5. **connect() ルール**: ラムダは使用しない。`onKifuLoadFinished` は名前付きスロットとして定義する。

### Step 4: 既存の同期ロードパスとの互換

- `chooseAndLoadKifuFile()` 等の既存入口を `loadKifuFileAsync()` に切り替える
- ドラッグ&ドロップからのロードも同様に非同期化する
- CSA棋譜受信からのロード（ネットワーク経由）は同期のまま維持可能（データは既にメモリ上）

### Step 5: ロード中のUI状態管理

- ロード中はナビゲーションボタン等を無効化する（二重ロード防止）
- `QApplication::setOverrideCursor(Qt::WaitCursor)` / `restoreOverrideCursor()` でカーソル変更
- `UiStatePolicyManager` に `Loading` 状態の追加を検討（必要に応じて）

### Step 6: テスト

```bash
cmake --build build
cd build && ctest --output-on-failure
```

**手動テスト項目**:
1. KIF/CSA/JKF/KI2 各形式の棋譜を開けること
2. ロード中にウィンドウ移動/リサイズが可能なこと
3. ロード中に別のファイルを開いた場合、前のロード結果が破棄されること
4. 壊れたファイルを開いた場合、エラーダイアログが表示されること
5. 空ファイルを開いた場合の挙動が既存と同じこと

## 注意事項

- `KifParseResult` がコピー可能であることを必ず確認する。`QObject` 派生や `unique_ptr` メンバがある場合は move セマンティクスまたは `shared_ptr` を使用する
- `SfenPositionTracer::buildSfenRecord()` と `buildGameMoves()` は static メソッドのため、スレッドセーフに呼べる
- パース処理内で `qDebug()` は使用可能（Qt のロギングはスレッドセーフ）

## 完了条件

- [ ] `KifuLoadResult` 構造体が定義されている
- [ ] `loadKifuCommon()` がパース部分と適用部分に分割されている
- [ ] ファイルロードが `QtConcurrent::run` でバックグラウンド実行される
- [ ] stale結果の破棄が `generation` で機能する
- [ ] ロード中のUI状態管理（カーソル/ボタン無効化）がある
- [ ] 全テスト pass
- [ ] 4形式の棋譜ロードが手動テストで動作確認済み
