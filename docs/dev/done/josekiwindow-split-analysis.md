# JosekiWindow 分割分析

## 現状

| ファイル | 行数 |
|---------|------|
| josekiwindow.cpp | 2,406 |
| josekiwindow.h | 450 |
| **合計** | **2,856** |

JosekiWindow はプロジェクト最大のファイルで、以下の責務が同居している:
- UI構築・レイアウト（setupUi: 313行）
- ファイルI/O・パース（loadJosekiFile: 183行、saveJosekiFile: 66行）
- 定跡データの検索・CRUD操作
- USI↔日本語の変換ロジック
- 棋譜マージフロー（onMergeFromKifuFile: 192行）
- テーブル表示・更新（updateJosekiDisplay: 164行）
- フォント管理・設定永続化

## 1. メソッド分類

### A: View（表示・UI操作）

| メソッド | 行数 | 説明 |
|---------|------|------|
| `setupUi()` | 313 | UI構築・ウィジェット生成・signal/slot接続 |
| `applyFontSize()` | 81 | フォントサイズ適用（全ウィジェット） |
| `clearTable()` | 5 | テーブル行クリア |
| `updateStatusDisplay()` | 55 | ステータスバー・ラベル更新 |
| `updatePositionSummary()` | 41 | 局面サマリーラベル更新 |
| `updateWindowTitle()` | 14 | ウィンドウタイトル更新 |
| `updateRecentFilesMenu()` | 25 | 最近使ったファイルメニュー更新 |
| `setModified()` | 29 | 変更状態セット＋UI反映 |
| `showEvent()` | 6 | 表示時のフォント再適用 |
| `closeEvent()` | 10 | 閉じる時の設定保存 |
| `eventFilter()` | 9 | ドック閉じるフィルタ |
| `setDockWidget()` | 6 | ドックウィジェット設定 |
| **小計** | **594** | |

### B: Presenter（状態遷移・操作フロー）

| メソッド | 行数 | 説明 |
|---------|------|------|
| `setCurrentSfen()` | 23 | SFEN設定→表示更新トリガ |
| `setHumanCanPlay()` | 7 | 着手可否設定 |
| `updateJosekiDisplay()` | 164 | 定跡検索＋テーブル表示（**混合**） |
| `onPlayButtonClicked()` | 24 | 着手ボタン処理 |
| `onEditButtonClicked()` | 72 | 編集ボタン処理 |
| `onDeleteButtonClicked()` | 71 | 削除ボタン処理 |
| `onAddMoveButtonClicked()` | 85 | 追加ボタン処理 |
| `onMergeFromCurrentKifu()` | 54 | 現在の棋譜からマージ |
| `onMergeFromKifuFile()` | 192 | 棋譜ファイルからマージ |
| `setKifuDataForMerge()` | 77 | マージ用棋譜データ受信 |
| `onMergeRegisterMove()` | 67 | マージ登録処理 |
| `confirmDiscardChanges()` | 26 | 未保存変更確認 |
| `onTableDoubleClicked()` | 15 | ダブルクリック着手 |
| `onTableContextMenu()` | 15 | コンテキストメニュー表示 |
| `onContextMenuPlay()` | 13 | コンテキスト:着手 |
| `onContextMenuEdit()` | 50 | コンテキスト:編集 |
| `onContextMenuDelete()` | 29 | コンテキスト:削除 |
| `onContextMenuCopyMove()` | 22 | コンテキスト:コピー |
| `onFontSizeIncrease()` | 7 | フォント拡大 |
| `onFontSizeDecrease()` | 7 | フォント縮小 |
| `onOpenButtonClicked()` | 39 | ファイル開く |
| `onNewButtonClicked()` | 20 | 新規作成 |
| `onSaveButtonClicked()` | 12 | 上書保存 |
| `onSaveAsButtonClicked()` | 40 | 名前を付けて保存 |
| `onStopButtonClicked()` | 18 | 表示停止/再開 |
| `onAutoLoadCheckBoxChanged()` | 5 | 自動読込切替 |
| `onSfenDetailToggled()` | 4 | SFEN詳細切替 |
| `onRecentFileClicked()` | 43 | 最近使ったファイル選択 |
| `onClearRecentFilesClicked()` | 12 | 履歴クリア |
| `onMoveResult()` | 8 | 着手結果受信 |
| `addToRecentFiles()` | 15 | 最近使ったファイル追加 |
| **小計** | **1,279** | |

