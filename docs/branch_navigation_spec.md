# 分岐ツリー・棋譜欄・分岐候補欄 リファクタリング仕様書

## 1. 概要

本仕様書は、分岐ツリー、棋譜欄、分岐候補欄を管理するプログラムの抜本的な再設計を行うための設計仕様である。

### 1.1 現行実装の問題点

1. **KifuLoadCoordinatorの肥大化**: 2000行以上あり、以下を全て担当している
   - 棋譜ファイルの読み込み
   - 分岐の解決（ResolvedRows構築）
   - 分岐候補表示計画の構築
   - ライブ対局の状態管理
   - UIの更新

2. **複雑な依存注入**: `pointer to pointer`のパターンが多用されており追跡が困難

3. **分散した状態管理**（移行完了）:
   - `KifuBranchTree` - MainWindow（主要データソース、m_resolvedRows を置換）
   - `m_activeResolvedRow` - MainWindow（ライン選択状態）
   - `m_liveGameState` - KifuLoadCoordinator
   - `m_branchDisplayPlan` - MainWindow（参照はKifuLoadCoordinator）

4. **ナビゲーションの複雑さ**:
   - 6つのボタン、分岐候補欄、分岐ツリー、棋譜欄の各選択が独立したコードパスを持つ

5. **ライブ対局と静的棋譜データの混在**: 分岐の扱いが不明確

---

## 2. 新設計のゴール

1. **単一責任の原則**: 各クラスは1つの責務のみを持つ
2. **データと表示の分離**: モデル層とビュー層を明確に分離
3. **統一されたナビゲーション**: 全ての入力（ボタン、クリック、選択）が同一のパスを通る
4. **明確な状態遷移**: 静的棋譜 → ライブ対局 → 確定 のフローを明確化
5. **イベント駆動**: 状態変更はシグナルで通知、UIは購読して更新

---

## 3. 新しいデータモデル

### 3.1 KifuBranchTree（新規クラス）

分岐構造を表すツリー型データモデル。UIとは独立。

```cpp
// src/kifu/kifubranchtree.h

// 終局手の種類
enum class TerminalType {
    None,           // 通常の指し手（終局手ではない）
    Resign,         // 投了
    Checkmate,      // 詰み
    Repetition,     // 千日手
    Impasse,        // 持将棋
    Timeout,        // 切れ負け
    IllegalWin,     // 反則勝ち
    IllegalLoss,    // 反則負け
    Forfeit,        // 不戦敗
    Interrupt,      // 中断
    NoCheckmate     // 不詰（詰将棋用）
};

class KifuBranchNode {
public:
    int nodeId;                    // ユニークID
    int ply;                       // 手数（0=開始局面、1以降=指し手）
    QString displayText;           // 表示テキスト（例: "▲７六歩(77)" または "△投了"）
    QString sfen;                  // この局面のSFEN（終局手の場合は直前と同じ）
    ShogiMove move;                // この手を表すShogiMove（ply=0や終局手は無効）
    QString comment;               // コメント
    TerminalType terminalType = TerminalType::None;  // 終局手の種類

    KifuBranchNode* parent;        // 親ノード（nullptr=ルート）
    QVector<KifuBranchNode*> children;  // 子ノード（分岐）

    bool isMainLine() const;       // 本譜かどうか
    int depth() const;             // ルートからの深さ
    QString lineName() const;      // "本譜", "分岐1", "分岐2"...

    // 終局手関連
    bool isTerminal() const { return terminalType != TerminalType::None; }
    bool isActualMove() const { return !isTerminal() && ply > 0; }  // 盤面を変化させる指し手か
};

class KifuBranchTree : public QObject {
    Q_OBJECT
public:
    // 構築
    void clear();
    void setRootSfen(const QString& sfen);
    KifuBranchNode* addMove(KifuBranchNode* parent, const ShogiMove& move,
                            const QString& displayText, const QString& sfen);

    // クエリ
    KifuBranchNode* root() const;
    KifuBranchNode* nodeAt(int nodeId) const;
    KifuBranchNode* findByPlyOnLine(KifuBranchNode* lineEnd, int ply) const;
    KifuBranchNode* findByPlyOnMainLine(int ply) const;  // 本譜のN手目を取得

    // ライン操作
    QVector<KifuBranchNode*> mainLine() const;        // 本譜のノード列
    QVector<KifuBranchNode*> pathToNode(KifuBranchNode* node) const;  // ルートからの経路
    QVector<KifuBranchNode*> branchesAt(KifuBranchNode* node) const;  // 分岐候補

    // 分岐判定
    bool hasBranch(KifuBranchNode* node) const;       // このノードに分岐があるか
    QSet<int> branchablePlysOnLine(const BranchLine& line) const;  // ラインの分岐あり手数集合

    // 分岐ライン列挙
    struct BranchLine {
        int lineIndex;                  // 0=本譜、1以降=分岐
        QString name;                   // "本譜", "分岐1"...
        QVector<KifuBranchNode*> nodes; // ノード列
        int branchPly;                  // 分岐開始手数
        KifuBranchNode* branchPoint;    // 分岐点のノード
    };
    QVector<BranchLine> allLines() const;

signals:
    void treeChanged();             // ツリー構造が変更された
    void nodeAdded(KifuBranchNode* node);
    void nodeRemoved(int nodeId);

private:
    KifuBranchNode* m_root = nullptr;
    QHash<int, KifuBranchNode*> m_nodeById;
    int m_nextNodeId = 1;
};
```

