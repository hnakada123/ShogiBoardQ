# FastMoveValidator クラスの GUI 内での使用状況

## 概要

`FastMoveValidator` は将棋の指し手の合法性を判定するコアクラスである。ビットボード（`std::bitset<81>`）を用いて各駒の利きを計算し、王手回避・二歩・打ち歩詰め等のルールを考慮した合法手判定を行う。

GUI 非依存のコアロジック（`src/core/`）に属し、QObject を継承しているが、直接的な signal/slot 接続による GUI 連携は行わない。4 つのクラスからローカルインスタンスとして生成・利用される。

## クラス定義

- ヘッダー: `src/core/fastmovevalidator.h`
- 実装: `src/core/fastmovevalidator.cpp`
- 関連型: `src/core/legalmovestatus.h`, `src/core/shogimove.h`

## 公開メソッド

### isLegalMove

```cpp
LegalMoveStatus isLegalMove(
    const Turn& turn,
    const QVector<QChar>& boardData,
    const QMap<QChar, int>& pieceStand,
    ShogiMove& currentMove
);
```

指定した指し手が合法かどうかを判定する。

| パラメータ | 型 | 説明 |
|---|---|---|
| `turn` | `Turn` (enum) | 手番（`BLACK`=先手, `WHITE`=後手） |
| `boardData` | `QVector<QChar>` | 盤面データ（81 要素、0-indexed） |
| `pieceStand` | `QMap<QChar, int>` | 駒台の駒数マップ（駒文字→枚数） |
| `currentMove` | `ShogiMove&` | 判定する指し手（参照渡し、内部で更新される場合あり） |

**戻り値**: `LegalMoveStatus`

```cpp
struct LegalMoveStatus {
    bool nonPromotingMoveExists;  // 不成での合法手が存在するか
    bool promotingMoveExists;     // 成りでの合法手が存在するか
};
```

### generateLegalMoves

```cpp
int generateLegalMoves(
    const Turn& turn,
    const QVector<QChar>& boardData,
    const QMap<QChar, int>& pieceStand
);
```

指定局面での全合法手数を返す。テスト用途で主に使用される。

| パラメータ | 型 | 説明 |
|---|---|---|
| `turn` | `Turn` | 手番 |
| `boardData` | `QVector<QChar>` | 盤面データ |
| `pieceStand` | `QMap<QChar, int>` | 駒台 |

**戻り値**: 合法手の総数（`int`）

### checkIfKingInCheck

```cpp
int checkIfKingInCheck(
    const Turn& turn,
    const QVector<QChar>& boardData
);
```

指定手番の玉が王手されているかを判定する。

| パラメータ | 型 | 説明 |
|---|---|---|
| `turn` | `Turn` | どちらの玉を調べるか |
| `boardData` | `QVector<QChar>` | 盤面データ |

**戻り値**: 王手の数（`int`）— 0=なし, 1=王手, 2=両王手, 3以上=エラー

## 入力データの詳細

### 盤面データ（boardData）

`QVector<QChar>` の 81 要素配列。インデックスは `rank * 9 + file`（0-indexed）。

| 文字 | 駒（先手） | 文字 | 駒（後手） |
|---|---|---|---|
| `P` | 歩 | `p` | 歩 |
| `L` | 香 | `l` | 香 |
| `N` | 桂 | `n` | 桂 |
| `S` | 銀 | `s` | 銀 |
| `G` | 金 | `g` | 金 |
| `B` | 角 | `b` | 角 |
| `R` | 飛 | `r` | 飛 |
| `K` | 玉 | `k` | 玉 |
| `Q` | と金 | `q` | と金 |
| `M` | 成香 | `m` | 成香 |
| `O` | 成桂 | `o` | 成桂 |
| `T` | 成銀 | `t` | 成銀 |
| `C` | 馬 | `c` | 馬 |
| `U` | 龍 | `u` | 龍 |
| ` ` | 空白 | | |

大文字=先手、小文字=後手。

