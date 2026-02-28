# アーキテクチャ概観

ShogiBoardQ のモジュール構成・依存関係・設計パターンを概観するドキュメント。
各モジュールの詳細な責務は [CLAUDE.md](../../CLAUDE.md) を参照。

## 1. レイヤー構成

```mermaid
graph TD
    subgraph "Presentation Layer"
        UI["ui/<br/>(96 files)<br/>controllers / coordinators<br/>presenters / wiring"]
        Views["views/<br/>(10 files)<br/>盤面描画"]
        Widgets["widgets/<br/>(51 files)<br/>カスタムウィジェット"]
        Dialogs["dialogs/<br/>(41 files)<br/>ダイアログ"]
    end

    subgraph "Application Layer"
        App["app/<br/>(71 files)<br/>MainWindow / Composition Root<br/>起動・終了パイプライン"]
    end

    subgraph "Domain Layer"
        Game["game/<br/>(46 files)<br/>対局進行・ターン管理"]
        Analysis["analysis/<br/>(20 files)<br/>解析・検討"]
        Kifu["kifu/<br/>(81 files)<br/>棋譜 I/O・形式変換"]
        Navigation["navigation/<br/>(4 files)<br/>ナビゲーション"]
        Board["board/<br/>(8 files)<br/>盤面操作・編集"]
    end

    subgraph "Infrastructure Layer"
        Engine["engine/<br/>(24 files)<br/>USI エンジン通信"]
        Network["network/<br/>(10 files)<br/>CSA ネットワーク対局"]
        Services["services/<br/>(33 files)<br/>設定・計時"]
    end

    subgraph "Foundation Layer"
        Core["core/<br/>(25 files)<br/>盤面状態・合法手・駒"]
        Common["common/<br/>(13 files)<br/>ErrorBus・ユーティリティ"]
        Models["models/<br/>(5 files)<br/>Qt データモデル"]
    end

    App --> Game
    App --> UI
    App --> Kifu
    UI --> Game
    UI --> Kifu
    UI --> Widgets
    UI --> Dialogs
    UI --> Analysis
    Game --> Core
    Game --> Engine
    Analysis --> Engine
    Analysis --> Core
    Kifu --> Core
    Engine --> Core
    Network --> Core
    Network --> Engine
    Views --> Core
    Widgets --> Services
    Dialogs --> Services
    Services --> Core
    Common --> Core
```

## 2. 依存関係ルール

レイヤーテスト (`tst_layer_dependencies.cpp`) で以下の制約を強制:

| モジュール | 依存禁止先 |
|---|---|
| **core** | app, ui, dialogs, widgets, views |
| **kifu/formats** | app, dialogs, widgets, views |
| **engine** | app, dialogs, widgets, views |
| **game** | dialogs, widgets, views |
| **services** | app, dialogs, widgets, views |

**設計方針**: 下位レイヤーは上位レイヤーに依存しない。GUI 非依存の Domain/Foundation 層は Presentation 層をインクルードできない。

### モジュール間依存マトリックス（実測値）

include 解析に基づく主要な依存関係（ヘッダー参照数）:

| 依存元 → | core | game | kifu | engine | services | common | widgets | views | dialogs | ui | app |
|---|---|---|---|---|---|---|---|---|---|---|---|
| **core** | - | | | | | 2 | | | | | |
| **game** | 8 | - | 5 | 3 | 3 | 1 | 3 | 1 | 2 | 4 | 1 |
| **kifu** | 5 | 2 | - | | 1 | 3 | 3 | 1 | 1 | 3 | 1 |
| **engine** | 4 | 1 | 2 | - | 1 | 1 | | | 1 | | |
| **analysis** | 6 | 2 | | 4 | 2 | 3 | 6 | 1 | 3 | | |
| **network** | 5 | 1 | 1 | 3 | 1 | 1 | | 1 | | | |
| **app** | 6 | 8 | 14 | 2 | 4 | 1 | 6 | 1 | 2 | 36 | - |
| **ui** | 7 | 5 | 12 | 4 | 8 | 4 | 12 | 1 | 9 | - | 6 |

## 3. モジュール責務一覧

