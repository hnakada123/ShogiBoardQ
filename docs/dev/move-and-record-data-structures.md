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
| `ShogiBoard::applyUsiMove()` | `src/core/shogiboard.cpp` | `UsiMove` を盤と駒台に適用する（合法性は見ない）。読み筋の盤・局面列の再生（`SfenReplay`）・棋譜の読み書き・対局の着手の確定（`ShogiGameController`）が使う |
| `ShogiGameController::applyMove()` | `src/game/shogigamecontroller.cpp` | 対局で指す。エンジンの手は合法手か確かめ、反則手は盤を変えずに断る（`std::nullopt`）。通信対局の手（サーバーが裁定する）は `applyRecordedMove()`、盤のクリック操作は `validateAndMove()` が成りを選ばせてから同じ確定処理を通る。どれも指した手を `RecordEntry`（4.4）で返す |
| `ShogiNotation` | `src/core/shoginotation.h` | 全角の筋・漢数字の段、駒の名前（指し手用は成香・成桂・成銀を2文字、局面図用は杏・圭・全）、CSA の駒種 |
| `KifMoveText::format()` | `src/core/kifmovetext.h` | 「▲７六歩(77)」「△同　銀(31)」「▲５五角打」の形の指し手。対局の棋譜・棋譜の読み込み（USI・USEN・CSA・JKF）・通信対局・定跡・詰将棋の読み筋が使う |
| `SfenReplay` | `src/core/sfenreplay.h` | 開始局面から USI の手を順に指して局面列を作る（合法手かは確かめない。棋譜の読み込み用）。局面は `ShogiBoard` で持つ。以前の `SfenPositionTracer`（`src/board`）を置き換えた |

SFEN の持ち駒は、盤（`ShogiBoard::convertStandToSfen`）も含めて標準の並び（先手の飛角金銀桂香歩、続いて後手）で書く。
以前の盤は先後を種類ごとに交互に書いていたので、局面を比べるキーは `SfenUtils::normalizeSfenKey()`（持ち駒を並べ直す）で作る。

エンジンの指し手は `Usi::matchMoveReady(UsiMove, ...)` で届き、共有の成りフラグ（以前の `ShogiGameController::setPromote()`）は無い。

SFEN の差分から指し手を推測する処理（以前の `UsiMoveConverter`、定跡のマージ用の `generateUsiMovesFromSfen`、USEN 書き出しの `inferUsiFromSfenDiff`）は無い。
棋譜の書き出し・自動化 API・定跡のマージに使う USI の指し手は、分岐ツリーのノードの手（`ShogiMove::move`。`UsiMove` をそのまま持つ）から作る（4.4 の `RecordLine`）。
以前の `ShogiUtils`（`src/core/shogiutils.*`）も無い。表記の関数は `ShogiNotation` と `KifMoveText` にある。
KI2 の読み書き（`Ki2MoveNotation`）も、盤は `ShogiBoard`、指し手は `UsiMove` で扱う。読み込みは局面の合法手
（`EngineMoveValidator::legalMoves`）から指し手を選び、書き出しは分岐ツリーのノードの手と局面から修飾語・「打」・「不成」を決める。
反則手を書いた棋譜のために、合法手で決まらない手だけは駒の動きだけで決める（`EngineMoveValidator::pseudoLegalMoves`）。
JKF の書き出し（`JkfExporter`・`JkfFormatter`）も、分岐ツリーのノードの手を盤に適用しながら、局面と指し手から
`piece`・`capture`・`promote`・`same`・`relative` を決める（7.6）。終局はノードの終局の種類（`TerminalType`）から `special` にする。

CSA の読み込み（`CsaLexer`）も、開始局面（PI・P1〜P9・P+/P-・00AL）を `ShogiBoard` に組み立て、指し手を `UsiMove` として
盤に適用する。移動元の駒と指し手の駒種が合わない手・持っていない駒を打つ手は読まない。
思考情報の読み筋の漢字化（`PvNotationFormatter`）も、思考を始めた局面（`ThinkingInfoPresenter::setBaseSfen`）を
`ShogiBoard` に持ち、info 行ごとに作業用の盤へ写して（`ShogiBoard::copyPositionFrom`）指し手を進め、`KifMoveText::append` で書く。
詰将棋の局面生成（`TsumeshogiPositionGenerator`）と駒を1枚除く処理（`TsumeshogiGenerator`）も `ShogiBoard` で局面を持つ。

2つの局面の差分（`SfenDiffUtils`、`src/core/sfendiffutils.h`。盤は `ShogiBoard` で読む）から手を求める所が残っている（意図して残している）:
盤のハイライト（`BoardSyncPresenter`）と読み筋の盤（`PvBoardController`、`src/dialogs`）。
どちらも別に持った指し手の一覧ではなく、手順の局面（分岐ツリー・読み筋）どうしを比べる。
待ったの後の「同」の基準は、局面の差分ではなく対局の手順の手の移動先（`RecordLine::destinationOf()`）から決める（`MatchCoordinator::undoTwoPlies()`。以前の `MatchUndoHandler` は無い）。

---

## 1. 指し手データ（ShogiMove）

**ファイル**: `src/core/shogimove.h` / `src/core/shogimove.cpp`

### 1.1 構造体定義

```cpp
struct ShogiMove {
    UsiMove move;                       // 指し手（移動元・移動先・成り・打つ駒）
    Piece movingPiece = Piece::None;    // 動かした駒（指す側の色の、成る前の駒）
    Piece capturedPiece = Piece::None;  // 取った駒（なければ None）

    bool isValid() const;      // 指し手が有効で、動かした駒がある
    bool isDrop() const;       // 駒打ちか（move.isDrop()）
    bool isPromotion() const;  // 成るか（move.promote）
    Turn mover() const;        // 指した側（動かした駒の色）
};
```

`ShogiMove` は指した手の記録である。指し手そのものは `UsiMove`（0章）で持ち、指す前の局面で決まる「動かした駒」「取った駒」を添える。
以前の0始まりの座標（`fromSquare`・`toSquare`）・成りのフィールド（`isPromotion`）と、`UsiMove` との変換（`fromUsiMove()`・`toUsiMove()`）は無い。

### 1.2 座標系

座標は2種類ある。どちらも将棋の筋・段の数字をそのまま使う（筋は右から 1〜9、段は上から 1〜9）。

| 座標 | 使う所 | 盤上 | 駒台 |
|---|---|---|---|
| 表示座標（`QPoint`） | 盤のクリック・ハイライト（`BoardCoordinates`、`src/core/boardcoordinates.h`） | `x` が筋、`y` が段。「7七」は `(7, 7)` | 先手 `x = 10`、後手 `x = 11`（駒箱は `x = 12`）。`y` は駒の並び |
| `BoardSquare`（`UsiMove` の `from`・`to`） | 指し手の値（`ShogiMove::move`）。USI の `7g` は `{7, 7}` | `file`・`rank` とも 1〜9（0 は「なし」） | 無い。駒打ちは `UsiMove::drop`（打つ駒を先手の生駒で表す）で持ち、`from` は無効 |

