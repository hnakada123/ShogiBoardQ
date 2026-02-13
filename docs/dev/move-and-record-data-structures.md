# 指し手データ・棋譜データ構造

ShogiBoardQ の「指し手データ」と「棋譜データ」のソースコード上の構造をまとめたドキュメント。
棋譜欄・分岐候補欄・分岐ツリー欄・将棋盤が連動して動作する仕組みの根幹となるデータモデルを解説する。

---

## 1. 指し手データ（ShogiMove）

**ファイル**: `src/core/shogimove.h` / `src/core/shogimove.cpp`

### 1.1 構造体定義

```cpp
struct ShogiMove {
    QPoint fromSquare{0, 0};                    // 移動元の座標
    QPoint toSquare{0, 0};                      // 移動先の座標
    QChar  movingPiece = QLatin1Char(' ');       // 動かした駒（SFEN表記）
    QChar  capturedPiece = QLatin1Char(' ');     // 取った駒（なければ ' '）
    bool   isPromotion = false;                  // 成りフラグ
};
```

### 1.2 座標系

座標は `QPoint(x, y)` で表現される。`x` が筋（file）、`y` が段（rank）に対応する。

| 範囲 | 意味 |
|---|---|
| `x: 0〜8, y: 0〜8` | 盤上の座標（0-indexed）。将棋の「9一」は `(0, 0)`、「1九」は `(8, 8)` |
| `x: 9` | 先手の駒台（持ち駒からの打ち） |
| `x: 10` | 後手の駒台（持ち駒からの打ち） |

**盤面座標と将棋筋段の対応**:

```
将棋の筋:  ９ ８ ７ ６ ５ ４ ３ ２ １
x (0-idx):  0  1  2  3  4  5  6  7  8

将棋の段:  一 二 三 四 五 六 七 八 九
y (0-idx):  0  1  2  3  4  5  6  7  8
```

表示時には1-indexed（`x + 1`, `y + 1`）に変換してデバッグ出力される。

### 1.3 駒の文字表現（SFEN記法）

`movingPiece` と `capturedPiece` は SFEN 記法の1文字で駒種を表す。
**大文字 = 先手の駒、小文字 = 後手の駒**。

| 駒種 | 先手（生駒） | 先手（成駒） | 後手（生駒） | 後手（成駒） |
|---|---|---|---|---|
| 玉 / 王 | `K` | — | `k` | — |
| 飛車 / 龍 | `R` | `U` | `r` | `u` |
| 角行 / 馬 | `B` | `C` | `b` | `c` |
| 金将 | `G` | — | `g` | — |
| 銀将 / 成銀 | `S` | `T` | `s` | `t` |
| 桂馬 / 成桂 | `N` | `O` | `n` | `o` |
| 香車 / 成香 | `L` | `M` | `l` | `m` |
| 歩兵 / と金 | `P` | `Q` | `p` | `q` |

> **注意**: SFEN標準では成駒は `+R`, `+B` 等と表記するが、ShogiBoardQ の内部表現では
> 独自の1文字マッピング（`R→U`, `B→C` 等）を使用する。

### 1.4 データ例

#### 通常の移動: ▲７六歩（77→76）

```
fromSquare:   QPoint(2, 6)   // ７七 = (9-7, 7-1) = (2, 6)
toSquare:     QPoint(2, 5)   // ７六 = (9-7, 6-1) = (2, 5)
movingPiece:  'P'            // 先手の歩
capturedPiece: ' '           // 取った駒なし
isPromotion:  false
```

#### 駒取り: ▲２二角成（88→22で角が銀を取って成る）

```
fromSquare:   QPoint(1, 7)   // ８八 = (9-8, 8-1) = (1, 7)
toSquare:     QPoint(7, 1)   // ２二 = (9-2, 2-1) = (7, 1)
movingPiece:  'B'            // 先手の角
capturedPiece: 's'           // 後手の銀を取った
isPromotion:  true           // 成り
```

#### 駒打ち: △４五桂（後手が持ち駒の桂馬を打つ）

```
fromSquare:   QPoint(10, 0)  // x=10 = 後手の駒台
toSquare:     QPoint(4, 4)   // ４五 = (9-4, 5-1) = (5, 4) ※実際は(4, 4)
movingPiece:  'n'            // 後手の桂馬
capturedPiece: ' '           // 取った駒なし（打ちなので）
isPromotion:  false
```

> `fromSquare.y()` の値は、駒台からの打ちの場合は駒種の識別には使われない。

---

## 2. 棋譜ツリーデータ（KifuBranchTree / KifuBranchNode）

### 2.1 KifuBranchNode（ノード）

**ファイル**: `src/kifu/kifubranchnode.h`

棋譜の1手（または開始局面）を表すノード。N 分木の各頂点。

```cpp
class KifuBranchNode {
    int          m_nodeId;         // ノードID（ツリー内で一意、1から連番）
    int          m_ply;            // 手数（0=開始局面、1以降=指し手）
    QString      m_displayText;    // 表示テキスト（例: "▲７六歩(77)"）
    QString      m_sfen;           // この局面のSFEN文字列
    ShogiMove    m_move;           // 指し手データ（ply=0や終局手では無効）
    QString      m_comment;        // コメント
    QString      m_bookmark;       // しおり
    QString      m_timeText;       // 消費時間テキスト（例: "( 0:10/00:00:10)"）
    TerminalType m_terminalType;   // 終局手の種類（通常はNone）

    KifuBranchNode*          m_parent;    // 親ノード（nullptr=ルート）
    QVector<KifuBranchNode*> m_children;  // 子ノードリスト
};
```

**主要な判定メソッド**:

| メソッド | 意味 |
|---|---|
| `isMainLine()` | 本譜かどうか（親の最初の子 `children[0]`） |
| `isTerminal()` | 終局手かどうか（`terminalType != None`） |
| `isActualMove()` | 盤面を変化させる指し手か（終局手でなく ply>0） |
| `hasBranch()` | 分岐があるか（子が2つ以上） |
| `isRoot()` | ルートノードか（`parent == nullptr`） |

### 2.2 TerminalType（終局手の種類）

```cpp
enum class TerminalType {
    None,           // 通常の指し手
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
```

### 2.3 KifuBranchTree（ツリー）

**ファイル**: `src/kifu/kifubranchtree.h`

N 分木全体を管理するデータモデル。ルートノードからの木構造と、`nodeId → ノード` の逆引きハッシュを保持する。

```cpp
class KifuBranchTree : public QObject {
    KifuBranchNode*               m_root;       // ルートノード
    QHash<int, KifuBranchNode*>   m_nodeById;   // nodeId → ノード の検索用
    int                           m_nextNodeId;  // 次に振るID（1始まり連番）

    // allLines() キャッシュ
    mutable QVector<BranchLine>   m_linesCache;      // DFS走査結果のキャッシュ
    mutable bool                  m_linesCacheDirty;  // キャッシュ無効化フラグ
};
```

**allLines() キャッシュ**: `allLines()` はツリー全体をDFS走査して全ラインを構築するが、
結果をキャッシュし、ツリー構造が変更されるまで再利用する。キャッシュは以下の操作で無効化される:
`addMove()`, `addTerminalMove()`, `addMoveQuiet()`, `clear()`。
コメント変更（`setComment()`）ではキャッシュは無効化されない（`BranchLine::nodes` はポインタを保持しており、ノードデータの変更はポインタ経由で自動的に反映される）。

**本譜と分岐の規約**:
- **本譜（mainline）**: 各ノードの `children[0]`（最初の子）を辿った経路
- **分岐**: `children[1]` 以降の子が分岐手。同じ手数で異なる指し手を表す

### 2.4 BranchLine（ライン情報）

```cpp
struct BranchLine {
    int lineIndex;                      // 0=本譜、1以降=分岐
    QString name;                       // "本譜", "分岐1", "分岐2"...
    QVector<KifuBranchNode*> nodes;     // ノード列（ルートから終端まで）
    int branchPly;                      // 分岐開始手数（本譜は0）
    KifuBranchNode* branchPoint;        // 分岐点のノード（本譜はnullptr）
};
```

### 2.5 データ例: 分岐を含むツリー構造

以下の棋譜を例にする:
- 本譜: ▲７六歩 → △３四歩 → ▲２六歩 → 中断
- 分岐: 3手目で ▲６六歩（▲２六歩の代わり）→ 中断

```
                     [Root: 開始局面]              nodeId=1, ply=0
                           |
                    [▲７六歩(77)]                nodeId=2, ply=1
                           |
                    [△３四歩(34)]                nodeId=3, ply=2
                      /          \
            [▲２六歩(26)]    [▲６六歩(66)]      nodeId=4,5, ply=3
                  |                 |
              [中断]            [中断]            nodeId=6,7
```

**ツリー内部データ**:

| nodeId | ply | displayText | parent | children | isMainLine |
|---|---|---|---|---|---|
| 1 | 0 | "開始局面" | null | [2] | — |
| 2 | 1 | "▲７六歩(77)" | 1 | [3] | true |
| 3 | 2 | "△３四歩(34)" | 2 | [4, 5] | true |
| 4 | 3 | "▲２六歩(26)" | 3 | [6] | true（children[0]）|
| 5 | 3 | "▲６六歩(66)" | 3 | [7] | false（children[1]）|
| 6 | — | "中断" | 4 | [] | — |
| 7 | — | "中断" | 5 | [] | — |

**allLines() の結果**:

| lineIndex | name | branchPly | nodes |
|---|---|---|---|
| 0 | "本譜" | 0 | [1, 2, 3, 4, 6] |
| 1 | "分岐1" | 3 | [1, 2, 3, 5, 7] |

分岐点（ply=2, nodeId=3）より前のノードは両ラインで共有される。

---

## 3. 表示用データ（KifDisplayItem / KifuDisplay）

### 3.1 KifDisplayItem（表示用構造体）

**ファイル**: `src/kifu/kifdisplayitem.h`

棋譜欄の1行に対応する軽量な表示データ。値コピーで使われる。

```cpp
struct KifDisplayItem {
    QString prettyMove;   // 表示用テキスト（例: "▲７六歩"）
    QString timeText;     // 消費時間テキスト
    QString comment;      // コメント
    QString bookmark;     // しおり
    int     ply = 0;      // 手数（0始まり）
};
```

`Q_DECLARE_METATYPE` により、シグナル/スロット引数として `QList<KifDisplayItem>` を受け渡し可能。

### 3.2 KifuDisplay（Qt モデル用オブジェクト）

**ファイル**: `src/widgets/kifudisplay.h`

`KifuRecordListModel` のアイテムとして使われる `QObject` 派生クラス。棋譜欄の各行表示データを保持する。

```cpp
class KifuDisplay : public QObject {
    QString m_currentMove;   // 指し手テキスト
    QString m_timeSpent;     // 消費時間
    QString m_comment;       // コメント
    QString m_bookmark;      // しおり
};
```

### 3.3 KifuRecordListModel（棋譜欄モデル）

**ファイル**: `src/models/kifurecordlistmodel.h`

棋譜欄（QTableView）のデータを管理する Qt モデル。

```cpp
class KifuRecordListModel : public AbstractListModel<KifuDisplay> {
    QSet<int> m_branchPlySet;        // 分岐のある手数の集合（行番号太字表示用）
    int       m_currentHighlightRow; // 現在ハイライト行（黄色背景）
};
```