### 3.2 KifuNavigationState（新規クラス）

現在のナビゲーション状態を管理する。

```cpp
// src/kifu/kifunavigationstate.h
class KifuNavigationState : public QObject {
    Q_OBJECT
public:
    // 現在位置
    KifuBranchNode* currentNode() const;
    int currentPly() const;
    int currentLineIndex() const;           // 0=本譜、1以降=分岐
    QString currentLineName() const;

    // 設定（内部からのみ）
    void setCurrentNode(KifuBranchNode* node);

    // クエリ
    bool isOnMainLine() const;
    bool canGoForward() const;
    bool canGoBack() const;
    int maxPlyOnCurrentLine() const;

    // 分岐候補
    QVector<KifuBranchNode*> branchCandidatesAtCurrent() const;
    bool hasBranchAtCurrent() const;

signals:
    void currentNodeChanged(KifuBranchNode* newNode, KifuBranchNode* oldNode);
    void lineChanged(int newLineIndex, int oldLineIndex);

private:
    KifuBranchNode* m_currentNode = nullptr;
    KifuBranchTree* m_tree = nullptr;
};
```

### 3.3 LiveGameSession（新規クラス）

ライブ対局のセッションを管理する。途中局面からの対局開始に対応。

```cpp
// src/kifu/livegamesession.h
class LiveGameSession : public QObject {
    Q_OBJECT
public:
    // セッション開始
    void startFromNode(KifuBranchNode* branchPoint);  // 任意の局面から開始
    void startFromRoot();                              // 開始局面から開始（新規対局）

    // 状態
    bool isActive() const;
    KifuBranchNode* branchPoint() const;              // 分岐起点（nullptrなら開始局面から）
    int anchorPly() const;                            // 分岐起点の手数
    QString anchorSfen() const;                       // 分岐起点の局面SFEN

    // 開始可否チェック
    static bool canStartFrom(KifuBranchNode* node);   // 終局手でないことを確認

    // 指し手追加
    void addMove(const ShogiMove& move, const QString& displayText,
                 const QString& sfen, const QString& elapsed);

    // 終局手追加（対局終了時）
    void addTerminalMove(TerminalType type, const QString& displayText,
                         const QString& elapsed);

    // 確定・破棄
    void commit();    // 現在のセッションをKifuBranchTreeに確定（新しい分岐として）
    void discard();   // 破棄（何も変更しない）

    // 一時データアクセス
    int moveCount() const;                            // 追加した手数
    int totalPly() const;                             // anchorPly + moveCount
    QVector<KifDisplayItem> moves() const;
    QString currentSfen() const;                      // 最新局面のSFEN

    // 確定時の分岐情報
    bool willCreateBranch() const;                    // 分岐を作成するか（途中からの場合true）
    QString newLineName() const;                      // 確定後の分岐名（"分岐N"）

signals:
    void sessionStarted(KifuBranchNode* branchPoint);
    void moveAdded(int ply, const QString& displayText);
    void terminalAdded(TerminalType type);
    void sessionCommitted(KifuBranchNode* newLineEnd);
    void sessionDiscarded();

private:
    bool m_active = false;
    bool m_hasTerminal = false;
    KifuBranchNode* m_branchPoint = nullptr;
    QVector<KifDisplayItem> m_moves;
    QVector<ShogiMove> m_gameMoves;
    QStringList m_sfens;
    KifuBranchTree* m_tree = nullptr;
};
```

**開始位置と結果の関係**:

| 開始位置 | `branchPoint()` | `willCreateBranch()` | 確定後の結果 |
|----------|-----------------|---------------------|--------------|
| 開始局面 | `nullptr` | `false` | 本譜を置換 |
| 本譜N手目 | N手目のノード | `true` | 新しい分岐を作成 |
| 分岐M手目 | M手目のノード | `true` | さらに新しい分岐を作成 |

---

## 4. 新しいコントローラー

### 4.1 KifuNavigationController（新規クラス）

全てのナビゲーション入力を統一的に処理する。