駒台の段は駒の並びを表す。先手の駒台は歩1・香2・桂3・銀4・金5・角6・飛7・玉8、後手の駒台は玉2・飛3・角4・金5・銀6・桂7・香8・歩9（`BoardCoordinates::standPieceAt()`）。

表示座標と指し手の変換は `BoardCoordinates` だけで行う。`fromSquare(move, mover)` は移動元（駒打ちは指す側の駒台のマス）、`toSquare(move)` は移動先、`usiMoveFromSquares(from, to, promote)` はクリックした2マスを `UsiMove` にする。USI の文字列との変換は `UsiMove::parse()`・`toString()` で行う。デバッグ出力（`operator<<`）は USI の文字列と駒の文字で出す。

### 1.3 駒の表現（Piece）

`movingPiece` と `capturedPiece` は `Piece`（`src/core/shogitypes.h`）で駒種と先後を表す。`Piece` の値は1文字で、**大文字 = 先手の駒、小文字 = 後手の駒**。駒がないときは `Piece::None`（`' '`）。

| 駒種 | 先手（生駒） | 先手（成駒） | 後手（生駒） | 後手（成駒） |
|---|---|---|---|---|
| 玉 / 王 | `BlackKing`（`K`） | — | `WhiteKing`（`k`） | — |
| 飛車 / 龍 | `BlackRook`（`R`） | `BlackDragon`（`U`） | `WhiteRook`（`r`） | `WhiteDragon`（`u`） |
| 角行 / 馬 | `BlackBishop`（`B`） | `BlackHorse`（`C`） | `WhiteBishop`（`b`） | `WhiteHorse`（`c`） |
| 金将 | `BlackGold`（`G`） | — | `WhiteGold`（`g`） | — |
| 銀将 / 成銀 | `BlackSilver`（`S`） | `BlackPromotedSilver`（`T`） | `WhiteSilver`（`s`） | `WhitePromotedSilver`（`t`） |
| 桂馬 / 成桂 | `BlackKnight`（`N`） | `BlackPromotedKnight`（`O`） | `WhiteKnight`（`n`） | `WhitePromotedKnight`（`o`） |
| 香車 / 成香 | `BlackLance`（`L`） | `BlackPromotedLance`（`M`） | `WhiteLance`（`l`） | `WhitePromotedLance`（`m`） |
| 歩兵 / と金 | `BlackPawn`（`P`） | `BlackPromotedPawn`（`Q`） | `WhitePawn`（`p`） | `WhitePromotedPawn`（`q`） |

> **注意**: SFEN では成駒を `+R`・`+B` などの2文字で書く。`Piece` の成駒の1文字（`R→U`、`B→C` など）は内部の値で、SFEN の文字列には出さない。SFEN との変換は `ShogiBoard`（`src/core/shogiboard.cpp`）が行い、文字との変換は `charToPiece()`・`pieceToChar()` を使う。

### 1.4 データ例

#### 通常の移動: ▲７六歩（77→76、USI `7g7f`）

```
move:          from {7, 7}, to {7, 6}, promote false
movingPiece:   Piece::BlackPawn    // 先手の歩
capturedPiece: Piece::None         // 取った駒なし
```

#### 駒取り: ▲２二角成（88→22で角が銀を取って成る、USI `8h2b+`）

```
move:          from {8, 8}, to {2, 2}, promote true
movingPiece:   Piece::BlackBishop  // 成る前の駒（先手の角）
capturedPiece: Piece::WhiteSilver  // 後手の銀を取った
```

#### 駒打ち: △４五桂（後手が持ち駒の桂馬を打つ、USI `N*4e`）

```
move:          drop Piece::BlackKnight, to {4, 5}（from は無効）
movingPiece:   Piece::WhiteKnight  // 指す側の色の駒（後手の桂馬）
capturedPiece: Piece::None         // 取った駒なし（打ちなので）
```

> 駒打ちかどうかは `isDrop()`（`UsiMove::drop` が `None` でない）で判断する。`drop` は先手の生駒で持ち、指す側の色の駒は `UsiMove::dropPieceFor(mover)` で得る。盤に描く移動元は後手の駒台の桂（表示座標 `(11, 7)`、`BoardCoordinates::fromSquare(move, Turn::White)`）。

---

## 2. 棋譜ツリーデータ（KifuBranchTree / KifuBranchNode）

### 2.1 KifuBranchNode（ノード）

**ファイル**: `src/kifu/kifubranchnode.h`

棋譜の1手（または開始局面）を表すノード。N 分木の各頂点。

```cpp
class KifuBranchNode {
    int         m_nodeId = -1;  // ノードID（ツリー内で一意、1から連番）
    int         m_ply = 0;      // 手数（0=開始局面、1以降=指し手。終局手は親の手数 + 1）
    RecordEntry m_record;       // この行（指した手か終局）とコメント・しおり（4.4）

    KifuBranchNode*        m_parent = nullptr;  // 親ノード（nullptr=ルート）
    QList<KifuBranchNode*> m_children;          // 子ノードリスト（KifuBranchTree が所有）
};
```

表示テキスト（`displayText()`。例: `"▲７六歩(77)"`）・この局面の SFEN（`sfen()`）・指し手（`move()`。開始局面と終局手では無効）・
コメント・しおり・消費時間（`time()`・`timeText()`）・終局手の種類（`terminalType()`）と終局を記録した側（`terminalSide()`）は、
どれも `m_record` から読む。`record()` は行をそのまま返す。

**主要な判定メソッド**:

| メソッド | 意味 |
|---|---|
| `isMainLine()` | 親の最初の子 `children[0]` か（祖先は見ない。表示上の本譜判定は `KifuNavigationState::isOnMainLine()`） |
| `isTerminal()` | 終局手かどうか（`terminalType() != None`） |
| `isActualMove()` | 盤面を変化させる指し手か（終局手でなく ply>0） |
| `hasBranch()` | 分岐があるか（子が2つ以上） |
| `lineIndex()` | このノードが属するラインのインデックス（0=本譜） |

ルートノードは `parent()` が `nullptr` のノードである。

### 2.2 TerminalType（終局手の種類）

**ファイル**: `src/core/terminaltype.h`

```cpp
enum class TerminalType {
    None,           // 通常の指し手
    Resign,         // 投了
    Checkmate,      // 詰み
    Repetition,     // 千日手
    PerpetualCheck, // 王手千日手（棋譜ファイルの終局語。対局では王手を続けた側の反則として記録する）
    Impasse,        // 持将棋
    Timeout,        // 時間切れ
    IllegalWin,     // 反則勝ち
    IllegalLoss,    // 反則負け
    Forfeit,        // 不戦敗
    Interrupt,      // 中断
    NoCheckmate,    // 不詰（詰将棋用）
    DeclarationWin, // 入玉宣言勝ち
    Draw,           // 引き分け
    MaxMoves,       // 最大手数到達
    Error,          // エラー
    ForfeitWin      // 不戦勝
};
```

