# Task 20260303-18: 入玉宣言テスト強化

## 概要
入玉宣言の判定ロジック（持将棋点数計算）のテストを強化する。現状は GameEndHandler の4テストと JishogiCalculator の基本テストのみで、様々な局面での点数計算テストが不足。

## 優先度
Low

## 背景
- `JishogiCalculator` は入玉宣言の点数計算（24点法・27点法）を担当
- `GameEndHandler::handleNyugyokuDeclaration` は宣言の成否判定と結果処理を担当
- 入玉宣言は複雑なルール（玉の位置、駒の点数、相手陣内の駒数）を含む
- 各種エッジケース（ギリギリの点数、片方のみ入玉等）のテストが不足

## 対象ファイル

### 参照（変更不要だが理解が必要）
- `src/common/jishogicalculator.h` - 持将棋計算クラス定義
- `src/common/jishogicalculator.cpp` - 持将棋計算実装
- `tests/tst_coredatastructures.cpp` - 既存の jishogi_calculate テスト確認
- `tests/tst_game_end_handler.cpp` - 既存の handleNyugyokuDeclaration テスト確認

### 新規作成
1. `tests/tst_jishogi_calculator.cpp` - テストファイル

### 修正対象
2. `tests/CMakeLists.txt` - テスト登録追加

## 実装手順

### Step 1: 既存テストの確認

まず `tst_coredatastructures.cpp` 内の `jishogi_calculate` テストを確認し、カバーされていないケースを特定する。

### Step 2: テストファイル新規作成

`tests/tst_jishogi_calculator.cpp` を新規作成する。

テストケース方針:

**基本点数計算テスト:**
1. `calculate_hirate_initialPosition` - 平手初期局面での点数（先手・後手とも）
2. `calculate_bothKingsInEnemyCamp` - 両玉入玉済みの局面
3. `calculate_onlyOneKingInEnemyCamp` - 片方のみ入玉

**駒点数テスト:**
4. `pieceScore_bigPieces` - 大駒（飛車・角）各5点
5. `pieceScore_smallPieces` - 小駒（その他）各1点
6. `pieceScore_handPieces_counted` - 持駒も点数に含まれる
7. `pieceScore_promotedPieces` - 成駒の点数（成銀は小駒1点等）

**宣言条件テスト:**
8. `declaration_24point_success` - 24点法で24点以上→宣言成功
9. `declaration_24point_fail` - 24点法で23点以下→宣言失敗
10. `declaration_27point_sente_success` - 27点法で先手28点以上→宣言成功
11. `declaration_27point_gote_success` - 27点法で後手27点以上→宣言成功
12. `declaration_kingNotInCamp_fail` - 玉が相手陣に入っていない→失敗

**エッジケーステスト:**
13. `calculate_emptyBoard` - 玉のみ（他の駒なし）の局面
14. `calculate_allPiecesOnesSide` - 全駒が片方のプレイヤーにある極端な局面
15. `calculate_exactThreshold` - ちょうど閾値の点数

テンプレート:
```cpp
/// @file tst_jishogi_calculator.cpp
/// @brief 持将棋・入玉宣言点数計算テスト

#include <QtTest>

#include "jishogicalculator.h"
#include "shogiboard.h"

class TestJishogiCalculator : public QObject
{
    Q_OBJECT

private:
    /// SFEN文字列から ShogiBoard を構築するヘルパー
    ShogiBoard boardFromSfen(const QString& sfen)
    {
        ShogiBoard board;
        board.fromSfen(sfen);
        return board;
    }

private slots:
    void calculate_hirate_initialPosition();
    void calculate_bothKingsInEnemyCamp();
    void calculate_onlyOneKingInEnemyCamp();
    void pieceScore_bigPieces();
    void pieceScore_smallPieces();
    void pieceScore_handPieces_counted();
    void declaration_24point_success();
    void declaration_24point_fail();
    void declaration_kingNotInCamp_fail();
    void calculate_emptyBoard();
    void calculate_exactThreshold();
};
```

実装指針:
- `JishogiCalculator` のAPIを確認してからテストを書く。点数計算関数のシグネチャ（引数が ShogiBoard なのか Piece 配列なのか）を確認
- テスト用のSFEN局面は手動で作成。入玉局面のSFEN例:
  - 先手玉が1一（相手陣内）: `"K8/9/9/9/9/9/9/9/8k b - 1"` （簡易版）
- 24点法・27点法の判定ロジックの閾値は `JishogiCalculator` の実装を確認してテスト値を設定
- `ShogiBoard::fromSfen()` で局面を構築し、計算関数に渡す

### Step 3: CMakeLists.txt にテスト登録

`tests/CMakeLists.txt` に以下を追加:

```cmake
# ============================================================
# Unit: JishogiCalculator テスト
# ============================================================
add_shogi_test(tst_jishogi_calculator
    tst_jishogi_calculator.cpp
    ${SRC}/common/logcategories.cpp
    ${SRC}/common/errorbus.cpp
    ${SRC}/common/jishogicalculator.cpp
    ${SRC}/core/shogiboard.cpp
    ${SRC}/core/shogiboard_edit.cpp
    ${SRC}/core/shogiboard_sfen.cpp
    ${SRC}/core/shogimove.cpp
    ${SRC}/core/shogiutils.cpp
)
```

### Step 4: ビルド・テスト実行
```bash
cmake -B build -S .
cmake --build build -j4
ctest --test-dir build --output-on-failure -R tst_jishogi_calculator
```

### Step 5: 全テスト実行
```bash
ctest --test-dir build --output-on-failure
```

## 受け入れ基準
- 点数計算テスト 5件以上
- 宣言条件テスト（24点法/27点法）3件以上
- エッジケーステスト 2件以上
- ビルド成功（warning なし）
- 既存テスト全件成功