### C: Repository（データI/O）

| メソッド | 行数 | 説明 |
|---------|------|------|
| `loadJosekiFile()` | 183 | やねうら王定跡ファイル読込・パース |
| `saveJosekiFile()` | 66 | 定跡ファイル書き込み |
| `loadSettings()` | 44 | 設定読込（JosekiSettings経由） |
| `saveSettings()` | 25 | 設定保存 |
| **小計** | **318** | |

### D: Model / データユーティリティ

| メソッド | 行数 | 説明 |
|---------|------|------|
| `normalizeSfen()` | 10 | SFEN正規化（手数除去） |
| `usiMoveToJapanese()` | 65 | USI→日本語変換 |
| `pieceToKanji()` (static) | 13 | 駒文字→漢字変換 |
| **小計** | **88** | |

### 分類サマリー

| カテゴリ | 行数 | 割合 |
|---------|------|------|
| A: View | 594 | 25% |
| B: Presenter | 1,279 | 53% |
| C: Repository | 318 | 13% |
| D: Model/Utility | 88 | 4% |
| その他（空行・コメント等） | ~127 | 5% |

## 2. QTableWidget → QAbstractTableModel 移行分析

### 現在の QTableWidget 使用パターン

#### データ書き込み（updateJosekiDisplay内）
- `setRowCount(moves.size())` — 行数設定
- `setItem(row, col, item)` — テキストセル（No., 定跡手, 予想応手, 評価値, 深さ, 出現頻度, コメント）
- `setCellWidget(row, col, button)` — ボタンセル（着手, 編集, 削除）
- `resizeColumnToContents(col)` — 列幅自動調整

#### データ読み取り
- `currentRow()` — コンテキストメニュー処理（4箇所）
- `indexAt(pos)` — コンテキストメニュー表示時
- `columnCount()`, `columnWidth(col)` — 設定保存時
- `horizontalHeaderItem(col)->text()` — フォントサイズ適用時

#### テーブル操作
- `setRowCount(0)` — クリア
- `selectRow(row)` — コンテキストメニュー時
- `viewport()->mapToGlobal(pos)` — コンテキストメニュー位置

#### 特殊: ボタンウィジェット
着手・編集・削除の3列はセル内にQPushButtonを配置している:
```cpp
QPushButton *playButton = new QPushButton(tr("着手"), this);
playButton->setProperty("row", i);
connect(playButton, &QPushButton::clicked, this, &JosekiWindow::onPlayButtonClicked);
m_tableWidget->setCellWidget(i, 1, playButton);
```

### 移行コスト評価

| 項目 | 難易度 | 説明 |
|------|--------|------|
| テキストセル | 低 | `data()` で返すだけ |
| 数値フォーマット | 低 | Model内でQLocale使用 |
| ボタンセル | **高** | QStyledItemDelegate必要 |
| 行選択 | 低 | QTableView標準機能 |
| 列幅保存 | 低 | QHeaderView標準機能 |

### 移行判定: **Phase 4（後回し）を推奨**

理由:
1. `setCellWidget()` によるボタン配置は QAbstractTableModel + QStyledItemDelegate で再実装が必要
2. 現在のデータ構造（`m_currentMoves` ← `m_josekiData[sfen]`）は既にテーブルと分離されている
3. Repository/Presenter 分割とは独立して実施可能
4. テーブル行数は通常数十行程度であり、パフォーマンス上の問題はない

## 3. 分割後のクラス構成

### 3.1 JosekiRepository（新規: `src/kifu/joseki/josekiRepository.h/.cpp`）

**責務**: 定跡データの読み書き・データ管理

