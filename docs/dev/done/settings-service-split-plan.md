# SettingsService ドメイン分割計画

## 現状

- `settingsservice.h`: 564行、216宣言
- `settingsservice.cpp`: 1608行
- `settingskeys.h`: 184行（INIキー定数の一元管理）
- 全関数が単一の `namespace SettingsService` に平坦に並んでいる
- 設定追加のたびに API が単調増加し、責務境界が曖昧

## 分類方針

タスク指示の7ドメイン案（WindowSettings / FontSettings / ...）は **型ベース分類**（サイズはサイズ、フォントはフォントで集約）だが、
実際の呼び出しパターンを分析すると、**各ダイアログ/機能はサイズとフォントを同時に読み書きする** ため、型ベースで分割するとインクルード依存が増える。

本計画では **機能ドメインベース分類** を採用する。同じ機能を使うコードが1つのヘッダーだけを include すれば済む構造にする。

## ドメイン分類一覧

### 1. DockSettings（ドック配置設定）— 52関数

**呼び出し元**: dockcreationservice.cpp, docklayoutmanager.cpp, mainwindowuibootstrapper.cpp

| # | getter | setter | INIキーグループ |
|---|--------|--------|----------------|
| 1 | `evalChartDockState()` | `setEvalChartDockState()` | EvalChartDock/ |
| 2 | `evalChartDockFloating()` | `setEvalChartDockFloating()` | EvalChartDock/ |
| 3 | `evalChartDockGeometry()` | `setEvalChartDockGeometry()` | EvalChartDock/ |
| 4 | `evalChartDockVisible()` | `setEvalChartDockVisible()` | EvalChartDock/ |
| 5 | `recordPaneDockFloating()` | `setRecordPaneDockFloating()` | RecordPaneDock/ |
| 6 | `recordPaneDockGeometry()` | `setRecordPaneDockGeometry()` | RecordPaneDock/ |
| 7 | `recordPaneDockVisible()` | `setRecordPaneDockVisible()` | RecordPaneDock/ |
| 8 | `analysisTabDockFloating()` | `setAnalysisTabDockFloating()` | AnalysisTabDock/ |
| 9 | `analysisTabDockGeometry()` | `setAnalysisTabDockGeometry()` | AnalysisTabDock/ |
| 10 | `analysisTabDockVisible()` | `setAnalysisTabDockVisible()` | AnalysisTabDock/ |
| 11 | `boardDockFloating()` | `setBoardDockFloating()` | BoardDock/ |
| 12 | `boardDockGeometry()` | `setBoardDockGeometry()` | BoardDock/ |
| 13 | `boardDockVisible()` | `setBoardDockVisible()` | BoardDock/ |
| 14 | `menuWindowDockFloating()` | `setMenuWindowDockFloating()` | MenuWindowDock/ |
| 15 | `menuWindowDockGeometry()` | `setMenuWindowDockGeometry()` | MenuWindowDock/ |
| 16 | `menuWindowDockVisible()` | `setMenuWindowDockVisible()` | MenuWindowDock/ |
| 17 | `josekiWindowDockFloating()` | `setJosekiWindowDockFloating()` | JosekiWindowDock/ |
| 18 | `josekiWindowDockGeometry()` | `setJosekiWindowDockGeometry()` | JosekiWindowDock/ |
| 19 | `josekiWindowDockVisible()` | `setJosekiWindowDockVisible()` | JosekiWindowDock/ |
| 20 | `kifuAnalysisResultsDockFloating()` | `setKifuAnalysisResultsDockFloating()` | KifuAnalysisResultsDock/ |
| 21 | `kifuAnalysisResultsDockGeometry()` | `setKifuAnalysisResultsDockGeometry()` | KifuAnalysisResultsDock/ |
| 22 | `kifuAnalysisResultsDockVisible()` | `setKifuAnalysisResultsDockVisible()` | KifuAnalysisResultsDock/ |
| 23 | `startupDockLayoutName()` | `setStartupDockLayoutName()` | DockLayout/ |
| 24 | `docksLocked()` | `setDocksLocked()` | Dock/ |

スタンドアロン関数（4件）:
- `savedDockLayoutNames()` — レイアウト名リスト取得
- `saveDockLayout(name, state)` — レイアウト保存
- `loadDockLayout(name)` — レイアウト読込
- `deleteDockLayout(name)` — レイアウト削除