- **行番号**: 行0 = 開始局面、行1 = 1手目、...
- `setBranchPlyMarks()`: 分岐のある手数をマークし、棋譜欄で太字表示する
- `setCurrentHighlightRow()`: 現在の棋譜位置を黄色ハイライトする

### 3.4 KifuBranchListModel（分岐候補欄モデル）

**ファイル**: `src/models/kifubranchlistmodel.h`

分岐候補欄（QTableView）のデータを管理する Qt モデル。
現在位置に分岐がある場合、選択可能な分岐手の一覧を表示する。

```cpp
class KifuBranchListModel : public AbstractListModel<KifuBranchDisplay> {
    QVector<RowItem> m_rows;           // 表示行データ
    bool   m_hasBackToMainRow;         // 「本譜へ戻る」行の有無
    bool   m_locked;                   // ロック状態（検討モード中等）
    int    m_currentHighlightRow;      // 現在ハイライト行
};
```

- 先頭行が「本譜へ戻る」（任意）、以降が分岐候補手
- ロック機能: 検討モード中に外部からの更新を防止

---

## 4. ナビゲーション状態（KifuNavigationState）

**ファイル**: `src/kifu/kifunavigationstate.h`

棋譜ナビゲーションの現在位置と分岐選択の記憶を一元管理する。

```cpp
class KifuNavigationState : public QObject {
    KifuBranchTree*  m_tree;                       // 参照するツリー
    KifuBranchNode*  m_currentNode;                // 現在のノード
    QHash<int, int>  m_lastSelectedLineAtBranch;   // 分岐点nodeId → 選択ラインindex
    int              m_preferredLineIndex = -1;     // 優先ライン（-1=未設定）
};
```

### 4.1 主要な状態

| プロパティ | 説明 |
|---|---|
| `currentNode()` | 現在位置のノード |
| `currentPly()` | 現在の手数 |
| `currentLineIndex()` | 現在のラインインデックス（0=本譜） |
| `isOnMainLine()` | 本譜上にいるか |
| `canGoForward()` | 前進可能か |
| `canGoBack()` | 後退可能か |

### 4.2 分岐選択の記憶

ユーザーが分岐を選択すると、その選択が記憶される:

- `rememberLineSelection(branchPoint, lineIndex)`: 分岐点での選択を記録
- `lastSelectedLineAt(branchPoint)`: 記録された選択を取得（未選択は0=本譜）
- `preferredLineIndex`: 分岐選択時に設定され、分岐点より前に戻っても維持される

これにより、分岐を選んだ状態で開始局面まで戻り、再度進めた場合に同じ分岐を自動的に辿ることができる。

### 4.3 シグナル

```cpp
signals:
    void currentNodeChanged(KifuBranchNode* newNode, KifuBranchNode* oldNode);
    void lineChanged(int newLineIndex, int oldLineIndex);
```

---

## 5. 中央管理（GameRecordModel）

**ファイル**: `src/kifu/gamerecordmodel.h`

コメント・しおりの一元管理と、棋譜の各種形式への出力を担当する。

### 5.1 内部データ

```cpp
class GameRecordModel : public QObject {
    // === 権威を持つ内部データ ===
    QVector<QString> m_comments;    // 手数index → コメント
    QVector<QString> m_bookmarks;   // 手数index → しおり
    bool             m_isDirty;     // 変更フラグ

    // === 外部データへの参照（同期更新用） ===
    QList<KifDisplayItem>*  m_liveDisp;    // 表示リスト
    KifuBranchTree*         m_branchTree;  // ツリー
    KifuNavigationState*    m_navState;    // ナビゲーション状態
};
```

### 5.2 3層同期パターン

コメントやしおりを編集すると、以下の3箇所が同期更新される:

```
setComment(ply, text)
    │
    ├─→ m_comments[ply] = text          // ① 内部配列（Single Source of Truth）
    ├─→ m_liveDisp[ply].comment = text  // ② 表示用リスト
    └─→ m_branchTree.setComment(...)    // ③ ツリーノード
```

| 層 | 保持場所 | 役割 |
|---|---|---|
| ① 内部配列 | `m_comments` / `m_bookmarks` | 権威データ。出力時の根拠 |
| ② 表示リスト | `m_liveDisp` (外部参照) | 棋譜欄モデルへの即時反映 |
| ③ ツリーノード | `KifuBranchTree` → `KifuBranchNode` | ツリー表示・ファイル出力用 |

### 5.3 出力形式

GameRecordModel は以下の形式へのエクスポートメソッドを提供する:

| メソッド | 形式 | 分岐対応 |
|---|---|---|
| `toKifLines()` | KIF形式 | あり |
| `toKi2Lines()` | KI2形式 | あり |
| `toCsaLines()` | CSA形式 | なし（本譜のみ） |
| `toJkfLines()` | JKF（JSON）形式 | あり |
| `toUsenLines()` | USEN形式 | あり |
| `toUsiLines()` | USI position コマンド形式 | なし（本譜のみ） |

---

## 6. GUI連動の全体像

### 6.1 4つのGUI部品

| GUI部品 | ウィジェット | モデル | 説明 |
|---|---|---|---|
| **棋譜欄** | `RecordPane` 内の `QTableView` | `KifuRecordListModel` | 手順リスト。クリックで手に移動 |
| **分岐候補欄** | `RecordPane` 内の `QTableView` | `KifuBranchListModel` | 現在位置の分岐候補 |
| **分岐ツリー欄** | `BranchTreeWidget` | — (直接描画) | ツリーのグラフィカル表示 |
| **将棋盤** | `ShogiView` | `ShogiBoard` | 盤面表示 |

### 6.2 Signal/Slot 接続図

