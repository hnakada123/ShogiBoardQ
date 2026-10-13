# 指し手データ・棋譜データ構造

ShogiBoardQ の「指し手データ」と「棋譜データ」のソースコード上の構造をまとめたドキュメント。
棋譜欄・分岐候補欄・分岐ツリー欄・将棋盤が連動して動作する仕組みの根幹となるデータモデルを解説する。

---

## 0. 指し手の値（UsiMove）と表記の共通部品

2026-10-13 に、USI の指し手と1対1の値型と、座標・表記の対応表を1か所にまとめた。
指し手の読み書き・盤への適用・日本語の表記は、次の部品だけを使う（同じ表を別のファイルに書かない）。

| 部品 | ファイル | 内容 |
|---|---|---|
| `UsiMove` / `BoardSquare` | `src/core/usimove.h` | USI の指し手（"7g7f"・"8h2b+"・"P*5e"）と1対1の値。成りも指し手の一部として持つ。`UsiMove::parse()` と `toString()` が USI の読み書きの唯一の実装 |
| `BoardCoordinates` | `src/core/boardcoordinates.h` | 盤面表示の座標（1始まり、駒台は筋10・11、駒箱は筋12）と駒・`UsiMove` の対応。駒台の段と駒の並びはここだけで決める |
| `ShogiBoard::applyUsiMove()` | `src/core/shogiboard.cpp` | `UsiMove` を盤と駒台に適用する（合法性は見ない）。読み筋の盤・SFEN 追跡・通信対局の盤の更新が使う |
| `ShogiGameController::applyMove()` | `src/game/shogigamecontroller.cpp` | 対局で指す。合法手か確かめ（エンジン・通信対局の手も）、反則手は盤を変えずに断る。盤のクリック操作は `validateAndMove()` が成りを選ばせてから同じ確定処理を通る |
| `ShogiNotation` | `src/core/shoginotation.h` | 全角の筋・漢数字の段、駒の名前（指し手用は成香・成桂・成銀を2文字、局面図用は杏・圭・全）、CSA の駒種 |
| `KifMoveText::format()` | `src/core/kifmovetext.h` | 「▲７六歩(77)」「△同　銀(31)」「▲５五角打」の形の指し手。対局の棋譜・棋譜の読み込み（USI・USEN・CSA・JKF）・通信対局・定跡・詰将棋の読み筋が使う |
| `SfenPositionTracer` | `src/board/sfenpositiontracer.cpp` | USI の手順から SFEN の局面列を作る。局面は `ShogiBoard` で持つ（以前は独自の盤を持っていた） |

SFEN の持ち駒は、盤（`ShogiBoard::convertStandToSfen`）も含めて標準の並び（先手の飛角金銀桂香歩、続いて後手）で書く。
以前の盤は先後を種類ごとに交互に書いていたので、局面を比べるキーは `SfenUtils::normalizeSfenKey()`（持ち駒を並べ直す）で作る。

エンジンの指し手は `Usi::matchMoveReady(UsiMove, ...)` で届き、共有の成りフラグ（以前の `ShogiGameController::setPromote()`）は無い。

SFEN の差分から指し手を推測する処理（以前の `UsiMoveConverter`、定跡のマージ用の `generateUsiMovesFromSfen`、USEN 書き出しの `inferUsiFromSfenDiff`）は無い。
棋譜の書き出し・自動化 API・定跡のマージに使う USI の指し手は、分岐ツリーのノードの手（`ShogiMove::toUsiMove()`）から作る（4.4 の `RecordLine`）。
以前の `ShogiUtils`（`src/core/shogiutils.*`）も無い。表記の関数は `ShogiNotation` と `KifMoveText` にある。
KI2 の読み書き（`Ki2MoveNotation`）も、盤は `ShogiBoard`、指し手は `UsiMove` で扱う。読み込みは局面の合法手
（`EngineMoveValidator::legalMoves`）から指し手を選び、書き出しは分岐ツリーのノードの手と局面から修飾語・「打」・「不成」を決める。
反則手を書いた棋譜のために、合法手で決まらない手だけは駒の動きだけで決める（`EngineMoveValidator::pseudoLegalMoves`）。