合計: 24 getter/setter ペア × 2 + 4 スタンドアロン = **52関数**

---

### 2. JosekiSettings（定跡設定）— 26関数

**呼び出し元**: josekiwindow.cpp, josekimovedialog.cpp, josekimergedialog.cpp

| # | getter | setter | INIキーグループ |
|---|--------|--------|----------------|
| 1 | `josekiWindowFontSize()` | `setJosekiWindowFontSize()` | JosekiWindow/ |
| 2 | `josekiWindowSfenFontSize()` | `setJosekiWindowSfenFontSize()` | JosekiWindow/ |
| 3 | `josekiWindowLastFilePath()` | `setJosekiWindowLastFilePath()` | JosekiWindow/ |
| 4 | `josekiWindowSize()` | `setJosekiWindowSize()` | JosekiWindow/ |
| 5 | `josekiWindowAutoLoadEnabled()` | `setJosekiWindowAutoLoadEnabled()` | JosekiWindow/ |
| 6 | `josekiWindowRecentFiles()` | `setJosekiWindowRecentFiles()` | JosekiWindow/ |
| 7 | `josekiWindowDisplayEnabled()` | `setJosekiWindowDisplayEnabled()` | JosekiWindow/ |
| 8 | `josekiWindowSfenDetailVisible()` | `setJosekiWindowSfenDetailVisible()` | JosekiWindow/ |
| 9 | `josekiWindowColumnWidths()` | `setJosekiWindowColumnWidths()` | JosekiWindow/ |
| 10 | `josekiMoveDialogFontSize()` | `setJosekiMoveDialogFontSize()` | JosekiWindow/ |
| 11 | `josekiMoveDialogSize()` | `setJosekiMoveDialogSize()` | SizeRelated/ |
| 12 | `josekiMergeDialogFontSize()` | `setJosekiMergeDialogFontSize()` | JosekiWindow/ |
| 13 | `josekiMergeDialogSize()` | `setJosekiMergeDialogSize()` | JosekiWindow/ |

合計: 13ペア = **26関数**

---

### 3. AnalysisSettings（解析・検討設定）— 46関数

**呼び出し元**: evaluationchartwidget.cpp, engineanalysistab.cpp, kifuanalysisdialog.cpp, analysisresultspresenter.cpp, considerationdialog.cpp, considerationtabmanager.cpp, pvboarddialog.cpp

| # | getter | setter | INIキーグループ |
|---|--------|--------|----------------|
| — | **評価値グラフ** | | |
| 1 | `evalChartYLimit()` | `setEvalChartYLimit()` | EvalChart/ |
| 2 | `evalChartXLimit()` | `setEvalChartXLimit()` | EvalChart/ |
| 3 | `evalChartXInterval()` | `setEvalChartXInterval()` | EvalChart/ |
| 4 | `evalChartLabelFontSize()` | `setEvalChartLabelFontSize()` | EvalChart/ |
| — | **解析タブ** | | |
| 5 | `engineInfoColumnWidths(idx)` | `setEngineInfoColumnWidths(idx, widths)` | EngineInfo/ |
| 6 | `thinkingViewColumnWidths(idx)` | `setThinkingViewColumnWidths(idx, widths)` | ThinkingView/ |
| 7 | `usiLogFontSize()` | `setUsiLogFontSize()` | FontSize/ |
| 8 | `thinkingFontSize()` | `setThinkingFontSize()` | FontSize/ |
| — | **棋譜解析** | | |
| 9 | `kifuAnalysisFontSize()` | `setKifuAnalysisFontSize()` | KifuAnalysis/ |
| 10 | `kifuAnalysisResultsWindowSize()` | `setKifuAnalysisResultsWindowSize()` | KifuAnalysis/ |
| 11 | `kifuAnalysisByoyomiSec()` | `setKifuAnalysisByoyomiSec()` | KifuAnalysis/ |
| 12 | `kifuAnalysisEngineIndex()` | `setKifuAnalysisEngineIndex()` | KifuAnalysis/ |
| 13 | `kifuAnalysisFullRange()` | `setKifuAnalysisFullRange()` | KifuAnalysis/ |
| 14 | `kifuAnalysisStartPly()` | `setKifuAnalysisStartPly()` | KifuAnalysis/ |
| 15 | `kifuAnalysisEndPly()` | `setKifuAnalysisEndPly()` | KifuAnalysis/ |
| 16 | `kifuAnalysisDialogSize()` | `setKifuAnalysisDialogSize()` | SizeRelated/ |
| — | **検討モード** | | |
| 17 | `considerationDialogFontSize()` | `setConsiderationDialogFontSize()` | FontSize/ |
| 18 | `considerationEngineIndex()` | `setConsiderationEngineIndex()` | Consideration/ |
| 19 | `considerationUnlimitedTime()` | `setConsiderationUnlimitedTime()` | Consideration/ |
| 20 | `considerationByoyomiSec()` | `setConsiderationByoyomiSec()` | Consideration/ |
| 21 | `considerationMultiPV()` | `setConsiderationMultiPV()` | Consideration/ |
| 22 | `considerationFontSize()` | `setConsiderationFontSize()` | ConsiderationTab/ |
| — | **読み筋盤面** | | |
| 23 | `pvBoardDialogSize()` | `setPvBoardDialogSize()` | SizeRelated/ |

