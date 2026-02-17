# MoveValidator アルゴリズムと問題点

## 1. 全体アーキテクチャ

MoveValidator は **ビットボード**（`std::bitset<81>`）を用いた合法手判定エンジンである。1,467 行の実装で、以下の処理パイプラインを持つ。

```
入力（盤面・駒台・指し手）
    ↓
盤面バリデーション（二歩・駒数・玉存在・行き場）
    ↓
ビットボード生成（駒配置 → 個別駒 → 利き）
    ↓
王手状態判定
    ↓
合法手生成 / 指し手判定
    ↓
出力（LegalMoveStatus / 合法手数）
```

## 2. ビットボードの構造

### 2.1 三層のビットボード

| 層 | 型 | 用途 | 生成関数 |
|---|---|---|---|
| 駒配置 | `BoardStateArray`<br>`[2][14] × bitset<81>` | 各手番×各駒種がどのマスに存在するか | `generateBitboard()` |
| 個別駒 | `QVector<QVector<bitset<81>>>` 28 要素 | 同種駒を個別に分離（例: 先手歩が 9 枚あれば 9 個の bitset） | `generateAllPieceBitboards()` |
| 利き | `QVector<QVector<bitset<81>>>` 28 要素 | 各個別駒が移動可能なマス | `generateAllIndividualPieceAttackBitboards()` |

### 2.2 駒種インデックス

```
先手: P_IDX(0)  L_IDX(1)  N_IDX(2)  S_IDX(3)  G_IDX(4)  B_IDX(5)  R_IDX(6)  K_IDX(7)
      Q_IDX(8)  M_IDX(9)  O_IDX(10) T_IDX(11) C_IDX(12) U_IDX(13)

後手: p_IDX(14) l_IDX(15) n_IDX(16) s_IDX(17) g_IDX(18) b_IDX(19) r_IDX(20) k_IDX(21)
      q_IDX(22) m_IDX(23) o_IDX(24) t_IDX(25) c_IDX(26) u_IDX(27)
```

`m_allPieces` 配列と 1:1 対応。`allPieceBitboards[i]` は `m_allPieces[i]` の駒種に対応する。

## 3. 利き計算アルゴリズム

### 3.1 generateAttackBitboard

各駒の利きは**方向リスト**と**連続移動フラグ**で定義される。

```cpp
void generateAttackBitboard(
    bitset<81>& attackBitboard,        // 出力: 利きビットボード
    const Turn& turn,                  // 手番
    const BoardStateArray& placed,     // 駒配置
    const bitset<81>& pieceBitboard,   // 対象駒の位置
    const QList<QPoint>& directions,   // 移動方向リスト {rank差, file差}
    bool continuous,                   // 飛び駒かどうか
    bool enemyOccupiedStop             // 敵駒で停止するか
);
```

**方向定義の例:**

| 駒 | 方向 | continuous | enemyOccupiedStop |
|---|---|---|---|
| 先手歩 | `{-1, 0}` | false | false |
| 先手桂 | `{-2, -1}, {-2, 1}` | false | false |
| 先手香 | `{-1, 0}` | true | true |
| 角 | `{-1,-1}, {-1,1}, {1,-1}, {1,1}` | true | true |
| 飛車 | `{-1,0}, {0,-1}, {0,1}, {1,0}` | true | true |
| 玉 | 8 方向全部 | false | false |
| 馬 | 角(continuous) + 十字(1 マス) | 2 回呼び出し | — |
| 龍 | 飛車(continuous) + 斜め(1 マス) | 2 回呼び出し | — |

**走査ロジック:**

```
指定方向に 1 マスずつ進む
  → 盤外に出たら停止
  → 味方駒があれば停止（そのマスには移動不可）
  → 敵駒があればそのマスを利きに加えて停止（取れる）
  → continuous でなければ 1 マスで停止
```

### 3.2 成り手の生成

`generateAllPromotions()` で処理:

- 先手: 移動元または移動先が rank 0〜2（敵陣 1〜3 段目）なら成り可能
- 後手: 移動元または移動先が rank 6〜8（敵陣 7〜9 段目）なら成り可能
- 金・玉・成駒は成り不可（switch で該当せず、スキップされる）

### 3.3 行き場のない駒の処理

不成の指し手は `generateShogiMoveFromBitboard(s)` 内で禁止:

- 先手歩・先手香 → rank 0（1 段目）に不成で移動する手を除外
- 後手歩・後手香 → rank 8（9 段目）に不成で移動する手を除外
- 先手桂 → rank 0, 1（1〜2 段目）に不成で移動する手を除外
- 後手桂 → rank 7, 8（8〜9 段目）に不成で移動する手を除外