終局語（「投了」「中断」など）との対応は `TerminalTypes` の表（`label()`・`fromWord()` など）だけで決める。
終局の行の表記は `RecordEntry::ending(type, side)` が `▲`／`△` と `TerminalTypes::label()` から作る。

### 2.3 KifuBranchTree（ツリー）

**ファイル**: `src/kifu/kifubranchtree.h`

N 分木全体を管理するデータモデル。ルートノードからの木構造と、`nodeId → ノード` の逆引きハッシュを保持する。

```cpp
class KifuBranchTree : public QObject {
    KifuBranchNode*             m_root = nullptr;  // ルートノード
    QHash<int, KifuBranchNode*> m_nodeById;        // nodeId → ノード の検索用
    int                         m_nextNodeId = 1;  // 次に振るID（1始まり連番。クリアしても再利用しない）

    int  m_batchDepth = 0;              // 一括更新（beginBatchUpdate/endBatchUpdate）の入れ子の深さ
    bool m_treeChangedPending = false;  // 一括更新中に変更があったか

    // allLines() キャッシュ
    mutable QList<BranchLine> m_linesCache;              // DFS走査結果のキャッシュ
    mutable bool              m_linesCacheDirty = true;  // キャッシュ無効化フラグ
    quint64                   m_revision = 0;            // 構造変更の回数（revision()）
};
```

**allLines() キャッシュ**: `allLines()` はツリー全体をDFS走査して全ラインを構築するが、
結果をキャッシュし、ツリー構造が変更されるまで再利用する。キャッシュは、ノードの追加（`addRecord()`・`addMove()`・`addTerminalMove()`・`addMoveQuiet()`）、
`clear()`、ノードの削除（`removeLeaf()`・`removeSubtree()`）、子の並べ替え（`moveChild()`・`moveChildQuiet()`・`promoteToMainLine()`）で無効化され、そのたびに `revision()` が増える。
コメント変更（`setComment()`）ではキャッシュは無効化されない（`BranchLine::nodes` はポインタを保持しており、ノードデータの変更はポインタ経由で自動的に反映される）。

**変更の通知**: 構造の変更は `treeChanged` で知らせる。`beginBatchUpdate()`〜`endBatchUpdate()` の間の変更は、最も外側の `endBatchUpdate()` で1回にまとめる。
`addMoveQuiet()`・`removeLeafQuiet()`・`moveChildQuiet()` は通知しない。終局手を追加すると、そのノードは親と同じ局面の SFEN と無効な指し手を持つ。

**本譜と分岐の規約**:
- **本譜（mainline）**: 各ノードの `children[0]`（最初の子）を辿った経路
- **分岐**: `children[1]` 以降の子が分岐手。同じ手数で異なる指し手を表す

### 2.4 BranchLine（ライン情報）

```cpp
struct BranchLine {
    int lineIndex = 0;                      // 0=本譜、1以降=分岐
    QString name;                           // "本譜", "分岐1", "分岐2"...
    QList<KifuBranchNode*> nodes;           // ノード列（ルートから終端まで）
    int branchPly = 0;                      // 分岐開始手数（本譜は0）
    KifuBranchNode* branchPoint = nullptr;  // 分岐点のノード（本譜はnullptr）
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
             [△中断]          [△中断]           nodeId=6,7, ply=4
```

**ツリー内部データ**:

| nodeId | ply | displayText | parent | children | isMainLine |
|---|---|---|---|---|---|
| 1 | 0 | "開始局面" | null | [2] | — |
| 2 | 1 | "▲７六歩(77)" | 1 | [3] | true |
| 3 | 2 | "△３四歩(34)" | 2 | [4, 5] | true |
| 4 | 3 | "▲２六歩(26)" | 3 | [6] | true（children[0]）|
| 5 | 3 | "▲６六歩(66)" | 3 | [7] | false（children[1]）|
| 6 | 4 | "△中断" | 4 | [] | true（親の最初の子） |
| 7 | 4 | "△中断" | 5 | [] | true（親の最初の子。祖先は見ない） |

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

棋譜欄・分岐候補欄の1行に対応する軽量な表示データ。値コピーで使われる。

```cpp
struct KifDisplayItem {
    QString prettyMove;   // 表示用テキスト（例: "▲７六歩"）
    QString timeText;     // 消費時間テキスト
    QString comment;      // コメント
    QString bookmark;     // しおり
    int     ply = 0;      // 手数（0始まり）

    // 表示と保存で共有する局面・終局の情報
    QString beforeSfen;                              // 指す前の局面
    QString usiMove;                                 // 指し手（USI。終局手は空）
    bool terminal = false;                           // 終局手か
    TerminalType terminalType = TerminalType::None;
    Turn terminalSide = Turn::Black;                 // 終局を記録した側
};
```

`Q_DECLARE_METATYPE` により、シグナル/スロット引数として `KifDisplayItem`・`QList<KifDisplayItem>` を受け渡し可能。

### 3.2 KifuDisplay（Qt モデル用オブジェクト）

**ファイル**: `src/models/kifudisplay.h`

`KifuRecordListModel` のアイテムとして使われる `QObject` 派生クラス。棋譜欄の各行表示データを保持する。

```cpp
class KifuDisplay : public QObject {
public:
    QString beforeSfen;      // 指す前の局面（表記を作り直すときに使う。KifuPresentation::label()）
    QString usiMove;         // 指し手（USI）
private:
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
    bool m_hasBackToMainRow = false;   // 末尾に「本譜へ戻る」行を表示するか
    int  m_currentHighlightRow = 0;    // 現在ハイライト行
};
```

- 候補手の行の後に、任意で「本譜へ戻る」行を置く（`isBackToMainRow()`）
- 候補は、`KifuSelectionSync::applyBranchCandidates()` が `KifuNavigationState::branchCandidatesAtCurrent()`（現在ノードの親の子。分岐が無ければ空）から `KifDisplayItem` を作って設定する（`updateBranchCandidates()`）

---

## 4. ナビゲーション状態（KifuNavigationState）

**ファイル**: `src/kifu/kifunavigationstate.h`

分岐ツリーのカーソル（表示中のノード）と分岐選択の記憶を一元管理する。
表示中の手数は `currentPly()` だけが持つ。MainWindow に手数の変数（以前の `KifuState::activePly`・`KifuState::currentSelectedPly`・`GameState::currentMoveIndex`）は無い。