合計: 23ペア = **46関数**

---

### 4. GameSettings（対局・棋譜設定）— 36関数

**呼び出し元**: kifufilecontroller.cpp, kifusavecoordinator.cpp, recordpane.cpp, startgamedialog.cpp, kifupastedialog.cpp, sfencollectiondialog.cpp, jishogiscoredialogcontroller.cpp, commenteditorpanel.cpp, gameinfopanecontroller.cpp

| # | getter | setter | INIキーグループ |
|---|--------|--------|----------------|
| — | **棋譜ファイルI/O** | | |
| 1 | `lastKifuDirectory()` | `setLastKifuDirectory()` | General/ |
| 2 | `lastKifuSaveDirectory()` | `setLastKifuSaveDirectory()` | General/ |
| — | **棋譜欄** | | |
| 3 | `kifuPaneFontSize()` | `setKifuPaneFontSize()` | FontSize/ |
| 4 | `kifuTimeColumnVisible()` | `setKifuTimeColumnVisible()` | RecordPane/ |
| 5 | `kifuBookmarkColumnVisible()` | `setKifuBookmarkColumnVisible()` | RecordPane/ |
| 6 | `kifuCommentColumnVisible()` | `setKifuCommentColumnVisible()` | RecordPane/ |
| — | **メインタブ（コメント・対局情報）** | | |
| 7 | `commentFontSize()` | `setCommentFontSize()` | FontSize/ |
| 8 | `gameInfoFontSize()` | `setGameInfoFontSize()` | FontSize/ |
| — | **対局開始** | | |
| 9 | `startGameDialogSize()` | `setStartGameDialogSize()` | SizeRelated/ |
| 10 | `startGameDialogFontSize()` | `setStartGameDialogFontSize()` | FontSize/ |
| — | **棋譜貼り付け** | | |
| 11 | `kifuPasteDialogSize()` | `setKifuPasteDialogSize()` | SizeRelated/ |
| 12 | `kifuPasteDialogFontSize()` | `setKifuPasteDialogFontSize()` | FontSize/ |
| — | **局面集** | | |
| 13 | `sfenCollectionDialogSize()` | `setSfenCollectionDialogSize()` | SizeRelated/ |
| 14 | `sfenCollectionRecentFiles()` | `setSfenCollectionRecentFiles()` | SfenCollection/ |
| 15 | `sfenCollectionSquareSize()` | `setSfenCollectionSquareSize()` | SfenCollection/ |
| 16 | `sfenCollectionLastDirectory()` | `setSfenCollectionLastDirectory()` | SfenCollection/ |
| — | **持将棋** | | |
| 17 | `jishogiScoreFontSize()` | `setJishogiScoreFontSize()` | FontSize/ |
| 18 | `jishogiScoreDialogSize()` | `setJishogiScoreDialogSize()` | JishogiScore/ |

合計: 18ペア = **36関数**

---

### 5. EngineSettings（エンジン設定ダイアログ）— 8関数

**呼び出し元**: changeenginesettingsdialog.cpp, engineregistrationdialog.cpp

