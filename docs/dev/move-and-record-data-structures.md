# 指し手データ・棋譜データ構造

ShogiBoardQ の「指し手データ」と「棋譜データ」のソースコード上の構造をまとめたドキュメント。
棋譜欄・分岐候補欄・分岐ツリー欄・将棋盤が連動して動作する仕組みの根幹となるデータモデルを解説する。

---

## 0. 指し手の値（UsiMove）と表記の共通部品

USI の指し手と1対1の値型と、座標・表記の対応表は1か所にまとめてある。
指し手の読み書き・盤への適用・日本語の表記は、次の部品だけを使う（同じ表を別のファイルに書かない）。

| 部品 | ファイル | 内容 |
|---|---|---|
| `UsiMove` / `BoardSquare` | `src/core/usimove.h` | USI の指し手（"7g7f"・"8h2b+"・"P*5e"）と1対1の値。成りも指し手の一部として持つ。`UsiMove::parse()` と `toString()` が USI の読み書きの唯一の実装 |
| `BoardCoordinates` | `src/core/boardcoordinates.h` | 盤面表示の座標（1始まり、駒台は筋10・11、駒箱は筋12）と駒・`UsiMove` の対応。駒台の段と駒の並びはここだけで決める |
| `Position::apply()` | `src/core/shogiposition.cpp` | `UsiMove` を局面の値に適用する（合法性は見ない）。読み筋の漢字化・1手の再生（`SfenReplay`）・棋譜の読み込み（`KifLineBuilder`）と書き出しが使う。駒打ちの扱いは `DropRule`（`RequireHand`・棋譜の読み込み用の `Lenient`） |
| `ShogiBoard::applyUsiMove()` | `src/core/shogiboard.cpp` | 表示の盤に `Position::apply()` で指し、変わったマス・駒台・手番を知らせる。対局の着手の確定（`ShogiGameController::commitMove()`）が使う |
| `ShogiGameController::applyMove()` | `src/game/shogigamecontroller.cpp` | 対局で指す。エンジンの手は合法手か確かめ、反則手は盤を変えずに断る（`std::nullopt`）。通信対局の手（サーバーが裁定する）は `applyRecordedMove()`、盤のクリック操作は `validateAndMove()` が成りを選ばせてから同じ確定処理を通る。どれも指した手を `RecordEntry`（4.4）で返す |
| `ShogiNotation` | `src/core/shoginotation.h` | 全角の筋・漢数字の段、駒の名前（指し手用は成香・成桂・成銀を2文字、局面図用は杏・圭・全）、CSA の駒種 |
| `KifMoveText::format()` | `src/core/kifmovetext.h` | 「▲７六歩(77)」「△同　銀(31)」「▲５五角打」の形の指し手。対局の棋譜・棋譜の読み込み（全形式。`KifLineBuilder` が指し手から作る）・通信対局・定跡・詰将棋の読み筋が使う |
| `SfenReplay` | `src/core/sfenreplay.h` | 局面に USI の1手を指す（`applyLenient()`・`positionAfter()`。合法手かは確かめない）。局面は `Position` で持つ |
| `KifLineBuilder` | `src/kifu/formats/kiflinebuilder.h` | 棋譜の読み込みで1本の手順（`RecordLine`）を組み立てる（全形式で共通。9.3）。手を指したその場で局面を進めて `RecordEntry` を作る |
| `UsiPosition` | `src/core/usiposition.h` | エンジンに探索させる局面（開始局面・指し手の列・指した後の局面・直前の手）。`command()` が position コマンドを作り、`RecordLine::usiPosition()` が手順から作る |

局面の値は `Position`（`src/core/shogiposition.h`。盤81マス・持ち駒・手番・手数）で、SFEN の読み書きと指し手の適用はここだけで行う。
表示の盤 `ShogiBoard` はこの値を1つ持ち（`position()`）、変更をシグナルで知らせる包みである。計算のための一時的な局面には `Position` を使う。
SFEN の持ち駒は標準の並び（先手の飛角金銀桂香歩、続いて後手。`PieceRules::kHandOrder`）で書く（`Position::handsSfen()`）。
棋譜ファイルやほかのソフトの SFEN は並びが違うことがあるので、局面を比べるキーは `SfenUtils::normalizeSfenKey()`（読めれば `Position::positionKey()`）で作る。

エンジンの指し手は `Usi::matchMoveReady(const UsiMatchMove&)`（指した側・指し手・考えた時間・評価値をまとめた値）で届く。
成り・不成は `UsiMove` の一部として持ち、指し手とは別の成りのフラグは無い。

棋譜の書き出し・自動化 API・定跡のマージに使う USI の指し手は、SFEN の差分から推測せず、
分岐ツリーのノードの手（`ShogiMove::move`。`UsiMove` をそのまま持つ）から作る（4.4 の `RecordLine`）。
表記の関数は `ShogiNotation` と `KifMoveText` にある。
KI2 の読み書き（`Ki2MoveNotation`）も、局面は `Position`、指し手は `UsiMove` で扱う。読み込みは KIF と同じ文法（`KifMoveGrammar`。KIF の手は「KI2 の手＋移動元 `(xy)`」）で
指し手の文字列を分け、移動元が書いてなければ局面の合法手（`EngineMoveValidator::legalMoves`）から指し手を選び、書き出しは分岐ツリーのノードの手と局面から修飾語・「打」・「不成」を決める。
反則手を書いた棋譜のために、合法手で決まらない手だけは駒の動きだけで決める（`EngineMoveValidator::pseudoLegalMoves`）。
JKF の書き出し（`JkfExporter`・`JkfFormatter`）も、分岐ツリーのノードの手を局面（`Position`）に適用しながら、局面と指し手から
`piece`・`capture`・`promote`・`same`・`relative` を決める（7.6）。終局はノードの終局の種類（`TerminalType`）から `special` にする。
KI2 と JKF の書き出しは、ノードを指す前の局面と一緒にたどる処理を `KifuNodeReplay`（`src/kifu/formats/kifunodereplay.h`）で共有する
（`board()`・`mover()`・`previousTo()` を読んで書き、`advance(node)` で進める。指せない手はノードの SFEN から局面を取り直し、変化は `branchFrom(parent)` でたどる）。

CSA の読み込み（`CsaLexer`）も、開始局面（PI・P1〜P9・P+/P-・00AL）を `Position` に組み立て（`parseStartPos()`）、指し手の行を局面の上の
`UsiMove` にする（`parseMoveLine()`。盤は変えない）。移動元の駒と指し手の駒種が合わない手は読まない。局面への適用は `KifLineBuilder::addMove()` が行い、
持っていない駒を打つ手は指さない（`Position::DropRule::RequireHand`）。
読み筋（`PvText::play`）も、思考を始めた局面（`ThinkingInfoFormatter::setSearchPosition` が探索の `UsiPosition` から決める）を
`Position` にして info 行ごとに指し手を進め、指せた手（指した側・動かした駒）を値で持つ。表記は `KifuPresentation::pv` が `KifMoveText::append` で書く。
読み筋の最初の手の「同」の基準は、探索の局面に至った手（`UsiPosition::lastMove()`。先読み中は予想した相手の手）の移動先である。
詰将棋の局面生成（`TsumeshogiPositionGenerator`）と駒を1枚除く処理（`TsumeCandidateJob`）も `Position` で局面を持つ。

2つの局面の差分（`SfenDiffUtils`、`src/core/sfendiffutils.h`。局面は `Position::fromSfenLike()` で読む）から手を求めるのは、
読み筋の盤の読み筋の値（`PvLine`、`src/dialogs/pvline.h`）だけである。開始局面に至った手（`PvLine::setArrival()` の `lastMove`）が分からないときに、至る前の局面と開始局面の盤を比べて、開始局面で強調する移動元と移動先を決める。読み筋の手を指した後の局面では、指した手そのものから強調する（`PvLine::highlight(ply)`。駒打ちの移動元は打った側の駒台）。
棋譜をたどるときの盤のハイライト（`BoardSyncPresenter`）は、表示中のノードの指し手（`KifuBranchNode::move()`）から求める（6.5.2）。
対局で次の手の「同」の基準は、対局の手順の最後の手の移動先（`MatchCoordinator::lastMoveDestination()` = `RecordLine::destinationOf(currentPly())`）から決め、指すたびに `ShogiGameController` に渡す。待った（`MatchCoordinator::undoTwoPlies()`）や中断の再開の後も、手順から導くので写しを直す処理は無い。

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

### 1.2 座標系

座標は2種類ある。どちらも将棋の筋・段の数字をそのまま使う（筋は右から 1〜9、段は上から 1〜9）。

| 座標 | 使う所 | 盤上 | 駒台 |
|---|---|---|---|
| 表示座標（`QPoint`） | 盤のクリック・ハイライト（`BoardCoordinates`、`src/core/boardcoordinates.h`） | `x` が筋、`y` が段。「7七」は `(7, 7)` | 先手 `x = 10`、後手 `x = 11`（駒箱は `x = 12`）。`y` は駒の並び |
| `BoardSquare`（`UsiMove` の `from`・`to`） | 指し手の値（`ShogiMove::move`）。USI の `7g` は `{7, 7}` | `file`・`rank` とも 1〜9（0 は「なし」） | 無い。駒打ちは `UsiMove::drop`（打つ駒を先手の生駒で表す）で持ち、`from` は無効 |