```
データ構造:
  QMap<QString, QVector<JosekiMove>> m_josekiData
  QMap<QString, QString> m_sfenWithPlyMap

メソッド:
  // ファイルI/O
  bool loadFromFile(const QString &filePath, QString *errorMessage)
  bool saveToFile(const QString &filePath, QString *errorMessage)
  void clear()

  // データアクセス
  bool containsPosition(const QString &normalizedSfen) const
  QVector<JosekiMove> movesForPosition(const QString &normalizedSfen) const
  QString sfenWithPly(const QString &normalizedSfen) const
  int positionCount() const
  bool isEmpty() const

  // データ変更
  void addMove(const QString &normalizedSfen, const QString &sfenWithPly, const JosekiMove &move)
  void updateMove(const QString &normalizedSfen, const QString &usiMove, const JosekiMove &updated)
  void removeMove(const QString &normalizedSfen, const QString &usiMove)
  bool hasDuplicateMove(const QString &normalizedSfen, const QString &usiMove) const

  // ユーティリティ
  static QString normalizeSfen(const QString &sfen)
```

**依存関係**: なし（純粋なデータ層）
**推定行数**: ~350行（.cpp）

### 3.2 JosekiMoveFormatter（新規: `src/kifu/joseki/josekimoveformatter.h/.cpp`）

**責務**: USI形式⇔日本語表記の変換

```
メソッド:
  static QString usiMoveToJapanese(const QString &usiMove, int plyNumber, SfenPositionTracer &tracer)
  static QString pieceToKanji(QChar pieceChar, bool promoted = false)
```

**依存関係**: SfenPositionTracer
**推定行数**: ~100行（.cpp）

### 3.3 JosekiPresenter（新規: `src/ui/presenters/josekipresenter.h/.cpp`）

**責務**: 操作フロー・状態管理・ビジネスロジック

```
データ構造:
  JosekiRepository *m_repository
  QString m_currentSfen
  QString m_currentFilePath
  QVector<JosekiMove> m_currentMoves
  QSet<QString> m_mergeRegisteredMoves
  QStringList m_recentFiles
  bool m_modified
  bool m_displayEnabled
  bool m_humanCanPlay
  bool m_autoLoadEnabled
  int m_fontSize

シグナル:
  void displayUpdated(const QVector<JosekiMove> &moves, int plyNumber, const QString &sfen)
  void statusChanged()
  void modifiedChanged(bool modified)
  void fileChanged(const QString &filePath)
  void positionSummaryChanged(const QString &summary, const QString &sfenDetail, const QString &sfenLine)
  void josekiMoveSelected(const QString &usiMove)

メソッド:
  // 局面操作
  void setCurrentSfen(const QString &sfen)
  void setHumanCanPlay(bool canPlay)

  // ファイル操作
  bool openFile(const QString &filePath)
  bool saveFile()
  bool saveFileAs(const QString &filePath)
  void newFile()
  bool hasUnsavedChanges() const

  // CRUD操作
  void addMove(const JosekiMove &move)
  void editMove(int row, const JosekiMove &updated)
  void deleteMove(int row)

  // マージ操作
  bool canMerge() const
  QString ensureSavePathForMerge(const QString &filePath)
  void prepareMergeFromKifuFile(const QString &kifFilePath, ...)
  void registerMergeMove(const QString &sfen, const QString &sfenWithPly, const QString &usiMove)

  // 表示操作
  void toggleDisplay()
  void toggleSfenDetail(bool visible)
  void setAutoLoad(bool enabled)
  void increaseFontSize()
  void decreaseFontSize()

  // 最近のファイル
  void addToRecentFiles(const QString &filePath)
  void clearRecentFiles()
  QStringList recentFiles() const

  // 設定
  void loadSettings()
  void saveSettings()
```

**依存関係**: JosekiRepository, JosekiMoveFormatter, JosekiSettings
**推定行数**: ~600行（.cpp）

### 3.4 JosekiWindow（縮小: `src/dialogs/josekiwindow.h/.cpp`）

**責務**: UIウィジェット構築・イベント委譲・表示更新

```
変更点:
  - JosekiPresenter を保持
  - 全スロットは Presenter のメソッド呼び出しに委譲
  - Presenter のシグナルを受けてUI更新

残存メソッド:
  // UI構築（そのまま残す）
  setupUi()

  // View更新スロット（Presenterシグナルを受ける）
  void onDisplayUpdated(const QVector<JosekiMove> &moves, int plyNumber, const QString &sfen)
  void onStatusChanged()
  void onModifiedChanged(bool modified)
  void onFileChanged(const QString &filePath)
  void onPositionSummaryChanged(...)

  // 薄いイベントハンドラ（Presenterへ委譲）
  void onOpenButtonClicked() { presenter->openFileWithDialog(); }
  void onSaveButtonClicked() { presenter->saveFile(); }
  ...

  // View固有
  applyFontSize()
  closeEvent() / showEvent() / eventFilter()
  setDockWidget()
```