| # | getter | setter | INIキーグループ |
|---|--------|--------|----------------|
| 1 | `engineSettingsFontSize()` | `setEngineSettingsFontSize()` | FontSize/ |
| 2 | `engineSettingsDialogSize()` | `setEngineSettingsDialogSize()` | EngineSettings/ |
| 3 | `engineRegistrationFontSize()` | `setEngineRegistrationFontSize()` | FontSize/ |
| 4 | `engineRegistrationDialogSize()` | `setEngineRegistrationDialogSize()` | EngineRegistration/ |

合計: 4ペア = **8関数**

---

### 6. NetworkSettings（CSAネットワーク設定）— 10関数

**呼び出し元**: csawaitingdialog.cpp, csagamedialog.cpp, engineanalysistab.cpp（CSAログタブ共用）

| # | getter | setter | INIキーグループ |
|---|--------|--------|----------------|
| 1 | `csaLogFontSize()` | `setCsaLogFontSize()` | FontSize/ |
| 2 | `csaWaitingDialogFontSize()` | `setCsaWaitingDialogFontSize()` | FontSize/ |
| 3 | `csaGameDialogFontSize()` | `setCsaGameDialogFontSize()` | FontSize/ |
| 4 | `csaLogWindowSize()` | `setCsaLogWindowSize()` | SizeRelated/ |
| 5 | `csaGameDialogSize()` | `setCsaGameDialogSize()` | SizeRelated/ |

合計: 5ペア = **10関数**

> **注**: `csaLogFontSize` は engineanalysistab.cpp（CSAログタブの表示用）からも呼ばれる。
> AnalysisSettings ではなく NetworkSettings に配置し、解析タブ側で `#include "networksettings.h"` する。

---

### 7. TsumeshogiSettings（詰将棋設定）— 20関数

**呼び出し元**: tsumeshogigeneratordialog.cpp

| # | getter | setter | INIキーグループ |
|---|--------|--------|----------------|
| 1 | `tsumeshogiGeneratorLastSaveDirectory()` | `setTsumeshogiGeneratorLastSaveDirectory()` | TsumeshogiGenerator/ |
| 2 | `tsumeshogiGeneratorDialogSize()` | `setTsumeshogiGeneratorDialogSize()` | SizeRelated/ |
| 3 | `tsumeshogiGeneratorFontSize()` | `setTsumeshogiGeneratorFontSize()` | FontSize/ |
| 4 | `tsumeshogiGeneratorEngineIndex()` | `setTsumeshogiGeneratorEngineIndex()` | TsumeshogiGenerator/ |
| 5 | `tsumeshogiGeneratorTargetMoves()` | `setTsumeshogiGeneratorTargetMoves()` | TsumeshogiGenerator/ |
| 6 | `tsumeshogiGeneratorMaxAttackPieces()` | `setTsumeshogiGeneratorMaxAttackPieces()` | TsumeshogiGenerator/ |
| 7 | `tsumeshogiGeneratorMaxDefendPieces()` | `setTsumeshogiGeneratorMaxDefendPieces()` | TsumeshogiGenerator/ |
| 8 | `tsumeshogiGeneratorAttackRange()` | `setTsumeshogiGeneratorAttackRange()` | TsumeshogiGenerator/ |
| 9 | `tsumeshogiGeneratorTimeoutSec()` | `setTsumeshogiGeneratorTimeoutSec()` | TsumeshogiGenerator/ |
| 10 | `tsumeshogiGeneratorMaxPositions()` | `setTsumeshogiGeneratorMaxPositions()` | TsumeshogiGenerator/ |

合計: 10ペア = **20関数**

---

### 8. AppSettings（アプリケーション全般設定）— 18関数

**呼び出し元**: main.cpp, mainwindowuibootstrapper.cpp, mainwindowlifecyclepipeline.cpp, mainwindowappearancecontroller.cpp, languagecontroller.cpp, menuwindow.cpp, playerinfowiring.cpp

| # | getter | setter | INIキーグループ |
|---|--------|--------|----------------|
| 1 | `language()` | `setLanguage()` | General/ |
| 2 | `lastSelectedTabIndex()` | `setLastSelectedTabIndex()` | UI/ |
| 3 | `toolbarVisible()` | `setToolbarVisible()` | UI/ |
| 4 | `menuWindowFavorites()` | `setMenuWindowFavorites()` | MenuWindow/ |
| 5 | `menuWindowSize()` | `setMenuWindowSize()` | MenuWindow/ |
| 6 | `menuWindowButtonSize()` | `setMenuWindowButtonSize()` | MenuWindow/ |
| 7 | `menuWindowFontSize()` | `setMenuWindowFontSize()` | MenuWindow/ |