まだ独自の表現が残っている所（意図して残している）:

- 思考情報の読み筋の漢字化（`ShogiEngineInfoParser`、info 行ごとの高速化のため文字の盤）
- JKF の書き出し（棋譜の表記から JKF の指し手と `relative` を作る）
- CSA 読み込みの局面（`CsaLexer::Board`）
- 2つの局面の差分（`SfenDiffUtils`）から手を求める所: 盤のハイライト（`BoardSyncPresenter`）、読み筋の盤（`PvBoardController`）、待ったの後の「同」の基準（`MatchUndoHandler`）。
  どれも別に持った指し手の一覧ではなく、手順の局面（分岐ツリー・読み筋・対局の局面列）どうしを比べる

---

## 1. 指し手データ（ShogiMove）

**ファイル**: `src/core/shogimove.h` / `src/core/shogimove.cpp`

### 1.1 構造体定義

```cpp
struct ShogiMove {
    QPoint fromSquare{0, 0};            // 移動元の座標
    QPoint toSquare{0, 0};              // 移動先の座標
    Piece movingPiece = Piece::None;    // 動かした駒
    Piece capturedPiece = Piece::None;  // 取った駒（なければ None）
    bool isPromotion = false;           // 成りフラグ

    static ShogiMove fromUsiMove(const UsiMove& move, Turn mover, Piece moving, Piece captured);
    UsiMove toUsiMove() const;
};
```

`ShogiMove` は指した手の記録（動かした駒・取った駒を含む）で、`UsiMove` との変換は `fromUsiMove()`・`toUsiMove()` だけで行う。

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

分岐ツリーのカーソル（表示中のノード）と分岐選択の記憶を一元管理する。
表示中の手数は `currentPly()` だけが持つ。MainWindow に手数の変数（以前の `KifuState::activePly`・`KifuState::currentSelectedPly`・`GameState::currentMoveIndex`）は無い。

```cpp
class KifuNavigationState : public QObject {
    KifuBranchTree*  m_tree;                       // 参照するツリー
    KifuBranchNode*  m_currentNode;                // 現在のノード（カーソル）
    QHash<int, int>  m_lastSelectedChildAtBranch;  // 分岐点nodeId → 選んだ子のインデックス
    int              m_preferredLineIndex = -1;    // 優先ライン（-1=未設定）
};
```

### 4.1 主要な状態

| プロパティ | 説明 |
|---|---|
| `currentNode()` | 現在位置のノード |
| `currentPly()` | 現在の手数（表示中の手数） |
| `currentLineIndex()` | 現在のラインインデックス（0=本譜） |
| `currentSfen()` | 現在位置の SFEN |
| `currentRecordLine()` | 表示中の手順（4.4 の `RecordLine`） |
| `isOnMainLine()` | 本譜上にいるか |
| `canGoForward()` | 前進可能か |
| `canGoBack()` | 後退可能か |

### 4.2 分岐選択の記憶

ユーザーが分岐を選択すると、その選択が記憶される:

- `rememberChildSelection(branchPoint, childIndex)`: 分岐点で選んだ子（`childAt()` の添字）を記録
- `lastSelectedChildAt(branchPoint)`: 記録された子を取得（未選択は0=最初の子）
- `rememberPathSelections(node)`: ルートから `node` までの経路上の全分岐点で、経路が通る子を記録
- `preferredLineIndex`: 分岐選択時に設定され、分岐点より前に戻っても維持される

これにより、分岐を選んだ状態で開始局面まで戻り、再度進めた場合に同じ分岐を自動的に辿ることができる。

### 4.3 カーソルを動かす所

`KifuNavigationState` はシグナルを持たない。カーソルは次の所が動かし、表示の更新は `KifuNavigationController` のシグナル（6.2）などで行う。