駒台の段は駒の並びを表す。先手の駒台は歩1・香2・桂3・銀4・金5・角6・飛7・玉8、後手の駒台は玉2・飛3・角4・金5・銀6・桂7・香8・歩9（`BoardCoordinates::standPieceAt()`）。

表示座標と指し手の変換は `BoardCoordinates` だけで行う。`fromSquare(move, mover)` は移動元（駒打ちは指す側の駒台のマス）、`toSquare(move)` は移動先、`usiMoveFromSquares(from, to, promote)` はクリックした2マスを `UsiMove` にする。USI の文字列との変換は `UsiMove::parse()`・`toString()` で行う。デバッグ出力（`operator<<`）は USI の文字列と駒の文字で出す。

### 1.3 駒の表現（Piece）

`movingPiece` と `capturedPiece` は `Piece`（`src/core/shogitypes.h`）で駒種と先後を表す。`Piece` の値は**駒の種類 `PieceKind` の値 ＋ 後手なら 16** で、駒がないときは `Piece::None`（`0xFF`）。`PieceKind` の並び（歩0・香1・桂2・銀3・金4・角5・飛6・玉7、と8・成香9・成桂10・成銀11・馬12・龍13）は合法手生成の `fmv::PieceType` と同じで、合法手生成の盤も同じ値を使う。成る・戻す・先後の反転（`promote()`・`demote()`・`flipSide()`）は値の計算だけの `constexpr` 関数である。

| 駒種 | 先手（生駒） | 先手（成駒） | 後手（生駒） | 後手（成駒） |
|---|---|---|---|---|
| 玉 / 王 | `BlackKing`（7） | — | `WhiteKing`（23） | — |
| 飛車 / 龍 | `BlackRook`（6） | `BlackDragon`（13） | `WhiteRook`（22） | `WhiteDragon`（29） |
| 角行 / 馬 | `BlackBishop`（5） | `BlackHorse`（12） | `WhiteBishop`（21） | `WhiteHorse`（28） |
| 金将 | `BlackGold`（4） | — | `WhiteGold`（20） | — |
| 銀将 / 成銀 | `BlackSilver`（3） | `BlackPromotedSilver`（11） | `WhiteSilver`（19） | `WhitePromotedSilver`（27） |
| 桂馬 / 成桂 | `BlackKnight`（2） | `BlackPromotedKnight`（10） | `WhiteKnight`（18） | `WhitePromotedKnight`（26） |
| 香車 / 成香 | `BlackLance`（1） | `BlackPromotedLance`（9） | `WhiteLance`（17） | `WhitePromotedLance`（25） |
| 歩兵 / と金 | `BlackPawn`（0） | `BlackPromotedPawn`（8） | `WhitePawn`（16） | `WhitePromotedPawn`（24） |

> **注意**: SFEN では成駒を `+R`・`+B` などの2文字で書く。SFEN との変換は `Position`（`src/core/shogiposition.cpp`）が行い、駒と文字の変換は `charToPiece()`（SFEN の駒の文字 `PLNSGBRK` を生駒にする）・`pieceLetter()`（成駒は元の駒の文字）・`pieceSfen()`（成駒は `+P` の形）を使う。
> 行き所のない段・成れる段・持ち駒を書き出す順は `src/core/piecerules.h`（`PieceRules`）に1か所でまとめる。

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

    KifuBranchNode* m_parent = nullptr;                        // 親ノード（nullptr=ルート。非所有）
    std::vector<std::unique_ptr<KifuBranchNode>> m_children;   // 子ノード（所有。先頭ほど本譜側）
};
```

ノードは子を所有し、ルートは `KifuBranchTree` が所有する。木の形（親子・並び・手数・ID）を変えられるのは
`KifuBranchTree`（friend）だけで、ノードの公開の関数は読むだけである（`children()` は `QList` の写しを返す。
`childCount()`・`childAt(index)`・`indexOfChild(child)` で写しを作らずに読める）。
ノードのデストラクタは子孫を再帰せずに順に解放するので、長い手順でもスタックを使い切らない。

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
本体のコードが手を足すのは `addRecord(parent, RecordEntry)` だけである（棋譜の読み込み・対局・盤上の変化）。

```cpp
class KifuBranchTree : public QObject {
    std::unique_ptr<KifuBranchNode> m_root;        // ルートノード（所有。子孫は各ノードが所有）
    QHash<int, KifuBranchNode*> m_nodeById;        // nodeId → ノード の検索用（非所有）
    int                         m_nextNodeId = 1;  // 次に振るID（1始まり連番。クリアしても再利用しない）

    int           m_batchDepth = 0;     // 一括更新（beginBatchUpdate/endBatchUpdate）の入れ子の深さ
    QList<Change> m_pendingChanges;     // 一括更新中に遅らせている構造の変更（古い順）

    // allLines() キャッシュ
    mutable QList<BranchLine> m_linesCache;              // DFS走査結果のキャッシュ
    mutable QHash<const KifuBranchNode*, int> m_lineIndexByLeaf;  // 終端のノード → 手順の番号
    mutable bool              m_linesCacheDirty = true;  // キャッシュ無効化フラグ
    quint64                   m_revision = 0;            // 構造変更の回数（revision()）
};
```

**allLines() キャッシュ**: `allLines()` はツリー全体をDFS走査して全ラインを構築するが、
結果をキャッシュし、ツリー構造が変更されるまで再利用する。走査は再帰せずに明示のスタックで行う（手数の深さの再帰は、長い棋譜でスタックを使い切る）。
ラインはノードの列を持たず終端のノードだけを持つ（手順ごとに列を持つと、手数×変化の数のメモリになる）。`findLineIndexForNode()` は、ノードから最初の子をたどった終端のラインを `m_lineIndexByLeaf` で引く。キャッシュは、ノードの追加（`addRecord()`）、
`clear()`・`setRootSfen()`、ノードの削除（`removeLeaf()`・`removeSubtree()`）、子の並べ替え（`moveChild()`・`promoteToMainLine()`）で無効化され、そのたびに `revision()` が増える。
`revision()` は、棋譜の文書（`KifuDocument`）が保存した時点と比べて未保存かを決めるのと、表示中の手（`KifuNavigationState`）が同じ構造をもう知らせたかを見るのに使う。
コメント変更（`setComment()`）ではキャッシュは無効化されない（`BranchLine` はノードへのポインタを保持しており、ノードデータの変更はポインタ経由で自動的に反映される）。

**変更の通知**: 構造の変更は、変わった所をノード ID で示す型付きのシグナルで知らせる。知らせを出さない変更の関数は無い。

| シグナル | 出す操作 |
|---|---|
| `nodeAppended(parentId, nodeId)` | `addRecord()`（手を親の子の末尾に足した） |
| `childrenReordered(parentId)` | `moveChild()`・`promoteToMainLine()`（後者は並びを変えた親のうち最もルートに近いもの。その先の並びも変わり得る） |
| `subtreeRemoved(parentId, nodeId)` | `removeSubtree()`・`removeLeaf()`（`removeLeaf()` は葉に限った `removeSubtree()`） |
| `reset()` | `clear()`・`setRootSfen()` |

`beginBatchUpdate()`〜`endBatchUpdate()` の間の変更は、最も外側の `endBatchUpdate()` まで遅らせて起きた順に出す。一括更新の中に作り直し（`clear()`・`setRootSfen()`）があれば、`reset()` を1回だけ出す（棋譜の読み込み）。
手の表記・コメント・しおりの変更は構造の変更ではなく、棋譜の文書（`KifuDocument::commentChanged` など）が知らせる。終局手を追加すると、そのノードは親と同じ局面の SFEN と無効な指し手を持つ。

テストで指し手を直接足す・本譜の N 手目や局面で手を探すときは、`tests/branchtree_test_util.h` の `TestTree::addMove()`・`addTerminalMove()`・`findByPlyOnMainLine()`・`findBySfen()` を使う（本体の `KifuBranchTree` には無い）。

**本譜と分岐の規約**:
- **本譜（mainline）**: 各ノードの `children[0]`（最初の子）を辿った経路
- **分岐**: `children[1]` 以降の子が分岐手。同じ手数で異なる指し手を表す

### 2.4 BranchLine（ライン情報）

```cpp
struct BranchLine {
    int lineIndex = 0;                      // 0=本譜、1以降=分岐
    KifuBranchNode* leaf = nullptr;         // 終端のノード（手がなければルート）
    int branchPly = 0;                      // 分岐開始手数（本譜は0）
    KifuBranchNode* branchPoint = nullptr;  // 分岐点のノード（本譜はnullptr）

    QList<KifuBranchNode*> nodes() const;            // ルートから終端まで（呼ぶたびに終端から親をたどって作る）
    QList<KifuBranchNode*> nodesFrom(int fromPly) const;  // fromPly 手目から終端まで
    int length() const;                               // ルートから終端までのノードの数
};
```

変化を書き出す・描くときは `nodesFrom(branchPly)` で変化の手だけを取る（全部の変化で `nodes()` を作ると、手数×変化の数の時間になる）。

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

| lineIndex | branchPly | leaf | nodes() |
|---|---|---|---|
| 0 | 0 | 6 | [1, 2, 3, 4, 6] |
| 1 | 3 | 7 | [1, 2, 3, 5, 7] |

分岐点（ply=2, nodeId=3）より前のノードは両ラインで共有される。

---

## 3. 棋譜欄・分岐候補欄のモデル

棋譜欄と分岐候補欄の行は、分岐ツリーのノードである。モデルは行のノード ID だけを持ち、指し手・消費時間・しおり・コメントの文字列は持たない。
表示するたびにノード（2.1）から作るので、棋譜の正本（分岐ツリー）と表示がずれない。

### 3.1 KifuRecordListModel（棋譜欄モデル）

**ファイル**: `src/models/kifurecordlistmodel.h`

棋譜欄（QTableView）の Qt モデル（`QAbstractTableModel`）。

```cpp
class KifuRecordListModel : public QAbstractTableModel {
public:
    enum Column { MoveColumn, TimeColumn, BookmarkColumn, CommentColumn, ColumnCount };

