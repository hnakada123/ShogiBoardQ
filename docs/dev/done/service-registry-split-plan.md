# MainWindowServiceRegistry 分割計画

## 1. 現状

| 項目 | 値 |
|------|-----|
| ファイル | `src/app/mainwindowserviceregistry.cpp` / `.h` |
| 行数 | 1,354 行（.cpp） / 138 行（.h） |
| ensure* メソッド数 | 50 |
| 非ensure メソッド数 | 17 |
| 合計メソッド数 | **67** |
| include 数 | 83 |
| std::bind 使用箇所 | 42 |

## 2. カテゴリ分類

### 2.1 UI系（ウィジェット・プレゼンター・ビュー・ドック・メニュー・通知）

| # | メソッド | 行数 | 概要 |
|---|---------|------|------|
| 1 | `ensureEvaluationGraphController()` | 16 | 評価値グラフUI |
| 2 | `ensureRecordPresenter()` | 19 | 棋譜表示プレゼンター |
| 3 | `ensurePlayerInfoController()` | 6 | プレイヤー情報コントローラ |
| 4 | `ensurePlayerInfoWiring()` | 32 | プレイヤー情報配線 |
| 5 | `ensureDialogCoordinator()` | 16 | ダイアログコーディネーター |
| 6 | `ensureMenuWiring()` | 12 | メニューウィンドウ配線 |
| 7 | `ensureDockLayoutManager()` | 20 | ドック配置管理 |
| 8 | `ensureDockCreationService()` | 7 | ドック生成サービス |
| 9 | `ensureUiStatePolicyManager()` | 4 | UI状態ポリシー |
| 10 | `ensureUiNotificationService()` | 12 | UI通知サービス |
| 11 | `ensureRecordNavigationHandler()` | 5 | 棋譜ナビUI配線 |
| 12 | `ensureLanguageController()` | 11 | 言語コントローラ |
| 13 | `unlockGameOverStyle()` | 6 | 終局スタイル解除 |
| 14 | `createMenuWindowDock()` | 5 | メニューウィンドウドック生成 |
| 15 | `clearEvalState()` | 13 | 評価値状態クリア |

**小計: 15 メソッド / 約 184 行**

### 2.2 Game系（対局・ゲーム進行・ターン管理・セッションライフサイクル）

| # | メソッド | 行数 | 概要 |
|---|---------|------|------|
| 1 | `ensureTimeController()` | 8 | 時間制御 |
| 2 | `ensureReplayController()` | 11 | リプレイ |
| 3 | `ensureMatchCoordinatorWiring()` | 47 | MatchCoordinator配線 |
| 4 | `ensureGameStateController()` | 20 | ゲーム状態コントローラ |
| 5 | `ensureGameStartCoordinator()` | 8 | 対局開始コーディネーター |
| 6 | `ensureCsaGameWiring()` | 34 | CSA通信対局配線 |
| 7 | `ensurePreStartCleanupHandler()` | 28 | 開始前クリーンアップ |
| 8 | `ensureTurnSyncBridge()` | 8 | ターン同期ブリッジ |
| 9 | `ensureTurnStateSyncService()` | 21 | 手番同期サービス |
| 10 | `ensureLiveGameSessionStarted()` | 5 | ライブセッション開始 |
| 11 | `ensureLiveGameSessionUpdater()` | 14 | ライブセッション更新 |
| 12 | `ensureUndoFlowService()` | 13 | 手戻しフロー |
| 13 | `ensureJishogiController()` | 5 | 持将棋コントローラ |
| 14 | `ensureNyugyokuHandler()` | 5 | 入玉宣言ハンドラ |
| 15 | `ensureConsecutiveGamesController()` | 14 | 連続対局コントローラ |
| 16 | `ensureGameSessionOrchestrator()` | 58 | 対局ライフサイクル |
| 17 | `ensureSessionLifecycleCoordinator()` | 37 | セッションライフサイクル |
| 18 | `ensureCoreInitCoordinator()` | 28 | コア初期化 |
| 19 | `clearGameStateFields()` | 25 | ゲーム状態フィールドクリア |
| 20 | `resetEngineState()` | 13 | エンジン状態リセット |
| 21 | `updateTurnStatus()` | 14 | 手番表示更新 |
| 22 | `initMatchCoordinator()` | 14 | MatchCoordinator初期化 |