| 場面 | 動かす所 |
|---|---|
| ナビゲーションボタン・分岐ツリー欄・分岐候補欄 | `KifuNavigationController`（`goToNode()` など） |
| 棋譜欄のクリック | `KifuDisplayCoordinator::onPositionChanged()` |
| 対局の開始・着手・待った | `KifuDisplayCoordinator`（`onLiveGameSessionStarted()` は分岐点か開始局面、`onLiveGameMoveAdded()` は最新手、`onLiveGameMovesAboutToBeUndone()` は戻した局面へ） |
| 終局 | `GameStateController` の `syncCursorToPly` フック → `KifuNavigationCoordinator::syncNavStateToPly()` |
| 棋譜の読み込み | `KifuApplyService`（`goToRoot()`） |
| 新規対局の開始前の整理 | `PreStartCleanupHandler::performCleanup()`（現在の局面から始めないときは `goToRoot()`） |

### 4.4 棋譜の正本と手順の値（RecordLine）

棋譜の正本は、分岐ツリー（`KifuBranchTree`）とそのカーソル（`KifuNavigationState`）である。
ほかの機能が手順を使うときは、局面列や指し手列を別々の一覧から探さずに、`RecordLine`（`src/kifu/recordline.h`、ヘッダーだけ）を受け取る。

```cpp
struct RecordLine {
    QStringList sfens;         // 開始局面（[0]）と各手の後の局面
    QList<ShogiMove> moves;    // 各手（moves[i] は i+1 手目）
    QStringList displayTexts;  // 各手の棋譜の表記（"▲７六歩(77)"）
    int currentPly = 0;        // 表示中の手数（0 は開始局面）

    UsiMove usiMove(int ply) const;                // ply 手目の指し手（範囲外は無効な値）
    QStringList usiMoves(int upToPly = -1) const;  // upToPly 手目までの USI の指し手（-1 は最後まで）
    BoardSquare destinationOf(int ply) const;      // ply 手目の移動先
    QString positionCommand(int ply) const;        // ply 手目の後の position コマンド（平手なら startpos）
};
```

| 取り出し方 | 中身 |
|---|---|
| `KifuNavigationState::currentRecordLine()` | 表示中の手順（選んでいる分岐。対局中はライブの手も含む）。`currentPly` は表示中の手数 |
| `KifuBranchTree::mainRecordLine()` | 本譜の手順（`currentPly` は0） |
| `KifuBranchTree::toRecordLine(nodes, currentPly)` | 任意のノード列（ライン・`pathToNode()` の結果など）。static |

どれも終局行（投了・中断など）を含まない。棋譜欄の行は終局行を含むので、棋譜欄の行数と `sfens.size()` は一致しないことがある。

**誰が何を持つか**:

| データ | 持ち主 | 役割 |
|---|---|---|
| 手順・局面・指し手・表記・コメント・しおり・消費時間 | `KifuBranchTree` のノード | 棋譜の正本 |
| 表示中の手数・選んでいる分岐 | `KifuNavigationState` | 棋譜のカーソル |
| 棋譜欄・分岐候補欄・分岐ツリー欄の表示 | 各モデル（`KifuRecordListModel` など） | ツリーから作り直す写し |
| 対局の局面列（`sfenRecord`）・指し手列（`gameMoves`） | MainWindow の `KifuState`（`MatchCoordinator` にポインタで渡す） | 対局の作業用の履歴。棋譜の読み込みでは書かず、対局開始時に表示中の手順から作り直す（8.2） |

以前あった並行の一覧（MainWindow の `KifuState::gameUsiMoves`・`positionStrList`、`KifuLoadCoordinator::kifuUsiMoves()`、`GameRecordPresenter` のライブ表示用の一覧 `liveDisp`、`KifuLoadResult` の `sfens`・`moves`・`positionCommands`）は無い。

---

## 5. 中央管理（GameRecordModel）

**ファイル**: `src/kifu/gamerecordmodel.h`

コメント・しおりの編集の受付と、棋譜の各種形式への出力を担当する。コメント・しおりの正本は分岐ツリーのノードにある。

### 5.1 内部データ

```cpp
class GameRecordModel : public QObject {
    // === ツリーがない場合のフォールバックデータ ===
    QList<QString> m_comments;      // 手数 → コメント
    QList<QString> m_bookmarks;     // 手数 → しおり
    bool           m_isDirty;       // 変更フラグ

    // === 参照（所有しない） ===
    KifuBranchTree*       m_branchTree;  // 棋譜の正本
    KifuNavigationState*  m_navState;    // 表示中のライン
};
```