成り手はこの禁止条件に関係なく `generateAllPromotions()` で追加される。

## 4. 王手判定アルゴリズム

### 4.1 isKingInCheck

全駒の利きビットボードと玉の位置ビットボードの AND を取り、非ゼロなら王手。

```
numChecks = 0
for each piece (自分の玉を除く全駒):
    if (piece の利き & 玉の位置) が非ゼロ:
        necessaryMovesBitboard = 利き（玉のマスを除く）| 王手元の駒のマス
        numChecks++
```

`necessaryMovesBitboard`: 合い駒を置くべきマスと王手元の駒のマスの和集合。王手回避手のフィルタリングに使用。

### 4.2 王手回避手の生成

`filterMovesThatBlockThreat()`:

- 玉の移動はすべて候補に含める
- 玉以外の駒は `necessaryMovesBitboard` 上のマスへの移動のみ候補
- 両王手（numChecks == 2）の場合は玉の移動のみ有効

### 4.3 filterLegalMovesList — 自殺手の除外

各候補手について:

1. 盤面に指し手を適用（`applyMovesToBoard`）
2. 新しい盤面で全ビットボードを再生成
3. 相手の全利きを再計算
4. 自玉が王手されているか確認
5. 王手されていなければ合法手

## 5. 駒打ちアルゴリズム

### 5.1 歩以外の駒打ち

`generateDropMoveForPiece()`:

- 空きマスビットボードを走査
- 香: rank 0（先手）/ rank 8（後手）には打てない
- 桂: rank 0-1（先手）/ rank 7-8（後手）には打てない

### 5.2 歩の駒打ち

`generateDropMoveForPawn()`:

1. `generateBitboardForEmptyFiles()` で同手番の歩が無い筋のビットボードを生成
2. 空きマスビットボードと AND 演算 → 二歩回避済みの打てるマス
3. さらに rank 0（先手）/ rank 8（後手）を除外

### 5.3 打ち歩詰め判定

`validateMoveWithoutChecks()` 内:

1. 歩を打つ手が合法かを判定（`isHandPieceMoveValid`）
2. 合法なら `isPawnInFrontOfKing()` で相手玉の直前に歩を打ったか確認
3. 直前に打っていた場合、盤面をシミュレーション
4. 相手の全合法手数を `generateLegalMoves()` で計算
5. 合法手が 0 なら打ち歩詰め（禁手）

## 6. isLegalMove と generateLegalMoves の処理フロー比較

### isLegalMove（1 手の検証）

```
validateMoveComponents()        ... 入力バリデーション
generateBitboard()              ... 駒配置ビットボード
generateAllPieceBitboards()     ... 個別駒ビットボード
generateAllIndividualPieceAttackBitboards() ... 利きビットボード
isKingInCheck(相手)             ... 不正局面チェック
isKingInCheck(自分)             ... 王手状態確認
validateMove()
  ├─ 盤上移動: generateBitboardAndValidateMove()
  │    → 対象駒の利きだけ再計算
  │    → isBoardMoveValid() で合法手リスト生成→照合
  └─ 駒打ち: validateMoveWithChecks/WithoutChecks()
       → isHandPieceMoveValid()
       → 打ち歩詰めチェック
```

### generateLegalMoves（全合法手生成）

```
validateBoardAndPieces()        ... 盤面バリデーション
generateBitboard()              ... 駒配置ビットボード
generateAllPieceBitboards()     ... 個別駒ビットボード
generateAllIndividualPieceAttackBitboards() ... 利きビットボード
generateShogiMoveFromBitboards() ... 全盤上駒の指し手リスト
isKingInCheck(相手)             ... 不正局面チェック
isKingInCheck(自分)             ... 王手状態確認
filterMovesThatBlockThreat()    ... 王手回避手候補
filterLegalMovesList()          ... ★各手で盤面再計算して自殺手除外
generateDropNonPawnMoves()      ... 駒打ち（歩以外）
generateDropMoveForPawn()       ... 駒打ち（歩）
```

---

## 7. 問題点

### 7.1 パフォーマンス: filterLegalMovesList の計算量

**最も深刻な問題。** `filterLegalMovesList()` は候補手ごとに:

1. 盤面コピー（`QVector<QChar>` 81 要素）
2. 全駒の個別ビットボード再生成（`generateAllPieceBitboards` — 81 マス × 28 駒種を走査）
3. 全駒の駒配置ビットボード再生成（`generateBitboard`）
4. 全駒の利きビットボード再生成（`generateAllIndividualPieceAttackBitboards`）
5. 相手の全指し手リスト生成（`generateShogiMoveFromBitboards`）
6. 王手判定（`isKingInCheck`）