```cpp
class KifuNavigationState : public QObject {
    KifuBranchTree*  m_tree;                       // 参照するツリー
    int             m_currentNodeId = -1;         // 現在のノードID
    QHash<int, int>  m_selectedChildIds;            // 分岐点ID → 選んだ子のID
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

- `rememberChildSelection(branchPoint, childIndex)`: 添字を子のノードIDへ変換して記録
- `lastSelectedChildAt(branchPoint)`: 記録された子を取得（未選択は0=最初の子）
- `rememberPathSelections(node)`: ルートから `node` までの経路上の全分岐点で、経路が通る子を記録
- `preferredLineIndex()`: 記憶した経路から現在の表示行番号を計算する互換API。行番号を独立した状態として保存しない

これにより、開始局面へ戻ってから再度進めても同じ分岐を辿る。兄弟の並べ替えや削除でも選択したノードは変わらない。現在ノードも参照時にIDで引き直し、削除済みノードを参照しない。ツリーをクリアしてもノードIDは再利用しない。

### 4.3 カーソルを動かす所

`KifuNavigationState` はシグナルを持たない。カーソルは次の所が動かし、表示の更新は `KifuNavigationController` のシグナル（6.2）などで行う。

| 場面 | 動かす所 |
|---|---|
| ナビゲーションボタン・分岐ツリー欄・分岐候補欄 | `KifuNavigationController`（`goToNode()` など） |
| 棋譜欄のクリック | `KifuDisplayCoordinator::onPositionChanged()` → `KifuNavigationController::goToLine()` |
| 対局の開始・着手・待った | `KifuDisplayCoordinator`（`LiveGameSession` のシグナルを受け、`onLiveGameSessionStarted()` は分岐点か開始局面、`onLiveGameMoveAdded()` は最新手、`onLiveGameMovesUndone()` は戻した局面のノードへ） |
| 終局 | `GameSession::onGameOverStateChanged()`（終局行を書いた後）→ `KifuNavigationCoordinator::syncNavStateToPly()` |
| 棋譜の読み込み | `KifuApplyService`（`goToRoot()`） |
| 新規対局の開始前の整理 | `PreStartCleanupHandler::performCleanup()`（現在の局面から始めるときは指定ノード（終局手なら親）、それ以外はツリーを作り直してルートへ） |
| 中断した対局の再開 | `GameSession::resumeInterruptedGame()`（中断した手の直前のノードへ） |

### 4.4 棋譜の正本と手順の値（RecordLine / RecordEntry）

棋譜の正本は、分岐ツリー（`KifuBranchTree`）とそのカーソル（`KifuNavigationState`）である。
ほかの機能が手順を使うときは、局面列や指し手列を別々の一覧から探さずに、`RecordLine`（`src/kifu/recordline.h`、ヘッダーだけ）を受け取る。

棋譜の1行は `RecordEntry`（`src/kifu/recordentry.h`）で表す。指した手と終局の行のどちらにも使い、
分岐ツリーのノード（2.1）・対局（`MatchCoordinator`）・棋譜の読み込み（`KifLine`、9.1）が同じ型で受け渡す。

```cpp
struct RecordEntry {
    ShogiMove move;            // 指した手（終局の行では無効）
    QString displayText;       // 棋譜欄の表記（"▲７六歩(77)"、"△投了"）
    QString sfen;              // この行の後の局面（終局の行は直前の局面）
    RecordTime time;           // 消費時間
    TerminalType terminal = TerminalType::None;
    Turn mover = Turn::Black;  // 指した側（終局の行は終局を記録した側）
    QString comment;
    QString bookmark;

    static RecordEntry played(const ShogiMove& move, const QString& displayText,
                              const QString& sfen, const RecordTime& time = {});   // 指した手の行
    static RecordEntry ending(TerminalType type, Turn side, const RecordTime& time = {});  // 終局の行
};

struct RecordLine {
    QString startSfen;           // 開始局面（完全な SFEN）
    QList<RecordEntry> entries;  // entries[i] は i+1 手目（終局の行は含めない）
    int currentPly = 0;          // 表示中の手数（0 は開始局面）

    int moveCount() const;
    QString sfenAt(int ply) const;                 // ply 手目の後の局面（0 は開始局面）
    QString lastSfen() const;                      // 最後の局面（手が無ければ開始局面）
    QStringList sfens() const;                     // 開始局面と各手の後の局面
    QList<ShogiMove> moves() const;
    QStringList displayTexts() const;
    UsiMove usiMove(int ply) const;                // ply 手目の指し手（範囲外は無効な値）
    QStringList usiMoves(int upToPly = -1) const;  // upToPly 手目までの USI の指し手（-1 は最後まで）
    BoardSquare destinationOf(int ply) const;      // ply 手目の移動先
    RecordLine upTo(int ply) const;                // ply 手目までの手順（currentPly も ply に合わせる）
    QString positionCommand(int ply) const;        // ply 手目の後の position コマンド（平手なら startpos）
    static RecordLine fromPositions(const QStringList& sfens, const QList<ShogiMove>& moves);
};
```

| 取り出し方 | 中身 |
|---|---|
| `KifuNavigationState::currentRecordLine()` | 表示中の手順（選んでいる分岐。対局中はライブの手も含む）。`currentPly` は表示中の手数 |
| `KifuBranchTree::mainRecordLine()` | 本譜の手順（`currentPly` は0） |
| `KifuBranchTree::toRecordLine(nodes, currentPly)` | 任意のノード列（ライン・`pathToNode()` の結果など）。static |
| `MatchCoordinator::record()` | 対局の手順（対局を始めた手順に、対局で指した手を足したもの） |

どれも終局行（投了・中断など）を含まない。棋譜欄の行は終局行を含むので、棋譜欄の行数と `sfens().size()` は一致しないことがある。

**誰が何を持つか**:

| データ | 持ち主 | 役割 |
|---|---|---|
| 手順・局面・指し手・表記・コメント・しおり・消費時間 | `KifuBranchTree` のノード（`RecordEntry`） | 棋譜の正本 |
| 表示中の手数・選んでいる分岐 | `KifuNavigationState` | 棋譜のカーソル |
| 棋譜欄・分岐候補欄・分岐ツリー欄の表示 | 各モデル（`KifuRecordListModel` など） | ツリーから作り直す写し |
| 対局の手順（`RecordLine`） | `MatchCoordinator`（`record()`） | 対局の作業用の手順。棋譜の読み込みでは書かず、対局開始時に表示中の手順から作る（6.5.4、8.2） |

以前あった並行の一覧（MainWindow の `KifuState::gameUsiMoves`・`positionStrList`・`sfenRecord`・`gameMoves`、`KifuLoadCoordinator::kifuUsiMoves()`、
`GameRecordPresenter` のライブ表示用の一覧 `liveDisp`、`KifuLoadResult` の `sfens`・`moves`・`positionCommands`）は無い。

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

保存・コピーで CSA・USEN・USI の書き出しに渡す本譜の USI の指し手は、`KifuExportController`・`KifuExportClipboard`（`src/ui/coordinators`）が `GameRecordModel::collectMainlineUsiForExport()`（`KifuBranchTree::mainRecordLine().usiMoves()`）から取る。USEN の分岐は、各ラインを `KifuBranchTree::toRecordLine()` にして指し手を取る。

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
│  (goForward/goBack/goToNode/goToLine/selectBranchCandidate/...)    │
│                                                                    │
│  emitUpdateSignals():                                              │
│  emit: navigationCompleted(node)                                   │
│  emit: boardUpdateRequired(sfen)                                   │
│  emit: recordHighlightRequired(ply)                                │
│  emit: branchTreeHighlightRequired(lineIndex, ply)                 │
│  emit: branchCandidatesUpdateRequired(candidates)                  │
│  emit: branchNodeHandled(ply, sfen, previousTo, usiMove)           │
└───────────────────────┬────────────────────────────────────────────┘
                        │ connect (KifuDisplayCoordinator::wireSignals)
                        ▼
┌────────────────────────────────────────────────────────────────────┐
│                    KifuDisplayCoordinator                          │
│                                                                    │
│  onNavigationCompleted(node)                                       │
│    ├─→ ラインが変わったら棋譜欄を作り直す                         │
│    │     (syncRecordViewToCurrentLine → populateRecordModel)       │
│    └─→ emit boardWithHighlightsRequired(sfen, 親の sfen)          │
│          → BranchNavigationWiring → BoardLoadService::             │
│            loadWithHighlights() ──→ 将棋盤更新                     │
│                                                                    │
│  onRecordHighlightRequired(ply)                                    │
│    └─→ 棋譜欄ハイライト行変更                                     │
│                                                                    │
│  onBranchTreeHighlightRequired(lineIndex, ply)                     │
│    └─→ 分岐ツリー欄のハイライト（KifuSelectionSync）              │
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

`branchNodeHandled` は `BranchNavigationWiring` を経て `KifuNavigationCoordinator::handleBranchNodeHandled()` に届き、
表示中の局面（`AppState::currentSfenStr`）・定跡ウィンドウ・検討中のエンジンの局面を合わせる。
`boardUpdateRequired` をつないでいる所は無い（盤は `boardWithHighlightsRequired` で描く）。

### 6.3 ナビゲーション操作の流れ

#### 6.3.1 ナビゲーションボタン（例: 1手進む）

```
1. ユーザーが「→」ボタンをクリック
2. QPushButton::clicked → KifuNavigationController::onNextClicked()
   （編集中のコメントがあれば canLeaveCurrentPosition() で確認し、やめたら移動しない）