**依存関係**: JosekiPresenter
**推定行数**: ~550行（.cpp）

### 3.5 クラス依存関係図

```
JosekiWindow (View)
    │
    ├─→ JosekiPresenter
    │       │
    │       ├─→ JosekiRepository
    │       │
    │       ├─→ JosekiMoveFormatter
    │       │       │
    │       │       └─→ SfenPositionTracer
    │       │
    │       └─→ JosekiSettings (既存)
    │
    └─→ JosekiMoveDialog (既存・変更なし)
        JosekiMergeDialog (既存・変更なし)

JosekiWindowWiring (既存・変更なし)
    │
    └─→ JosekiWindow
```

## 4. 段階的実装計画

### Phase 1: JosekiRepository 抽出（難易度: 低）

**目的**: データI/Oをクラスとして独立させる

**手順**:
1. `src/kifu/joseki/josekiRepository.h/.cpp` を新規作成
2. `JosekiMove`, `JosekiEntry` 構造体を `josekiwindow.h` から `josekiRepository.h` へ移動
3. `loadJosekiFile()`, `saveJosekiFile()`, `normalizeSfen()` を移植
4. `m_josekiData`, `m_sfenWithPlyMap` を Repository に移動
5. JosekiWindow に `JosekiRepository*` メンバを追加し、呼び出し元を書き換え
6. CMakeLists.txt に追加

**ビルド確認ポイント**: 各ステップでビルド可能

**影響範囲**:
- `josekiwindow.cpp` → Repository メソッド呼び出しに変更
- `josekiwindow.h` → 構造体移動、メンバ変数移動
- テスト: `tst_josekiwindow.cpp` は `#include` パス変更のみ

**削減量**: josekiwindow.cpp から約 260行削減

### Phase 2: JosekiMoveFormatter 抽出（難易度: 低）

**目的**: 変換ロジックを独立ユーティリティにする

**手順**:
1. `src/kifu/joseki/josekimoveformatter.h/.cpp` を新規作成
2. `usiMoveToJapanese()`, `pieceToKanji()` を移植
3. `kZenkakuDigits`, `kKanjiRanks` 定数も移動
4. JosekiWindow の呼び出し元を書き換え

**ビルド確認ポイント**: 移植後即ビルド可能

**削減量**: josekiwindow.cpp から約 80行削減

### Phase 3: JosekiPresenter 抽出（難易度: 中）

**目的**: ビジネスロジックを View から分離する

**手順**:
1. `src/ui/presenters/josekipresenter.h/.cpp` を新規作成
2. 状態変数を Presenter に移動: `m_currentSfen`, `m_currentFilePath`, `m_modified`, `m_displayEnabled`, `m_humanCanPlay`, `m_autoLoadEnabled`, `m_fontSize`, `m_recentFiles`, `m_currentMoves`, `m_mergeRegisteredMoves`
3. Presenter にシグナルを定義（displayUpdated, statusChanged, modifiedChanged 等）
4. 以下のメソッドを段階的に移植:
   - a. 設定系: `loadSettings()`, `saveSettings()`, font増減
   - b. ファイル操作系: `onOpenButtonClicked()`, `onNewButtonClicked()`, `onSaveButtonClicked()`, `onSaveAsButtonClicked()`, `addToRecentFiles()`, 最近のファイル系
   - c. 表示更新系: `updateJosekiDisplay()` のデータ検索部分を Presenter に、テーブル描画部分を View に分離
   - d. CRUD系: `onAddMoveButtonClicked()`, `onEditButtonClicked()`, `onDeleteButtonClicked()`
   - e. マージ系: `onMergeFromCurrentKifu()`, `onMergeFromKifuFile()`, `setKifuDataForMerge()`, `onMergeRegisterMove()`
   - f. コンテキストメニュー系: `onContextMenuPlay/Edit/Delete/CopyMove()`