    void setPath(const QList<KifuBranchNode*>& path);  // 行を手順 path（開始局面から）に合わせる
    KifuBranchNode* nodeAt(int row) const;              // row 行目のノード（範囲外や消えたノードは nullptr）
    void setCurrentHighlightRow(int row);               // 強調する現在の行（-1 は強調しない）
    void refreshNode(int nodeId);                       // そのノードの行を描き直す（コメント・しおりの変更）
    void refreshAll();                                  // 全行を描き直す（表記の変更・分岐の印の変化）

private:
    const KifuBranchTree* m_tree = nullptr;
    QList<int> m_nodeIds;   // 行のノード ID（行0は開始局面）
    int m_currentRow = -1;
};
```

- **行番号**: 行0 = 開始局面、行 n = n 手目（行番号＝手数）。終局の行（投了・中断など）も1行になる
- `setPath()`: 今の行と先頭が同じ部分は残し、違う部分だけ削って足す（対局で1手指すたびに全行を並べ直さない）
- 指し手の列は、手数・指し手の表記（`KifuPresentation::moveLabel()`。表記の設定で作り直す）と、
  分岐のある手（親の子が2つ以上）の印 `+`（`KifuPresentation::branchMark()`）。分岐のある手の行は背景をオレンジ系にする
- どの手順を出すかは `KifuDisplayPresenter`（6.4）が決める

### 3.2 KifuBranchListModel（分岐候補欄モデル）

**ファイル**: `src/models/kifubranchlistmodel.h`

分岐候補欄（QTableView）の Qt モデル。行は表示中の手と同じ親を持つ手（兄弟のノード）で、分岐が無ければ空になる。

```cpp
class KifuBranchListModel : public QAbstractTableModel {
public:
    // 候補・強調する行・「本譜へ戻る」を表示する
    void setCandidates(const QList<KifuBranchNode*>& candidates, int currentRow, bool backToMain);
    int candidateCount() const;           // 「本譜へ戻る」を除いた候補の数
    bool isBackToMainRow(int row) const;  // 候補の後ろの行が「本譜へ戻る」
    void refreshAll();                    // 全行を描き直す（表記の変更）

private:
    const KifuBranchTree* m_tree = nullptr;
    QList<int> m_nodeIds;
    bool m_backToMain = false;
    int m_currentRow = -1;
};
```

- 候補は `KifuNavigationState::branchCandidatesAtCurrent()`、「本譜へ戻る」を付けるかは
  `KifuNavigationState::offersBackToMainLine()`（分岐候補があり、本譜以外を表示しているとき）で決める
- 行の並びは `KifuNavigationController::selectBranchCandidateRow()`（6.3.4）と同じ

---

## 4. ナビゲーション状態（KifuNavigationState）

**ファイル**: `src/kifu/kifunavigationstate.h`

分岐ツリーのカーソル（表示中の手）と、分岐点ごとに選んだ子の記憶を持つ。棋譜の表示はここだけを正とする。
表示中の手数・局面・手順・分岐候補は、すべてノード ID から導き、写しを持たない。
変わるたびに `changed()` を1回出し、棋譜欄・分岐候補欄・分岐ツリー・盤・コメント欄・評価値グラフなどはそれを受けて表示を作り直す（6.2）。

```cpp
class KifuNavigationState : public QObject {
    KifuBranchTree*  m_tree = nullptr;        // 参照するツリー
    int              m_currentNodeId = -1;    // 表示中のノードID
    QHash<int, int>  m_selectedChildIds;      // 分岐点のノードID → 選んだ子のノードID
    quint64          m_notifiedRevision = 0;  // 最後に changed() を出したときのツリーの構造の版
};
```

### 4.1 主要な状態

| メソッド | 説明 |
|---|---|
| `currentNode()` | 表示中のノード |
| `currentPly()` | 表示中の手数 |
| `currentSfen()` | 表示中の局面の SFEN（終局の行は直前の局面） |
| `selectedPath()` | 開始局面から、分岐点で選んだ子をたどった手順の終わりまでのノード列 |
| `currentLineIndex()` | 表示中の手順の番号（0=本譜。`KifuBranchTree::allLines()` の添字） |
| `isOnMainLine()` | 表示中の手順が本譜か |
| `currentRecordLine()` | 表示中の手順の、表示中の手数までの手順（4.4 の `RecordLine`） |
| `nextNode(node)` | `node` の次に進む手（選んだ子。選んでいなければ最初の子） |
| `branchCandidatesAtCurrent()` | 表示中の手と同じ親を持つ手（兄弟が無ければ空） |
| `offersBackToMainLine()` | 分岐候補欄に「本譜へ戻る」を出すか |

### 4.2 分岐選択の記憶

分岐点ごとに選んだ子を、ノード ID の組（`m_selectedChildIds`）で覚える。表示中の手は次の関数で動かし、どれも変わったときだけ `changed()` を1回出す。

| メソッド | 動き |
|---|---|
| `setCurrentNode(node)` | `node` へ移り、開始局面から `node` までの経路の選択を足す |
| `setCurrentNode(node, lineEnd)` | `node` へ移り、開始局面から `lineEnd`（`node` の子孫）までの経路の選択を足す（分岐の手順を指定して移るとき） |
| `resetToNode(node)` | 選択を忘れてから、開始局面から `node` までの経路を記憶して移る（本譜に戻るとき・対局を始めるとき） |
| `goToRoot()` | 選択を忘れて開始局面へ移る |

選択は足していくだけなので、開始局面へ戻ってから再度進めても同じ分岐を辿る（`nextNode()`・`selectedPath()` が選択を使う）。
行番号（ライン番号）は独立した状態として持たず、`selectedPath()` の終わりのノードから `KifuBranchTree::findLineIndexForNode()` で求める。
選択はノード ID で持つので、兄弟を並べ替えても選んだ手順は変わらない。ノード ID は再利用しないので、削除したノードを指したままにもならない。

**ツリーが変わったとき**: `KifuBranchTree` の構造の変更の知らせ（`nodeAppended`・`childrenReordered`・`subtreeRemoved`・`reset` の4つ）を受けた `onTreeChanged()` は、表示中のノードが消えていれば
（棋譜の読み込み・新規など、ツリーを作り直したとき）`goToRoot()` で開始局面へ戻る。
ノードが残っていても構造の版（`KifuBranchTree::revision()`）が変わっていれば `changed()` を出す（分岐が増えると棋譜欄の印が変わるため）。
手を足してから表示中の手を移した場合は、その版をもう知らせているので、後から届くツリーの知らせで同じ通知を繰り返さない。

### 4.3 表示中の手を動かす所

| 場面 | 動かす所 |
|---|---|
| ナビゲーションボタン・棋譜欄・分岐候補欄・分岐ツリー欄・評価値グラフ | `KifuNavigationController`（`onNextClicked()`・`selectRecordRow()`・`selectBranchCandidateRow()`・`selectBranchTreeNode()`・`selectPly()` など。移る前に確認を通す。6.3） |
| 対局の開始 | `LiveGameSession::startFromNode()`（対局を始めた局面のノードへ `resetToNode()`） |
| 対局の着手・終局 | `LiveGameSession::appendRecord()`（足した行のノードへ `setCurrentNode()`） |
| 待った | `LiveGameSession::undoMoves()`（戻した局面のノードへ） |
| 待機中に盤で指した変化 | `BranchTreeEditController::recordBoardMove()`（指した手のノードへ `resetToNode()`） |
| 分岐の並べ替え・削除 | `BranchTreeEditController::apply()`（表示中の手のまま。削除する範囲にあれば削除する手の直前へ） |
| 棋譜の読み込み・新規・局面編集 | ツリーを作り直すと表示中のノードが消えるので、`KifuNavigationState::onTreeChanged()` が開始局面へ戻す |
| 中断した対局の再開 | `GameSession::resumeInterruptedGame()`（中断した手の直前のノードへ `resetToNode()`） |

### 4.4 棋譜の正本と手順の値（RecordLine / RecordEntry）

棋譜の正本は、分岐ツリー（`KifuBranchTree`）とそのカーソル（`KifuNavigationState`）である。
ほかの機能が手順を使うときは、局面列や指し手列を別々の一覧から探さずに、`RecordLine`（`src/kifu/recordline.h`、ヘッダーだけ）を受け取る。

棋譜の1行は `RecordEntry`（`src/kifu/recordentry.h`）で表す。指した手と終局の行のどちらにも使い、
分岐ツリーのノード（2.1）・対局（`MatchCoordinator`）・棋譜の読み込み（`RecordLine`、9.1）が同じ型で受け渡す。
手順の型は `RecordLine` の1つで、棋譜の読み込みの本譜・変化、書き出し、表示中の手順、対局の手順のすべてに使う（文字列で1行を持つ別の型は無い）。

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

    bool isTerminal() const;   // 終局の行か
    static RecordEntry played(const ShogiMove& move, const QString& displayText,
                              const QString& sfen, const RecordTime& time = {});   // 指した手の行
    static RecordEntry ending(TerminalType type, Turn side, const RecordTime& time = {});  // 終局の行
    static Turn endingSide(TerminalType type, Turn sideToMove, Turn loser);         // 終局の行を記録する側
};

struct RecordLine {
    QString startSfen;           // 開始局面（完全な SFEN）
    int startPly = 1;            // 最初の手の手数（変化は分岐した手の手数）
    QString openingComment;      // 開始局面のコメント（本譜だけ）
    QString openingBookmark;     // 開始局面のしおり（本譜だけ）
    QList<RecordEntry> entries;  // entries[i] は i+1 手目。末尾は終局の行のことがある
    int currentPly = 0;          // 表示中の手数（0 は開始局面）

    bool isEmpty() const;                          // 開始局面が無い
    bool endsWithTerminal() const;                 // 終局の行で終わるか
    const RecordEntry* terminal() const;           // 終局の行（なければ nullptr）
    int moveCount() const;                         // 指した手の数（終局の行は数えない）
    QString sfenAt(int ply) const;                 // ply 手目の後の局面（0 は開始局面）
    QString lastSfen() const;                      // 最後の局面（手が無ければ開始局面）
    QStringList sfens() const;                     // 開始局面と各手の後の局面
    QList<ShogiMove> moves() const;
    QStringList displayTexts() const;
    QString displayText(int ply) const;            // ply 手目の表記
    UsiMove usiMove(int ply) const;                // ply 手目の指し手（範囲外は無効な値）
    QStringList usiMoves(int upToPly = -1) const;  // upToPly 手目までの USI の指し手（-1 は最後まで）
    BoardSquare destinationOf(int ply) const;      // ply 手目の移動先
    RecordLine upTo(int ply) const;                // ply 手目までの手順（終局の行は含めない。currentPly も ply に合わせる）
    UsiPosition usiPosition(int ply) const;        // ply 手目の後の局面をエンジンに送る値（command() は平手なら startpos）
    static RecordLine fromPositions(const QStringList& sfens, const QList<ShogiMove>& moves);
};
```