```
┌────────────────────────────────────────────────────────────────────┐
│                    KifuNavigationController                        │
│  (goForward/goBack/goToNode/selectBranchCandidate/...)            │
│                                                                    │
│  emit: navigationCompleted(node)                                   │
│  emit: boardUpdateRequired(sfen)                                   │
│  emit: recordHighlightRequired(ply)                                │
│  emit: branchTreeHighlightRequired(lineIndex, ply)                 │
│  emit: branchCandidatesUpdateRequired(candidates)                  │
└───────────────────────┬────────────────────────────────────────────┘
                        │ connect (wireSignals)
                        ▼
┌────────────────────────────────────────────────────────────────────┐
│                    KifuDisplayCoordinator                          │
│                                                                    │
│  onNavigationCompleted(node)                                       │
│    ├─→ 棋譜欄の内容更新 (populateRecordModel)                     │
│    ├─→ 分岐マーク更新 (populateBranchMarks)                       │
│    └─→ 分岐候補欄更新 (updateBranchCandidatesView)                │
│                                                                    │
│  onBoardUpdateRequired(sfen)                                       │
│    └─→ emit boardSfenChanged(sfen) ──→ 将棋盤更新                 │
│                                                                    │
│  onRecordHighlightRequired(ply)                                    │
│    └─→ 棋譜欄ハイライト行変更                                     │
│                                                                    │
│  onBranchTreeHighlightRequired(lineIndex, ply)                     │
│    └─→ BranchTreeWidget::highlightNode()                           │
│                                                                    │
│  onBranchCandidatesUpdateRequired(candidates)                      │
│    └─→ KifuBranchListModel 更新                                   │
└────────────────────────────────────────────────────────────────────┘
        ▲                  ▲                    ▲
        │                  │                    │
   棋譜欄クリック    分岐候補クリック    分岐ツリークリック
   RecordPane::       RecordPane::        BranchTreeWidget::
   mainRowChanged     branchActivated     nodeClicked
```

### 6.3 ナビゲーション操作の流れ

#### 6.3.1 ナビゲーションボタン（例: 1手進む）

```
1. ユーザーが「→」ボタンをクリック
2. QPushButton::clicked → KifuNavigationController::onNextClicked()
3. KifuNavigationController::goForward(1)
   a. KifuNavigationState から次の子ノードを取得
      （分岐記憶がある場合はそちらを優先）
   b. state->setCurrentNode(nextNode)
   c. emitUpdateSignals() で5つのシグナルを一括発行
4. KifuDisplayCoordinator が各シグナルを受信
   a. 棋譜欄のハイライト行を更新
   b. 将棋盤のSFENを更新（boardSfenChanged → ShogiBoard::updateBoard）
   c. 分岐ツリーのハイライトノードを更新
   d. 分岐候補欄を現在位置の候補で更新
```

#### 6.3.2 棋譜欄の指し手をクリック

棋譜欄は RecordPane 内の QTableView で、ナビゲーションボタン経由とは異なるパスを通る。

```
1. ユーザーが棋譜欄の行をクリック
2. QItemSelectionModel::currentRowChanged
     → RecordPane::onKifuCurrentRowChanged(row)
     → emit RecordPane::mainRowChanged(row)
3. RecordPaneWiring により MainWindow::onRecordPaneMainRowChanged(row) に接続
4. MainWindow → RecordNavigationHandler::onMainRowChanged(row)
   a. 分岐ライン上の場合:
      - KifuBranchTree から該当ラインの SFEN を取得
      - emit branchBoardSyncRequired(currentSfen, prevSfen)
        → 将棋盤を更新（ハイライト付き）
   b. 本譜の場合:
      - emit boardSyncRequired(row)
        → sfenRecord[row] の SFEN で将棋盤を更新
   c. 手数トラッキング変数を更新（activePly, currentSelectedPly 等）
   d. 棋譜欄のハイライト行を更新
   e. 手番表示・評価値グラフの縦線を更新
   f. KifuDisplayCoordinator::onPositionChanged(lineIndex, ply, sfen)
      - 分岐ツリーのハイライトを更新
      - 分岐候補欄を更新
      - 分岐選択記憶を更新
```

> **ボタン経由との違い**: ボタンは KifuNavigationController が全 UI 更新シグナルを一括発行するが、
> 棋譜欄クリックは RecordNavigationHandler がメインの処理を行い、
> KifuDisplayCoordinator には `onPositionChanged()` で追加の更新（分岐ツリー・分岐候補欄）を通知する。

#### 6.3.3 分岐ツリー欄のノードをクリック

分岐ツリーは **2つの独立したウィジェット** で描画され、それぞれ別のクリック処理経路を持つ。

| ウィジェット | シグナル | ハンドラ |
|---|---|---|
| BranchTreeWidget（独立ウィジェット） | `nodeClicked(lineIndex, ply)` | `KifuDisplayCoordinator::onBranchTreeNodeClicked` |
| EngineAnalysisTab 内の分岐ツリー | `branchNodeActivated(row, ply)` | `MainWindow::onBranchNodeActivated` → `KifuNavigationController::handleBranchNodeActivated` |

**経路A: BranchTreeWidget（独立ウィジェット）**

```
1. ユーザーが分岐ツリーのノード（長方形）をクリック
2. BranchTreeWidget::eventFilter が MouseButtonPress を検出
   a. クリック位置の QGraphicsPathItem を特定
   b. ノードに埋め込まれた ROLE_LINE_INDEX, ROLE_PLY を取得
   c. emit BranchTreeWidget::nodeClicked(lineIndex, ply)
3. KifuDisplayCoordinator::onBranchTreeNodeClicked(lineIndex, ply)
   a. preferredLineIndex を lineIndex に設定
   b. KifuBranchTree::allLines() からラインを取得
   c. ライン内のノード列から ply が一致するノードを検索
   d. KifuNavigationController::goToNode(node) を呼び出し
4. KifuNavigationController::goToNode(node)
   a. ルートからノードまでの全分岐点で選択を記憶
      （while ループで親を辿り、rememberLineSelection）
   b. state->setCurrentNode(node)
   c. emitUpdateSignals() で5つのシグナルを一括発行
5. KifuDisplayCoordinator が各シグナルを受信（6.3.1 の手順4と同じ）
   - 棋譜欄・将棋盤・分岐ツリー・分岐候補欄を更新
```