```cpp
// src/navigation/kifunavigationcontroller.h
class KifuNavigationController : public QObject {
    Q_OBJECT
public:
    KifuNavigationController(KifuBranchTree* tree,
                             KifuNavigationState* state,
                             QObject* parent = nullptr);

    // ナビゲーション操作（全ての入力元から呼び出し可能）
public slots:
    void goToFirst();                           // 開始局面へ
    void goToLast();                            // 現在ラインの最終手へ
    void goBack(int count = 1);                 // N手戻る
    void goForward(int count = 1);              // N手進む
    void goToNode(KifuBranchNode* node);        // 指定ノードへ
    void goToPly(int ply);                      // 現在ラインの指定手数へ
    void switchToLine(int lineIndex);           // 分岐ラインを切り替え
    void selectBranchCandidate(int candidateIndex);  // 分岐候補を選択

signals:
    // UI更新用シグナル
    void navigationCompleted(KifuBranchNode* newNode);
    void boardUpdateRequired(const QString& sfen);
    void recordHighlightRequired(int ply);
    void branchTreeHighlightRequired(int lineIndex, int ply);
    void branchCandidatesUpdateRequired(const QVector<KifuBranchNode*>& candidates);

private:
    KifuBranchTree* m_tree;
    KifuNavigationState* m_state;
};
```

### 4.2 KifuDisplayCoordinator（新規クラス）

UI更新を統括する。

```cpp
// src/ui/coordinators/kifudisplaycoordinator.h
class KifuDisplayCoordinator : public QObject {
    Q_OBJECT
public:
    KifuDisplayCoordinator(
        KifuBranchTree* tree,
        KifuNavigationState* state,
        KifuNavigationController* navController,
        QObject* parent = nullptr);

    // UI要素の設定
    void setRecordPane(RecordPane* pane);
    void setBranchTreeWidget(BranchTreeWidget* widget);

public slots:
    // KifuNavigationControllerからのシグナルを受けて各UIを更新
    void onNavigationCompleted(KifuBranchNode* node);
    void onBoardUpdateRequired(const QString& sfen);
    void onRecordHighlightRequired(int ply);
    void onBranchTreeHighlightRequired(int lineIndex, int ply);
    void onBranchCandidatesUpdateRequired(const QVector<KifuBranchNode*>& candidates);

    // ツリー変更時
    void onTreeChanged();

private:
    void updateRecordView();
    void updateBranchCandidatesView();
    void updateBranchTreeView();
    void highlightCurrentPosition();

    KifuBranchTree* m_tree;
    KifuNavigationState* m_state;
    KifuNavigationController* m_navController;
    RecordPane* m_recordPane = nullptr;
    BranchTreeWidget* m_branchTreeWidget = nullptr;
};
```

---

## 5. UI変更と表示仕様

### 5.0 UI維持の方針

**既存のUI外観とレイアウトは維持する。** 変更されるのは内部のアーキテクチャ（データの流れと接続方法）のみ。

| コンポーネント | ウィジェット | 変更内容 |
|---------------|-------------|----------|
| 分岐ツリー | `QGraphicsView` in `EngineAnalysisTab` | 描画ロジックを`BranchTreeWidget`に分離して委譲 |
| 棋譜欄 | `QTableView` in `RecordPane` | 既存の`KifuRecordListModel`を維持、データソースを新モデルに |
| 分岐候補欄 | `QTableView` in `RecordPane` | 既存の`KifuBranchListModel`を維持、データソースを新モデルに |
| 6つのナビゲーションボタン | `QPushButton` in `RecordPane` | 接続先を`KifuNavigationController`に変更 |
| 本譜に戻るボタン | `QPushButton` in `RecordPane` | 既存を流用 |

**維持される要素**:
- ノードの形状・色・配置
- テーブルの列構成
- ボタンのアイコンと配置
- ドックウィジェットのレイアウト

**変更される要素**:
- 内部のデータフロー
- シグナル/スロットの接続先
- 状態管理の方法

### 5.1 色定義

| 用途 | 色 | RGB/説明 |
|------|----|----|
| 現在位置ハイライト | 黄色 | `#FFFF00` または `Qt::yellow` |
| 分岐あり行の背景 | 薄いオレンジ | `#FFE4B5` (Moccasin) または `#FFDAB9` (PeachPuff) |
| 通常背景 | 白 | `#FFFFFF` |
| 選択行背景 | システムハイライト色 | `QPalette::Highlight` |

### 5.2 棋譜欄（RecordPane）の変更

```cpp
// 既存のRecordPaneを拡張
class RecordPane : public QWidget {
    // ...既存のメンバ...

public:
    // 現在のラインの表示情報を設定
    void setLineInfo(const QString& lineName, bool isMainLine);

    // 分岐マーク（+）のある手数を設定
    void setBranchMarkers(const QSet<int>& plysWithBranches);

    // 現在位置のハイライト（黄色）
    void setCurrentPlyHighlight(int ply);

signals:
    // 棋譜行クリック時
    void recordRowClicked(int ply);

    // 分岐候補クリック時
    void branchCandidateClicked(int candidateIndex);
};
```

**表示要件**:

| 項目 | 仕様 |
|------|------|
| ヘッダー | 現在のライン名を表示（"本譜" または "分岐N"） |
| 指し手の表示形式 | `"N ▲７六歩(77)"` (Nは手数) |
| 分岐のある手 | `"N ▲７六歩(77)+"` （末尾に`+`を付加、行背景を薄いオレンジに） |
| 現在位置 | 行背景を黄色にハイライト |
| 列構成 | 指し手、消費時間、コメント |