終局の行を記録する側は `RecordEntry::endingSide()` で決める（投了・時間切れ・反則負けは負けた側、入玉宣言勝ち・反則勝ちは勝った側、
千日手・持将棋・中断などの勝敗のない終局は手番側）。対局・通信対局・棋譜の読み込みで同じ規則を使う。

| 取り出し方 | 中身 |
|---|---|
| `KifuNavigationState::currentRecordLine()` | 表示中の手順（選んでいる分岐。対局中はライブの手も含む）。`currentPly` は表示中の手数 |
| `KifuBranchTree::mainRecordLine()` | 本譜の手順（`currentPly` は0） |
| `KifuBranchTree::toRecordLine(nodes, currentPly)` | 任意のノード列（ライン・`pathToNode()` の結果など）。static |
| `MatchCoordinator::record()` | 対局の手順（対局を始めた手順に、対局で指した手を足したもの） |
| `KifuBranchTree::exportLine(lineIndex)` | ラインの手順。末尾の終局行と、開始局面のコメント・しおり（`openingComment`・`openingBookmark`）も含む。書き出し・CLI 用（CLI の変換結果の本譜 `KifuConversionService::Result::mainline` は `exportLine(0)`） |

`exportLine()` と棋譜の読み込み結果（9.1）のほかは終局行（投了・中断など）を含まない。棋譜欄の行は終局行を含むので、棋譜欄の行数と `sfens().size()` は一致しないことがある。

**誰が何を持つか**:

| データ | 持ち主 | 役割 |
|---|---|---|
| 手順・局面・指し手・表記・コメント・しおり・消費時間 | `KifuBranchTree` のノード（`RecordEntry`） | 棋譜の正本 |
| 表示中の手数・選んでいる分岐 | `KifuNavigationState` | 棋譜のカーソル |
| 棋譜欄・分岐候補欄・分岐ツリー欄の表示 | 各モデル（`KifuRecordListModel` など）と `BranchTreeView` | 行・図形はノード ID だけを持ち、表示のたびにノードを読む |
| 対局の手順（`RecordLine`） | `MatchCoordinator`（`record()`） | 対局の作業用の手順。棋譜の読み込みでは書かず、対局開始時に表示中の手順から作る（6.5.4、8.2） |

---

## 5. 棋譜の文書（KifuDocument）

**ファイル**: `src/kifu/kifudocument.h`

開いている棋譜の文書。分岐ツリー・対局情報・対局の記録・ファイルの紐づけを1つにまとめ、コメント・しおりの書き込みと書き出し・保存の窓口になる。
コメント・しおりの正本は分岐ツリーのノードにあり、このクラスはノードに書いて、書いたノードを知らせる。書き出しは `KifuExport`（5.3）が形式ごとの Exporter に任せる。

### 5.1 内部データ

```cpp
class KifuDocument : public QObject {
    KifuBranchTree*        m_tree = nullptr;   // 棋譜の正本（非所有。合成ルートが持ち、文書は中身を置き換える）
    QList<KifGameInfoItem> m_header;           // 対局情報（対局情報欄はこれを編集する画面）
    MatchPlayers           m_players;          // 最後に始めた対局の対局者
    MatchTimeRecord        m_timeRecord;       // 持ち時間と開始・終了の日時（src/kifu/matchtimerecord.h）
    QString                m_filePath;         // 上書き保存先
    quint64                m_annotationRevision = 0;  // コメント・しおりの変更の回数
    SavedState             m_saved;            // 保存（読み込み）した時点のツリーの revision・コメントの回数・対局情報
    bool                   m_unsavedContent = false;  // 貼り付けた棋譜などファイルにない内容
};
```

未保存かどうか（`isModified()`）は変更のたびに印を付けず、ツリーの `revision()`・コメントとしおりの変更の回数・対局情報が
保存した時点（`m_saved`）と違うか、ファイルにない内容を読み込んだか（`markUnsavedContent()`）で求める。
棋譜の追記（対局の手・待機中に盤で指した変化・分岐の編集）はツリーの revision が進むので、それだけで未保存になる（待ったで取り消した手も、対局で足したときに revision が進んでいる）。
保存とファイルの読み込みは `markSaved(path)` で今の内容を保存した時点にする。
新規・読み込み・局面の反映は文書を丸ごと置き換える操作（`startNew()`・`replaceWithRecord()`・`replaceWithPosition()`）で、
前の棋譜の対局者・持ち時間・上書き保存先を消し、置き換えた内容を変更なしとする。

### 5.2 コメント・しおりの書き込み

コメント欄の「コメント更新」は、`CommentCoordinator` が表示中の手のノード（`KifuNavigationState::currentNode()`）に書く:

```
CommentCoordinator::onCommentUpdated(moveIndex, text)
    │
    └─→ KifuDocument::setComment(表示中のノード, text)
          ├─ node->record().comment = text  // 棋譜の正本に書く（同じ文なら何もしない）
          ├─ ++m_annotationRevision         // 保存した時点と違うので未保存になる
          └─ emit commentChanged(nodeId, text)
               ├─ KifuRecordListModel::refreshNode(nodeId)   // そのノードの行だけ描き直す
               └─ CommentCoordinator::showCurrentComment()   // コメント欄
```

しおり（棋譜欄の「しおり」ボタン → `CommentCoordinator::onBookmarkEditRequested()`）も同じで、
`KifuDocument::setBookmark()` がノードに書いて `bookmarkChanged` を出し、棋譜欄がその行を描き直す。
別の手へ移る前に、未更新のコメントを破棄してよいかを `CommentCoordinator::confirmLeaveEditingMove()` で確かめる（6.3）。
棋譜を読み込んだときは、コメント・しおりは `KifuBranchTreeBuilder` がツリーを作るときに各行の `RecordEntry` と一緒にノードへ入る
（開始局面のコメント・しおりは本譜の `RecordLine::openingComment`・`openingBookmark` から）。
ノードには項目ごとの setter は無く、変えるときは `KifuBranchNode::record()`（const でない版）の値を直接変える。

### 5.3 出力形式

書き出しは `KifuExport`（`src/kifu/kifuexport.h`）の形式ごとの関数の表で選ぶ。`KifuExport::lines(document, format)` は
`metadataOf(document)` で書き出しの文脈（`KifuExportMetadata`：対局情報・開始局面・対局者・持ち時間・開始と終了の日時）を作り、
`format` の Exporter の `exportLines(const KifuBranchTree&, const KifuExportMetadata&)` を呼ぶ。GUI の保存・コピーと CLI の変換で同じ表を使う。

| Exporter（`src/kifu/formats/`） | 形式 | 分岐対応 |
|---|---|---|
| `KifExporter` | KIF形式（SFEN・BOD を指定したときもこれ） | あり |
| `Ki2Exporter` | KI2形式 | あり |
| `CsaExporter` | CSA形式 | なし（本譜のみ） |
| `JkfExporter` | JKF（JSON）形式 | あり |
| `UsenExporter` | USEN形式 | あり |
| `UsiExporter` | USI position コマンド形式 | なし（本譜のみ） |