| モジュール | 責務 | Qt 依存 |
|---|---|---|
| **core** | 盤面状態、駒、合法手判定、SFEN 変換 | QObject のみ |
| **game** | 対局進行、ターン管理、終局処理、戦略パターン | QObject |
| **kifu** | 棋譜 I/O、7 形式のコンバータ、エクスポート | Widgets (ダイアログ) |
| **engine** | USI プロトコル、エンジンプロセス管理、設定 | QProcess |
| **analysis** | 棋譜解析、検討モード、解析結果表示 | Widgets |
| **network** | CSA プロトコル、ネットワーク対局 | QTcpSocket |
| **navigation** | 棋譜ナビゲーション制御 | QObject |
| **board** | 盤面インタラクション、局面編集、画像エクスポート | QGraphicsView |
| **app** | MainWindow Facade、Composition Root、起動/終了パイプライン | QMainWindow |
| **ui** | Presenter、Controller、Coordinator、Wiring | QObject |
| **views** | 盤面・駒の Qt Graphics View 描画 | QGraphicsView |
| **widgets** | カスタム UI ウィジェット（評価グラフ、棋譜欄等） | QWidget |
| **dialogs** | 各種ダイアログ（対局設定、エンジン登録等） | QDialog |
| **services** | 設定永続化、計時、通知 | QSettings |
| **common** | ErrorBus、持将棋計算、ログカテゴリ | 最小限 |
| **models** | Qt Item Model（通信ログ、思考ログ） | QAbstractItemModel |

## 4. 主要フロー

### 4.1 対局開始フロー

```mermaid
sequenceDiagram
    participant User as ユーザー
    participant MW as MainWindow
    participant GSC as GameStartCoordinator
    participant MC as MatchCoordinator
    participant GSO as GameStartOrchestrator
    participant GC as ShogiGameController
    participant Engine as Usi (USI Engine)

    User->>MW: メニュー「対局開始」
    MW->>GSC: start(StartParams)
    GSC->>GSC: configureAndStart(StartOptions)
    GSC-->>MW: requestPreStartCleanup()
    GSC-->>MW: requestApplyTimeControl()
    GSC->>MC: prepareAndStartGame()
    MC->>GSO: buildStartOptions()
    MC->>GC: newGame(sfen)
    MC->>MC: createAndStartModeStrategy()
    MC->>Engine: initEngine() / startGame()
    GSC-->>MW: playerNamesResolved()
    GSC-->>MW: started()
```

### 4.2 棋譜ロードフロー

```mermaid
sequenceDiagram
    participant User as ユーザー
    participant KFC as KifuFileController
    participant KLC as KifuLoadCoordinator
    participant Conv as 形式コンバータ
    participant GC as ShogiGameController
    participant View as ShogiView

    User->>KFC: ファイル選択
    KFC->>KLC: loadKifuFromFile(path)
    KLC->>KLC: 形式判定 (拡張子)
    KLC->>Conv: convert(text) → SFEN列
    Conv-->>KLC: SFENリスト
    KLC->>GC: newGame(startSfen)
    loop 各手
        KLC->>GC: validateAndMove(sfen)
    end
    KLC-->>View: 盤面更新
    KLC-->>KFC: kifuLoaded()
```

**対応形式**: KIF, KI2, CSA, JKF, USEN, USI, BOD/SFEN（局面のみ）

### 4.3 解析フロー

```mermaid
sequenceDiagram
    participant User as ユーザー
    participant AFC as AnalysisFlowController
    participant AC as AnalysisCoordinator
    participant Engine as Usi (USI Engine)
    participant Presenter as 結果表示

    User->>AFC: 解析開始
    AFC->>AC: startAnalyzeRange(startPly, endPly)
    loop 各局面
        AC->>Engine: position + go
        Engine-->>AC: info / bestmove
        AC-->>AFC: analysisProgress()
        AFC-->>Presenter: 結果更新
    end
    AC-->>AFC: analysisFinished()
```

## 5. 設計パターン

### 5.1 Deps / Hooks / Refs パターン

MatchCoordinator とその子ハンドラ間で使用される依存性注入パターン。