**分岐マークの表示例**:
```
=== 開始局面 ===         (1手 / 合計)
1 ▲７六歩(77)           00:01/00:00:01
2 △３四歩(33)           00:02/00:00:02
3 ▲２六歩(27)+          00:02/00:00:03    ← 薄いオレンジ背景、+マーク
4 △８四歩(83)           00:01/00:00:03    ← 現在位置なら黄色背景
5 ▲２五歩(26)           00:01/00:00:04
```

### 5.3 分岐ツリー（BranchTreeWidget）

現在の`EngineAnalysisTab`内の分岐ツリー描画ロジックを独立したウィジェットに**分離**する。
UIの外観は変更なし。

```cpp
// src/widgets/branchtreewidget.h
class BranchTreeWidget : public QWidget {
    Q_OBJECT
public:
    explicit BranchTreeWidget(QWidget* parent = nullptr);

    // データ設定
    void setTree(KifuBranchTree* tree);

    // ハイライト（黄色）
    void highlightNode(int lineIndex, int ply);
    void clearHighlight();

    // 表示設定
    void setClickEnabled(bool enabled);

signals:
    void nodeClicked(int lineIndex, int ply);

private:
    void rebuildVisualization();
    void drawLine(QPainter& painter, const KifuBranchTree::BranchLine& line);

    QGraphicsView* m_view;
    QGraphicsScene* m_scene;
    KifuBranchTree* m_tree = nullptr;

    // ノード位置管理
    struct NodeVisual {
        QGraphicsPathItem* item;
        int lineIndex;
        int ply;
        QBrush originalBrush;  // ハイライト解除時に戻すための元の色
    };
    QVector<NodeVisual> m_nodes;
    QHash<QPair<int,int>, QGraphicsPathItem*> m_nodeIndex;
    QGraphicsPathItem* m_highlightedNode = nullptr;  // 現在ハイライト中のノード
};
```

**描画仕様**:
```
開始局面 ─ 1手目 ─ 2手目 ─ 3手目 ─ 4手目 ─ 5手目 ─ 6手目    ← 本譜（row=0）
                    │
                    └─ 3手目' ─ 4手目' ─ 5手目'              ← 分岐1（row=1）
                            │
                            └─ 4手目'' ─ 5手目''             ← 分岐2（row=2）
```

| 項目 | 仕様 |
|------|------|
| 開始局面ノード | "開始局面" |
| 各ノード | "▲７六歩(77)" 形式 |
| 本譜 | 水平に配置（row=0） |
| 分岐 | 親ノードから垂直に分岐 |
| 現在位置ハイライト | **黄色背景** (`#FFFF00`) |
| クリック | nodeClickedシグナルを発行 |
| ノード間隔 | 水平: 110px, 垂直: 56px |

### 5.4 分岐候補欄（BranchCandidatesView）

既存の`KifuBranchListModel`を維持し、データソースを新しい`KifuBranchTree`に変更する。
UIの外観は変更なし。

```cpp
// 既存のKifuBranchListModelを拡張
class KifuBranchListModel : public AbstractListModel<KifuBranchDisplay> {
public:
    // 既存API維持
    // ...

    // 新規追加: KifuBranchTreeからデータを設定
    void setCandidatesFromTree(const QVector<KifuBranchNode*>& candidates);
    void setCurrentLineIndex(int lineIndex);  // 現在選択中のライン（黄色ハイライト用）

private:
    int m_currentLineIndex = 0;
};
```

**表示要件**:

| 項目 | 仕様 |
|------|------|
| 各行の形式 | `"[ライン名] 指し手"` (例: `"[本譜] ▲２六歩(27)"`) |
| 現在選択中のライン | **黄色背景** (`#FFFF00`) でハイライト |
| クリック | 該当ラインに切り替え |
| 空の場合 | 何も表示しない |

**分岐候補欄の表示例**（3手目で分岐がある場合）:
```
[本譜]   ▲２六歩(27)    ← 現在本譜を表示中なら黄色背景
[分岐1]  ▲１六歩(17)
[分岐2]  ▲５六歩(57)
```

### 5.5 3つのUIの連動

現在位置が変更されたとき、以下の3つのUIを同期的に更新する:

```
[ナビゲーション操作]
       │
       ▼
┌──────────────────────────────────────────────────────────┐
│  KifuDisplayCoordinator::highlightCurrentPosition()      │
│       │                                                  │
│       ├─→ 棋譜欄: 該当行を黄色ハイライト                │
│       │          分岐あり行は薄いオレンジ背景+マーク    │
│       │                                                  │
│       ├─→ 分岐ツリー: 該当ノードを黄色ハイライト        │
│       │                                                  │
│       └─→ 分岐候補欄: 現在ラインを黄色ハイライト        │
│                       (分岐がある手の場合のみ表示)       │
└──────────────────────────────────────────────────────────┘
```

---

## 6. データフロー

### 6.1 棋譜読み込み時