3. KifuNavigationController::goForward(1)
   a. KifuNavigationState::nextNode() で次の子ノードを取得
      （分岐選択の記憶がある場合はそちらを優先）
   b. state->setCurrentNode(nextNode)
   c. emitUpdateSignals() で6つのシグナルを一括発行
4. KifuDisplayCoordinator が各シグナルを受信
   a. 棋譜欄のハイライト行を更新
   b. 将棋盤を更新（boardWithHighlightsRequired → BoardLoadService::loadWithHighlights()
      → BoardSyncPresenter::loadBoardWithHighlights()）
   c. 分岐ツリーのハイライトノードを更新
   d. 分岐候補欄を現在位置の候補で更新
```

#### 6.3.2 棋譜欄の指し手をクリック

棋譜欄は RecordPane 内の QTableView で、行の選択をナビゲーション要求に変えてから、ボタンと同じ経路（`KifuNavigationController::goToNode()`）を通る。

```
1. ユーザーが棋譜欄の行をクリック
2. QItemSelectionModel::currentRowChanged
     → RecordPane::onKifuCurrentRowChanged()
     → emit RecordPane::mainRowChanged(row)
3. MainWindowActions::onRecordPaneMainRowChanged(row)（MainWindowComposer が接続）
   （編集中のコメントがあれば、移る前に確認する。やめたら選択と強調を現在の手に戻す）
4. RecordNavigationHandler::onMainRowChanged(row)（src/ui/coordinators/recordnavigationhandler.cpp）
   - ライブ対局中（LiveGameSession::canAddMove()）は何もしない
   - KifuDisplayCoordinator::onPositionChanged(表示中のライン, row, {})
5. KifuDisplayCoordinator::onPositionChanged(lineIndex, ply, sfen)
   - 表示中のラインで ply 手目のノードを探し、今のノードと違えば
     KifuNavigationController::goToLine(lineIndex, ply) → goToNode(node)
   - 以降は 6.3.1 の手順4と同じ
```

対局中は棋譜欄の行が変わっても盤やカーソルを動かさない。カーソルは `KifuDisplayCoordinator::onLiveGameMoveAdded()` が最新手に合わせる。

#### 6.3.3 分岐ツリー欄のノードをクリック

分岐ツリーは `EngineAnalysisTab` 内の `QGraphicsView` を `BranchTreeManager` が描画し、クリックは次の経路で処理される。

```
1. ユーザーが分岐ツリーのノードをクリック
2. BranchTreeManager::eventFilter が MouseButtonRelease を検出
   a. クリック位置のノードを特定
   b. ノードに埋め込まれた ROLE_ROW, ROLE_PLY を取得し、そのノードを強調
   c. emit BranchTreeManager::branchNodeActivated(row, ply)
      → EngineAnalysisTab::branchNodeActivated（中継）
3. BranchNavigationWiring::onBranchNodeActivated(row, ply)（AnalysisTabWiring が接続）
   → KifuNavigationController::handleBranchNodeActivated(row, ply)
   a. 編集中のコメントの破棄をやめたときは、強調を現在の手に戻して終わる
   b. ply == 0 なら、優先ラインを解いてルートノードへ goToNode()
   c. クリックした行と現在のラインで ply 手目のノードが同じ（分岐前の共有ノード）なら、
      現在のラインにとどまる（effectiveRow = 現在のライン）
   d. goToLine(effectiveRow, ply)
      - 分岐ライン（> 0）なら setPreferredLineIndex()、本譜なら resetPreferredLineIndex()
      - ライン内のノード列から ply が一致するノードを探して goToNode(node)
      - 以降は 6.3.1 の手順4と同じ