### 5.2 コメント・しおりの書き込み

コメントやしおりを編集すると、表示中のラインのノードに書き、変更シグナルで表示に知らせる:

```
setComment(ply, text)
    │
    ├─→ 表示中のライン（m_navState->currentLineIndex()）の ply 番目のノード
    │     └─ node->setComment(text)      // 棋譜の正本に書く
    │      （分岐ツリーが空のときだけ m_comments[ply] に書く）
    └─→ emit commentChanged(ply, text)
          └─ CommentCoordinator が棋譜欄の行とコメント欄を更新
```

`setBookmark()` も同じ流れで `bookmarkChanged` を出す。編集する手数は、`CommentCoordinator` が分岐ツリーのカーソル（`KifuNavigationState::currentPly()`）から決める。
棋譜を読み込んだときは、`initializeFromDisplayItems()` が本譜のノードにコメント・しおりを書く。

棋譜欄の表示用の一覧への書き込み（以前の `GameRecordModel::bind()`・`m_liveDisp`）は無い。その一覧には開始局面（0手目）の行が無く、1手ずれた行に書いていた。

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

保存・コピーで CSA・USEN・USI の書き出しに渡す本譜の USI の指し手は、`KifuExportController`・`KifuExportClipboard` が `collectMainlineUsiForExport()`（`KifuBranchTree::mainRecordLine().usiMoves()`）から取る。USEN の分岐は、各ラインを `KifuBranchTree::toRecordLine()` にして指し手を取る。

---

## 6. GUI連動の全体像

### 6.1 4つのGUI部品

| GUI部品 | ウィジェット | モデル | 説明 |
|---|---|---|---|
| **棋譜欄** | `RecordPane` 内の `QTableView` | `KifuRecordListModel` | 手順リスト。クリックで手に移動 |
| **分岐候補欄** | `RecordPane` 内の `QTableView` | `KifuBranchListModel` | 現在位置の分岐候補 |
| **分岐ツリー欄** | `BranchTreeManager`（`EngineAnalysisTab` 内の `QGraphicsView`） | — (直接描画) | ツリーのグラフィカル表示 |
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
│    └─→ BranchTreeManager::highlightBranchTreeAt()                  │
│                                                                    │
│  onBranchCandidatesUpdateRequired(candidates)                      │
│    └─→ KifuBranchListModel 更新                                   │
└────────────────────────────────────────────────────────────────────┘
        ▲                  ▲                    ▲
        │                  │                    │
   棋譜欄クリック    分岐候補クリック    分岐ツリークリック
   RecordPane::       RecordPane::        BranchTreeManager::
   mainRowChanged     branchActivated     branchNodeActivated
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
   a. 表示中の手順（分岐ツリーの表示中のライン）から row 行目の局面と直前の局面を取る（lineSfenAt）
      - 取れた場合: emit branchBoardSyncRequired(currentSfen, prevSfen)
        → BoardSyncPresenter::loadBoardWithHighlights() で将棋盤を更新（ハイライト付き）
      - 取れない場合: emit boardSyncRequired(row)
        → BoardSyncPresenter::syncBoardAndHighlightsAtRow(row)（表示中の手順から描く）
      本譜か分岐かは区別しない。対局の局面列（sfenRecord）は見ない
   b. 棋譜欄のハイライト行を更新
   c. 手番表示・評価値グラフの縦線を更新
   d. KifuDisplayCoordinator::onPositionChanged(lineIndex, ply, sfen)
      - 表示中の手数（KifuNavigationState のカーソル）をその行のノードへ動かす
      - 分岐選択記憶を更新
      - 分岐候補欄・分岐ツリーのハイライトを更新