```
[ファイル読み込み]
     │
     ▼
[KifuParser] ─→ KifParseResult（生データ）
     │
     ▼
[KifuBranchTreeBuilder] ─→ KifuBranchTree（ツリー構造）
     │
     ▼
[KifuBranchTree::treeChanged シグナル]
     │
     ├─→ [KifuDisplayCoordinator::onTreeChanged]
     │        ├─→ 棋譜欄を更新
     │        ├─→ 分岐ツリーを再描画
     │        └─→ 分岐候補欄を更新
     │
     └─→ [KifuNavigationController]
              └─→ 開始局面に移動
```

### 6.2 ナビゲーション時

```
[入力元: ボタン/棋譜欄クリック/分岐ツリークリック/分岐候補クリック]
     │
     ▼
[KifuNavigationController::goToXxx]
     │
     ├─→ [KifuNavigationState::setCurrentNode]
     │        └─→ currentNodeChanged シグナル
     │
     └─→ [KifuNavigationController シグナル群]
              │
              ├─→ boardUpdateRequired ─→ 盤面更新
              ├─→ recordHighlightRequired ─→ 棋譜欄ハイライト
              ├─→ branchTreeHighlightRequired ─→ 分岐ツリーハイライト
              └─→ branchCandidatesUpdateRequired ─→ 分岐候補欄更新
```

### 6.3 対局開始時（途中局面から）

任意の局面から対局を開始できる。開始位置に応じて分岐が作成される。

#### 6.3.1 開始位置のパターン

| 開始位置 | 動作 |
|----------|------|
| 開始局面（ply=0） | 本譜を上書き、または新しい本譜として開始 |
| 本譜の途中（ply=N） | N手目から分岐を作成 |
| 分岐の途中 | その分岐からさらに分岐を作成 |
| 終局手の位置 | 対局開始不可（エラーまたは1手戻って開始） |

#### 6.3.2 データフロー

```
[対局開始リクエスト（現在位置から）]
     │
     ▼
[現在位置の確認]
     │
     ├─ 終局手の場合 → エラー or 1手戻る
     │
     └─ 通常の局面 →
            │
            ▼
[LiveGameSession::startFromNode(currentNode)]
     │
     ├─→ branchPoint = currentNode（分岐起点を記憶）
     │
     ├─→ sessionStarted シグナル
     │        └─→ UIを対局モードに切り替え
     │            ・ナビゲーションボタン無効化
     │            ・分岐ツリーのクリック無効化
     │            ・棋譜欄のクリック無効化
     │
     ▼
[指し手着手]
     │
     ▼
[LiveGameSession::addMove]
     │
     ├─→ 一時的な手リストに追加
     │
     └─→ moveAdded シグナル
              └─→ 棋譜欄に手を追加（ライブ対局行として表示）
```

#### 6.3.3 具体例

**例1: 本譜3手目から対局開始**
```
対局開始前:
  開始局面 ─ 1手目 ─ 2手目 ─ 3手目 ─ 4手目 ─ [△投了]  ← 本譜
                              ↑
                         現在位置（ここから対局開始）

対局中（2手指した後）:
  開始局面 ─ 1手目 ─ 2手目 ─ 3手目 ─ 4手目 ─ [△投了]  ← 本譜
                              │
                              └─ 4手目' ─ 5手目'        ← ライブ対局（未確定）

対局終了後:
  開始局面 ─ 1手目 ─ 2手目 ─ 3手目 ─ 4手目 ─ [△投了]  ← 本譜
                              │
                              └─ 4手目' ─ 5手目' ─ [▲投了]  ← 分岐1（確定）
```

**例2: 分岐1の途中から対局開始**
```
対局開始前:
  開始局面 ─ 1手目 ─ 2手目 ─ 3手目 ─ ...               ← 本譜
                              │
                              └─ 3手目' ─ 4手目' ─ ...  ← 分岐1
                                          ↑
                                     現在位置（ここから対局開始）

対局終了後:
  開始局面 ─ 1手目 ─ 2手目 ─ 3手目 ─ ...               ← 本譜
                              │
                              └─ 3手目' ─ 4手目' ─ ...  ← 分岐1
                                          │
                                          └─ 5手目'' ─ [△投了]  ← 分岐2（新規）
```

#### 6.3.4 棋譜欄の表示（ライブ対局中）

ライブ対局中は、分岐起点以降の手を表示する:

```
=== 3手目から対局開始 ===     ← ヘッダーに起点情報を表示
3 ▲２六歩(27)               ← 分岐起点（変更不可）
4 △８四歩(83)               ← ライブで追加した手
5 ▲２五歩(26)               ← ライブで追加した手
                             ← 対局中は黄色ハイライトが最新手に追従
```

#### 6.3.5 特殊な開始局面

駒落ちや任意局面からの対局開始にも対応:

| 開始局面 | 処理 |
|----------|------|
| 平手初期局面 | 通常の対局開始 |
| 駒落ち初期局面 | 開始SFENを駒落ち局面に設定 |
| 任意局面（局面編集後） | 開始SFENを編集後の局面に設定 |
| 棋譜途中の局面 | その局面から分岐を作成 |

```cpp
// KifuBranchTree
void setRootSfen(const QString& sfen);  // 開始局面を設定

// 駒落ちの例
tree->setRootSfen("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1");  // 平手
tree->setRootSfen("lnsgkgsn1/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");  // 香落ち
```