**経路B: EngineAnalysisTab 内の分岐ツリー**

```
1. ユーザーがエンジン解析タブ内の分岐ツリーノードをクリック
2. EngineAnalysisTab::eventFilter が MouseButtonPress を検出
   a. クリック位置の QGraphicsPathItem を特定
   b. ノードに埋め込まれた ROLE_ROW, ROLE_PLY を取得
   c. emit EngineAnalysisTab::branchNodeActivated(row, ply)
3. MainWindow::onBranchNodeActivated(row, ply)
   → KifuNavigationController::handleBranchNodeActivated(row, ply)
   a. 本譜行（row==0）で分岐前の共有ノードをクリックした場合:
      現在の分岐ライン上にとどまる（effectiveRow = currentLine）
   b. 分岐行（row > 0）をクリックした場合:
      そのラインに切り替える（effectiveRow = row）
   c. preferredLineIndex を effectiveRow に設定
   d. ライン内のノード列から ply が一致するノードを検索
   e. goToNode(node) を呼び出し → 以降は経路Aの手順4-5と同じ
```

> **共有ノードの判定**: 本譜行（row==0）のクリックでは、ply が現在ラインの branchPly より前の場合、
> 分岐前の共有ノードと見なして現在ラインに留まる。分岐行（row > 0）の明示的なクリックでは
> 常にそのラインに切り替わる。

> **分岐選択記憶の自動更新**: `goToNode()` はノードから親を辿り、
> 経路上の全分岐点で `rememberLineSelection()` を呼ぶ。
> これにより、分岐ツリーで深い分岐のノードをクリックした後に
> 開始局面まで戻って再度進めると、同じ分岐パスが自動的に辿られる。

#### 6.3.4 分岐候補欄の指し手をクリック

分岐候補欄は RecordPane 内の QTableView で、現在位置に分岐がある場合に選択可能な手の一覧が表示される。

```
1. ユーザーが分岐候補欄の行をクリック
2. QTableView::clicked または QTableView::activated
     → RecordPane::branchActivated(index)
3. KifuDisplayCoordinator::onBranchCandidateActivated(index)
   a. 「本譜へ戻る」行の場合:
      → KifuNavigationController::goToMainLineAtCurrentPly()
        - 優先ラインをリセット、分岐記憶をクリア
        - 本譜の同じ手数のノードに移動
   b. 通常の分岐候補の場合:
      → KifuNavigationController::selectBranchCandidate(row)
        i.   KifuNavigationState::branchCandidatesAtCurrent() で候補を取得
        ii.  候補ノードの lineIndex を取得
        iii. 分岐の場合: state->setPreferredLineIndex(lineIndex)
             本譜の場合: state->resetPreferredLineIndex()
        iv.  emit lineSelectionChanged(lineIndex) — ライン変更を通知
        v.   goToNode(candidate) — ノードに移動し全 UI 更新
4. KifuDisplayCoordinator が各シグナルを受信（6.3.1 の手順4と同じ）
   - ラインが変更された場合は棋譜欄の内容を全更新
   - 棋譜欄・将棋盤・分岐ツリー・分岐候補欄を更新
```

> **優先ライン（preferredLineIndex）**: 分岐候補をクリックすると `preferredLineIndex` が設定される。
> これにより、分岐選択後に分岐点より前に戻っても `currentLineIndex()` が選択した分岐のインデックスを返し続ける。
> 棋譜欄に表示されるラインが維持され、ユーザーは分岐ラインの中をそのままナビゲーションできる。

### 6.4 KifuDisplayCoordinator の役割

`KifuDisplayCoordinator`（`src/ui/coordinators/kifudisplaycoordinator.h`）は、
ナビゲーションコントローラからのシグナルと各 UI コンポーネントの間を仲介する統合管理クラス。

**管理対象**:

| 設定メソッド | 対象 |
|---|---|
| `setRecordPane()` | 棋譜ペイン |
| `setBranchTreeWidget()` | 分岐ツリーウィジェット |
| `setRecordModel()` | 棋譜欄モデル |
| `setBranchModel()` | 分岐候補モデル |
| `setAnalysisTab()` | エンジン解析タブ |
| `setLiveGameSession()` | ライブ対局セッション |

**ライン変更検出**: `m_lastLineIndex` と `m_lastModelLineIndex` で、
ラインが変わった場合のみ棋譜欄の全更新（`populateRecordModel`）を行い、
同一ライン内の移動ではハイライト更新のみに留める最適化がされている。

### 6.5 データ一元管理の保証

4つのGUI部品（棋譜欄・分岐候補欄・分岐ツリー欄・将棋盤）は、すべて **KifuBranchTree** を中央データストアとして共有している。各GUI部品がどのようにデータを取得し、一元性がどう担保されているかを以下にまとめる。

#### 6.5.1 中央データストア

| データストア | 管理対象 | 役割 |
|---|---|---|
| **KifuBranchTree** | ツリー構造・指し手・SFEN・コメント・しおり・消費時間 | 棋譜データの正本（Single Source of Truth） |
| **GameRecordModel** | コメント・しおり（3層同期） | 編集操作の受付とツリーへの同期（セクション5.2参照） |
| **KifuNavigationState** | 現在位置・分岐選択記憶 | どのラインのどの手にいるかの状態管理 |