5. JosekiWindow にシグナル/スロット接続を追加

**注意点**:
- `updateJosekiDisplay()` は検索ロジック（Presenter）とテーブル描画（View）が混在しているため、分離が必要
- ダイアログ表示（`QFileDialog`, `QMessageBox`, `JosekiMoveDialog`, `JosekiMergeDialog`）は View に残す。Presenter は結果を受け取る
- `confirmDiscardChanges()` のようなUI操作を含むメソッドは View 側に残し、Presenter は `hasUnsavedChanges()` を提供

**ビルド確認ポイント**: メソッド群ごと（a〜f）にビルド確認

**削減量**: josekiwindow.cpp から約 800〜1,000行削減

### Phase 4（任意）: QTableWidget → QAbstractTableModel 移行

**目的**: データとビューの完全分離、将来のソート・フィルタ対応

**手順**:
1. `JosekiTableModel` (QAbstractTableModel) を新規作成
2. `QTableWidget` → `QTableView` + `JosekiTableModel` に置換
3. ボタン列は `QStyledItemDelegate` で実装
4. テストを更新

**前提**: Phase 1〜3 完了後

**注意**: 現状パフォーマンス問題はないため、機能追加（ソート・フィルタ等）が必要になった時点で実施を検討

## 5. テストへの影響

### 既存テスト（tst_josekiwindow.cpp）

テスト内容:
- JosekiSettings 永続化テスト（5件）
- フォント適用テスト（3件）
- setCurrentSfen テスト（5件）

#### Phase 1 影響
- `JosekiMove`, `JosekiEntry` の include パスが変わる → テストでは直接使用していないため影響なし
- `JosekiWindow` の公開API (`setCurrentSfen`, `onFontSizeIncrease` 等) は変更なし → テスト変更不要

#### Phase 2 影響
- テスト変更不要（JosekiMoveFormatter は JosekiWindow 経由で間接的にテスト済み）

#### Phase 3 影響
- `JosekiWindow` の公開スロット (`onFontSizeIncrease` 等) は残存するため、テスト変更不要
- `setCurrentSfen()` は View 側に残り、内部で Presenter を呼ぶ形になるため、テスト変更不要

### 追加テスト（推奨）

| クラス | テスト内容 |
|--------|-----------|
| JosekiRepository | ファイル読込・保存の往復テスト、normalizeSfen テスト |
| JosekiMoveFormatter | usiMoveToJapanese の各パターン（通常移動、駒打ち、成り） |
| JosekiPresenter | 状態遷移テスト（modified, displayEnabled）、CRUD操作テスト |

## 6. ファイル配置

```
src/kifu/joseki/
  josekiRepository.h          (新規)
  josekiRepository.cpp         (新規, ~350行)
  josekimoveformatter.h       (新規)
  josekimoveformatter.cpp     (新規, ~100行)

src/ui/presenters/
  josekipresenter.h           (新規)
  josekipresenter.cpp         (新規, ~600行)

src/dialogs/
  josekiwindow.h              (縮小: 450→~250行)
  josekiwindow.cpp            (縮小: 2,406→~550行)
  josekimovedialog.h/.cpp     (変更なし)
  josekimergedialog.h/.cpp    (変更なし)

src/services/
  josekisettings.h/.cpp       (変更なし)

src/ui/wiring/
  josekiwindowwiring.h/.cpp   (変更なし)
```

## 7. 補足: 重複コードの統合機会

Phase 3 実施時に以下の重複を統合可能:

1. **onEditButtonClicked() と onContextMenuEdit()**: 編集ダイアログ表示ロジックが重複（72行 + 50行）→ Presenter の `editMoveAtRow(int row)` に統合
2. **onDeleteButtonClicked() と onContextMenuDelete()**: 削除確認ロジックが重複（71行 + 29行）→ Presenter の `deleteMoveAtRow(int row)` に統合
3. **onMergeFromCurrentKifu() と onMergeFromKifuFile()**: 保存先確認フロー（~30行）が重複 → `ensureSavePathForMerge()` に抽出
4. **plyNumber 抽出コード**: 5箇所で同じ SFEN パース → `extractPlyNumber(const QString &sfen)` に統合