### 6.4 対局終了時

```
[対局終了]
     │
     ▼
[LiveGameSession::commit]
     │
     ├─→ KifuBranchTree にノード群を追加
     │        └─→ 新しい分岐ラインとして登録
     │
     └─→ sessionCommitted シグナル
              │
              ├─→ 棋譜欄: 新しい分岐ラインを反映
              └─→ 分岐ツリー: 新しいラインを描画
```

---

## 7. 6つのナビゲーションボタンの動作

| ボタン | アイコン | 操作 | 呼び出すメソッド |
|--------|----------|------|------------------|
| 最初へ | ▲\| | 開始局面へ移動 | `goToFirst()` |
| 10手戻る | ▲▲ | 現在位置から10手戻る | `goBack(10)` |
| 1手戻る | ▲ | 現在位置から1手戻る | `goBack(1)` |
| 1手進む | ▼ | 現在位置から1手進む | `goForward(1)` |
| 10手進む | ▼▼ | 現在位置から10手進む | `goForward(10)` |
| 最後へ | ▼\| | 現在ラインの最終手へ | `goToLast()` |

### 7.1 分岐時の動作詳細

**戻る操作 (`goBack`)**:
- 親ノードに戻る
- 分岐点を超えて本譜に戻ることも可能
- 最後に選択していたラインの情報は保持する

**進む操作 (`goForward`)**:
- 分岐がない場合: 次のノードへ
- 分岐がある場合: **最後に選択していたライン**の次のノードへ
  - 例: 分岐1を選択後に戻ってから進むと、分岐1の方向へ進む
  - 一度も分岐を選択していない場合は本譜を優先

### 7.2 本譜に戻るボタン

分岐候補欄に「本譜に戻る」ボタンを配置:

| 状態 | ボタンの動作 |
|------|-------------|
| 本譜を表示中 | ボタン非表示または無効化 |
| 分岐を表示中 | **同じ手数の本譜に移動** |

```cpp
// KifuNavigationController
void goToMainLineAtCurrentPly() {
    int currentPly = m_state->currentPly();
    KifuBranchNode* mainLineNode = m_tree->findByPlyOnMainLine(currentPly);
    if (mainLineNode) {
        goToNode(mainLineNode);
    }
}
```

### 7.3 ライン選択の記憶

```cpp
class KifuNavigationState {
    // ...
private:
    // 各分岐点で最後に選択したライン（分岐点のnodeId → 選択したラインのindex）
    QHash<int, int> m_lastSelectedLineAtBranch;

public:
    // 分岐点で選択を記憶
    void rememberLineSelection(KifuBranchNode* branchPoint, int lineIndex);

    // 分岐点での最後の選択を取得（未選択の場合は0=本譜）
    int lastSelectedLineAt(KifuBranchNode* branchPoint) const;
};
```

---

## 8. 実装計画

### Phase 1: データモデル構築
1. `KifuBranchNode` の実装
2. `KifuBranchTree` の実装
3. `KifuBranchTreeBuilder` の実装（既存パーサーからツリーを構築）
4. 単体テストの作成

### Phase 2: ナビゲーション
1. `KifuNavigationState` の実装
2. `KifuNavigationController` の実装
3. 6つのボタンの接続

### Phase 3: UI更新
1. `BranchTreeWidget` の新規作成
2. `BranchCandidatesModel` の実装
3. `KifuDisplayCoordinator` の実装
4. 既存UIとの接続

### Phase 4: ライブ対局対応
1. `LiveGameSession` の実装
2. 対局開始フローとの統合
3. 対局終了時のコミット処理

### Phase 5: 移行と統合
1. 既存の `KifuLoadCoordinator` からデータモデル関連を移行
2. 既存の `NavigationContextAdapter` を新しいコントローラーに置換
3. 既存の `EngineAnalysisTab` 内の分岐ツリー描画を `BranchTreeWidget` に置換
4. レグレッションテスト

---

## 9. ファイル構成

### 新規追加ファイル

```
src/
├── kifu/
│   ├── kifubranchnode.h/.cpp       # ノードクラス（新規）
│   ├── kifubranchtree.h/.cpp       # ツリークラス（新規）
│   ├── kifubranchtreebuilder.h/.cpp # ツリー構築（新規）
│   ├── kifunavigationstate.h/.cpp  # ナビゲーション状態（新規）
│   └── livegamesession.h/.cpp      # ライブ対局セッション（新規）
├── navigation/
│   └── kifunavigationcontroller.h/.cpp # ナビゲーションコントローラー（新規）
├── widgets/
│   └── branchtreewidget.h/.cpp     # 分岐ツリーウィジェット（EngineAnalysisTabから分離）
└── ui/coordinators/
    └── kifudisplaycoordinator.h/.cpp # 表示統括（新規）
```

### 既存ファイル（維持・修正）