#### 6.5.2 各GUI部品のデータ取得経路

```
                 KifuBranchTree（正本）
                  │
                  ├──→ 棋譜欄 (KifuRecordListModel)
                  │      KifuDisplayCoordinator::populateRecordModel() が
                  │      allLines()[lineIndex].nodes からノード列を取得し、
                  │      各ノードの displayText / comment / bookmark / timeText を読み出して
                  │      KifuRecordListModel の行として再構築する。
                  │
                  ├──→ 分岐候補欄 (KifuBranchListModel)
                  │      KifuDisplayCoordinator::updateBranchCandidatesView() が
                  │      現在ノードの兄弟ノード（parent->children()）から
                  │      候補テキストを抽出して設定する。
                  │
                  ├──→ 分岐ツリー欄 (BranchTreeWidget)
                  │      treeChanged シグナルで allLines() を再取得し、
                  │      ノード情報からグラフィカルなツリーを再描画する。
                  │
                  └──→ 将棋盤 (ShogiBoard)
                         ナビゲーション時に KifuNavigationController が
                         currentNode()->sfen() を取得し、
                         boardUpdateRequired シグナルで将棋盤に渡す。
```

#### 6.5.3 一元性を担保する仕組み

**読み取り**: 4つのGUI部品はいずれも KifuBranchTree のノードからデータを読み取る。棋譜欄・分岐候補欄はナビゲーション時にツリーから再構築され、分岐ツリー欄は `treeChanged` で再描画される。将棋盤はノードの SFEN を直接使用する。GUI部品が独自のデータコピーを長期保持することはなく、表示更新のたびにツリーから最新データを取得する。

**書き込み**: コメント・しおりの編集は GameRecordModel 経由で行われ、3層同期パターン（セクション5.2）により内部配列・表示リスト・ツリーノードが同時に更新される。指し手の追加は LiveGameSession 経由で `addMoveQuiet()` を呼び、ツリーに直接ノードを追加する。

**ナビゲーション**: KifuNavigationState が「どのラインのどの手にいるか」を一元管理する。GUI部品は自身の位置状態を持たず、ナビゲーション操作はすべて KifuNavigationController → KifuNavigationState → シグナル → KifuDisplayCoordinator の経路で各GUI部品に通知される。

#### 6.5.4 補足: m_sfenRecord の位置づけ

`MainWindow::m_sfenRecord`（QStringList）は本譜の SFEN 列を保持するレガシーなデータストア。KifuBranchTree にも同じ SFEN が格納されているため冗長だが、ツリーが空の場合（旧コードパスや単純な棋譜再生）のフォールバックとして残されている。棋譜読み込み・対局のどちらの場合も、m_sfenRecord とツリーは同じタイミングで同じソース（パース結果 or エンジン応答）から構築されるため、不整合のリスクは低い。

---

## 7. シリアライズ形式

### 7.1 SFEN（Shogi Forsyth-Edwards Notation）

局面を文字列で表現する。盤面・手番・持ち駒・手数の4パートからなる。

```
lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1
│                                                           │ │ │
│                                                           │ │ └ 手数
│                                                           │ └ 持ち駒（-=なし）
│                                                           └ 手番（b=先手,w=後手）
└ 盤面（/で段区切り、数字=空マス連続数、大文字=先手、小文字=後手）
```

### 7.2 USI形式

エンジン通信で使われる `position` コマンド文字列。

```
position startpos moves 7g7f 3c3d 2g2f
```

指し手表記: `{from}{to}[+]` — 例: `7g7f`（七七→七六）、`B*5e`（角打ち五五）

### 7.3 KIF形式

日本語の棋譜ファイル形式。

```
# ---- Kifu for Windows V7 形式 ----
手数----指手---------消費時間--
   1 ７六歩(77)   ( 0:03/00:00:03)
   2 ３四歩(34)   ( 0:05/00:00:05)
   3 ２六歩(26)   ( 0:02/00:00:07)

変化：3手目
   3 ６六歩(67)   ( 0:04/00:00:07)
```

### 7.4 CSA形式

コンピュータ将棋協会の標準形式。分岐非対応。

```
+7776FU
-3334FU
+2726FU
```

### 7.5 KI2形式

手番記号付きの簡潔な表記。分岐対応。

```
▲７六歩 △３四歩 ▲２六歩
```

### 7.6 JKF形式（JSON棋譜フォーマット）

JSON 形式で棋譜を表現。分岐対応。

```json
{
  "header": { "先手": "..." },
  "initial": { "preset": "HIRATE" },
  "moves": [
    {},
    { "move": { "from": {"x":7,"y":7}, "to": {"x":7,"y":6}, "piece": "FU" } }
  ]
}
```

### 7.7 USEN形式（URL Safe sfen-Extended Notation）

URL セーフな短い文字列で棋譜を表現。分岐対応。

---

## 8. 棋譜データの作成フロー

各ユースケースにおいて、棋譜データ構造がどのように作成・初期化されるかを解説する。

### 8.1 KIF形式の棋譜を読み込んだ場合

棋譜ファイルを開くと、パース → データ構築 → ツリー構築 → UI 表示の順で処理される。