平手初期局面の合法手数は 30 だが、中盤では候補手が 100〜200 手になり得る。各候補で上記 6 ステップを繰り返すため、**O(候補手数 × 全駒数 × 81)** の計算量になる。

通常の将棋エンジンでは「指し手適用 → 自玉への利きのみ差分チェック」で O(1) に近い判定を行うが、現実装は毎回フルリビルドしている。

### 7.2 パフォーマンス: 毎回のフルビットボード再構築

`isLegalMove()` と `generateLegalMoves()` は呼び出しごとに:

- `generateBitboard()` — 81 マスの switch 文走査
- `generateAllPieceBitboards()` — 28 駒種 × 81 マスの走査
- `generateAllIndividualPieceAttackBitboards()` — 全個別駒の利き計算

呼び出し元（`ShogiGameController::validateAndMove`）は毎回 `MoveValidator validator;` でローカル生成して使い捨てるため、キャッシュの余地がない。

### 7.3 設計: エラーハンドリングが signal に依存しているが未接続

全バリデーションエラーは `errorOccurred(QString)` シグナルで通知されるが:

- ヘッダーに「現在connect先なし」と記載されている
- シグナル発行後も処理を続行する箇所がある（`checkDoublePawn` や `checkPieceCount` は `return` するが、`validateBoardAndPieces` がそれを検知せず次のチェックに進む）
- エラーが呼び出し元に伝搬しないため、不正な盤面でも判定処理が最後まで走る

```cpp
void validateBoardAndPieces(...) {
    checkDoublePawn(boardData);       // エラー検出しても return するだけ
    checkPieceCount(boardData, ...);  // 前のエラーを無視して続行
    checkCorrectPosition(boardData);
    checkKingPresence(boardData, ...);
}
```

### 7.4 設計: QObject 継承の不必要性

MoveValidator は QObject を継承しているが:

- signal は `errorOccurred` のみで未接続
- slot は持たない
- 親子関係は使われていない（ローカル変数として生成される）
- QObject のコピー禁止制約が適用される

QObject 継承をやめれば、コピー可能になり、`tr()` は `QCoreApplication::translate()` で代替できる。

### 7.5 設計: checkPieceCount が成駒を考慮しない

`checkPieceCount()` の `maxTotalPieceCount` マップには基本駒（`P`, `L`, `N` 等）しか定義されていない。成駒（`Q`, `M`, `O`, `T`, `C`, `U`）が盤上にある場合、対応する基本駒との合算チェックが行われない。

例: 先手の歩（`P`）が 9 枚盤上にあり、と金（`Q`）が 10 枚盤上にあっても、`P` は 9 ≤ 18、`Q` はマップに無いためチェックされない。実際には歩＋と金の合計は 18 枚以下でなければならない。

### 7.6 アルゴリズム: isKingInCheck が自駒の利きも走査する

```cpp
for (qsizetype i = 0; i < allPieceBitboards.size(); ++i) {
    if (i == kingIndex) continue;  // 自分の玉だけスキップ
    ...
}
```

先手玉の王手判定で `turn == BLACK` の場合、`kingIndex = K_IDX(7)` のみスキップし、先手の他の駒（`P_IDX(0)`〜`U_IDX(13)`）の利きも走査する。本来は**相手の駒の利きのみ**を調べるべきだが、自駒の利きが自玉と重なっても `numChecks` がインクリメントされてしまう。

ただし実際には、自駒の利きビットボードの生成時に味方駒のマスへの移動がブロックされる（`isPieceOnSquare` で味方駒なら break）ため、通常の局面では発火しない。しかし、ピン（飛び駒の利き筋上に味方駒がある場合）の処理が別ロジックのため、不要な走査が計算コストを増やしている。

### 7.7 アルゴリズム: necessaryMovesBitboard が両王手で上書きされる

```cpp
if ((kingBitboard & attackBitboard).any()) {
    attackWithoutKingBitboard = attackBitboard & ~kingBitboard;
    necessaryMovesBitboard = attackWithoutKingBitboard | allPieceBitboards[i][j];
    ++numChecks;
}
```

両王手（numChecks == 2）の場合、`necessaryMovesBitboard` は 2 番目の王手元の情報で上書きされ、1 番目の情報が失われる。コメントに「numChecks==1のときのみ有効」とあるが、`generateLegalMoves` 内で `numChecks >= 2` の場合は玉の移動のみ返すため、実用上は問題にならない。ただし意図が明確でなく、保守性に影響する。

