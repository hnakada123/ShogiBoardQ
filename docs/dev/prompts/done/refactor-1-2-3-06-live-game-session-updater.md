# Step 06: LiveGameSessionUpdater 導入と updateGameRecord 委譲

## 設計書

`docs/dev/mainwindow-refactor-1-2-3-plan.md` のフェーズ2に対応。

## 前提

フェーズ1（Step 01〜05）が完了済み。

## 背景

`MainWindow::updateGameRecord()` に「棋譜表示更新」と「LiveGameSession更新」が混在している。SFEN 構築ロジックが UI 層にあるため、再利用と検証が難しい。LiveGameSession 関連のロジックを `LiveGameSessionUpdater` に分離する。

## タスク

### 1. LiveGameSessionUpdater クラスの作成

- 配置: `src/app/livegamesessionupdater.h`, `src/app/livegamesessionupdater.cpp`
- 役割:
  - セッション未開始時の開始処理（ルート/途中局面分岐の判定）
  - 現局面 SFEN の構築（board + 手番 + 持ち駒 + 手数）
  - `liveGameSession->addMove(...)` 実行

### 2. 主要 API

```cpp
class LiveGameSessionUpdater {
public:
    struct Deps {
        LiveGameSession* liveSession = nullptr;
        KifuBranchTree* branchTree = nullptr;
        ShogiGameController* gameController = nullptr;
        QStringList* sfenRecord = nullptr;
        QString* currentSfenStr = nullptr;
    };

    void updateDeps(const Deps& deps);

    // ensureLiveGameSessionStarted() から移植
    void ensureSessionStarted();

    // updateGameRecord() の LiveGameSession 更新部分から移植
    void appendMove(const ShogiMove& move, const QString& moveText, const QString& elapsedTime);
};
```

### 3. 既存ロジックの移植

#### ensureLiveGameSessionStarted() → ensureSessionStarted()

`MainWindow::ensureLiveGameSessionStarted()` (mainwindow.cpp) のロジックをそのまま移植:
- `liveGameSession` と `branchTree` の null チェック
- `isActive()` チェック
- `root()` の null チェック
- 途中局面判定（`currentSfenStr` を使用）
- `findBySfen()` で分岐ポイントを探す
- `startFromNode()` or `startFromRoot()` の呼び分け

#### updateGameRecord() 後半 → appendMove()

`MainWindow::updateGameRecord()` の LiveGameSession 更新部分（`if (m_branchNav.liveGameSession != nullptr ...)` 以降）を移植:
- セッション未開始なら `ensureSessionStarted()` を呼ぶ
- SFEN を board から構築（`convertBoardToSfen()`, `convertStandToSfen()`, 手番, 手数）
- フォールバック: board がなければ `sfenRecord->last()` を使用
- `liveGameSession->addMove(move, moveText, sfen, elapsedTime)` を実行

### 4. MainWindow 側の変更

#### updateGameRecord() を簡素化

**Before:**
```cpp
void MainWindow::updateGameRecord(const QString& elapsedTime)
{
    // ... gameOverAppended チェック ...
    // ... recordPresenter への追記（棋譜表示更新）...

    if (m_branchNav.liveGameSession != nullptr && !m_state.lastMove.isEmpty()) {
        // 40行以上のLiveGameSession更新ロジック
    }
    m_state.lastMove.clear();
}
```

**After:**
```cpp
void MainWindow::updateGameRecord(const QString& elapsedTime)
{
    // ... gameOverAppended チェック ...
    // ... recordPresenter への追記（棋譜表示更新）...

    if (m_branchNav.liveGameSession != nullptr && !m_state.lastMove.isEmpty()) {
        ensureLiveGameSessionUpdater();
        ShogiMove move;
        if (!m_kifu.gameMoves.isEmpty()) move = m_kifu.gameMoves.last();
        m_liveGameSessionUpdater->appendMove(move, m_state.lastMove, elapsedTime);
    }
    m_state.lastMove.clear();
}
```

#### ensureLiveGameSessionStarted() を薄い委譲ラッパー化

```cpp
void MainWindow::ensureLiveGameSessionStarted()
{
    ensureLiveGameSessionUpdater();
    m_liveGameSessionUpdater->ensureSessionStarted();
}
```

#### ensure メソッド追加

- `ensureLiveGameSessionUpdater()` を追加（Deps 構築 + `updateDeps()`）
- `MainWindowDepsFactory` に `createLiveGameSessionUpdaterDeps()` を追加（任意）

### 5. CMakeLists.txt 更新

新規ファイルを追加。

## 制約

- 「棋譜表示更新」（`recordPresenter` への追記）部分は `MainWindow` に残す
- `updateGameRecord()` の外部呼び出しシグネチャは変更しない
- ログ出力（`qCDebug`, `qCWarning`）は移植先でも維持する
- debug logging は `docs/dev/debug-logging-guidelines.md` に従う

## 回帰確認

ビルド成功後、以下を手動確認:
- 通常対局での毎手棋譜追加（棋譜行が正しく増える）
- 途中局面開始からの継続対局
- 終局時の分岐ツリー反映（分岐ビューに対局手順が表示される）
- 検討モードから対局への切り替え

## 完了条件

- `LiveGameSessionUpdater` クラスが存在する
- `updateGameRecord()` が「棋譜表示更新責務」にほぼ限定される
- LiveGameSession の開始条件と SFEN 構築が `LiveGameSessionUpdater` に集約される
- `cmake -B build -S . && cmake --build build` が成功する
- コミットメッセージ: 「LiveGameSessionUpdater 導入: updateGameRecord から LiveGameSession 更新を分離」