```
1. KifuLoadCoordinator::loadKifuFromFile(filePath)
   → loadKifuCommon()

2. KIF パーサが KifParseResult を生成
   KifToSfenConverter::parseWithVariations()
     ├─ mainline: KifLine（本譜の指し手列・SFEN列・表示データ）
     └─ variations: QVector<KifVariation>（分岐リスト）

3. applyParsedResultCommon() で各データ構造を構築
   a. USI指し手リスト（m_kifuUsiMoves）を保存
   b. rebuildSfenRecord() → m_sfenRecord（局面SFEN列）を再構築
   c. rebuildGameMoves()  → m_gameMoves（ShogiMove列）を再構築
   d. emit displayGameRecord(disp)
      → MainWindow::displayGameRecord()
        → GameRecordModel::initializeFromDisplayItems()
        → KifuRecordListModel に行を追加（棋譜欄表示）

4. KifuBranchTree の構築
   a. navState->setCurrentNode(nullptr)  // ダングリングポインタ防止
   b. KifuBranchTreeBuilder::buildFromKifParseResult(tree, result, sfen)
      ├─ tree->clear() + tree->setRootSfen(initialSfen)
      ├─ addKifLineToTree(tree, mainline, 1)  // 本譜
      │   → root → move1 → move2 → ... → endNode
      └─ for each variation:
           addKifLineToTree(tree, var.line, var.startPly)
             i.  findBySfen(baseSfen) で分岐点を検索
             ii. フォールバック: findByPlyOnMainLine(startPly-1)
             iii. 分岐点の子としてノードを追加
   c. navState->goToRoot()  // ナビゲーション状態を初期化
   d. emit branchTreeBuilt()

5. 分岐マーク・解析タブの初期化
   a. applyBranchMarksForCurrentLine()  // 棋譜欄の分岐行を太字表示
   b. 解析タブにBranchLineデータを設定
```

**読み込み後のデータ状態**:

| データ構造 | 内容 |
|---|---|
| `KifuBranchTree` | 本譜＋全分岐を含む完全なN分木 |
| `m_sfenRecord` | 本譜の各手後のSFEN列（`[開始局面, 1手目後, 2手目後, ...]`） |
| `m_gameMoves` | 本譜のShogiMove列 |
| `KifuRecordListModel` | 棋譜欄の表示データ（開始局面＋各手の表示行） |
| `GameRecordModel` | コメント・しおりの一元管理（m_comments, m_bookmarks） |
| `KifuNavigationState` | ルートノードを指し、手数0の状態 |

### 8.2 棋譜読み込み後、途中局面から対局した場合

読み込んだ棋譜の途中の手（例: 30手目）を選択した状態で対局を開始すると、
既存のツリーに新しい分岐が作成される。

```
前提: KIF読み込み済み。ユーザーが30手目を選択中。

1. GameStartCoordinator::start()
   → emit requestPreStartCleanup()

2. PreStartCleanupHandler::performCleanup()
   a. 現在位置を保存
      - m_savedCurrentSfen = 30手目のSFEN
      - m_savedSelectedPly = 30
      - m_savedCurrentNode = navState->currentNode()（30手目のノード）

   b. isStartFromCurrentPosition() = true
      （selectedPly=30 > 0 かつ currentSfen ≠ 初期局面）

   c. cleanupKifuModel(true, keepRow=30)
      - pathToNode(branchPoint) でルート→30手目の経路を取得
      - 棋譜欄をクリアし、経路上のノード（0〜30手目）で再構築
      → 棋譜欄には0手目〜30手目のみが表示される

   d. ensureBranchTreeRoot()
      → ルートは読み込み時に作成済みなので何もしない
      → 既存のツリー（本譜＋分岐）はそのまま保持される

   e. startLiveGameSession()
      - isNewGame = false（SFENが初期局面ではない）
      - hasExistingMoves = true（読み込んだ棋譜がある）
      - savedPly = 30 > 0
      → savedCurrentNode（30手目のノード）を branchPoint とする
      → LiveGameSession::startFromNode(branchPoint)
         ├─ m_branchPoint = 30手目のノード
         ├─ m_sfens = [30手目のSFEN]
         └─ emit sessionStarted(branchPoint)

3. MatchCoordinator::configureAndStart()
   → 30手目のSFENを使って盤面を初期化

4. 対局中の指し手追加（各手ごとに繰り返し）
   LiveGameSession::addMove(move, displayText, sfen, elapsed)
   a. ply = anchorPly(30) + moveCount + 1  → 31, 32, 33, ...
   b. parent = m_liveParent（初回は m_branchPoint = 30手目のノード）
   c. tree->addMoveQuiet(parent, move, displayText, sfen, elapsed)
      → 30手目ノードの子として新ノードを追加
      → 既存の31手目（本譜）と並列する分岐として作成される
   d. m_liveParent = 新ノード（次の手の親になる）
   e. emit moveAdded(), recordModelUpdateRequired()
      → 棋譜欄に新しい行が追加される

5. 対局終了時
   LiveGameSession::commit()
   → m_liveParent が既にツリーに追加済みなので、そのまま返す
   → emit sessionCommitted(lastNode)
```

**対局後のツリー構造（例: 本譜100手、30手目から分岐して5手指した場合）**:

```
root → 1手目 → ... → 30手目 → 31手目(本譜) → ... → 100手目
                          │
                          └→ 31手目(分岐) → 32手目 → ... → 35手目
```

### 8.3 棋譜読み込みなしで直接対局した場合

メニューから新規対局を開始すると、空のツリーから対局が始まる。

