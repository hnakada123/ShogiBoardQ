# Step 01: MainWindowRuntimeRefs + MainWindowDepsFactory 追加（未接続）

## 設計書

`docs/dev/mainwindow-refactor-1-2-3-plan.md` のフェーズ1・ステップ1に対応。

## 背景

`MainWindow` が DI コンテナのように多数の `Deps` を直接組み立てている。`ensure*()` ごとに状態参照の書き方がばらつき、保守コストが高い。この問題を解消するため、まず参照をまとめる値オブジェクトと、Deps生成を担うファクトリを作成する。

## タスク

以下の2つの新規クラスを作成してください。**この段階ではまだ既存コードから使用しない（未接続で導入）。**

### 1. `MainWindowRuntimeRefs` (ヘッダーオンリーの値オブジェクト)

- 配置: `src/app/mainwindowruntimerefs.h`
- 役割: `MainWindow` が保持する注入対象の参照をまとめて運ぶ構造体
- 構成（`MainWindow` の既存メンバから抽出）:
  - **UI 参照**: `Ui::MainWindow* ui`, `QMainWindow* mainWindow`
  - **サービス参照**: `MatchCoordinator* match`, `ShogiGameController* gameController`, `ShogiView* shogiView`, `KifuLoadCoordinator* kifuLoadCoordinator`
  - **モデル参照**: `GameRecordModel* kifuRecordModel`, `ConsiderationTableModel* considerationModel`
  - **状態参照（ポインタ）**: `PlayMode* playMode`, `int* currentMoveIndex`, `QString* currentSfenStr`, `QString* startSfenStr`, `int* activePly`, `int* currentSelectedPly`, `bool* skipBoardSyncForBranchNav`
  - **棋譜参照**: `QStringList* sfenRecord`, `QList<ShogiMove>* gameMoves`, `QStringList* gameUsiMoves`, `QList<MoveRecord>* moveRecords`, `QList<PositionStr>* positionStrList`, `QString* saveFileName`
  - **分岐ナビ**: `KifuBranchTree* branchTree`, `BranchNavigationState* navState`, `BranchDisplayCoordinator* displayCoordinator`, `LiveGameSession* liveGameSession`
  - **その他**: `GameInfoController* gameInfoController`, `EvalGraphController* evalGraphController`, `CsaGameCoordinator* csaGameCoordinator`, `QWidget* evalChart`, `QWidget* analysisTab`, `QStatusBar* statusBar`
- 全メンバは `nullptr`（ポインタ）またはゼロ初期化のデフォルト値を持つこと
- 実際にどのメンバが必要かは `ensureDialogCoordinator()`, `ensureKifuFileController()`, `ensureRecordNavigationHandler()` の3メソッドの Deps 組み立てコードを読んで判断すること

### 2. `MainWindowDepsFactory`

- 配置: `src/app/mainwindowdepsfactory.h`, `src/app/mainwindowdepsfactory.cpp`
- 役割: `MainWindowRuntimeRefs` を受け取り、各 Wiring/Controller 用の `Deps` 構造体を生成する純粋ファクトリ
- 生成メソッド（staticでもインスタンスでも可、設計に合う方を選択）:
  - `DialogCoordinatorWiring::Deps createDialogCoordinatorDeps(const MainWindowRuntimeRefs& refs, ...)`
  - `KifuFileController::Deps createKifuFileControllerDeps(const MainWindowRuntimeRefs& refs, ...)`
  - `RecordNavigationWiring::Deps createRecordNavigationDeps(const MainWindowRuntimeRefs& refs)`
- **注意**: `DialogCoordinatorWiring::Deps` と `KifuFileController::Deps` には `std::function` コールバックが含まれる。これらはファクトリメソッドの追加引数として受け取る設計にすること（ファクトリ自体はコールバックを保持しない）
- 現在の `MainWindow` の各 `ensure*()` メソッド内の `Deps d; d.xxx = ...;` コードを参考に、同じ値をセットする

### CMakeLists.txt 更新

新規ファイルを `CMakeLists.txt` のソースリストに追加する。`scripts/update-sources.sh` でファイル一覧を生成可能。

## 制約

- この段階では `MainWindow` のコードを変更しない
- 新規クラスはビルドが通ることだけ確認する
- CLAUDE.md のコードスタイル（lambda不使用、clazy警告回避等）に従う
- ヘッダーの include は必要最小限に（前方宣言を活用）

## 完了条件

- `MainWindowRuntimeRefs` と `MainWindowDepsFactory` が追加されている
- `cmake -B build -S . && cmake --build build` が成功する
- `MainWindow` の既存コードは未変更
- コミットメッセージ: 「MainWindowRuntimeRefs + MainWindowDepsFactory 追加（未接続）」