書き出しは分岐ツリーを直接読む。CSA・USI は本譜（`KifuBranchTree::mainLine()`、USI は `mainRecordLine().usiMoves()`）、
USEN は本譜と各ライン（`allLines()`）のノードの指し手から書く。
形式をまたいで共有する部品: KIF・KI2 の先頭（文字コード宣言・対局情報・盤面図）は `KifExporter::appendHeaderLines()`、USEN の符号（指し手の3文字・開始局面・手順）は読み込みと書き出しで `UsenCodec`（`src/kifu/formats/usencodec.h`）、USI の position 文字列は `UsiPosition::commandFor(startSfen, moves)`（`UsiPosition::command()` と共有）。
「USI形式（現在の指し手まで）」のコピーは、`KifuFileController`（`src/ui/coordinators`）が表示中の手順（`KifuNavigationState::selectedPath()`）を
表示中の手数で切り、`KifuBranchTree::toRecordLine()` にして指し手を取る。

---

## 6. GUI連動の全体像

### 6.1 4つのGUI部品

| GUI部品 | ウィジェット | モデル | 説明 |
|---|---|---|---|
| **棋譜欄** | `RecordPane` 内の `QTableView` | `KifuRecordListModel` | 表示中の手順。クリックで手に移動 |
| **分岐候補欄** | `RecordPane` 内の `QTableView` | `KifuBranchListModel` | 表示中の手の分岐候補 |
| **分岐ツリー欄** | `BranchTreeView`（`QGraphicsView` の派生で、分岐ツリードックの中身そのもの。1手は `BranchNodeItem`） | — (直接描画。`setTree()` でツリーを読む) | ツリーのグラフィカル表示 |
| **将棋盤** | `ShogiView` | `ShogiBoard`（`ShogiGameController` の盤） | 盤面表示 |

### 6.2 Signal/Slot 接続図

利用者の操作は `KifuNavigationController` が受けて表示中の手（`KifuNavigationState`）を動かし、
表示する部品はそれぞれ `KifuNavigationState::changed()` を受けて自分の表示を合わせる。
操作の接続は `MainWindowComposer::connectRecord()`、`changed()` の接続は各部品のコンストラクタが行う。

```
利用者の操作                                         受けるスロット
  ナビゲーションボタン（棋譜欄・評価値グラフ）        → 局面送りの QAction（KifuNavigationController::action(Step)）
                                                        → KifuNavigationController::onFirstClicked() など
                                                        （bindButtons() が2組のボタンを同じ QAction に結ぶ）
  棋譜欄の行      RecordPane::mainRowChanged          → KifuNavigationController::selectRecordRow()
  分岐候補欄の行  RecordPane::branchRowActivated      → KifuNavigationController::selectBranchCandidateRow()
  分岐ツリー      BranchTreeView::nodeActivated       → KifuNavigationController::selectBranchTreeNode()
  評価値グラフ    EvaluationChartWidget::plyClicked   → KifuNavigationController::selectPly()
        │
        │  confirmLeave()（局面編集中、または未更新のコメントの破棄をやめたら移らない
        │                  → navigationRejected → KifuDisplayPresenter::refresh() で強調を表示中の手に戻す）
        ▼
KifuNavigationState::setCurrentNode() / resetToNode() / goToRoot()
        │
        └─ emit changed()（表示中の手か表示中の手順が変わったときだけ1回）
             ├─→ KifuDisplayPresenter::refresh()             棋譜欄・分岐候補欄・分岐ツリー欄
             ├─→ BoardSyncPresenter::syncToCurrentNode()     将棋盤の局面と指し手のハイライト
             ├─→ CommentCoordinator::showCurrentComment()    コメント欄
             ├─→ EvaluationGraphPresenter::showCurrentPly()  評価値グラフの現在手（対局中は動かさない）
             ├─→ JosekiWindowController::updateJosekiWindow() 定跡ウィンドウ
             └─→ AnalysisSession::followDisplayedPosition()  検討中のエンジンの局面
```

`KifuNavigationController` は表示を更新しない。プログラムからの移動（`goToNode()`・`goToLine()`・`goForward()` など）は確認を通さない。
ツリーの変更も同じ経路で届く。`KifuBranchTree` の構造の変更の知らせを受けた `KifuNavigationState::onTreeChanged()` が、
表示中のノードが消えていれば開始局面へ戻り、構造が変わっていれば `changed()` を出す（4.2）。
分岐ツリー欄（`BranchTreeView`）と棋譜欄の分岐の印（`KifuDisplayPresenter::refreshBranchMarks()`）は、ツリーの知らせを直接受けて描き直す。

### 6.3 ナビゲーション操作の流れ

#### 6.3.1 ナビゲーションボタン（例: 1手進む）

```
1. ユーザーが「1手進む」ボタン（棋譜欄の ▼ か評価値グラフの ▶）をクリック
2. QAbstractButton::clicked → QAction::trigger（action(Next)）→ KifuNavigationController::onNextClicked()
   （confirmLeave() で確認し、やめたら移らない）
3. KifuNavigationController::goForward(1)
   a. KifuNavigationState::nextNode() で次の子ノードを取得
      （分岐選択の記憶があればその子、なければ最初の子）
   b. state->setCurrentNode(nextNode)
4. KifuNavigationState が changed() を出し、各部品が表示を合わせる（6.2）
   a. KifuDisplayPresenter: 棋譜欄の強調行・分岐候補欄・分岐ツリーの強調
   b. BoardSyncPresenter: 盤の局面（ノードの sfen() と違うときだけ入れ替える）と、
      ノードの指し手（KifuBranchNode::move()）の移動元・移動先のハイライト
   c. コメント欄・評価値グラフ・定跡ウィンドウ・検討中のエンジン
```

#### 6.3.2 棋譜欄の指し手をクリック

棋譜欄の行は表示中の手順のノードなので（3.1）、行番号から手数やラインを探し直さずにそのノードへ移る。

```
1. ユーザーが棋譜欄の行をクリック
2. QItemSelectionModel::currentRowChanged
     → RecordPane::onKifuCurrentRowChanged()
     → emit RecordPane::mainRowChanged(row)
3. KifuNavigationController::selectRecordRow(row)
   - 表示中の手順（KifuNavigationState::selectedPath()）の row 番目のノードを取る。
     範囲外か、表示中の手と同じなら何もしない
   - confirmLeave() でやめたら navigationRejected を出して終わる
     （KifuDisplayPresenter::refresh() が選択と強調を表示中の手に戻す）
   - state->setCurrentNode(path[row])
4. 以降は 6.3.1 の手順4と同じ
```

対局中は棋譜欄・ナビゲーションボタン・分岐ツリーのクリックを無効にする（`UiStatePolicyManager` の `WidgetNavigation`・`WidgetBranchTree`。ナビゲーションボタンは `KifuNavigationController::setStepActionsEnabled()` で局面送りの QAction ごと無効にする）。
表示中の手は `LiveGameSession` が最新手に合わせる（8.6）。
`KifuDisplayPresenter::refresh()` が行を足し削りして選択を合わせる間は選択モデルのシグナルを止めるので、表示の更新が利用者の操作として扱われることはない。

#### 6.3.3 分岐ツリー欄のノードをクリック

分岐ツリーは分岐ツリードックの `BranchTreeView`（`QGraphicsView` の派生）が描画し、クリックは次の経路で処理される。

```
1. ユーザーが分岐ツリーのノードをクリック
2. BranchTreeView::mouseReleaseEvent()（QGraphicsView のイベント関数の上書き）
   a. クリック位置の手（nodeItemAt()。BranchNodeItem）を特定（選べない間（setClickEnabled(false)）は何もしない）
   b. その手のノード ID（BranchNodeItem::nodeId()）で setCurrentNode() して強調
   c. emit BranchTreeView::nodeActivated(nodeId)
      （キーボードで手を移ったときも同じシグナルを出す）
3. KifuNavigationController::selectBranchTreeNode(nodeId)
   a. nodeAt(nodeId) が無ければ何もしない。confirmLeave() でやめたら navigationRejected を出して終わる
   b. ルート（開始局面）なら、選択を忘れて開始局面へ（KifuNavigationState::goToRoot()）
   c. 表示中の手順（selectedPath()）にある手（分岐前の共有の手など）なら、
      表示中の手順のまま移る（state->setCurrentNode(node, 表示中の手順の終わり)）
   d. それ以外は、その手から先頭の子をたどった手順の終わりまでを選んで移る
      （state->setCurrentNode(node, 先頭の子をたどった終わり)。図でその手を描いた行の手順）
      - 以降は 6.3.1 の手順4と同じ
```

> **共有の手の判定**: 分岐ツリーでは、分岐前の共有の手は親ライン（本譜または親の変化）の行に描かれる。
> クリックした手が表示中の手順にあれば、手順を切り替えない。
> 図とはノード ID だけでやり取りするので、行（ライン番号）や手数を突き合わせない。

> **分岐選択記憶の自動更新**: `setCurrentNode(node, lineEnd)` は、ルートからその手順の終わりまでの経路上の全分岐点の選択を記憶する（4.2）。
> これにより、分岐ツリーで深い分岐のノードをクリックした後に
> 開始局面まで戻って再度進めると、同じ分岐の手順が自動的に辿られる。

#### 6.3.4 分岐候補欄の指し手をクリック

分岐候補欄は RecordPane 内の QTableView で、表示中の手に分岐がある場合に選択可能な手の一覧が表示される。

