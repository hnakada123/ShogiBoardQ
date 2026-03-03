# Task 20260303-08: 詰将棋局面生成テスト追加

## 概要
TsumeshogiPositionGenerator と TsumePositionUtil のユニットテストを新規追加する。現状は設定永続化テスト(tst_settings_roundtrip)のみで、生成ロジック本体のテストが皆無。

## 優先度
High

## 背景
- `TsumeshogiPositionGenerator` は非QObject、UI非依存の純粋ロジッククラスで単体テストに最適
- `TsumePositionUtil` はヘッダーオンリーの static メソッド群で、入出力が明確
- 詰将棋局面生成はユーザーの主要機能の一つだがテストカバレッジがゼロ

## 対象ファイル

### 参照（変更不要だが理解が必要）
- `src/analysis/tsumeshogipositiongenerator.h` - 生成クラス定義（Settings構造体、generate(), generateBatch()）
- `src/analysis/tsumeshogipositiongenerator.cpp` - 生成ロジック実装（382行）
- `src/common/tsumepositionutil.h` - static ユーティリティ（buildPositionWithMoves, buildPositionForMate）

### 新規作成
1. `tests/tst_tsumeshogi_generator.cpp` - テストファイル

### 修正対象
2. `tests/CMakeLists.txt` - テスト登録追加

## 実装手順

### Step 1: テストファイル新規作成

`tests/tst_tsumeshogi_generator.cpp` を新規作成する。

テストケース方針:

**TsumePositionUtil::buildPositionWithMoves テスト:**
1. `nullMoves_returnsStartpos` - usiMoves=nullptr 時に "position startpos" を返す
2. `emptyMoves_returnsStartpos` - 空リスト時に "position startpos" を返す
3. `zeroIndex_returnsStartpos` - selectedIndex=0 時に開始局面を返す
4. `withMoves_returnsCorrectPositionCmd` - 正常な指し手リストで "position startpos moves 7g7f 3c3d" 形式を返す
5. `sfenStart_returnsSfenPosition` - startPositionCmd が "sfen ..." の場合の処理
6. `indexBeyondMoves_clampsToSize` - selectedIndex がリストサイズ超えの場合

**TsumePositionUtil::buildPositionForMate テスト:**
7. `sfenRecord_returnsPositionWithForcedTurn` - sfenRecord から局面取得+手番強制
8. `onlyAttackerKing_turnIsWhite` - 攻方玉のみ（小文字k）→ 手番 b
9. `onlyDefenderKing_turnIsBlack` - 受方玉のみ（大文字K）→ 手番 w
10. `bothKings_preservesTurn` - 両玉ありの場合は元の手番維持
11. `allSourcesEmpty_returnsEmpty` - 全入力が空の場合は空文字列
12. `fallbackToStartSfen` - sfenRecord が空で startSfenStr にフォールバック

**TsumeshogiPositionGenerator テスト:**
13. `generate_returnsValidSfen` - 生成結果が有効なSFEN形式か
14. `generate_noAttackerKing` - 攻方（先手）に玉がないことを確認（詰将棋の慣例）
15. `generate_hasDefenderKing` - 受方（後手）に玉があることを確認
16. `generate_respectsMaxAttackPieces` - maxAttackPieces 設定を超えないことを確認
17. `generate_respectsMaxDefendPieces` - maxDefendPieces 設定を超えないことを確認
18. `generateBatch_returnsRequestedCount` - 指定数の候補を返す
19. `generateBatch_cancellation` - CancelFlag で中断できることを確認
20. `generate_multipleCalls_produceDifferentResults` - 複数回呼び出しで異なる結果（ランダム性確認）