```

> **共有ノードの判定**: 分岐ツリーでは、分岐前の共有ノードは親ライン（本譜または親の変化）の行に描かれる。
> クリックした行と現在のラインで同じ手数のノードが同一なら、ラインを切り替えない。
> 入れ子の分岐にも対応するため、手数ではなくノードが同一かで比べる。

> **分岐選択記憶の自動更新**: `goToNode()` は `KifuNavigationState::rememberPathSelections(node)` で、
> ルートからノードまでの経路上の全分岐点の選択を記憶する。
> これにより、分岐ツリーで深い分岐のノードをクリックした後に
> 開始局面まで戻って再度進めると、同じ分岐パスが自動的に辿られる。

#### 6.3.4 分岐候補欄の指し手をクリック

分岐候補欄は RecordPane 内の QTableView で、現在位置に分岐がある場合に選択可能な手の一覧が表示される。

```
1. ユーザーが分岐候補欄の行をクリック
2. QTableView::clicked または QTableView::activated
     → RecordPane::branchActivated(index)（クリック直後の activated の二重処理は除く）
3. KifuDisplayCoordinator::onBranchCandidateActivated(index)
   （編集中のコメントの破棄をやめたときは、強調を現在の手に戻して終わる）
   a. 「本譜へ戻る」行の場合:
      → KifuNavigationController::goToMainLineAtCurrentPly()
        - 優先ラインをリセット、分岐選択の記憶をクリア
        - 本譜の同じ手数のノード（本譜が短ければ本譜の最終手）に移動
   b. 通常の分岐候補の場合:
      → KifuNavigationController::selectBranchCandidate(row)
        i.   KifuNavigationState::branchCandidatesAtCurrent() で候補を取得
        ii.  候補ノードのラインを KifuBranchTree::findLineIndexForNode() で求める
        iii. 分岐の場合: state->setPreferredLineIndex(lineIndex)
             本譜の場合: state->resetPreferredLineIndex()
        iv.  emit lineSelectionChanged(lineIndex) — ライン変更を通知（つないでいる所は無い）
        v.   goToNode(candidate) — ノードに移動し全 UI 更新
4. KifuDisplayCoordinator が各シグナルを受信（6.3.1 の手順4と同じ）
   - ラインが変更された場合は棋譜欄の内容を全更新
   - 棋譜欄・将棋盤・分岐ツリー・分岐候補欄を更新
```

> **優先ライン（preferredLineIndex）**: `setPreferredLineIndex()` は、そのラインの経路を分岐選択の記憶（4.2）に書く。
> これにより、分岐選択後に分岐点より前に戻っても `currentLineIndex()` が選択した分岐のインデックスを返し続ける。
> 棋譜欄に表示されるラインが維持され、ユーザーは分岐ラインの中をそのままナビゲーションできる。

### 6.4 KifuDisplayCoordinator の役割

`KifuDisplayCoordinator`（`src/ui/coordinators/kifudisplaycoordinator.h`）は、
ナビゲーションコントローラ・分岐ツリー・ライブ対局セッションからのシグナルと各 UI コンポーネントの間を仲介する統合管理クラス。
棋譜欄の行の作成は `KifuDisplayPresenter`、ハイライトと選択の同期は `KifuSelectionSync` に任せる。

**管理対象**:

| 設定メソッド | 対象 |
|---|---|
| `setRecordPane()` | 棋譜ペイン |
| `setRecordModel()` | 棋譜欄モデル |
| `setBranchModel()` | 分岐候補モデル |
| `setBranchTreeManager()` | 分岐ツリー欄 |
| `setLiveGameSession()` | ライブ対局セッション |
| `setBoardSfenProvider()` | 盤の局面を読む関数（表示の一致の検証に使う） |

**ライン変更検出**: `m_lastLineIndex` と `KifuDisplayPresenter::lastModelLineIndex()`（棋譜欄に作ったライン）で、
ラインが変わった場合のみ棋譜欄の全更新（`populateRecordModel()`）を行い、
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
                  │      KifuDisplayPresenter::populateRecordModel() が
                  │      allLines()[lineIndex].nodes からノード列を取得し、
                  │      各ノードの displayText / comment / bookmark / timeText を読み出して
                  │      KifuRecordListModel の行として再構築する。
                  │      （対局中は KifuDisplayPresenter::presentLiveNode() が1行ずつ足す）
                  │
                  ├──→ 分岐候補欄 (KifuBranchListModel)
                  │      KifuNavigationState::branchCandidatesAtCurrent() の候補ノードから
                  │      KifuSelectionSync::applyBranchCandidates() が行を作って設定する。
                  │
                  ├──→ 分岐ツリー欄 (BranchTreeManager)
                  │      treeChanged を受けた KifuDisplayCoordinator::onTreeChanged() が
                  │      BranchTreeManager::setBranchTree() を呼び、allLines() から
                  │      行の配置（BranchTreeLayout）を作り直して再描画する。
                  │      1つの手順の末尾に1手増えただけなら、その手だけを描き足す。
                  │
                  └──→ 将棋盤 (ShogiBoard)
                         ナビゲーション時に KifuDisplayCoordinator::onNavigationCompleted() が
                         ノードと親ノードの sfen() を boardWithHighlightsRequired で渡す。
```

#### 6.5.3 一元性を担保する仕組み

**読み取り**: 4つのGUI部品はいずれも KifuBranchTree のノードからデータを読み取る。棋譜欄・分岐候補欄はナビゲーション時にツリーから再構築され、分岐ツリー欄は `treeChanged` で再描画される。将棋盤はノードの SFEN を直接使用する。GUI部品が独自のデータコピーを長期保持することはなく、表示更新のたびにツリーから最新データを取得する。

**書き込み**: コメント・しおりの編集は GameRecordModel 経由で行われ、表示中のラインのノードに書いたあと、変更シグナルで棋譜欄の行とコメント欄を更新する（セクション5.2）。
対局の指し手は、`MatchCoordinator` が確定した行（`RecordEntry`）を棋譜の受け手（`MatchRecordSink`。実体は `GameSession`）に渡し、
`LiveGameSession::appendRecord()` が `KifuBranchTree::addRecord()` でツリーにノードを追加する（8.6）。

**ナビゲーション**: KifuNavigationState が「どのラインのどの手にいるか」を一元管理する。GUI部品は自身の位置状態を持たず、ナビゲーション操作はすべて KifuNavigationController → KifuNavigationState → シグナル → KifuDisplayCoordinator の経路で各GUI部品に通知される。

#### 6.5.4 補足: 対局の手順の位置づけ

対局中の手順は `MatchCoordinator` が `RecordLine`（`record()`）で持つ。棋譜の正本ではなく、対局の作業用の手順である。
以前の MainWindow の `KifuState::sfenRecord`（局面列）・`KifuState::gameMoves`（指し手列）と、それを司令塔にポインタで渡す仕組みは無い。

- 棋譜の読み込み（`KifuApplyService`・`KifuLoadCoordinator`）はこの手順を書かない
- 対局開始時に、表示中の手順（`MatchStartOptions::recordLine`）を開始する手数まで切って作る（`MatchCoordinator::start()`、8.2）
- 一手を確定するたびに、`MatchCoordinator` が手順に行を足し、同じ行を `MatchRecordSink::appendRecord()` で渡して分岐ツリーに積む。
  待ったでは末尾2手を取り除き、`MatchRecordSink::undoRecords(2)` で分岐ツリー側も戻す（8.5）