**小計: 22 メソッド / 約 422 行**

### 2.3 Kifu系（棋譜・ナビゲーション・ファイルI/O・コメント・定跡）

| # | メソッド | 行数 | 概要 |
|---|---------|------|------|
| 1 | `ensureBranchNavigationWiring()` | 33 | 分岐ナビゲーション配線 |
| 2 | `ensureKifuFileController()` | 25 | 棋譜ファイルコントローラ |
| 3 | `ensureKifuExportController()` | 20 | 棋譜エクスポート |
| 4 | `ensureGameRecordUpdateService()` | 23 | 棋譜追記サービス |
| 5 | `ensureGameRecordLoadService()` | 24 | 棋譜データ読み込み |
| 6 | `ensureKifuLoadCoordinatorForLive()` | 8 | ライブ棋譜ロード |
| 7 | `ensureGameRecordModel()` | 16 | 棋譜モデル構築 |
| 8 | `ensureCommentCoordinator()` | 7 | コメントコーディネーター |
| 9 | `ensureKifuNavigationCoordinator()` | 16 | 棋譜ナビゲーション |
| 10 | `ensureJosekiWiring()` | 27 | 定跡ウィンドウ配線 |
| 11 | `clearUiBeforeKifuLoad()` | 21 | 棋譜読込前UIクリア |
| 12 | `updateJosekiWindow()` | 9 | 定跡ウィンドウ更新 |

**小計: 12 メソッド / 約 229 行**

### 2.4 Analysis系（解析・検討モード・エンジン連携）

| # | メソッド | 行数 | 概要 |
|---|---------|------|------|
| 1 | `ensurePvClickController()` | 24 | 読み筋クリック |
| 2 | `ensureConsiderationPositionService()` | 20 | 検討局面サービス |
| 3 | `ensureAnalysisPresenter()` | 5 | 解析結果プレゼンター |
| 4 | `ensureConsiderationWiring()` | 13 | 検討モード配線 |
| 5 | `ensureUsiCommandController()` | 8 | USIコマンドコントローラ |

**小計: 5 メソッド / 約 70 行**

### 2.5 その他/共通（盤面操作・リセット・横断的操作）

| # | メソッド | 行数 | 概要 |
|---|---------|------|------|
| 1 | `ensureBoardSetupController()` | 24 | 盤面セットアップ |
| 2 | `ensurePositionEditCoordinator()` | 22 | 局面編集コーディネーター |
| 3 | `ensurePositionEditController()` | 5 | 局面編集コントローラ |
| 4 | `ensureBoardSyncPresenter()` | 24 | 盤面同期プレゼンター |
| 5 | `ensureBoardLoadService()` | 21 | 盤面読込サービス |
| 6 | `setupBoardInteractionController()` | 14 | BoardInteractionController構築 |
| 7 | `handleMoveRequested()` | 17 | 着手リクエスト処理 |
| 8 | `handleMoveCommitted()` | 14 | 着手確定処理 |
| 9 | `handleBeginPositionEditing()` | 10 | 局面編集開始 |
| 10 | `handleFinishPositionEditing()` | 9 | 局面編集終了 |
| 11 | `resetModels()` | 21 | モデルリセット |
| 12 | `resetUiState()` | 13 | UI状態リセット |
| 13 | `clearSessionDependentUi()` | 21 | セッション依存UIクリア |

**小計: 13 メソッド / 約 215 行**

### サイズ比較

```
Game系     ████████████████████████████████████████████  422行 (22メソッド)
Kifu系     ███████████████████████                       229行 (12メソッド)
共通/Board ██████████████████████                        215行 (13メソッド)
UI系       ██████████████████                            184行 (15メソッド)
Analysis系 ███████                                        70行  (5メソッド)
```

## 3. カテゴリ間依存関係

### 3.1 依存マトリクス

呼び出し元（行）が呼び出し先（列）の `ensure*` を呼ぶ回数:

| From ＼ To | UI | Game | Kifu | Analysis | 共通 |
|------------|:---:|:----:|:----:|:--------:|:----:|
| **UI**     | -   | 0    | 2    | 1        | 0    |
| **Game**   | 5   | -    | 2    | 0        | 1    |
| **Kifu**   | 2   | 1    | -    | 0        | 1    |
| **Analysis** | 1 | 0    | 0    | -        | 0    |
| **共通**   | 2   | 1    | 3    | 0        | -    |

### 3.2 詳細な依存一覧

#### UI → Kifu（2件）
- `ensureRecordPresenter` → `ensureCommentCoordinator`
- `ensureDialogCoordinator` → `ensureKifuNavigationCoordinator`

#### UI → Analysis（1件）
- `ensureDialogCoordinator` → `ensureConsiderationWiring`

#### Game → UI（5件）
- `ensureMatchCoordinatorWiring` → `ensureEvaluationGraphController`, `ensurePlayerInfoWiring`, `ensureDialogCoordinator`
- `ensureCsaGameWiring` → `ensureUiStatePolicyManager`, `ensureUiNotificationService`
- `ensureGameStateController` → `ensureUiStatePolicyManager`
- `ensureSessionLifecycleCoordinator` → `ensurePlayerInfoWiring`
- `ensureCoreInitCoordinator` → `ensurePlayerInfoWiring`（callback経由）

#### Game → Kifu（2件）
- `ensureGameStateController` → `ensureKifuNavigationCoordinator`
- `ensureCsaGameWiring` → `ensureGameRecordUpdateService`

#### Game → 共通（1件）
- `ensureMatchCoordinatorWiring` → `ensureCoreInitCoordinator`（共通から移動する場合）

#### Kifu → UI（2件）
- `ensureKifuFileController` → `ensurePlayerInfoWiring`
- `ensureGameRecordUpdateService` → `ensureRecordPresenter`
- `ensureGameRecordLoadService` → `ensureRecordPresenter`

#### Kifu → Game（1件）
- `ensureGameRecordUpdateService` → `ensureLiveGameSessionUpdater`

#### Kifu → 共通（1件）
- `ensureKifuNavigationCoordinator` → `ensureBoardSyncPresenter`

#### Analysis → UI（1件）
- `ensureConsiderationWiring` → `ensureDialogCoordinator`

#### 共通 → UI（2件）
- `ensurePositionEditCoordinator` → `ensureUiStatePolicyManager`
- `ensureBoardSetupController` → `ensureEvaluationGraphController`

#### 共通 → Game（1件）
- `ensureBoardSetupController` → `ensureTimeController`

#### 共通 → Kifu（3件）
- `handleMoveCommitted` → `ensureGameRecordUpdateService`, `updateJosekiWindow`
- `ensureBoardLoadService` → `ensureKifuNavigationCoordinator`

### 3.3 GameSessionOrchestrator の特殊性

`ensureGameSessionOrchestrator` は callback 経由で多数のカテゴリに依存する（Game内 7件 + UI 1件 + 共通 2件 + Kifu 1件）。ただし callback は `std::bind` / `std::function` なので、分割後も呼び出し先のメソッドポインタを渡せば動作する。

## 4. 分割先クラス名の案

| カテゴリ | 分割先クラス名 | メソッド数 | 推定行数 |
|---------|---------------|-----------|---------|
| UI系 | `MainWindowUiRegistry` | 15 | ~250 |
| Game系 | `MainWindowGameRegistry` | 22 | ~500 |
| Kifu系 | `MainWindowKifuRegistry` | 12 | ~300 |
| Analysis系 | `MainWindowAnalysisRegistry` | 5 | ~120 |
| 共通/Board系 | `MainWindowBoardRegistry` | 13 | ~280 |

※ 推定行数は include / コンストラクタ / コメント含む

### 4.1 共通パターン

分割後の各レジストリクラスは同じパターンに従う:

```cpp
class MainWindowXxxRegistry : public QObject
{
    Q_OBJECT
public:
    explicit MainWindowXxxRegistry(MainWindow& mw, QObject* parent = nullptr);
    // ensure* / 非ensure メソッド群
private:
    MainWindow& m_mw;
};
```