インフラストラクチャ関数（4件）:
- `settingsFilePath()` — INIファイルパス取得（各ドメインから共用）
- `loadWindowSize(QWidget*)` — メインウィンドウサイズ読込
- `saveWindowAndBoard(QWidget*, ShogiView*)` — メインウィンドウ＋盤面サイズ保存
- `migrateSettingsIfNeeded()` — 設定バージョンマイグレーション

合計: 7ペア + 4インフラ = **18関数**

---

## 集計

| ドメイン | ペア数 | 関数数 | 主要呼び出し元 |
|---------|--------|--------|---------------|
| DockSettings | 24+4 | 52 | dockcreationservice, docklayoutmanager |
| JosekiSettings | 13 | 26 | josekiwindow, josekimovedialog, josekimergedialog |
| AnalysisSettings | 23 | 46 | evaluationchartwidget, engineanalysistab, kifuanalysisdialog, considerationdialog |
| GameSettings | 18 | 36 | recordpane, startgamedialog, sfencollectiondialog, kifufilecontroller |
| EngineSettings | 4 | 8 | changeenginesettingsdialog, engineregistrationdialog |
| NetworkSettings | 5 | 10 | csawaitingdialog, csagamedialog |
| TsumeshogiSettings | 10 | 20 | tsumeshogigeneratordialog |
| AppSettings | 7+4 | 18 | main, mainwindowuibootstrapper, menuwindow |
| **合計** | | **216** | |

## 構造体化の方針

### 方針A: namespace 分割（推奨）

現在の `namespace SettingsService` をドメインごとの namespace に分割する。

```
src/services/
  settingsservice.h          → settingscommon.h (settingsFilePath, openSettings, migrateSettingsIfNeeded)
  settingsservice.cpp        → settingscommon.cpp
  settingskeys.h             → 変更なし（全キーを一元管理のまま維持）
  docksettings.h/.cpp        → namespace DockSettings
  josekisettings.h/.cpp      → namespace JosekiSettings
  analysissettings.h/.cpp    → namespace AnalysisSettings
  gamesettings.h/.cpp        → namespace GameSettings
  enginesettings.h/.cpp      → namespace EngineSettings（既存の engine/enginesettingscoordinator.h と名前衝突に注意）
  networksettings.h/.cpp     → namespace NetworkSettings
  tsumeshogisettings.h/.cpp  → namespace TsumeshogiSettings
  appsettings.h/.cpp         → namespace AppSettings
```

**長所**:
- 既存コードとの差分が最小（`SettingsService::foo()` → `DockSettings::foo()`）
- 各呼び出し元は必要なヘッダーだけ include すればよい
- INIキーは `settingskeys.h` で一元管理のまま（QSettings キー名は変更しない）

**短所**:
- namespace 名の変更が必要（grep + 一括置換で対応可能）

### 方針B: 構造体化（将来検討）

ドメインごとの設定を struct にまとめ、一括 load/save する。

```cpp
// 例: josekisettings.h
struct JosekiSettings {
    int fontSize = 10;
    int sfenFontSize = 9;
    QString lastFilePath;
    QSize windowSize{800, 500};
    bool autoLoadEnabled = true;
    QStringList recentFiles;
    bool displayEnabled = true;
    bool sfenDetailVisible = false;
    // ...

    static JosekiSettings load();
    void save() const;
};
```

**長所**:
- 設定をまとめて読み書きでき、QSettings アクセス回数が減る
- 型安全性が向上（int/bool/QString の混在を struct メンバで明示）

**短所**:
- 既存の呼び出しパターン（個別 getter/setter）との差分が大きい
- 一部の設定だけ変更したい場合に load → modify → save 全体が必要
- 現状の「変更時即座に永続化」パターンからの大きな変更

### 推奨: 方針A を先行し、必要に応じて方針B へ段階的に移行

## 互換性

- **QSettings キー名は変更しない**: `settingskeys.h` は現状維持。INI ファイルの互換性が保たれる
- **settingsservice.h を互換ヘッダーとして残す**: 移行期間中、全ドメインヘッダーを include する互換ヘッダーを提供