```
src/
├── widgets/
│   ├── recordpane.h/.cpp           # 維持（シグナル接続変更のみ）
│   └── engineanalysistab.h/.cpp    # 維持（分岐ツリー描画をBranchTreeWidgetに委譲）
├── models/
│   ├── kifurecordlistmodel.h/.cpp  # 維持（データソース変更）
│   └── kifubranchlistmodel.h/.cpp  # 維持（データソース変更）
└── kifu/
    └── kifuloadcoordinator.h/.cpp  # 維持（分岐関連メソッドを削除）
```

---

## 10. 既存クラスへの影響

### 削除済み
- `KifuLoadCoordinator` の分岐関連メソッド（buildResolvedLinesAfterLoad等）✅
- `NavigationContextAdapter`（新しいコントローラーに置換）✅
- `NavigationController`（`KifuNavigationController`に統合）✅
- `RecordNavigationController`（削除）✅
- `m_resolvedRows`（MainWindow、KifuLoadCoordinator、GameRecordModelから削除）✅

### 維持（互換性のため）
- `ResolvedRow` 構造体（`KifuBranchTreeBuilder::fromResolvedRows`で使用）
- `LiveGameState` 構造体（KifuLoadCoordinator内で使用）
- `BranchCandidateDisplay` 関連（旧システムとの互換性維持）

### 変更（UIは維持、内部ロジックを変更）
- `MainWindow`: 新しいクラス群のインスタンス管理
- `RecordPane`: ライン情報表示の追加、シグナル接続先の変更
- `EngineAnalysisTab`: 分岐ツリー描画部分を `BranchTreeWidget` に委譲

### 維持（外観・APIを維持）
| クラス | 役割 | 変更点 |
|--------|------|--------|
| `KifuRecordListModel` | 棋譜欄の表示モデル | データソースを`KifuBranchTree`に変更 |
| `KifuBranchListModel` | 分岐候補欄の表示モデル | データソースを`KifuBranchTree`に変更 |
| `RecordPane` | 棋譜欄・分岐候補欄・ボタンのコンテナ | シグナル接続先を変更 |
| `NavigationController` | 6ボタンのイベントハンドラ | `KifuNavigationController`に統合または委譲 |
| 棋譜パーサー類 | ファイル読み込み | そのまま維持 |

### UIウィジェットの流用

```
既存                              新規
─────────────────────────────────────────────────────────
RecordPane                   →   RecordPane（そのまま）
├─ m_kifu (QTableView)       →   維持
├─ m_branch (QTableView)     →   維持
├─ m_btn1〜6 (QPushButton)   →   維持（接続先変更）
└─ backToMainButton          →   維持

EngineAnalysisTab            →   EngineAnalysisTab
└─ m_branchTree (QGraphicsView) → BranchTreeWidgetに委譲
   m_scene (QGraphicsScene)  →   BranchTreeWidget内部に移動
```

---

## 11. 追加検討事項

### 11.1 コメント編集
- `KifuBranchNode` がコメントを保持
- 編集時は `KifuBranchTree::setComment(nodeId, text)` で更新
- シグナル `commentChanged` を発行して UI を更新

### 11.2 棋譜保存
- `KifuBranchTree` から各種フォーマットへの変換
- `KifuExporter` クラスが `KifuBranchTree` を受け取って出力

### 11.3 局面検索・定跡
- `KifuBranchTree::findNodesBySfen(sfen)` で同一局面を検索
- 定跡ウィンドウとの連携

---

## 12. 終局手（ターミナルノード）の仕様

### 12.1 終局手の種類

| TerminalType | 日本語表記 | 説明 |
|--------------|-----------|------|
| `None` | - | 通常の指し手（終局手ではない） |
| `Resign` | 投了 | 手番側が投了 |
| `Checkmate` | 詰み | 詰みで終了 |
| `Repetition` | 千日手 | 同一局面4回で引き分け |
| `Impasse` | 持将棋 | 入玉宣言法による引き分け |
| `Timeout` | 切れ負け | 時間切れによる負け |
| `IllegalWin` | 反則勝ち | 相手の反則による勝ち |
| `IllegalLoss` | 反則負け | 自分の反則による負け |
| `Forfeit` | 不戦敗 | 不戦敗 |
| `Interrupt` | 中断 | 対局の中断 |
| `NoCheckmate` | 不詰 | 詰将棋で詰みなし |

### 12.2 終局手ノードの特性

```cpp
// 終局手ノードの特徴
struct TerminalNodeCharacteristics {
    // 1. 盤面は変化しない（SFENは直前の局面と同じ）
    // 2. ShogiMoveは無効値
    // 3. childrenは常に空（分岐を持てない）
    // 4. 棋譜欄では手数としてカウントしない
};
```

**重要なルール**:
- 終局手ノードは**子ノード（分岐）を持てない**
- 終局手の次に新しい手を追加することはできない
- ナビゲーションで終局手に到達したら、それがラインの終端

### 12.3 終局手の表示

**棋譜欄での表示**:
```
5 ▲２五歩(26)           00:01/00:00:04
6 △投了                 00:03/00:00:06    ← 終局手
```