### 4.2 カテゴリ間呼び出しの解決方法

分割後、あるレジストリが別カテゴリの `ensure*` を呼ぶ場合は以下の方針:

1. **MainWindowServiceRegistry をファサードとして残す** — 全サブレジストリを保持し、カテゴリ横断の `ensure*` 呼び出しをファサード経由で解決する
2. 各サブレジストリは `MainWindowServiceRegistry&` への参照を持ち、カテゴリ外の `ensure*` はファサード経由で呼ぶ
3. あるいは callback (`std::function`) で inject して疎結合にする

**推奨**: 方針 1（ファサード）が最もシンプル。`MainWindowServiceRegistry` は 5 サブレジストリの所有と転送のみの薄いクラスになる。

## 5. 推奨分割順序

依存の少ないカテゴリから着手し、段階的に分割する:

### Step 1: Analysis系（最優先）

- **理由**: 最小（5メソッド / 70行）、incoming cross-dep が 1 件のみ（UI → `ensureConsiderationWiring`）
- **作業量**: 小
- **リスク**: 低

### Step 2: 共通/Board系

- **理由**: 盤面操作系は凝集度が高く自然なグループ。incoming cross-dep 3件
- **作業量**: 中
- **リスク**: 低〜中（`handleMoveCommitted` が Kifu 系を呼ぶ）

### Step 3: Kifu系

- **理由**: incoming cross-dep 5件だが、棋譜関連で凝集度が高い
- **作業量**: 中
- **リスク**: 中（UI系の `ensureRecordPresenter` / `ensurePlayerInfoWiring` に依存）

### Step 4: UI系

- **理由**: incoming cross-dep 10件と最多。他が先に分割されている前提だと依存解決が容易
- **作業量**: 中
- **リスク**: 中

### Step 5: Game系（最後）

- **理由**: 最大（22メソッド / 422行）、outgoing cross-dep が最多。他の 4 カテゴリが分割済みなら残りが Game 系となり自然に抽出できる
- **作業量**: 大
- **リスク**: 中〜高（`ensureMatchCoordinatorWiring` / `ensureGameSessionOrchestrator` が多方面に依存）

### Step 6: ファサード化

- 全サブレジストリの分割が完了したら `MainWindowServiceRegistry` を薄いファサードに再構成
- MainWindow 側の呼び出しコードは `m_serviceRegistry->ensureXxx()` のまま変更不要

## 6. Game系の追加分割検討

Game系は 422 行と突出して大きいため、以下のサブ分割を検討する:

| サブグループ | メソッド | 行数 |
|------------|---------|------|
| **Match/Session** (MCW, GSO, SLC, CoreInit, initMC) | 8 | ~240 |
| **Turn/Time** (Time, Replay, TurnSync, TurnStateSync, updateTurn) | 5 | ~62 |
| **Game State** (GSC, GSCoord, Cleanup, Jishogi, Nyugyoku, CG, clearFields, resetEngine) | 9 | ~120 |

ただし現時点では 5 分割で十分な改善が見込めるため、Game 系の追加分割は必要に応じて後続タスクで対応する。

## 7. まとめ

```
MainWindowServiceRegistry (ファサード: 97行、67メソッドすべて1行委譲)
├── MainWindowUiRegistry        (15 methods,  299 lines) ✅ 完了
├── MainWindowGameRegistry      (22 methods,  593 lines) ✅ 完了
├── MainWindowKifuRegistry      (12 methods,  328 lines) ✅ 完了
├── MainWindowAnalysisRegistry  ( 5 methods,  119 lines) ✅ 完了
└── MainWindowBoardRegistry     (13 methods,  315 lines) ✅ 完了
                                合計:        1,654 lines
```

- 分割は Analysis → Board → Kifu → UI → Game の順で段階的に実施 → **全ステップ完了**
- MainWindowServiceRegistry はファサードとして残し、MainWindow 側の呼び出しコードの変更を最小化
- カテゴリ間の cross-dep は合計 24 件あるが、ファサード経由の転送で解決済み
- mainwindowserviceregistry.cpp: **945行 → 97行**（90% 削減）