```mermaid
graph LR
    MC["MatchCoordinator<br/>(親)"] -->|"Refs<br/>(状態参照)"| Handler["GameEndHandler<br/>(子)"]
    Handler -->|"Hooks<br/>(コールバック)"| MC
```

| 構造体 | 方向 | 用途 |
|---|---|---|
| **Refs** | 親 → 子 | 親の内部状態へのポインタ・プロバイダ関数 |
| **Hooks** | 子 → 親 | `std::function` コールバックで親メソッドを呼び出す |
| **Deps** | 外部 → 対象 | 依存オブジェクト + 遅延初期化コールバック |

```cpp
// Refs: 親の状態を読み取るための参照
struct Refs {
    ShogiGameController* gc = nullptr;
    std::function<Usi*()> usi1Provider;  // 動的変更に対応
};

// Hooks: 親のメソッドをコールバックで呼び出す
struct Hooks {
    std::function<void(const GameEndInfo&)> setGameOver;
    std::function<void()> appendKifuLine;
};

// Deps: 遅延初期化対応のダブルポインタ
struct Deps {
    Controller** controller = nullptr;                 // ダブルポインタ
    std::function<void()> ensureController;            // 遅延初期化
};
```

### 5.2 Wiring 層

`src/ui/wiring/` に配置。Signal/Slot の接続と依存性の構築を集約する。

| Wiring クラス | 責務 |
|---|---|
| MatchCoordinatorWiring | MC 生成・配線・GameStartCoordinator 管理 |
| PlayerInfoWiring | 対局者名・エンジン名の UI 反映 |
| CsaGameWiring | CSA ネットワーク対局の配線 |
| DialogCoordinatorWiring | ダイアログ起動・結果処理の配線 |
| ConsiderationWiring | 検討モードの配線 |
| AnalysisTabWiring | 解析タブの配線 |
| その他 13 クラス | メニュー操作・ナビゲーション等の配線 |

### 5.3 MVP (Model-View-Presenter)

`src/ui/presenters/` に配置。Model → View への表示データ変換のみを担当（リードオンリー）。

| Presenter | 責務 |
|---|---|
| BoardSyncPresenter | SFEN 適用・盤面同期・ハイライト |
| NavigationPresenter | ナビゲーションボタン状態 |
| TimeDisplayPresenter | 残り時間テキスト |
| GameRecordPresenter | 棋譜欄・手数記録 |
| EvalGraphPresenter | 評価値グラフ描画 |
| EngineAnalysisPresenter | エンジン分析結果表示 |
| KifuDisplayPresenter | 棋譜テキスト表示 |

### 5.4 Composition Root

`src/app/mainwindowcompositionroot.cpp` に配置。`ensure*()` メソッドによる遅延初期化パターンで、依存オブジェクトを初回アクセス時に生成する。

```mermaid
graph TD
    MW["MainWindow"] --> CR["CompositionRoot<br/>(ensure* methods)"]
    CR --> |"ensures"| W["Wiring classes"]
    CR --> |"ensures"| C["Controllers"]
    CR --> |"ensures"| Co["Coordinators"]
    MW --> LP["LifecyclePipeline<br/>(startup / shutdown)"]
    MW --> SR["ServiceRegistry<br/>(delegated methods)"]
```

## 6. ファイル命名規約

| サフィックス | 用途 | 代表例 |
|---|---|---|
| **Controller** | ユーザー入力・UI 状態制御 | BoardInteractionController, GameStateController |
| **Coordinator** | 複数オブジェクト間の統合・オーケストレーション | GameStartCoordinator, DialogCoordinator |
| **Service** | 純粋ロジック・ポリシー判定 | PlayModePolicyService, SettingsService |
| **Manager** | リソース寿命・初期化/破棄管理 | EngineLifecycleManager, ConsiderationTabManager |
| **Handler** | 特定イベント・フロー処理 | GameEndHandler, MatchUndoHandler |
| **Presenter** | Model → View の表示データ変換 | BoardSyncPresenter, EvalGraphPresenter |
| **Wiring** | Signal/Slot 接続・依存性構築 | MatchCoordinatorWiring, CsaGameWiring |
| **Orchestrator** | 非 QObject のフロー制御 | GameStartOrchestrator, GameSessionOrchestrator |
