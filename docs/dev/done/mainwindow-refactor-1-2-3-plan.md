# MainWindow 安全分割計画 (1 -> 2 -> 3)

## 目的

`MainWindow` から以下の責務を、安全に段階移行で外出しする。

1. 依存注入・配線組み立ての集中
2. 棋譜追記時のライブ分岐セッション更新
3. 棋譜ナビゲーション同期

本ドキュメントは「設計」と「実装手順」をセットで定義する。

## 前提と方針

- 既存挙動を変更しない（UI/局面同期/連続対局/検討モード）。
- 移行中は `MainWindow` の既存メソッド名を残し、内部を段階的に委譲する。
- 1フェーズ内で大きく広げず、差分を小さく刻む。
- Qt signal/slot はメンバ関数ポインタ構文を維持し、新規 lambda `connect()` は増やさない。

## 対象外

- ゲームルール本体 (`MatchCoordinator`/`ShogiGameController`) の仕様変更
- UIレイアウト仕様変更
- 既存の保存データ形式変更

---

## フェーズ1: 依存注入・配線組み立ての分離

### 解消したい問題

- `MainWindow` が DI コンテナのように多数の `Deps` を直接組み立てている。
- `ensure*()` ごとに状態参照の書き方がばらつき、保守コストが高い。

### 新規クラス設計

#### 1. `MainWindowRuntimeRefs` (値オブジェクト)

- 役割: `MainWindow` の注入対象参照をまとめて運ぶ。
- 配置: `src/app/mainwindowruntimerefs.h`
- 構成:
  - UI 参照: `Ui::MainWindow*`, `QMainWindow*`
  - サービス参照: `MatchCoordinator*`, `ShogiGameController*`, `ShogiView*`, etc.
  - 状態参照: `PlayMode*`, `int* currentMoveIndex`, `QString* currentSfenStr`, etc.
  - 既存 state 集約構造体 (`m_state`, `m_kifu`, `m_branchNav`) の必要参照

#### 2. `MainWindowDepsFactory` (純粋組み立てサービス)

- 役割: 各 `Wiring/Controller` が必要な `Deps` を生成する。
- 配置:
  - `src/app/mainwindowdepsfactory.h`
  - `src/app/mainwindowdepsfactory.cpp`
- 生成対象:
  - `DialogCoordinatorWiring::Deps`
  - `KifuFileController::Deps`
  - `RecordNavigationWiring::Deps`
  - 将来的に `BoardSetupController`/`GameStateController` の依存構築も集約可能

#### 3. `MainWindowCompositionRoot` (生成順序の管理)

- 役割: 「いつ factory を呼ぶか」のみを担当し、実体生成順を制御する。
- 配置:
  - `src/app/mainwindowcompositionroot.h`
  - `src/app/mainwindowcompositionroot.cpp`
- 主要 API:
  - `ensureDialogCoordinator(MainWindowRuntimeRefs&)`
  - `ensureKifuFileController(MainWindowRuntimeRefs&)`
  - `ensureRecordNavigationWiring(MainWindowRuntimeRefs&)`
- 注意: 所有権は `MainWindow` 側に残し、CompositionRoot は所有しない。

### 既存メソッドからの移行先

- `MainWindow::ensureDialogCoordinator` -> `MainWindowCompositionRoot::ensureDialogCoordinator`
- `MainWindow::ensureKifuFileController` -> `MainWindowCompositionRoot::ensureKifuFileController`
- `MainWindow::ensureRecordNavigationHandler` -> `MainWindowCompositionRoot::ensureRecordNavigationWiring`

### 段階的リファクタ手順

1. `MainWindowRuntimeRefs` と `MainWindowDepsFactory` を追加（未使用で導入）。
2. `ensureDialogCoordinator()` の `Deps` 組み立てのみ factory へ移動。
3. `ensureKifuFileController()` の `Deps` 組み立てを factory へ移動。
4. `ensureRecordNavigationHandler()` の `Deps` 組み立てを factory へ移動。
5. 3箇所が安定後、`MainWindowCompositionRoot` を導入し `ensure*` 本体から呼ぶ。
6. 最後に `MainWindow` 側の重複コード（同種の getter/bind）を削除。

### フェーズ1の完了条件

- `MainWindow` 内で `Deps d; d.xxx = ...` の直接構築コードが対象3箇所で消える。
- 対象3機能の起動順序・動作（ダイアログ起動、棋譜I/O、行ナビ）が不変。

### フェーズ1の回帰確認

- アプリ起動直後のメニュー操作
- 棋譜ファイル読み込み/保存/エクスポート
- 棋譜行クリックでの盤面追従

---

## フェーズ2: ライブ分岐セッション更新の分離

### 解消したい問題

- `updateGameRecord()` に「棋譜表示更新」と「LiveGameSession更新」が混在している。
- SFEN 構築ロジックが UI 層にあるため、再利用と検証が難しい。

### 新規クラス設計

#### `LiveGameSessionUpdater`

- 配置:
  - `src/app/livegamesessionupdater.h`
  - `src/app/livegamesessionupdater.cpp`
- 役割:
  - セッション未開始時の開始処理（ルート/途中局面分岐）
  - 現局面 SFEN の構築（board + 手番 + 持ち駒 + 手数）
  - `liveGameSession->addMove(...)` 実行
  - 終局時 `commit/discard` の方針提供（必要に応じて）

### 主要 API（案）

```cpp
class LiveGameSessionUpdater {
public:
    struct Deps {
        LiveGameSession* liveSession = nullptr;
        KifuBranchTree* branchTree = nullptr;
        ShogiGameController* gameController = nullptr;
        QStringList* sfenRecord = nullptr;
    };

    struct AppendInput {
        QString currentSfenStr;
        QList<ShogiMove>* gameMoves = nullptr;
        QString moveText;
        QString elapsedTime;
    };

    explicit LiveGameSessionUpdater(const Deps& deps);

    void appendLiveMove(const AppendInput& in);
    void ensureSessionStarted(const QString& currentSfenStr);
};
```