```
1. ユーザーが分岐候補欄の行をクリック
2. QTableView::clicked または QTableView::activated
     → RecordPane::onBranchClicked() / onBranchActivated()（クリック直後の activated の二重処理は除く）
     → emit RecordPane::branchRowActivated(row)
3. KifuNavigationController::selectBranchCandidateRow(row)
   a. 候補（KifuNavigationState::branchCandidatesAtCurrent()）にも「本譜へ戻る」行にも当たらなければ何もしない
   b. confirmLeave() でやめたら navigationRejected を出して終わる
   c. 「本譜へ戻る」行（候補の後ろの行）の場合:
      → goToMainLineAtCurrentPly()
        - 本譜の同じ手数のノード（本譜が短ければ本譜の最終手）へ resetToNode()（分岐選択の記憶を消す）
   d. 通常の分岐候補の場合:
      - 候補から最初の子をたどった手順の終わりを求め、
        state->setCurrentNode(candidate, end)（その手順を選んで候補へ移る）
4. 以降は 6.3.1 の手順4と同じ（表示中の手順が変わったので、棋譜欄はその手順の行になる）
```

> **選んだ手順の維持**: 候補の手順の終わりまでを分岐選択の記憶（4.2）に書くので、
> 分岐点より前に戻っても `currentLineIndex()` は選んだ分岐の番号を返し続ける。
> 棋譜欄に表示される手順が維持され、ユーザーは分岐の中をそのままナビゲーションできる。

### 6.4 KifuDisplayPresenter の役割

`KifuDisplayPresenter`（`src/ui/presenters/kifudisplaypresenter.h`）は、表示中の手（`KifuNavigationState`）が変わるたびに、
棋譜欄・分岐候補欄・分岐ツリー欄を合わせる。状態の写しは持たず、毎回 `KifuNavigationState` から作る。

**依存（`KifuDisplayPresenter::Deps`）**:

| メンバ | 対象 |
|---|---|
| `navState` | 表示中の手（必須） |
| `liveSession` | 対局中の手順（必須） |
| `recordModel` | 棋譜欄モデル（必須） |
| `branchModel` | 分岐候補欄モデル（必須） |
| `recordView` | 棋譜欄の表（選択と表示位置を合わせる） |
| `treeView` | 分岐ツリー欄（`BranchTreeView`。コンストラクタで1度だけ `setTree(tree)` でツリーを渡す） |

**`refresh()` の処理**:

1. 棋譜欄に出す手順を決める。対局中（`LiveGameSession::isActive()`）は開始局面から対局の最新の手（`liveNode()`）まで
   （`KifuBranchTree::pathToNode()`。その先にある以前の手順は出さない）、それ以外は `KifuNavigationState::selectedPath()`
2. `KifuRecordListModel::setPath()` で行を合わせる（先頭が同じ行は残す）。表示中の手の行を強調して選択し、見える位置へスクロールする
3. `KifuBranchListModel::setCandidates()` で分岐候補欄を作る
4. 分岐ツリー欄に表示中の手を `BranchTreeView::setCurrentNode(nodeId)` で伝える（ツリーの構造に合わせる描き直しは図が自分で行う）

ツリーの構造の変更（`nodeAppended`・`childrenReordered`・`subtreeRemoved`・`reset`）は `refreshBranchMarks()` にもつなぎ、
棋譜欄の全行を描き直す（分岐が増える・減る・並びが変わると分岐の印が変わるため）。

`refresh()` は `KifuNavigationState::changed()` と `LiveGameSession::activeChanged()`（対局の開始・終了）で呼ばれ、
移るのをやめたとき（`KifuNavigationController::navigationRejected`）にも強調を戻すために呼ばれる。
表記・言語を変えたときは `refreshLabels()` が全行と分岐ツリーの手の表記（`BranchTreeView::refreshLabels()`）を描き直す。

### 6.5 データ一元管理の保証

4つのGUI部品（棋譜欄・分岐候補欄・分岐ツリー欄・将棋盤）は、すべて **KifuBranchTree** を中央データストアとして共有している。各GUI部品がどのようにデータを取得し、一元性がどう担保されているかを以下にまとめる。

#### 6.5.1 中央データストア

| データストア | 管理対象 | 役割 |
|---|---|---|
| **KifuBranchTree** | ツリー構造・指し手・SFEN・コメント・しおり・消費時間 | 棋譜データの正本（Single Source of Truth） |
| **KifuDocument** | 対局情報・対局の記録・上書き保存先・コメントとしおりの書き込み | 編集操作を受け付け、表示中の手のノードに書く。未保存かどうかは保存した時点との比較で求める（セクション5参照） |
| **KifuNavigationState** | 表示中の手・分岐選択記憶 | どのラインのどの手にいるかの状態管理（棋譜のカーソル） |

#### 6.5.2 各GUI部品のデータ取得経路

```
                 KifuBranchTree（正本）
                  │
                  ├──→ 棋譜欄 (KifuRecordListModel)
                  │      KifuDisplayPresenter::refresh() が表示する手順のノード列を setPath() で渡す。
                  │      モデルは行のノード ID だけを持ち、表示のたびにノードの
                  │      displayText / timeText / bookmark / comment を読む。
                  │
                  ├──→ 分岐候補欄 (KifuBranchListModel)
                  │      KifuNavigationState::branchCandidatesAtCurrent() の候補ノードを
                  │      KifuDisplayPresenter::refresh() が setCandidates() で渡す。
                  │
                  ├──→ 分岐ツリー欄 (BranchTreeView)
                  │      setTree(tree) で受けたツリーの構造の変更の知らせで描き直す。nodeAppended で
                  │      1つの手順の末尾に1手増えただけならその手だけを描き足し、それ以外（新しい変化・
                  │      並べ替え・削除・作り直し）は allLines() から行の配置（BranchTreeLayout）を作り直して
                  │      再描画する。表示中の手は KifuDisplayPresenter::refresh() が setCurrentNode(nodeId) で
                  │      伝え、まだ描いていない手なら描いた時点で強調する。コメントの印は
                  │      KifuDocument::commentChanged → refreshNode(nodeId) で描き直す。
                  │
                  └──→ 将棋盤 (ShogiBoard)
                         BoardSyncPresenter::syncToCurrentNode() が、表示中のノードの sfen() が
                         盤と違うときだけ ShogiBoard::setSfen() で入れ替え、ノードの指し手から
                         移動元・移動先を強調する（BoardCoordinates::fromSquare()・toSquare()）。
```

#### 6.5.3 一元性を担保する仕組み

**読み取り**: 4つのGUI部品はいずれも KifuBranchTree のノードからデータを読み取る。棋譜欄・分岐候補欄のモデルは行のノード ID だけを持ち、表示のたびにノードを読む。分岐ツリー欄は構造が変わったときに描き直し、将棋盤はノードの SFEN を使う。GUI部品が指し手や表記の写しを長期保持することはない。

**書き込み**: コメント・しおりの編集は KifuDocument 経由で行われ、表示中の手のノードに書いたあと、変更シグナルで棋譜欄の行とコメント欄を更新する（セクション5.2）。
対局の指し手は、`MatchCoordinator` が確定した行（`RecordEntry`）を棋譜の受け手（`MatchRecordSink`。実体は `GameSession`）に渡し、
`LiveGameSession::appendRecord()` が `KifuBranchTree::addRecord()` でツリーにノードを追加する（8.6）。

**ナビゲーション**: KifuNavigationState が「どのラインのどの手にいるか」を一元管理する。GUI部品は自身の位置状態を持たず、ナビゲーション操作はすべて KifuNavigationController → KifuNavigationState → `changed()` → 各GUI部品の経路で反映される。

#### 6.5.4 補足: 対局の手順の位置づけ

対局中の手順は `MatchCoordinator` が `RecordLine`（`record()`）で持つ。棋譜の正本ではなく、対局の作業用の手順である。

- 棋譜の読み込み（`KifuLoader`）はこの手順を書かない
- 対局開始時に、表示中の手順（`MatchStartOptions::recordLine`）を開始する手数まで切って作る（`MatchCoordinator::start()`、8.2）
- 一手を確定するたびに、`MatchCoordinator` が手順に行を足し、同じ行を `MatchRecordSink::appendRecord()` で渡して分岐ツリーに積む。
  待ったでは末尾2手を取り除き、`MatchRecordSink::undoRecords(2)` で分岐ツリー側も戻す（8.5）
- 千日手の判定（`RecordLine::sfens()`）、エンジンに探索させる局面（`RecordLine::usiPosition()`）、次の手の「同」の基準（`destinationOf()`。待った・再開の後も同じ）、
  中断の保存（`InterruptedGameState::record`）はこの手順を使う

GUI部品の表示（盤面・棋譜欄のクリックなど）は、この手順ではなく分岐ツリーから読む。
対局中にエンジンの評価値を描く手数は、`MatchCoordinator::engineMovePlayed(side, ply, score)` の `ply`（この手順の手数）を使う。

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

書き出し（`JkfExporter`）は、分岐ツリーのノードの指し手（USI）を開始局面から局面の値（`Position`）に適用しながら（`KifuNodeReplay`）、
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
- 終局はノードの `TerminalType` から `special` にする（`TerminalCodes::csaSpecial()`。CSA と同じ表で、千日手は王手千日手を区別する）。反則勝ちは、手番でない側の反則
  （先手番なら `-ILLEGAL_ACTION`）。JKF に無い不戦勝・不戦敗は書かない。
- `initial.preset` は、盤・手番・持ち駒が手合割の開始局面と同じときだけ書き（`JkfMoveParser::presetFromSfen`。
  読み込みと同じ表）、それ以外は `OTHER` と `data`（盤と持ち駒）で書く。