- 千日手の判定（`RecordLine::sfens()`）、エンジンへの `position`（`RecordLine::positionCommand()`）、待ったの後の「同」の基準（`destinationOf()`）、
  中断の保存（`InterruptedGameState::record`）はこの手順を使う

GUI部品の表示（盤面・棋譜欄のクリックなど）は、この手順ではなく分岐ツリーから読む。
対局中にエンジンの評価値を描く手数は、`MatchCoordinator::engineMovePlayed(side, ply)` の `ply`（この手順の手数）を使う。

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
    { "move": { "color": 0, "from": {"x":7,"y":7}, "to": {"x":7,"y":6}, "piece": "FU" } }
  ]
}
```

書き出し（`JkfExporter`）は、分岐ツリーのノードの指し手（USI）を開始局面から盤（`ShogiBoard`）に適用しながら、
指す前の局面と指し手で `move` を作る（`JkfFormatter::moveToJkf`）。棋譜欄の表示の文字列は使わない。

| 項目 | 決め方 |
|---|---|
| `from` | 盤上の駒を動かす手の移動元。駒打ちには無い |
| `piece` | 動かす前の駒（成る手でも成る前の駒） |
| `same` | 直前の手と同じマスへ指す手で `true`。変化の最初の手は分岐点の直前の手と比べる |
| `capture` | 取る駒。成駒は成駒の種類（`UM` など） |
| `promote` | 成る手で `true`、成れるのに成らない手で `false`、成れない手では書かない |
| `relative` | KI2 の修飾語と「打」（`Ki2MoveNotation::annotate`）を記号（`L` `C` `R` `U` `M` `D` `H`）にしたもの。同じ駒で同じマスへ動ける駒がほかにあるときだけ |

- 変化は、分岐点で代わられる手（親の先頭の子）の要素の `forks` に、親の2番目以降の子からの手順を入れる。変化の中の分岐も同じ。
- 終局はノードの `TerminalType` から `special` にする（`JkfFormatter::terminalToJkfSpecial`）。反則勝ちは、手番でない側の反則
  （先手番なら `-ILLEGAL_ACTION`）。JKF に無い不戦勝・不戦敗は書かない。
- `initial.preset` は、盤・手番・持ち駒が手合割の開始局面と同じときだけ書き（`JkfMoveParser::presetFromSfen`。
  読み込みと同じ表）、それ以外は `OTHER` と `data`（盤と持ち駒）で書く。

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
   KifuLoadParser::parseFile() がファイルを1回読み、parseContent() → KifToSfenConverter::parseText()
     （行に分けて1回の走査で本譜と各変化を読む。表示データと USI の指し手は同じ走査で作るので並びがずれない）
     ├─ mainline: KifLine（本譜の行 RecordEntry の並び。表示データ・USI の指し手・局面列は行から作る）
     └─ variations: QList<KifVariation>（分岐リスト。parentVariation に親の手順を記録。
        KIF/KI2 は findVariationBase() で復元した親、JKF/USEN は形式上の親を保持する）

3. KifuApplyService::applyParsedResult()
   → validateVariations() で親指定と開始手数を検証してから applyToRecordView()
   a. 対局情報の表示（populateGameInfo・applyPlayersFromGameInfo。applyLoadResult が先に呼ぶ）
   b. displayGameRecord(disp) フック
      → KifuLoadCoordinator::displayGameRecord シグナル → GameRecordLoadService::loadGameRecord()
        （KifuLoadCoordinatorFactory::createAndWire() が接続）
        → GameRecordModel::initializeFromDisplayItems()
        → GameRecordPresenter::displayAndWire() で棋譜欄に行を表示
   c. 対局の手順（MatchCoordinator::record()）には触れない
      （対局開始時に表示中の手順から作る。8.2）

4. KifuApplyService::applyBranchTree() で KifuBranchTree を構築
   a. navState->setCurrentNode(nullptr)  // ダングリングポインタ防止
   b. KifuBranchTreeBuilder::buildFromKifParseResult(tree, result, sfen)
      ├─ tree->clear() + tree->setRootSfen(initialSfen)
      ├─ addKifLineToTree(tree, mainline, {root})  // 本譜
      │   → root → move1 → move2 → ... → endNode
      └─ for each variation:
           i.  parentVariation で親の手順を取得（-1 は本譜）
           ii. 親の経路から startPly-1 手目までを取得
           iii. その経路末尾に変化を追加。同一SFENの別経路とは混同しない
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
| 対局の手順（`MatchCoordinator::record()`） | 読み込みでは変わらない。対局開始時に表示中の手順から作る（8.2） |

### 8.2 棋譜読み込み後、途中局面から対局した場合

`GameSession::startGameFromDialog()` は表示中の手順（`KifuNavigationState::currentRecordLine()`）と現在ノードIDを
`GameStartCoordinator::Ctx` に入れ、`GameStartCoordinator::initializeGame()` に渡す。コーディネータは開始する手数までの手順を
切り出し（`RecordLine::upTo()`）、玉の数を検証してから `requestPreStartCleanup`（`GameStartPreparation::CurrentPosition`）を発行する。
検証に失敗した場合は棋譜・時計・セッションを変更しない。

`GameSession::prepareStart()` は `PreStartCleanupHandler::performCleanup()` で指定ノードまでの棋譜欄を作り直し、
返されたノードから `LiveGameSession::startFromNode()` でライブ対局セッションを始める。
`MatchCoordinator::start()` は確定済みの `MatchStartOptions::recordLine` を対局の手順（`record()`）にする。
USI の `position` はこの手順から生成するため、同じ局面に至る別の分岐を選んでも手順を取り違えない。
SFENの手数欄とツリー上の手数も混同しない。

### 8.3 平手・駒落ちで新しく対局した場合

`GameStartPreparation::NewGame` に開始局面（`rootSfen`）を明示する。
`PreStartCleanupHandler::performCleanup()` がツリーをその局面で作り直し（`KifuBranchTree::setRootSfen()`）、
`GameSession::prepareStart()` がルートからライブセッションを開始する。
`MatchSessionState` が開始オプション（対局者を含む）・終局状態・エンジンの先読みの局面をまとめて所有する。
持ち時間は `GameStartCoordinator::start()` で一度設定し、表示側（`GameSession::applyTimeControl()` → `TimeControlController::presentTimeControl()`）は設定を表示するだけにする。

リセット（`GameSession::resetToInitialState()`）には別の `GameStartPreparation::Reset` を用い、ライブセッションを開始しない。
共有SFENや文字列の特殊値から新規対局・途中再開・リセットを推測する処理はない。

### 8.4 対局後に途中局面から再度対局した場合

8.2と同じ処理で、前回の手順を残したまま選択ノードから対局する。
新しい手は `LiveGameSession::appendRecord()` により、そのノードの子として追加される（同じ手の子が既にあれば、そのノードを使う）。

```text
root → 1手目 → ... → 20手目 → 21手目(前回本譜) → ... → 80手目 → [投了]
                          │
                          └→ 21手目(今回分岐) → ... → 40手目 → [投了]