### 既存メソッドからの移行先

- `MainWindow::ensureLiveGameSessionStarted` -> `LiveGameSessionUpdater::ensureSessionStarted`
- `MainWindow::updateGameRecord` 後半 (LiveGameSession 更新部) -> `LiveGameSessionUpdater::appendLiveMove`

### 段階的リファクタ手順

1. 新クラスを追加し、既存ロジックをそのまま移植（挙動変更なし）。
2. `MainWindow` で `LiveGameSessionUpdater` を `ensure` 生成。
3. `updateGameRecord()` から LiveGameSession 関連コードを削除し 1 行委譲へ置換。
4. `ensureLiveGameSessionStarted()` を薄い委譲ラッパー化。
5. 終局時処理 (`onRequestAppendGameOverMove` 等) で必要なら updater API を追加。

### フェーズ2の完了条件

- `updateGameRecord()` が「棋譜表示更新責務」にほぼ限定される。
- LiveGameSession の開始条件と SFEN 構築が単一クラスに集約される。

### フェーズ2の回帰確認

- 通常対局での毎手棋譜追加
- 途中局面開始からの継続対局
- 終局時の分岐ツリー反映

---

## フェーズ3: 棋譜ナビゲーション同期の分離

### 解消したい問題

- `syncBoardAndHighlightsAtRow` / `navigateKifuViewToRow` / `onBranchNodeHandled` の責務境界が曖昧。
- 本譜/分岐/検討モードの条件分岐が `MainWindow` に散在している。

### 新規クラス設計

#### `KifuNavigationCoordinator`

- 配置:
  - `src/app/kifunavigationcoordinator.h`
  - `src/app/kifunavigationcoordinator.cpp`
- 役割:
  - 指定行ナビゲーション（selection + scroll + highlight）
  - 盤面/ハイライト同期トリガ
  - 現在SFEN・選択ply・currentMoveIndex の同期
  - 分岐ライン中かつ対局中かの判定に基づく SFEN 更新規則の適用
  - コメント更新通知の起点（必要最小限）

### 主要 API（案）

```cpp
class KifuNavigationCoordinator : public QObject {
    Q_OBJECT
public:
    struct Deps { /* recordPane, models, boardSync, navState, branchTree, ... */ };
    explicit KifuNavigationCoordinator(const Deps& deps, QObject* parent = nullptr);

    void navigateToRow(int ply);
    void syncBoardAndHighlightsAtRow(int ply);
    void handleBranchNodeHandled(int ply, const QString& sfen,
                                 int previousFileTo, int previousRankTo,
                                 const QString& lastUsiMove);
};
```

### 既存メソッドからの移行先

- `MainWindow::navigateKifuViewToRow` -> `KifuNavigationCoordinator::navigateToRow`
- `MainWindow::syncBoardAndHighlightsAtRow` -> `KifuNavigationCoordinator::syncBoardAndHighlightsAtRow`
- `MainWindow::onBranchNodeHandled` -> `KifuNavigationCoordinator::handleBranchNodeHandled`

### 段階的リファクタ手順

1. `KifuNavigationCoordinator` を追加し、`navigateToRow` のみ先に移植。
2. `syncBoardAndHighlightsAtRow` を移植し、`MainWindow` は委譲のみへ。
3. `onBranchNodeHandled` を移植（検討モードへの `updateConsiderationPosition` 含む）。
4. `onRecordRowChangedByPresenter` はコメント通知専任を維持し、重複同期を禁止。
5. `MainWindow` に残るナビ関連状態更新を coordinator に寄せる。

### フェーズ3の完了条件

- 棋譜ナビ同期の主要分岐が `KifuNavigationCoordinator` に集約される。
- `MainWindow` はイベント受信と委譲に限定される。

### フェーズ3の回帰確認

- 本譜ナビゲーション（先頭/末尾/前後）
- 分岐ライン選択時の盤面同期
- 検討モードでの局面再送
- コメント編集未保存ガードとの整合

---

## 実装順序とコミット分割

### 推奨コミット

1. `MainWindowRuntimeRefs` + `MainWindowDepsFactory` 追加（未接続）
2. `ensureDialogCoordinator` の factory 化
3. `ensureKifuFileController` の factory 化
4. `ensureRecordNavigationHandler` の factory 化
5. `MainWindowCompositionRoot` 導入
6. `LiveGameSessionUpdater` 導入と `updateGameRecord` 委譲
7. `KifuNavigationCoordinator` 導入 (`navigateToRow` -> `sync` -> `branchHandled`)

### 各コミットで必ず行う検証

```bash
cmake -B build -S .
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure -j4
```

※ `ctest` が未整備の場合は、少なくとも起動確認と主要手動シナリオを実施する。

---

## リスクとガード

- リスク: 初期化順序崩れによる null 参照
  - ガード: factory/composition root は「構築のみ」、生成順は既存順を固定
- リスク: 分岐同期の二重反映
  - ガード: `skipBoardSyncForBranchNav` の制御責務を coordinator 側で一元化
- リスク: 途中局面開始時の LiveSession 起点ずれ
  - ガード: updater 導入時に既存ログ出力を保持し比較検証

---

## 完了時の期待効果

- `MainWindow` は「イベント受信 + 委譲」のハブへ近づく。
- 依存注入コード、ライブ分岐更新、棋譜ナビ同期の責務境界が明確になる。
- 以後の分割（例: タブ配線、対局終了処理）を同じパターンで進めやすくなる。