### ShogiMove 構造体

```cpp
struct ShogiMove {
    QPoint fromSquare{0, 0};               // 移動元（盤上:0-8、駒台:9=先手,10=後手）
    QPoint toSquare{0, 0};                 // 移動先（0-8）
    QChar movingPiece = QLatin1Char(' ');   // 動かす駒（SFEN 表記）
    QChar capturedPiece = QLatin1Char(' '); // 取る駒（なければ空白）
    bool isPromotion = false;              // 成りフラグ
};
```

座標系は 0-indexed。`fromSquare.x()` が 9 なら先手の駒台から、10 なら後手の駒台からの駒打ち。

### 駒台データ（pieceStand）

`QMap<QChar, int>` 形式。キーは駒文字（先手:大文字 `P`,`L`,`N`,`S`,`G`,`B`,`R`、後手:小文字）、値は枚数。

## 呼び出し元と使用パターン

### 1. ShogiGameController（対局中の指し手検証）

**ファイル**: `src/game/shogigamecontroller.cpp`

対局中にユーザーが駒を動かした際、指し手の合法性を判定し、成り/不成の選択を処理する。

```
ユーザー操作 (BoardInteractionController)
    ↓
ShogiGameController::validateAndMove()
    ├─ FastMoveValidator validator; （ローカル生成）
    ├─ Turn turn = getCurrentTurnForValidator(validator);
    ├─ ShogiMove currentMove(fromPoint, toPoint, movingPiece, capturedPiece, promote);
    └─ decidePromotion(playMode, validator, turn, ...)
         └─ LegalMoveStatus status = validator.isLegalMove(turn, boardData, pieceStand, currentMove);
              ├─ 成りも不成も可能 → 成り確認ダイアログ表示
              ├─ 成りのみ可能     → 自動的に成り
              ├─ 不成のみ可能     → 成らない
              └─ どちらも不可     → 指し手を拒否（false を返す）
```

**座標変換**: GUI の 1-indexed 座標から FastMoveValidator の 0-indexed 座標へ変換:
```cpp
QPoint fromPoint(fileFrom - 1, rankFrom - 1);
QPoint toPoint(fileTo - 1, rankTo - 1);
```

**データソース**: `board()->boardData()` と `board()->getPieceStand()` から `ShogiBoard` 経由で取得。

### 2. SennichiteDetector（千日手の連続王手判定）

**ファイル**: `src/game/sennichitedetector.cpp`

千日手（同一局面 4 回）の検出時に、連続王手による千日手かどうかを判定する。

```
千日手検出
    ↓
SennichiteDetector::checkContinuousCheck()
    ├─ FastMoveValidator validator; （ローカル生成）
    ├─ SFEN 文字列から手番を判定
    │   └─ Turn turn = sideToMoveIsBlack ? FastMoveValidator::BLACK : FastMoveValidator::WHITE;
    └─ ループ: 繰り返し区間の各局面で
         └─ int checks = validator.checkIfKingInCheck(turn, board.boardData());
              └─ checks > 0 なら王手回数をカウント
```

使用メソッドは `checkIfKingInCheck` のみ。盤面データは ShogiBoard に SFEN を読み込ませて取得。

### 3. JishogiScoreDialogController（持将棋スコア判定）

**ファイル**: `src/ui/controllers/jishogiscoredialogcontroller.cpp`

持将棋（入玉宣言法）のスコア判定ダイアログで、王手されている側は宣言できないルールの確認に使用。

```
持将棋スコアダイアログ表示
    ↓
JishogiScoreDialogController::showDialog()
    ├─ FastMoveValidator validator;
    ├─ bool senteInCheck = validator.checkIfKingInCheck(FastMoveValidator::BLACK, board->boardData()) > 0;
    └─ bool goteInCheck  = validator.checkIfKingInCheck(FastMoveValidator::WHITE, board->boardData()) > 0;
         └─ 王手中なら宣言不可の表示
```