```
1. GameStartCoordinator::start()
   → emit requestPreStartCleanup()

2. PreStartCleanupHandler::performCleanup()
   a. 現在位置を保存
      - m_savedCurrentSfen = "" または "startpos"
      - m_savedSelectedPly = 0

   b. isStartFromCurrentPosition() = false
      （currentSfen が空または初期局面）

   c. cleanupKifuModel(false, keepRow=0)
      → 棋譜欄を全クリアし、「=== 開始局面 ===」ヘッダ行のみ追加

   d. ensureBranchTreeRoot()
      → KifuBranchTree が空なので、ルートノードを新規作成
      → tree->setRootSfen(平手初期SFEN)
         （startSfenStr が設定されていればそちらを使用）

   e. startLiveGameSession()
      - isNewGame = true（SFENが空 or "startpos" or 平手初期局面）
      → LiveGameSession::startFromRoot()
         ├─ m_branchPoint = nullptr
         ├─ m_sfens = [ルートのSFEN]
         └─ emit sessionStarted(nullptr)

3. MatchCoordinator::configureAndStart()
   → initializeNewGame(sfenStart) で盤面を初期化
   → 対局モードに応じたエンジン起動

4. 対局中の指し手追加（各手ごとに繰り返し）
   LiveGameSession::addMove(move, displayText, sfen, elapsed)
   a. ply = anchorPly(0) + moveCount + 1  → 1, 2, 3, ...
   b. parent = m_liveParent（初回は root）
   c. tree->addMoveQuiet(parent, move, ...)
      → ルートの子として本譜のノードが作成される
   d. m_liveParent = 新ノード
   e. emit moveAdded(), recordModelUpdateRequired()

5. 対局終了時
   LiveGameSession::addTerminalMove(type, displayText, elapsed)
   → 終局手を記録（SFENは前回と同じ、盤面変化なし）
   LiveGameSession::commit()
   → emit sessionCommitted(lastNode)
```

**対局後のツリー構造（例: 80手で投了）**:

```
root → 1手目 → 2手目 → ... → 80手目 → [投了]
```

### 8.4 対局後に途中局面から再度対局した場合

前回の対局のツリーを保持したまま、途中局面から新しい分岐を作成する。
データフローは「8.2 棋譜読み込み後、途中局面から対局」と同じ。

```
前提: 直接対局（8.3）で80手まで進んだ後、20手目を選択して再度対局開始。

1. GameStartCoordinator::start()
   → emit requestPreStartCleanup()

2. PreStartCleanupHandler::performCleanup()
   a. 現在位置を保存
      - m_savedCurrentNode = 20手目のノード（前回対局で作成されたツリーのノード）
      - m_savedSelectedPly = 20

   b. isStartFromCurrentPosition() = true

   c. cleanupKifuModel(true, keepRow=20)
      - pathToNode(20手目) で経路取得: [root, 1手目, ..., 20手目]
      - 棋譜欄を経路のノードで再構築（0〜20手目を表示）

   d. ensureBranchTreeRoot()
      → ルートは前回対局時に作成済み → 何もしない
      → 前回の本譜（80手＋投了）はツリーにそのまま残る

   e. startLiveGameSession()
      → LiveGameSession::startFromNode(20手目のノード)
         ├─ m_branchPoint = 20手目のノード
         └─ emit sessionStarted(branchPoint)

3. 対局中の指し手追加
   LiveGameSession::addMove(...)
   → ply = 20 + moveCount + 1  → 21, 22, 23, ...
   → 20手目ノードの子として新ノードを追加
   → 既存の21手目（前回の本譜）と並列する分岐

4. 対局終了
   LiveGameSession::commit()
```

**対局後のツリー構造（例: 20手目から分岐して40手目で投了）**:

```
root → 1手目 → ... → 20手目 → 21手目(前回本譜) → ... → 80手目 → [投了]
                          │
                          └→ 21手目(今回分岐) → ... → 40手目 → [投了]
```

### 8.5 addMove と addMoveQuiet の使い分け

| メソッド | treeChanged シグナル | 使用場面 |
|---|---|---|
| `addMove()` | 発火する | 棋譜読み込み時のツリー構築、セッション commit 時のフォールバック |
| `addMoveQuiet()` | 発火しない（nodeAdded のみ） | ライブ対局中のリアルタイム追加 |

`addMoveQuiet()` が対局中に使われる理由: `treeChanged` が発火すると
`KifuDisplayCoordinator::onTreeChanged()` が棋譜モデルを再構築し、
対局中のエンジン思考に干渉するため。対局中は `nodeAdded` のみで最小限の通知を行い、
`recordModelUpdateRequired` シグナルで棋譜欄の行追加だけを処理する。

---

## 9. 補足: パース用データ構造

### 9.1 KifParseResult / KifLine / KifVariation

**ファイル**: `src/kifu/kifparsetypes.h`

KIF/KI2/CSA 等のファイル読み込み時のパース結果を格納する中間構造。

```cpp
struct KifLine {
    int startPly;              // 開始手数
    QString baseSfen;          // 分岐開始局面SFEN
    QStringList usiMoves;      // USI指し手列
    QList<KifDisplayItem> disp; // 表示用データ
    QStringList sfenList;      // 局面列
    QVector<ShogiMove> gameMoves; // 指し手列
    bool endsWithTerminal;     // 終局手で終わるか
};

struct KifVariation {
    int startPly;              // 変化開始手数
    KifLine line;              // 変化の手順
};

struct KifParseResult {
    KifLine mainline;                  // 本譜
    QVector<KifVariation> variations;  // 変化リスト
};
```

パース後、この `KifParseResult` から `KifuBranchTree` が構築される。

### 9.2 ResolvedRow

**ファイル**: `src/kifu/kifutypes.h`

分岐を含む棋譜データを行単位で平坦化した結果。

```cpp
struct ResolvedRow {
    int startPly;               // 開始手数
    int parent;                 // 親行のインデックス（本譜は -1）
    QList<KifDisplayItem> disp; // 表示用アイテムリスト
    QStringList sfen;           // 各手数のSFENリスト
    QVector<ShogiMove> gm;     // 指し手リスト
    int varIndex;               // 変化インデックス（本譜 = -1）
    QStringList comments;       // 各手のコメント
};
```

### 9.3 KifGameInfoItem

**ファイル**: `src/kifu/kifparsetypes.h`

KIF ヘッダの「キーワード：内容」ペア。

```cpp
struct KifGameInfoItem {
    QString key;    // キーワード（例: "開始日時"）
    QString value;  // 内容（例: "2025/03/02 09:00"）
};
```