### 7.7 USEN形式（URL Safe sfen-Extended Notation）

URL セーフな短い文字列で棋譜を表現。分岐対応。

---

## 8. 棋譜データの作成フロー

各ユースケースにおいて、棋譜データ構造がどのように作成・初期化されるかを解説する。

### 8.1 KIF形式の棋譜を読み込んだ場合

棋譜ファイルを開くと、パース → ツリー構築 → 表示中の手を開始局面へ戻す、の順で処理される。
パースはワーカースレッド（`QtConcurrent::run`）で行い、結果のツリーへの適用はメインスレッドで行う。
棋譜欄・分岐ツリー欄・盤は、ツリーと表示中の手の変化を受けて各部品が自分で合わせる。

```
1. KifuFileController::chooseAndLoadKifuFile() → startAsyncLoad(filePath, false)
   → KifuLoader::loadFileAsync(filePath)
   → KifuLoadParser::parseFile()（ワーカースレッド）
       ├─ KifuFormats::detectFile() で形式を決める
       ├─ readFileText() で文字コードを判別して1回読む
       └─ parseContent() → KifTextParser::parseKif()（KifTextParser::parse(text, Dialect::Kif)）
   → KifuLoader::onLoadFinished() → applyLoadResult(result)
     （同期版の loadFile()・loadText() も applyLoadResult() を通る）

2. KIF の変換器が KifuParseOutcome を返し、parseContent() が KifuLoadResult に写す（棋譜は KifuLoadResult::record）
   （行に分けて1回の走査で本譜と各変化を読む。手順ごとに KifLineBuilder が手を指しながら RecordEntry を作るので、
     表記・USI の指し手・局面の並びがずれない。読めない手は ParseOptions に従い、既定ではその手の手前で打ち切る）
     ├─ mainline: RecordLine（本譜の行 RecordEntry の並び。末尾に終局の行。開始局面のコメント・しおりは openingComment・openingBookmark）
     └─ variations: QList<KifVariation>（変化。分岐元の手順 parentVariation と手数は
        KifParseResult::latestBranchPoint() で決め、addVariation() で足す。
        KIF/KI2/USEN は「変化：N手」の N-1 手目を持つ手順のうち最後に書かれたもの、
        JKF は形式上の親（branchPoint()）を分岐元にする）

3. KifuLoader::applyRecord()
   a. 指し手が無く、開始局面が手合割の名前だけの平手なら失敗（局面図・局面指定・駒落ちの開始局面は0手でも読む）
   b. KifuDocument::replaceWithRecord(record, result.gameInfo)
      → KifuBranchTreeBuilder::buildFromKifParseResult(tree, record)（開始局面は record.mainline.startSfen）
      ├─ 変化の分岐元が先に書かれた手順で、分岐の手の前まで続いているかを確かめる
      │  （だめならツリーを変えずに false）
      ├─ beginBatchUpdate() → tree->clear() + tree->setRootSfen(開始局面)
      ├─ addKifLineToTree(tree, mainline, {root})  // 本譜
      │   → root → move1 → move2 → ... → endNode（各行を addRecord() で積む）
      ├─ for each variation:
      │    分岐元の手順の経路を startPly 手目の前までで切り、その末尾に変化の行を積む。
      │    同一SFENの別経路とは混同しない
      └─ endBatchUpdate()  // 作り直しを含むので reset を1回だけ出す
   c. 文書の後始末（KifuDocument::finishReplacement()）: 対局情報を読んだものにし、前の対局者・持ち時間・
      上書き保存先を消し、今の内容を保存した時点とする
      → emit replaced() → EvaluationGraphPresenter::clear()・EngineDisplayPresenter::clearAll()
        （前の対局・解析の評価値グラフ・通信ログ・思考情報・解析結果を消す）
      → emit headerReset(header) → GameInfoPane::setGameInfo()（対局情報欄）・
        PlayerInfoPresenter（盤の対局者名）

4. 表示
   reset → KifuNavigationState::onTreeChanged()（分岐ツリー欄・棋譜欄の分岐の印も reset を受けて作り直す）
     表示中のノードが消えたので goToRoot() → changed()
     → KifuDisplayPresenter（棋譜欄は本譜の行、分岐候補欄、分岐ツリー欄）
     → BoardSyncPresenter（開始局面の盤）・CommentCoordinator（開始局面のコメント）など
   loadFinished(true) → KifuFileController::onAsyncLoadFinished() → finishLoad()
                        （ファイルなら KifuDocument::markSaved(path) で上書き保存先にする。貼り付けた棋譜は markUnsavedContent()）
```

**読み込み後のデータ状態**:

| データ構造 | 内容 |
|---|---|
| `KifuBranchTree` | 本譜＋全分岐を含む完全なN分木（棋譜の正本）。コメント・しおりもノードに入っている |
| `KifuNavigationState` | ルートノードを指し、手数0・分岐選択の記憶なし（表示中の手順は本譜） |
| `KifuRecordListModel` | 本譜のノード列（開始局面＋各手の行） |
| `KifuDocument` | 対局情報は読んだもの、対局者・持ち時間は空。ファイルなら上書き保存先にし変更なし（貼り付けた棋譜は未保存） |
| 対局の手順（`MatchCoordinator::record()`） | 読み込みでは変わらない。対局開始時に表示中の手順から作る（8.2） |

### 8.2 棋譜読み込み後、途中局面から対局した場合

`GameSession::startGameFromDialog()` は、表示中の手順（`KifuNavigationState::currentRecordLine()`）と表示中のノードIDを
`GameStartRequest` に入れて `GameStartOptionsBuilder::plan()` に渡す。`plan()` は手順を表示中の手数で切り（`RecordLine::upTo()`）、
その局面を検証（`validatePosition()`。玉の数と `PositionValidator` の検査）してから、開始の計画（`GameStartPlan`）を返す。
計画の `preparation` は `GameStartPreparation::Mode::CurrentPosition` と開始ノードIDを持つ。
検証に失敗した場合は計画を作らず、棋譜・時計・セッションを変更しない。

`GameSession::startGame()` は `SessionResetCoordinator::prepareGameStart()` で棋譜を整える。ツリーは作り直さず、
開始ノードが終局の行なら直前の手から始める（中断の行で子が無ければ消す）。評価値グラフは開始する手数まで残し、
`LiveGameSession::startFromNode()` でそのノードからライブ対局を始める（表示中の手がそのノードになり、棋譜欄はそこまでの手順になる）。
開始ノードがもう無いときは何も変えずに失敗する（別の分岐や同じSFENの局面を探し直さない）。
`MatchCoordinator::start()` は計画の `MatchStartOptions::recordLine` を対局の手順（`record()`）にする。
USI の `position` はこの手順から生成するため、同じ局面に至る別の分岐を選んでも手順を取り違えない。
SFENの手数欄とツリー上の手数も混同しない。

### 8.3 平手・駒落ちで新しく対局した場合

`GameStartOptionsBuilder::plan()` は `GameStartPreparation::Mode::NewGame` と開始局面（`rootSfen`。手合割の局面）を計画に入れる。
`SessionResetCoordinator::prepareGameStart()` がツリーをその局面で作り直し（`KifuBranchTree::setRootSfen()`）、
ルートからライブ対局を始める。
`MatchSessionState` が開始オプション（対局者を含む）・終局状態をまとめて所有する（エンジンの先読みの局面はエンジンの `UsiMatchHandler` だけが持つ）。
持ち時間は `GameSession::startGame()` で一度だけ時計に設定し（`TimeControlUtil::applyToClock()`）、
`KifuDocument::beginMatch()` が対局者と、持ち時間・開始日時（`MatchTimeRecord::begin()`）を記録する。`TimeDisplayPresenter` は時計の残り時間を表示するだけにする。

連続対局の次局（`GameSession::startNextGame()`）は、`SessionResetCoordinator::resetToNewRecord()` で独立した棋譜にしてから同じ手順で始める。
「新規」（`GameSession::resetToInitialState()`）は `resetToNewRecord()` で平手の開始局面だけの棋譜にし、ライブ対局を始めない。
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
      ├─ 盤を4手目の後の局面（record().sfenAt(4)）に戻し、「同」の基準
      │  （record().destinationOf(4)）を合わせる。対局の手順の末尾2手を取り除き、時計を戻す
      ├─ MatchRecordSink::undoRecords(2) → GameSession::undoRecords(2)
      │    → LiveGameSession::undoMoves(2)
      │       ├─ m_liveParent を4手目のノードへ戻す（5・6手目のノードは残す）
      │       ├─ 5手目のノードを m_undoneNodeIds に記録（このセッションで追加したノードの場合のみ）
      │       └─ KifuNavigationState::setCurrentNode(4手目のノード)
      │            → changed() で棋譜欄（4手目まで）・盤・コメント欄などが合わせる
      └─ emit movesUndone(4) → EvaluationGraphPresenter::trimToPly(4)
   （未保存の印は付けない。取り消した手は対局でツリーに足したノードなので、ツリーの revision が進んでいて未保存になっている）

