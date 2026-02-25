# Step 07: KifuNavigationCoordinator 導入

## 設計書

`docs/dev/mainwindow-refactor-1-2-3-plan.md` のフェーズ3に対応。

## 前提

フェーズ1〜2（Step 01〜06）が完了済み。

## 背景

`syncBoardAndHighlightsAtRow` / `navigateKifuViewToRow` / `onBranchNodeHandled` の責務境界が曖昧で、本譜/分岐/検討モードの条件分岐が `MainWindow` に散在している。これらを `KifuNavigationCoordinator` に集約する。

## タスク

### ステップ A: KifuNavigationCoordinator の作成と navigateToRow の移植

#### 1. クラス作成

- 配置: `src/app/kifunavigationcoordinator.h`, `src/app/kifunavigationcoordinator.cpp`
- `QObject` を継承する（シグナル/スロット使用のため）

#### 2. Deps 構造体

```cpp
struct Deps {
    // UI
    QWidget* recordPane = nullptr;  // RecordPane* を使う場合はその型
    GameRecordModel* kifuRecordModel = nullptr;

    // Board sync
    BoardSyncPresenter* boardSync = nullptr;
    ShogiView* shogiView = nullptr;

    // Navigation state
    BranchNavigationState* navState = nullptr;
    KifuBranchTree* branchTree = nullptr;

    // External state pointers
    QStringList* sfenRecord = nullptr;
    int* activePly = nullptr;
    int* currentSelectedPly = nullptr;
    int* currentMoveIndex = nullptr;
    QString* currentSfenStr = nullptr;
    bool* skipBoardSyncForBranchNav = nullptr;
    PlayMode* playMode = nullptr;

    // Coordinators
    MatchCoordinator* match = nullptr;
    QWidget* analysisTab = nullptr;  // AnalysisTab*

    // Callbacks (MainWindow に残す必要がある操作)
    std::function<void()> enableArrowButtons;
    std::function<void()> setCurrentTurn;
    std::function<void()> updateJosekiWindow;
    std::function<void()> ensureBoardSyncPresenter;
    std::function<bool()> isGameActivelyInProgress;
};
```

実際に必要なメンバは `navigateKifuViewToRow()`, `syncBoardAndHighlightsAtRow()`, `onBranchNodeHandled()` の実装を読んで確定すること。

#### 3. navigateToRow() の移植

`MainWindow::navigateKifuViewToRow(int ply)` のロジックをそのまま移植:
- `recordPane` / `kifuRecordModel` の null チェック
- `kifuView()` の取得
- safe ply のバウンドチェック
- selection 更新 + scroll
- `setCurrentHighlightRow(safe)`
- `syncBoardAndHighlightsAtRow(safe)` の呼び出し（自クラスのメソッド）
- ply トラッキング変数の更新
- `setCurrentTurn()` 呼び出し

#### 4. MainWindow 側: navigateKifuViewToRow を委譲化

```cpp
void MainWindow::navigateKifuViewToRow(int ply)
{
    ensureKifuNavigationCoordinator();
    m_kifuNavCoordinator->navigateToRow(ply);
}
```

#### 5. ビルド確認

この時点で `cmake --build build` を実行し、問題なければ次のステップへ。

---

### ステップ B: syncBoardAndHighlightsAtRow の移植

#### 1. syncBoardAndHighlightsAtRow() の移植

`MainWindow::syncBoardAndHighlightsAtRow(int ply)` のロジックを移植:
- `skipBoardSyncForBranchNav` の再入ガード
- `positionEditMode` チェック（`shogiView` 経由）
- `ensureBoardSyncPresenter()` + `boardSync->syncBoardAndHighlightsAtRow(ply)`
- `enableArrowButtons()` 呼び出し
- 現在SFEN更新ロジック（分岐ツリー優先、フォールバックは sfenRecord）
- `updateJosekiWindow()` 呼び出し

#### 2. MainWindow 側: syncBoardAndHighlightsAtRow を委譲化

```cpp
void MainWindow::syncBoardAndHighlightsAtRow(int ply)
{
    ensureKifuNavigationCoordinator();
    m_kifuNavCoordinator->syncBoardAndHighlightsAtRow(ply);
}
```

**注意**: `syncBoardAndHighlightsAtRow` は `RecordNavigationWiring` からシグナル接続されているため、`MainWindow` のスロットとしてのシグネチャは残す必要がある。

#### 3. ビルド確認

---

### ステップ C: onBranchNodeHandled の移植

#### 1. handleBranchNodeHandled() の移植

`MainWindow::onBranchNodeHandled(int ply, const QString& sfen, int previousFileTo, int previousRankTo, const QString& lastUsiMove)` のロジックを移植:
- ply トラッキング変数の更新
- `currentSfenStr` の更新
- `updateJosekiWindow()` 呼び出し
- 検討モード時のエンジンへの局面送信（`match->updateConsiderationPosition()`）
- `analysisTab->startElapsedTimer()` の呼び出し

#### 2. MainWindow 側: onBranchNodeHandled を委譲化

```cpp
void MainWindow::onBranchNodeHandled(int ply, const QString& sfen,
                                     int previousFileTo, int previousRankTo,
                                     const QString& lastUsiMove)
{
    ensureKifuNavigationCoordinator();
    m_kifuNavCoordinator->handleBranchNodeHandled(ply, sfen, previousFileTo, previousRankTo, lastUsiMove);
}
```

#### 3. ビルド確認

---

### ステップ D: ensure メソッドと CMake 更新

#### 1. ensureKifuNavigationCoordinator() の追加

- `MainWindow` に `ensureKifuNavigationCoordinator()` を追加
- Deps を構築して `updateDeps()` する
- `MainWindowDepsFactory` に `createKifuNavigationCoordinatorDeps()` を追加（任意）

#### 2. CMakeLists.txt 更新

新規ファイルを追加。

#### 3. MainWindow 側の残るナビ関連コードを整理

- `onRecordRowChangedByPresenter` はコメント通知専任を維持（変更しない）
- MainWindow に残った委譲メソッドが薄いラッパーのみであることを確認

## 制約

- `MainWindow` のスロットシグネチャ（`syncBoardAndHighlightsAtRow`, `navigateKifuViewToRow`, `onBranchNodeHandled`）は残す（外部から connect されているため）
- `RecordNavigationWiring` の signal 接続先は変更しない
- 検討モードでのエンジン局面送信ロジックは正確に移植すること
- ログ出力は移植先でも維持

## 回帰確認

ビルド成功後、以下を手動確認:
- 本譜ナビゲーション（先頭/末尾/前後ボタン）
- 棋譜行クリックでの盤面追従
- 分岐ライン選択時の盤面同期
- 検討モードでの局面再送（分岐ノード選択時にエンジンに位置が送られる）
- コメント編集未保存ガードとの整合

## 完了条件

- `KifuNavigationCoordinator` が `navigateToRow`, `syncBoardAndHighlightsAtRow`, `handleBranchNodeHandled` を提供
- `MainWindow` の3メソッドはすべて薄い委譲ラッパーのみ
- 棋譜ナビ同期の主要分岐が `KifuNavigationCoordinator` に集約されている
- `cmake -B build -S . && cmake --build build` が成功する
- コミットメッセージ: 「KifuNavigationCoordinator 導入: 棋譜ナビゲーション同期を集約」