```

対局中（盤面を対局ロジックが更新しているとき）は、手順 a の盤面の再設定と手順 d の通知をしない。カーソルは `KifuDisplayCoordinator::onLiveGameMoveAdded()` が最新手に合わせる。

> **ボタン経由との違い**: ボタンは KifuNavigationController が全 UI 更新シグナルを一括発行するが、
> 棋譜欄クリックは RecordNavigationHandler がメインの処理を行い、
> KifuDisplayCoordinator には `onPositionChanged()` で追加の更新（分岐ツリー・分岐候補欄）を通知する。

#### 6.3.3 分岐ツリー欄のノードをクリック

分岐ツリーは `EngineAnalysisTab` 内の `QGraphicsView` を `BranchTreeManager` が描画し、クリックは次の経路で処理される。

| ウィジェット | シグナル | ハンドラ |
|---|---|---|
| EngineAnalysisTab 内の分岐ツリー | `branchNodeActivated(row, ply)` | `BranchNavigationWiring::onBranchNodeActivated` → `KifuNavigationController::handleBranchNodeActivated` |

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
| **GameRecordModel** | コメント・しおりの編集 | 編集操作を受け付け、表示中のラインのノードに書く（セクション5.2参照） |
| **KifuNavigationState** | 現在位置（表示中の手数）・分岐選択記憶 | どのラインのどの手にいるかの状態管理（棋譜のカーソル） |

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
                  ├──→ 分岐ツリー欄 (BranchTreeManager)
                  │      treeChanged シグナルで KifuDisplayPresenter::buildBranchTreeRows() が
                  │      allLines() から行データを構築し、グラフィカルなツリーを再描画する。
                  │
                  └──→ 将棋盤 (ShogiBoard)
                         ナビゲーション時に KifuNavigationController が
                         currentNode()->sfen() を取得し、
                         boardUpdateRequired シグナルで将棋盤に渡す。
```

#### 6.5.3 一元性を担保する仕組み

**読み取り**: 4つのGUI部品はいずれも KifuBranchTree のノードからデータを読み取る。棋譜欄・分岐候補欄はナビゲーション時にツリーから再構築され、分岐ツリー欄は `treeChanged` で再描画される。将棋盤はノードの SFEN を直接使用する。GUI部品が独自のデータコピーを長期保持することはなく、表示更新のたびにツリーから最新データを取得する。

**書き込み**: コメント・しおりの編集は GameRecordModel 経由で行われ、表示中のラインのノードに書いたあと、変更シグナルで棋譜欄の行とコメント欄を更新する（セクション5.2）。指し手の追加は LiveGameSession 経由で `addMoveQuiet()` を呼び、ツリーに直接ノードを追加する。

**ナビゲーション**: KifuNavigationState が「どのラインのどの手にいるか」を一元管理する。GUI部品は自身の位置状態を持たず、ナビゲーション操作はすべて KifuNavigationController → KifuNavigationState → シグナル → KifuDisplayCoordinator の経路で各GUI部品に通知される。

#### 6.5.4 補足: 対局の局面列・指し手列の位置づけ

MainWindow の `KifuState::sfenRecord`（局面列）と `KifuState::gameMoves`（指し手列）は、`MatchCoordinator` にポインタで渡す対局の作業用の履歴である（`MatchCoordinator` 側の名前は `m_sfenHistory`・`gameMovesRef()`）。棋譜の正本ではない。

- 棋譜の読み込み（`KifuApplyService`・`KifuLoadCoordinator`）はこの履歴を書かない
- 対局開始時に、表示中の手順（`RecordLine`）の開始局面から開始する手数までで作り直す（8.2）
- 対局中は着手・待ったでこの履歴を更新し、同じ手を `LiveGameSession` が分岐ツリーに積む

GUI部品の表示（盤面・棋譜欄のクリック・評価値グラフの横軸の長さなど）は、この履歴ではなく分岐ツリーから読む。対局中の評価値を描く手数の計算は、この履歴（局面列の長さ）を使う。

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

棋譜ファイルを開くと、パース → 棋譜欄の表示 → ツリー構築 → 盤面表示の順で処理される。
パースは別スレッド（`QtConcurrent::run`）で行い、結果の適用はメインスレッドで行う。