### 4. NyugyokuDeclarationHandler（入玉宣言処理）

**ファイル**: `src/ui/controllers/nyugyokudeclarationhandler.cpp`

入玉宣言の条件判定で王手状態を確認する。使用パターンは JishogiScoreDialogController と同一。

```
入玉宣言操作
    ↓
NyugyokuDeclarationHandler::handleDeclaration()
    ├─ FastMoveValidator validator;
    └─ bool declarerInCheck = validator.checkIfKingInCheck(turn, board->boardData()) > 0;
         └─ 王手中なら宣言不可
```

## データフロー図

```
┌──────────────────────┐     ┌──────────────────────┐
│ BoardInteraction     │     │ ShogiBoard           │
│ Controller           │     │ (盤面状態保持)        │
│                      │     │                      │
│ ユーザーの駒操作     │     │ boardData()          │
│ → from/to 座標       │     │ getPieceStand()      │
└──────┬───────────────┘     └──────┬───────────────┘
       │                            │
       ↓                            ↓
┌──────────────────────────────────────────────────┐
│ ShogiGameController::validateAndMove()           │
│                                                  │
│  1. 座標を 1-indexed → 0-indexed に変換          │
│  2. ShogiMove 構造体を組み立て                   │
│  3. FastMoveValidator をローカル生成                  │
│  4. decidePromotion() で合法性判定               │
└──────┬───────────────────────────────────────────┘
       │
       ↓
┌──────────────────────────────────────────────────┐
│ FastMoveValidator::isLegalMove()                     │
│                                                  │
│  入力:                                           │
│   - Turn (BLACK/WHITE)                           │
│   - boardData (QVector<QChar>, 81要素)           │
│   - pieceStand (QMap<QChar,int>)                 │
│   - ShogiMove (from, to, piece, captured, promo) │
│                                                  │
│  処理:                                           │
│   1. 盤面検証 (二歩、駒数、玉の存在)            │
│   2. ビットボード生成 (28枚: 14駒種×2手番)       │
│   3. 利きビットボード計算                        │
│   4. 王手状態の判定                              │
│   5. 合法手リスト生成・フィルタリング            │
│   6. 該当指し手の成り/不成の存在確認             │
│                                                  │
│  出力:                                           │
│   - LegalMoveStatus                              │
│     { nonPromotingMoveExists, promotingMoveExists }│
└──────┬───────────────────────────────────────────┘
       │
       ↓
┌──────────────────────────────────────────────────┐
│ 成り判定の分岐                                   │
│                                                  │
│  成り可 ∧ 不成可 → 成り確認ダイアログ表示       │
│  成り可 ∧ 不成不可 → 自動成り                    │
│  成り不可 ∧ 不成可 → そのまま移動               │
│  成り不可 ∧ 不成不可 → 不合法手として拒否        │
└──────────────────────────────────────────────────┘
```

## 定数

```cpp
FastMoveValidator::BLACK            // 0: 先手
FastMoveValidator::WHITE            // 1: 後手
FastMoveValidator::BLACK_HAND_FILE  // 9: 先手駒台の X 座標
FastMoveValidator::WHITE_HAND_FILE  // 10: 後手駒台の X 座標
FastMoveValidator::BOARD_SIZE       // 9: 盤の一辺
FastMoveValidator::NUM_BOARD_SQUARES // 81: 盤のマス数
```

## 実装上の特徴

- **ステートレス**: メンバ変数に盤面状態を保持しない。毎回引数で受け取る設計のため、ローカル変数として生成して使い捨てられる
- **エラー通知**: `errorOccurred(QString)` シグナルを持つが、現在どこにも connect されていない（コメントに「現在connect先なし」と記載）
- **ビットボード方式**: `std::bitset<81>` を利用し、駒種ごと・プレイヤーごとの利きを効率的に計算
- **打ち歩詰め判定**: 歩を相手玉の直前に打った場合、打った後の局面で相手に合法手があるかをシミュレーションして判定