テンプレート:
```cpp
/// @file tst_tsumeshogi_generator.cpp
/// @brief 詰将棋局面生成テスト

#include <QtTest>
#include <memory>

#include "tsumeshogipositiongenerator.h"
#include "tsumepositionutil.h"
#include "threadtypes.h"

class TestTsumeshogiGenerator : public QObject
{
    Q_OBJECT

private:
    /// SFEN文字列の基本バリデーション（スペース区切りで4トークン: 盤面 手番 持駒 手数）
    static bool isValidSfenFormat(const QString& sfen)
    {
        const QStringList tokens = sfen.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (tokens.size() != 4) return false;
        // 手番チェック
        if (tokens[1] != QLatin1String("b") && tokens[1] != QLatin1String("w")) return false;
        return true;
    }

    /// SFEN盤面部分から指定文字の出現数をカウント
    static int countPieceInBoard(const QString& boardPart, QChar piece)
    {
        int count = 0;
        for (const QChar ch : boardPart) {
            if (ch == piece) ++count;
        }
        return count;
    }

private slots:
    // --- TsumePositionUtil::buildPositionWithMoves ---
    void nullMoves_returnsStartpos();
    void emptyMoves_returnsStartpos();
    void zeroIndex_returnsStartpos();
    void withMoves_returnsCorrectPositionCmd();
    void sfenStart_returnsSfenPosition();
    void indexBeyondMoves_clampsToSize();

    // --- TsumePositionUtil::buildPositionForMate ---
    void sfenRecord_returnsPositionWithForcedTurn();
    void onlyDefenderKing_turnIsWhite();
    void onlyAttackerKing_turnIsBlack();
    void bothKings_preservesTurn();
    void allSourcesEmpty_returnsEmpty();
    void fallbackToStartSfen();

    // --- TsumeshogiPositionGenerator ---
    void generate_returnsValidSfen();
    void generate_noAttackerKing();
    void generate_hasDefenderKing();
    void generate_respectsMaxAttackPieces();
    void generate_respectsMaxDefendPieces();
    void generateBatch_returnsRequestedCount();
    void generateBatch_cancellation();
    void generate_multipleCalls_produceDifferentResults();
};
```

各テストメソッドの実装指針:
- `buildPositionWithMoves` テストは入力と期待される文字列を直接比較
- `buildPositionForMate` テストは返り値の "position sfen" プレフィックスと手番トークンを検証
- `generate()` テストはSFEN文字列のフォーマットと駒の存在・数を検証
- `generateBatch()` テストは `std::make_shared<std::atomic_bool>(false)` で CancelFlag を作成

### Step 2: CMakeLists.txt にテスト登録

`tests/CMakeLists.txt` に以下を追加:

```cmake
# ============================================================
# Unit: TsumeshogiPositionGenerator + TsumePositionUtil テスト
# ============================================================
add_shogi_test(tst_tsumeshogi_generator
    tst_tsumeshogi_generator.cpp
    ${SRC}/common/logcategories.cpp
    ${SRC}/analysis/tsumeshogipositiongenerator.cpp
    ${SRC}/common/tsumepositionutil.cpp
    ${SRC}/core/shogiboard.cpp
    ${SRC}/core/shogiboard_edit.cpp
    ${SRC}/core/shogiboard_sfen.cpp
    ${SRC}/core/shogimove.cpp
    ${SRC}/common/errorbus.cpp
)
```

注意: コンパイルエラーが出た場合、不足している依存ソース（shogitypes.h 関連等）を追加すること。`threadtypes.h` は `CancelFlag` = `std::shared_ptr<std::atomic_bool>` の typedef を含む。

### Step 3: ビルド・テスト実行
```bash
cmake -B build -S .
cmake --build build -j4
ctest --test-dir build --output-on-failure -R tst_tsumeshogi_generator
```

### Step 4: 全テスト実行
```bash
ctest --test-dir build --output-on-failure
```

## 受け入れ基準
- TsumePositionUtil の static メソッド 6テスト以上が追加される
- TsumeshogiPositionGenerator の生成ロジック 6テスト以上が追加される
- 生成されたSFENの基本的な妥当性（形式、玉の有無、駒数制約）が検証される
- ビルド成功（warning なし）
- 既存テスト全件成功