### 7.8 データ構造: QVector<QVector<bitset<81>>> のヒープ割り当て

個別駒ビットボードに `QVector<QVector<std::bitset<81>>>` を使用している。

- 外側 QVector: 28 要素（駒種数）
- 内側 QVector: 0〜N 要素（盤上のその駒種の枚数分）

各 `bitset<81>` は 16 バイト（128 ビットに切り上げ）だが、QVector の動的メモリ割り当てとコピーコストが支配的。`filterLegalMovesList` 内のループで毎回 `generateAllPieceBitboards` が呼ばれるため、候補手ごとに `QVector` の `clear()` → `append()` が繰り返される。

`std::array` ベースの固定サイズ構造にすれば、ヒープ割り当てを回避できる。

### 7.9 コード重複: generateShogiMoveFromBitboard(s)

`generateShogiMoveFromBitboards()`（複数形、全駒の指し手一括生成）と `generateShogiMoveFromBitboard()`（単数形、1 駒の指し手生成）がほぼ同一のロジックを持つ:

- 行き場のない駒のチェック（`disallowedMove` の条件式）が両方に同一コードで存在
- `generateAllPromotions()` の呼び出しパターンも同一
- 差異は走査対象が全駒か 1 駒かだけ

### 7.10 コード重複: applyMovesToBoard が駒打ちを考慮しない

`applyMovesToBoard()` は `fromIndex = fromSquare.y() * 9 + fromSquare.x()` で計算するが、駒打ち（`fromSquare.x() >= 9`）の場合、`fromIndex` が 81 以上になり `boardDataAfterMove[fromIndex]` が範囲外アクセスになる。

現状は `filterLegalMovesList` 内で使われる盤上移動の手と、`validateMoveWithoutChecks` 内の打ち歩詰め判定でのみ呼ばれる。打ち歩詰め判定では `fromSquare.x() == 9 or 10` のため、`fromIndex` が `rank * 9 + 9` = 最大 `8 * 9 + 10 = 82` となる。`boardDataAfterMove` は 81 要素なので、`boardDataAfterMove[fromIndex] = ' '` は**範囲外書き込み**になる可能性がある。ただし Qt の `QVector::operator[]` は release ビルドで範囲チェックを行わないため、サイレントなメモリ破壊の可能性がある。

### 7.11 テストカバレッジの不足

ユニットテストは 6 ケースのみ:

| テスト名 | 検証内容 |
|---|---|
| `legalMoves_hirate` | 平手初期局面の合法手数が 30 |
| `isLegalMove_pawnAdvance` | 歩の前進が合法 |
| `isLegalMove_pawnBackward_illegal` | 歩の後退が不合法 |
| `twoPawn_illegal` | 二歩が不合法 |
| `kingInCheck` | 王手検出 |
| `promotionInEnemyTerritory` | 敵陣での成り |
| `mandatoryPromotion_pawnOnFirstRank` | 1 段目の強制成り |

以下のケースがテストされていない:

- 両王手の回避手
- 打ち歩詰め判定
- ピンされた駒の移動制限
- 桂馬の跳び越え
- 馬・龍の複合移動
- 後手番の合法手生成
- 駒台からの各種駒打ち（香・桂の行き場制限含む）
- 千日手判定と連動した王手検出の正確性

### 7.12 設計: isPawnInFrontOfKing の走査が非効率

相手玉の位置を `boardData` 全体を走査して探すが、既にビットボードで玉の位置は分かっている。呼び出しタイミングがビットボード生成後の別コンテキストのため、情報が引き継がれていない。

また、玉が見つかった場合 `return true/false` で即座に戻るが、見つからなかった場合（玉不在）は `return false` となり、エラー通知が無い。

## 8. 改善の方向性

| 項目 | 優先度 | 概要 |
|---|---|---|
| applyMovesToBoard の範囲外アクセス | 高 | 駒打ち時の fromIndex が 81 以上になる問題を修正 |
| filterLegalMovesList の差分更新化 | 中 | 全ビットボード再構築ではなく差分チェックに変更 |
| エラーハンドリングの伝搬 | 中 | signal 依存をやめ、戻り値またはフラグで伝搬 |
| checkPieceCount の成駒対応 | 中 | 基本駒+成駒の合計でチェック |
| isKingInCheck の走査範囲限定 | 低 | 相手駒のインデックス範囲のみ走査 |
| QVector → std::array 化 | 低 | ヒープ割り当て削減 |
| テストケース追加 | 中 | 打ち歩詰め、ピン、両王手、後手番等 |
| QObject 依存除去 | 低 | 純粋な C++ クラスへの変更 |