- 手数は連番でカウント（例: 6手目）
- 手番記号（▲/△）は投了した側を示す
- 消費時間は終局手を指した（宣言した）時点の時間

**分岐ツリーでの表示**:
```
... ─ 5手目 ─ [△投了]    ← 終局手ノード（角丸四角形、他と同じ形状）
```

**分岐候補欄での表示**:
- 終局手がある位置では分岐候補を表示しない（終局手には分岐がないため）

### 12.4 終局手と分岐の関係

```
開始局面 ─ 1手目 ─ 2手目 ─ 3手目 ─ 4手目 ─ 5手目 ─ [△投了]    ← 本譜
                    │
                    └─ 3手目' ─ 4手目' ─ [▲投了]               ← 分岐1（別の終局）
```

- 各ラインは独立した終局手を持てる
- 終局手の手前の手には分岐を作成可能
- 終局手自体からの分岐は不可

### 12.5 ライブ対局での終局手追加

```cpp
class LiveGameSession {
public:
    // 終局手を追加してセッションを終了
    void addTerminalMove(TerminalType type, const QString& displayText,
                         const QString& elapsed);

    // 終局手追加後はaddMove()を呼べなくなる
    bool canAddMove() const { return m_active && !m_hasTerminal; }

private:
    bool m_hasTerminal = false;
};
```

### 12.6 終局手の判定関数

```cpp
// KifuBranchTree内
static TerminalType detectTerminalType(const QString& displayText) {
    if (displayText.contains(QStringLiteral("投了"))) return TerminalType::Resign;
    if (displayText.contains(QStringLiteral("詰み"))) return TerminalType::Checkmate;
    if (displayText.contains(QStringLiteral("千日手"))) return TerminalType::Repetition;
    if (displayText.contains(QStringLiteral("持将棋"))) return TerminalType::Impasse;
    if (displayText.contains(QStringLiteral("切れ負け"))) return TerminalType::Timeout;
    if (displayText.contains(QStringLiteral("反則勝ち"))) return TerminalType::IllegalWin;
    if (displayText.contains(QStringLiteral("反則負け"))) return TerminalType::IllegalLoss;
    if (displayText.contains(QStringLiteral("不戦敗"))) return TerminalType::Forfeit;
    if (displayText.contains(QStringLiteral("中断"))) return TerminalType::Interrupt;
    if (displayText.contains(QStringLiteral("不詰"))) return TerminalType::NoCheckmate;
    return TerminalType::None;
}
```

---

## 13. 決定事項

以下の方針で実装する:

| 項目 | 決定 | 詳細 |
|------|------|------|
| 分岐の自動選択 | **(B) 最後に選択していたラインを優先** | 進むボタンを押した時、分岐がある場合は最後に選択していたラインを優先する。未選択の場合は本譜を優先。 |
| 分岐の命名 | **(A) 自動命名** | "本譜", "分岐1", "分岐2"... と自動で命名する。ユーザーによる命名機能は実装しない。 |
| 分岐の削除 | **(A) 不要** | 読み込んだ分岐は保持する。削除機能は実装しない。 |
| 本譜への戻り | **(A) 同じ手数の本譜に移動** | 分岐を見ている時に「本譜に戻る」ボタンを押すと、現在の手数と同じ手数の本譜に移動する。 |

---

## 14. 表示仕様まとめ

### 14.1 棋譜欄の表示

```
列: [指し手] [消費時間] [コメント]

=== 開始局面 ===         (1手 / 合計)
1 ▲７六歩(77)           00:01/00:00:01
2 △３四歩(33)           00:02/00:00:02
3 ▲２六歩(27)+          00:02/00:00:03    ← 分岐あり: 薄いオレンジ背景 + "+"マーク
4 △８四歩(83)           00:01/00:00:03    ← 現在位置: 黄色背景
5 ▲２五歩(26)           00:01/00:00:04
6 △投了                 00:03/00:00:06
```

### 14.2 分岐ツリーの表示

```
┌────────┐   ┌──────────────┐   ┌──────────────┐   ┌──────────────┐
│開始局面│───│▲７六歩(77)  │───│△３四歩(33)  │───│▲２六歩(27)  │─── ...
└────────┘   └──────────────┘   └──────────────┘   └──────┬───────┘
                                                         │
                                          ┌──────────────┴───────────────┐
                                          │▲１六歩(17)  │───│△９四歩(93)│─── ...  分岐1
                                          └──────────────┘   └──────────────┘
```

- 現在位置のノード: **黄色背景**

### 14.3 分岐候補欄の表示

分岐がある手数にいる場合のみ表示:

```
┌─────────────────────────────┐
│ [本譜]   ▲２六歩(27)       │ ← 現在のライン: 黄色背景
│ [分岐1]  ▲１六歩(17)       │
│ [分岐2]  ▲５六歩(57)       │
├─────────────────────────────┤
│ [本譜に戻る]                │ ← 分岐表示中のみ有効
└─────────────────────────────┘
```

---

*作成日: 2026-01-30*
*更新日: 2026-02-01*
*対象バージョン: ShogiBoardQ (Qt6/C++17)*