```

開始処理の詳細と責務は [game-start-flow.md](game-start-flow.md) を参照。

### 8.5 対局中に「待った」をした場合

待ったで取り消した手順はツリーから削除せず、分岐として残す。

```
前提: 直接対局（8.3）で6手目まで進んだ後、待った（2手戻し）をして別の手を指した。

1. メニュー「待った」→ GameSession::undoTwoMoves()
   → MatchCoordinator::undoTwoPlies()
      ├─ 終局後・手順が2手未満・人間の手番でない・棋譜の受け手が戻せない
      │  （MatchRecordSink::canUndoRecords(2)）ときは何もしない
      ├─ 盤を4手目の後の局面（record().sfenAt(4)）に戻し、手番と「同」の基準
      │  （record().destinationOf(4)）を合わせる。対局の手順の末尾2手を取り除き、時計を戻す
      ├─ MatchRecordSink::undoRecords(2) → GameSession::undoRecords(2)
      │    → LiveGameSession::undoMoves(2)
      │       ├─ m_liveParent を4手目のノードへ戻す（5・6手目のノードは残す）
      │       ├─ 5手目のノードを m_undoneNodeIds に記録（このセッションで追加したノードの場合のみ）
      │       └─ emit movesUndone()
      │            → KifuDisplayCoordinator::onLiveGameMovesUndone() がカーソル（表示中の手数）を
      │              4手目のノードへ動かし、棋譜欄を4手目までで作り直す
      └─ emit movesUndone(4) → GameSession::onMovesUndone(4)
           （盤・ハイライト・手番と時計の表示・評価値グラフを4手目に合わせる）

2. 別の5手目を指す
   MatchRecordSink::appendRecord() → LiveGameSession::appendRecord(...)
   → 4手目のノードの子として新ノードを追加
   → placeBeforeUndoneSiblings(): 取り消した5手目より前へ移す（moveChildQuiet）
   → 実際に指した手順が本譜、取り消した手順が分岐になる

3. 同じ5手目を指し直した場合
   → KifuBranchTree::findMatchingChild() で取り消したノードを再利用し、分岐は増えない
```

**対局後のツリー構造（例: 4手目の後で待ったをして別の手を指し、8手目で投了）**:

```
root → 1手目 → ... → 4手目 → 5手目(指し直し) → ... → 8手目 → [投了]
                          │
                          └→ 5手目(待ったで取り消し) → 6手目
```

以前の棋譜から再利用した手（8.2・8.4で既存ノードに合流した手）を取り消した場合は記録しないため、以前の本譜の並びは変わらない。
棋譜欄は対局中も現在手までを表示し、分岐のできた手には「+」を付ける（表示中ラインの経路から `KifuDisplayPresenter` が計算する）。
終局後は待機中の扱いになり、盤上で指した手は `HumanTurnPolicy::isHumanTurnNow()`（→ `MainWindowActions::canRecordVariationMove()`）で受け付け、
`BranchTreeEditController::recordBoardMove()` が表示中の手の次の手（変化）として分岐ツリーに加える。続きを対局として指すときは、8.4 のように「現在の局面」から対局を開始する。

### 8.6 ツリーへの追加と通知

| メソッド | treeChanged シグナル | 使用場面 |
|---|---|---|
| `addRecord()`（`addMove()`・`addTerminalMove()` も同じ） | 発火する（一括更新中は `endBatchUpdate()` で1回にまとめる） | 棋譜読み込み時のツリー構築（`KifuBranchTreeBuilder`）、ライブ対局（`LiveGameSession::appendRecord()`）、待機中に盤で指した変化（`BranchTreeEditController`） |
| `addMoveQuiet()` | 発火しない | 本体のコードでは使っていない（テストが使う） |

対局中の追加も `treeChanged` を出すが、ライブ対局のセッションに手があるあいだ、`KifuDisplayCoordinator::onTreeChanged()` は棋譜欄と分岐ツリー欄を作り直さない。
棋譜欄には `LiveGameSession::moveAdded` を受けた `KifuDisplayCoordinator::onLiveGameMoveAdded()` が `KifuDisplayPresenter::presentLiveNode()` で1行ずつ追記し、
分岐ツリー欄には追加した1ノードだけを描き足す（`BranchTreeManager::appendNode()`。できなければ全体を作り直す）。

---

## 9. 補足: パース用データ構造

### 9.1 KifParseResult / KifLine / KifVariation

**ファイル**: `src/kifu/kifparsetypes.h`

KIF/KI2/CSA 等のファイル読み込み時のパース結果を格納する中間構造。

```cpp
struct KifLine {
    int startPly = 1;                       // 開始手数
    QString baseSfen;                       // 分岐開始局面SFEN
    std::optional<KifDisplayItem> opening;  // 開始局面の行（コメント・しおり）
    QList<RecordEntry> entries;             // 指した手と終局の行（4.4）

    bool endsWithTerminal() const;               // 終局手で終わるか
    int moveCount() const;                       // 指した手の数（終局の行を除く）
    QStringList usiMoves() const;                // USI指し手列（entries から作る）
    QStringList sfenList() const;                // 局面列（baseSfen と各手の後の局面）
    QList<KifDisplayItem> displayItems() const;  // 表示用データ（entries から作る）
};

struct KifVariation {
    int startPly = 1;          // 変化開始手数
    KifLine line;              // 変化の手順
    int parentVariation = -1;  // -1=本譜、その他=先に定義された変化の添字
};

struct KifParseResult {
    KifLine mainline;                  // 本譜
    QList<KifVariation> variations;    // 親が子より先に並ぶ変化リスト
};
```

パース後、この `KifParseResult` から `KifuBranchTree` が構築される（`KifuBranchTreeBuilder::addKifLineToTree()` が `entries` の各行を `addRecord()` で積む）。
指し手列・局面列・表示データは `entries` から作るので、別々の一覧として持たず、並びがずれない。

### 9.2 KifGameInfoItem

**ファイル**: `src/kifu/kifparsetypes.h`

KIF ヘッダの「キーワード：内容」ペア。

```cpp
struct KifGameInfoItem {
    QString key;    // キーワード（例: "開始日時"）
    QString value;  // 内容（例: "2025/03/02 09:00"）
};
```

以前の `ResolvedRow`（`src/kifu/kifutypes.h`。分岐を含む棋譜を行単位で平坦化した結果）は無い。
ラインごとの表示データと局面列は `KifuBranchTree::displayItemsForLine()`・`sfenListForLine()` で取る。