```
1. KifuLoadCoordinator::loadFileAsync(filePath)
   → KifuLoadParser::parseFile()（別スレッド。形式は内容から判定）
   → onLoadFinished() → applyLoadResult(result)
     （同期版の loadKifuFromFile() なども applyLoadResult() を通る）

2. KIF パーサが KifParseResult を生成（KifuLoadResult::record）
   KifToSfenConverter::parseWithVariations()
     ├─ mainline: KifLine（本譜の指し手列・SFEN列・表示データ）
     └─ variations: QVector<KifVariation>（分岐リスト）

3. KifuApplyService::applyParsedResult() → applyToRecordView()
   a. 対局情報の表示（populateGameInfo・applyPlayersFromGameInfo。applyLoadResult が先に呼ぶ）
   b. displayGameRecord(disp) フック
      → MainWindow::displayGameRecord() → GameRecordLoadService::loadGameRecord()
        → GameRecordModel::initializeFromDisplayItems()
        → GameRecordPresenter::displayAndWire() で棋譜欄に行を表示
   c. 対局の局面列・指し手列（sfenRecord / gameMoves）は書かない
      （対局開始時に表示中の手順から作り直す。8.2）

4. KifuApplyService::applyBranchTree() で KifuBranchTree を構築
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
      （構築中の treeChanged は beginBatchUpdate()/endBatchUpdate() で1回にまとめる）
   d. branchTreeBuilt フック

5. 分岐マーク・盤面の初期化
   a. applyBranchMarksForCurrentLine()  // 棋譜欄の分岐行を太字表示
   b. 分岐ツリー欄の開始局面をハイライト
   c. syncBoardAndHighlightsAtRow(0) フックで開始局面を盤に表示
```

**読み込み後のデータ状態**:

| データ構造 | 内容 |
|---|---|
| `KifuBranchTree` | 本譜＋全分岐を含む完全なN分木（棋譜の正本） |
| `KifuNavigationState` | ルートノードを指し、手数0の状態 |
| `KifuRecordListModel` | 棋譜欄の表示データ（開始局面＋各手の表示行） |
| `GameRecordModel` | 本譜のノードにコメント・しおりを書いた状態（ツリーが空のときだけ m_comments, m_bookmarks） |
| 対局の `sfenRecord` / `gameMoves` | 読み込みでは変わらない。対局開始時に表示中の手順から作り直す（8.2） |

### 8.2 棋譜読み込み後、途中局面から対局した場合

読み込んだ棋譜の途中の手（例: 30手目）を選択した状態で対局を開始すると、
既存のツリーに新しい分岐が作成される。