```cpp
// settingsservice.h（互換アダプタ）
#ifndef SETTINGSSERVICE_H
#define SETTINGSSERVICE_H
#include "settingscommon.h"
#include "appsettings.h"
#include "docksettings.h"
#include "josekisettings.h"
#include "analysissettings.h"
#include "gamesettings.h"
#include "enginesettings.h"
#include "networksettings.h"
#include "tsumeshogisettings.h"

// 名前空間エイリアスで互換性を保つ
namespace SettingsService {
    using namespace AppSettings;
    using namespace DockSettings;
    using namespace JosekiSettings;
    using namespace AnalysisSettings;
    using namespace GameSettings;
    using namespace EngineSettings;
    using namespace NetworkSettings;
    using namespace TsumeshogiSettings;
}
#endif
```

> **注意**: `using namespace` による互換アダプタは、名前衝突がないことが前提。
> 現状の関数名はすべてプレフィックスで区別されており（`joseki*`, `csa*`, `eval*` 等）、衝突リスクは低い。

## 分割順序（推奨）

各ステップは独立してビルド・テスト可能。

### Step 1: インフラ分離
- `settingscommon.h/.cpp` を作成（`settingsFilePath`, `openSettings`, `migrateSettingsIfNeeded`）
- `settingsservice.h` を互換アダプタ化
- **理由**: 全ドメインが依存する共通基盤を先に確立

### Step 2: DockSettings 分離（52関数）
- **理由**: 最大のドメイン。呼び出し元が2ファイル（dockcreationservice, docklayoutmanager）に集中しており影響範囲が限定的

### Step 3: JosekiSettings 分離（26関数）
- **理由**: 呼び出し元が定跡関連の3ファイルに閉じている。独立性が高い

### Step 4: TsumeshogiSettings 分離（20関数）
- **理由**: 呼び出し元が1ファイル（tsumeshogigeneratordialog.cpp）のみ。最も独立性が高い

### Step 5: NetworkSettings 分離（10関数）
- **理由**: 呼び出し元が3ファイルに限定。csaLogFontSize の engineanalysistab 共用に注意

### Step 6: EngineSettings 分離（8関数）
- **理由**: 小規模。`engine/enginesettingscoordinator.h` との名前衝突を避ける命名に注意（`EngineDialogSettings` 等を検討）

### Step 7: AnalysisSettings 分離（46関数）
- **理由**: 呼び出し元が多岐にわたるため後半に配置。NetworkSettings の csaLogFontSize 依存解決後に実施

### Step 8: GameSettings 分離（36関数）
- **理由**: 呼び出し元が最も多様。最後に実施

### Step 9: AppSettings 残余化
- settingsservice.h/.cpp から残った関数を appsettings.h/.cpp に移動
- settingsservice.h を互換アダプタのみに

### Step 10: 互換アダプタ除去（任意）
- 全呼び出し元が個別ヘッダーを include 済みになった後、`settingsservice.h` 互換アダプタを削除
- `#include "settingsservice.h"` を個別ドメインヘッダーに置換

## クロスドメイン依存の注意点

| 関数 | 所属ドメイン | 他ドメインからの呼び出し元 |
|------|-------------|-------------------------|
| `csaLogFontSize()` | NetworkSettings | engineanalysistab.cpp（AnalysisSettings のコンテキスト）|
| `settingsFilePath()` | AppSettings (Common) | 多数のダイアログが QSettings 直接アクセスに使用 |
| `lastSelectedTabIndex()` | AppSettings | playerinfowiring.cpp（UIワイヤリング） |

## settingsFilePath() の直接QSettingsアクセスについて

以下のファイルは `settingsFilePath()` を使って QSettings を直接構築し、エンジンリスト等を読み書きしている:
- changeenginesettingsdialog.cpp
- considerationdialog.cpp
- csagamedialog.cpp
- startgamedialog.cpp
- engineregistrationdialog.cpp
- tsumeshogigeneratordialog.cpp
- kifuanalysisdialog.cpp
- nyugyokudeclarationhandler.cpp
- csagamecoordinator.cpp
- shogiview.cpp
- usiprotocolhandler.cpp
- considerationflowcontroller.cpp
- considerationtabmanager.cpp
- gamestartorchestrator.cpp

これらは SettingsService の getter/setter を経由せず、エンジン設定・オプション等を直接読み書きしている。
将来的にはこれらも SettingsService API（特に EngineSettings ドメイン）に統合することを検討するが、本分割計画のスコープ外とする。