2. 別の5手目を指す
   MatchRecordSink::appendRecord() → LiveGameSession::appendRecord(...)
   → 4手目のノードの子として新ノードを追加
   → placeBeforeUndoneSiblings(): 取り消した5手目より前へ移す（一括更新の中の moveChild）
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
棋譜欄は対局中、開始局面から最新手までを表示し、分岐のある手には「+」を付ける（`KifuRecordListModel` がノードの親の子の数から決める）。
終局すると動作状態は待機中（`AppActivityState::Activity::Idle`）に戻り、盤上で指した手は `HumanTurnPolicy::isHumanTurnNow()`
（→ `BranchTreeEditController::canRecordBoardMove()`。ライブ対局が終わっていれば受け付ける）で受け付け、
`BoardMoveController::playMove()` から `BranchTreeEditController::recordBoardMove()` が表示中の手の次の手（変化）として分岐ツリーに加える。
続きを対局として指すときは、8.4 のように「現在の局面」から対局を開始する。

### 8.6 ツリーへの追加と通知

| メソッド | シグナル | 使用場面 |
|---|---|---|
| `addRecord()` | `nodeAppended(parentId, nodeId)`（一括更新中は `endBatchUpdate()` まで遅らせる。作り直しを含む一括更新では `reset()` 1回にまとめる） | 棋譜読み込み時のツリー構築（`KifuBranchTreeBuilder`）、ライブ対局（`LiveGameSession::appendRecord()`）、待機中に盤で指した変化（`BranchTreeEditController`） |

手を足す関数は `addRecord()` だけである（テストで指し手を直接足すときは `tests/branchtree_test_util.h` の `TestTree::addMove()`・`addTerminalMove()`）。

対局中の追加は、`LiveGameSession::appendRecord()` が `beginBatchUpdate()`〜`endBatchUpdate()` の間で行う（待ったの後の並べ替え `moveChild()` も同じ一括更新の中）。
ノードを足した後に表示中の手をそのノードへ移す（`KifuNavigationState::setCurrentNode()` が `changed()` を出す）ので、
`endBatchUpdate()` で届く `nodeAppended`（並べ替えたときは続けて `childrenReordered`）では、`KifuNavigationState` は同じ構造をもう知らせているので `changed()` を繰り返さない（4.2）。
`changed()` を受けた `KifuDisplayPresenter::refresh()` は、棋譜欄に最新手までの手順を `setPath()` で渡し（増えた1行だけを足す）、
分岐ツリー欄に表示中の手を `setCurrentNode(nodeId)` で伝える。分岐ツリー欄（`BranchTreeView`）はこの時点ではまだその手を描いていないので、
`nodeAppended` を受けて追加した1手だけを描き足した時点で強調する（新しい変化などで描き足せなければ全体を作り直す）。

---

## 9. 補足: パース用データ構造

### 9.1 KifParseResult / KifVariation / KifuParseOutcome

**ファイル**: `src/kifu/kifparsetypes.h`

KIF/KI2/CSA 等の棋譜を読み込んだときのパース結果を格納する中間構造。手順は `RecordLine`（4.4）で持ち、読み込み用の別の手順の型は無い。

```cpp
struct KifVariation {
    int startPly = 1;          // 変化開始手数
    RecordLine line;           // 変化の手順（line.startPly == startPly）
    int parentVariation = -1;  // 分岐元の手順。-1=本譜、その他=先に書かれた変化の添字
};

struct KifBranchPoint {        // 変化の分岐元
    int parentVariation = -1;  // 分岐元の手順（-1 は本譜）
    int startPly = 1;          // 変化の最初の手の手数
    QString sfen;              // 変化の最初の手を指す前の局面
    std::optional<UsiMove> previousMove;  // その局面に至った手（「同」の解決用。開始局面なら nullopt）
};

struct KifParseResult {
    RecordLine mainline;               // 本譜（開始局面のコメント・しおりは openingComment・openingBookmark）
    QList<KifVariation> variations;    // 変化リスト（書かれた順。分岐元は先に並ぶ）

    const RecordLine& lineAt(int variation) const;                              // 手順（-1 は本譜）
    std::optional<KifBranchPoint> branchPoint(int parent, int startPly) const;  // 手順 parent の startPly 手目の前の分岐元
    std::optional<KifBranchPoint> latestBranchPoint(int startPly) const;        // 「変化：N手」の分岐元
    int firstPlyOf(int variation) const;                                        // 手順の最初の手の手数
    void addVariation(const KifBranchPoint& from, RecordLine line);             // 変化を足す
};

struct KifuParseOutcome {              // 形式ごとの parse() が返す1回の解析の結果
    bool success = false;
    KifParseResult record;
    QString initialSfen;               // 開始局面（record.mainline.startSfen と同じ）
    QString teaiLabel;                 // 手合割の名前（局面図なら "BOD"、USI・CSA は「平手」「局面指定」）
    QList<KifGameInfoItem> gameInfo;   // 対局情報（書かれた順）
    QString warning;                   // 警告（失敗したときは理由）
    QString positionOnly;              // 指し手のない局面だけの棋譜（USI・SFEN・BOD）の局面
};
```

パース後、この `KifParseResult` から `KifuBranchTree` が構築される（`KifuBranchTreeBuilder::addKifLineToTree()` が `entries` の各行を `addRecord()` で積む）。
指し手列・局面列・表記は `entries` から作るので、別々の一覧として持たず、並びがずれない。

読めない手（表記が解釈できない・局面に合わない）・指せない手・パスの扱いは `ParseOptions` の1か所で決める。
既定の `UnreadableMove::TruncateLine` はその手の手前で手順を打ち切って警告に「何手目まで読んだか」を書き、
`FailRecord` は棋譜全体を読めなかったことにする。変換器は読み込み中の警告と失敗を `ParseReport`（`warn()`・`rejectMove()`・`rejectPass()`・`fail()`）に記録する。

### 9.2 KifGameInfoItem

**ファイル**: `src/kifu/kifparsetypes.h`

KIF ヘッダの「キーワード：内容」ペア。

```cpp
struct KifGameInfoItem {
    QString key;    // キーワード（例: "開始日時"）
    QString value;  // 内容（例: "2025/03/02 09:00"）
};
```

ラインごとの手順と局面列は、分岐ツリーから `KifuBranchTree::exportLine()`（`RecordLine`）・`sfenListForLine()` で取る。

### 9.3 KifLineBuilder（1本の手順の組み立て）

**ファイル**: `src/kifu/formats/kiflinebuilder.h`

棋譜の読み込みで1本の手順（本譜か変化）を組み立てる部品。全形式（KIF・KI2・CSA・JKF・USEN・USI）のパーサが、読んだ順に次の関数を呼ぶ。
手を指したその場で局面（`Position`）を進めて `RecordEntry` を作るので、表示の指し手と USI の指し手を別々に積んで突き合わせることはない。
手番・「同」の基準（直前の手。変化は分岐元の `KifBranchPoint::previousMove`）・両者の累計の消費時間はこの部品が持つ。

```cpp
class KifLineBuilder {
public:
    enum class LineKind { Mainline, Variation };  // 最初の手より前のコメント・しおりを開始局面に付けるか、最初の手に付けるか
    explicit KifLineBuilder(const Position& start, int startPly = 1,
                            std::optional<UsiMove> previous = std::nullopt, LineKind kind = LineKind::Mainline);

    bool addMove(const UsiMove& move, const RecordTime& time = zeroTime(),
                 const KifMoveText::Notes& notes = {},
                 Position::DropRule rule = Position::DropRule::Lenient);   // 指せなければ何も変えずに false
    void addTerminal(TerminalType type, Turn side, const RecordTime& time = zeroTime());
    void appendComment(const QString& text);    // 最後の行（手がまだなければ開始局面・変化の最初の手）
    void appendBookmark(const QString& text);
    RecordTime timeFor(Turn side, qint64 elapsedMs, std::optional<qint64> totalMs = std::nullopt);  // 累計を足した消費時間
    void setLastTime(const RecordTime& time);   // 最後の行の消費時間を置き換える
    void stop();                                // 手順を打ち切る（読めない手の後）
    const QString& lastError() const;           // 指せなかった理由（"Source square is empty: 7g7e" など）
    QString cannotRestore(const QString& format, int ply) const;  // "KIF: Cannot restore move 3: ..."
    RecordLine take();                          // 組み立てた手順
};
```

- 棋譜欄の表記は、取り込んだ原文を写さず、いつも指し手と直前の手から `KifMoveText::format()` で作る。
  棋譜に書かれていた相対位置の修飾語（「右」「上」など）・「不成」は `KifMoveText::Notes` で渡して残す（KIF・KI2・JKF）。
- 打つ駒が持ち駒にない手は既定で指す（`Position::DropRule::Lenient`）。CSA だけは `RequireHand` で指し、持っていない駒を打つ手は読まない。
- CSA の T 行・JKF の time は、1手の消費時間（ミリ秒）から `timeFor()` が累計を足して `RecordTime::fromMilliseconds()` で作る（文字列を経由しない）。
- 指せなかった手の警告は、全形式で `cannotRestore()` の同じ文面にする（扱いは 9.1 の `ParseOptions`）。

KIF と KI2 は `KifTextParser::parse(text, Dialect)`（`src/kifu/formats/kiftextparser.h`）が同じ処理で読む。行の分類（ヘッダ・局面図・コメント・しおり）、
「変化：N手」の区切り、開始局面（BOD・手合割・手番の行）、指し手の文法（`KifMoveGrammar::parseMoveText()`・`resolve()`）を共有し、
手数で始まる KIF の行と手番の印で始まる KI2 の行をどちらの形式でも読む。`Dialect` で違うのは、KI2 の「まで…」の結果の行、対局情報の終わりの見分け方、警告の見出しだけ。