```
前提: KIF読み込み済み。ユーザーが30手目を選択中（分岐ツリーのカーソルが30手目のノードにある）。

1. GameSessionOrchestrator::initializeGame()（対局開始ダイアログの OK 後）
   a. line = navState->currentRecordLine()   // 表示中の手順。本譜でも分岐でもよい
      selectedPly = navState->currentPly()   // 30
   b. MainWindowGameStartService::prepareForInitializeGame()
      → rebuildMatchHistoryFromLine()
         ├─ sfenRecord = line.sfens[0..30]  // 開始局面〜30手目の後の局面（31件）
         └─ gameMoves  = line.moves[0..30)  // 1〜30手目（30件）
      → currentSfenStr = sfenRecord[30]
      0手目（開始局面）から始める場合も同じく作り直す（sfenRecord は1件、gameMoves は空）

2. GameStartCoordinator::initializeGame(Ctx) → prepareDataCurrentPosition()
   → emit requestPreStartCleanup()
   → PreStartCleanupHandler::performCleanup()
   a. 現在位置を保存（棋譜欄を作り直すと行選択の通知でカーソルが動くので、最初に控える）
      - m_savedCurrentSfen = 30手目のSFEN
      - m_savedSelectedPly = navState->currentPly() = 30
      - m_savedCurrentNode = navState->currentNode()（30手目のノード）

   b. isStartFromCurrentPosition() = true
      （selectedPly=30 > 0 かつ currentSfen ≠ 初期局面）

   c. cleanupKifuModel(true) → 30（保持した最後の行）
      - pathToNode(branchPoint) でルート→30手目の経路を取得
      - 棋譜欄をクリアし、経路上のノード（0〜30手目）で再構築
      → 棋譜欄には0手目〜30手目のみが表示される
      （現在の局面から始めない場合は、ここで navState->goToRoot() でカーソルを開始局面へ戻す）

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
            → KifuDisplayCoordinator::onLiveGameSessionStarted() がカーソルを分岐点（30手目のノード）に合わせる

3. 開始SFENの seed（GameStartCoordinator::initializeGame のステップ3.5）
   → sfenRecord の 0..selectedPly を残し、末尾を正規化した開始SFENに置き換える

4. MatchCoordinator::configureAndStart()
   a. alignGameMovesWithSfenHistory()
      → gameMoves を sfenRecord.size() - 1 件にそろえる（この例では手順 1 で30件にしてあるので変わらない。
        手順 3 やプリセットからの開始で局面列が短くなったときは、その長さまで切り詰める）
        （開始する局面より後の手が残っていると、moveCommitted の手数や待ったが前の手を数えてしまうため）
   b. 30手目のSFENを使って盤面を初期化

5. 対局中の指し手追加（各手ごとに繰り返し）
   LiveGameSession::addMove(move, displayText, sfen, elapsed)
   a. ply = anchorPly(30) + moveCount + 1  → 31, 32, 33, ...
   b. parent = m_liveParent（初回は m_branchPoint = 30手目のノード）
   c. tree->addMoveQuiet(parent, move, displayText, sfen, elapsed)
      → 30手目ノードの子として新ノードを追加
      → 既存の31手目（本譜）と並列する分岐として作成される
   d. m_liveParent = 新ノード（次の手の親になる）
   e. emit moveAdded()
      → 棋譜欄に新しい行が追加される（行は GameRecordPresenter が棋譜欄のモデルに足す）
      → KifuDisplayCoordinator::onLiveGameMoveAdded() がカーソルを新ノードに合わせる

6. 対局終了時
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
1. GameSessionOrchestrator::initializeGame()
   → MainWindowGameStartService::prepareForInitializeGame()
     （表示中の手順から局面列・指し手列を作り直す。手順が空なら何もしない）
   → GameStartCoordinator::initializeGame(Ctx)
   → emit requestPreStartCleanup()

2. PreStartCleanupHandler::performCleanup()
   a. 現在位置を保存
      - m_savedCurrentSfen = "" または "startpos"
      - m_savedSelectedPly = navState->currentPly()（0）

   b. isStartFromCurrentPosition() = false
      （currentSfen が空または初期局面）

   c. cleanupKifuModel(false) → 0
      → 棋譜欄を全クリアし、「=== 開始局面 ===」ヘッダ行のみ追加
      → navState->goToRoot() でカーソルを開始局面へ戻す

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
   → alignGameMovesWithSfenHistory() で gameMoves を局面列にそろえる（局面列は開始局面の1件なので空になる）
   → initializeNewGame(sfenStart) で盤面を初期化
   → 対局モードに応じたエンジン起動

4. 対局中の指し手追加（各手ごとに繰り返し）
   LiveGameSession::addMove(move, displayText, sfen, elapsed)
   a. ply = anchorPly(0) + moveCount + 1  → 1, 2, 3, ...
   b. parent = m_liveParent（初回は root）
   c. tree->addMoveQuiet(parent, move, ...)
      → ルートの子として本譜のノードが作成される
   d. m_liveParent = 新ノード
   e. emit moveAdded()

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

1. GameSessionOrchestrator::initializeGame()
   → rebuildMatchHistoryFromLine()
      ├─ sfenRecord = 開始局面〜20手目の後の局面（21件）
      └─ gameMoves  = 1〜20手目（前回の対局の21〜80手目は残らない）
   → GameStartCoordinator::initializeGame(Ctx)
   → emit requestPreStartCleanup()

2. PreStartCleanupHandler::performCleanup()
   a. 現在位置を保存
      - m_savedCurrentNode = 20手目のノード（前回対局で作成されたツリーのノード）
      - m_savedSelectedPly = navState->currentPly() = 20

   b. isStartFromCurrentPosition() = true

   c. cleanupKifuModel(true) → 20
      - pathToNode(20手目) で経路取得: [root, 1手目, ..., 20手目]
      - 棋譜欄を経路のノードで再構築（0〜20手目を表示）

   d. ensureBranchTreeRoot()
      → ルートは前回対局時に作成済み → 何もしない
      → 前回の本譜（80手＋投了）はツリーにそのまま残る

   e. startLiveGameSession()
      → LiveGameSession::startFromNode(20手目のノード)
         ├─ m_branchPoint = 20手目のノード
         └─ emit sessionStarted(branchPoint)

3. MatchCoordinator::configureAndStart()
   → alignGameMovesWithSfenHistory() で gameMoves を20件にそろえる

4. 対局中の指し手追加
   LiveGameSession::addMove(...)
   → ply = 20 + moveCount + 1  → 21, 22, 23, ...
   → 20手目ノードの子として新ノードを追加
   → 既存の21手目（前回の本譜）と並列する分岐

5. 対局終了
   LiveGameSession::commit()
```

**対局後のツリー構造（例: 20手目から分岐して40手目で投了）**:

```
root → 1手目 → ... → 20手目 → 21手目(前回本譜) → ... → 80手目 → [投了]
                          │
                          └→ 21手目(今回分岐) → ... → 40手目 → [投了]
```

### 8.5 対局中に「待った」をした場合

待ったで取り消した手順はツリーから削除せず、分岐として残す。

```
前提: 直接対局（8.3）で6手目まで進んだ後、待った（2手戻し）をして別の手を指した。

1. UndoFlowService::undoLastTwoMoves()
   → MatchCoordinator::undoTwoPlies()（盤面・対局の局面列・指し手列・棋譜欄の末尾2行を戻す）
   → GameRecordPresenter::syncCurrentMoveIndexToLastRow()（棋譜欄の手数を最後の行に合わせる）
   → LiveGameSession::undoMoves(2)
      ├─ emit movesAboutToBeUndone(4手目のノード)
      │    → KifuDisplayCoordinator がカーソル（表示中の手数）を4手目のノードへ動かす
      ├─ m_liveParent を4手目のノードへ戻す（5・6手目のノードは残す）
      └─ 5手目のノードを m_undoneNodeIds に記録（このセッションで追加したノードの場合のみ）

2. 別の5手目を指す
   LiveGameSession::addMove(...)
   → 4手目のノードの子として新ノードを追加
   → placeBeforeUndoneSiblings(): 取り消した5手目より前へ移す（moveChildQuiet）
   → 実際に指した手順が本譜、取り消した手順が分岐になる

3. 同じ5手目を指し直した場合
   → findMatchingChild() で取り消したノードを再利用し、分岐は増えない
```

**対局後のツリー構造（例: 4手目の後で待ったをして別の手を指し、8手目で投了）**:

```
root → 1手目 → ... → 4手目 → 5手目(指し直し) → ... → 8手目 → [投了]
                          │
                          └→ 5手目(待ったで取り消し) → 6手目
```

以前の棋譜から再利用した手（8.2・8.4で既存ノードに合流した手）を取り消した場合は記録しないため、以前の本譜の並びは変わらない。
棋譜欄は対局中も現在手までを表示し、分岐のできた手には `KifuDisplayPresenter::populateBranchMarks()` で「+」を付ける。
終局後は `PlayModePolicyService::isHumanTurnNow()` が false になり盤上の着手を受け付けないため、終局後に分岐を作るときは 8.4 のように「現在の局面」から対局を開始する。

### 8.6 addMove と addMoveQuiet の使い分け

| メソッド | treeChanged シグナル | 使用場面 |
|---|---|---|
| `addMove()` | 発火する | 棋譜読み込み時のツリー構築、セッション commit 時のフォールバック |
| `addMoveQuiet()` | 発火しない（nodeAdded のみ） | ライブ対局中のリアルタイム追加 |

`addMoveQuiet()` が対局中に使われる理由: `treeChanged` が発火すると
`KifuDisplayCoordinator::onTreeChanged()` が棋譜モデルを再構築し、
対局中のエンジン思考に干渉するため。対局中は `nodeAdded` のみで最小限の通知を行い、
棋譜欄には `GameRecordPresenter` が1行ずつ追記する（棋譜欄は追加した行だけを描き直す）。

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
