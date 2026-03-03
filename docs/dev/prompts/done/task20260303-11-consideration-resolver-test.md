# Task 20260303-11: 検討モード局面解決テスト追加

## 概要
ConsiderationPositionResolver のユニットテストを新規追加する。現状は配線契約テスト(tst_wiring_consideration)とUIステートテストのみで、局面解決ロジック本体のテストがない。

## 優先度
Medium

## 背景
- `ConsiderationPositionResolver` は非QObject、UI非依存で入出力が明確
- `Inputs` 構造体にポインタを渡し、`resolveForRow(int row)` で `UpdateParams` を返す純粋なデータ変換
- 検討モードの局面更新は棋譜ナビゲーションと連動する重要な機能

## 対象ファイル

### 参照（変更不要だが理解が必要）
- `src/kifu/considerationpositionresolver.h` - クラス定義（Inputs, UpdateParams 構造体）
- `src/kifu/considerationpositionresolver.cpp` - 実装（161行）
- `src/app/considerationpositionservice.h/.cpp` - アプリ層サービス（56/65行）

### 新規作成
1. `tests/tst_consideration_resolver.cpp` - テストファイル

### 修正対象
2. `tests/CMakeLists.txt` - テスト登録追加

## 実装手順

### Step 1: テストファイル新規作成

`tests/tst_consideration_resolver.cpp` を新規作成する。

テストケース方針:

**基本解決テスト:**
1. `resolveForRow_initialPosition` - row=0（初期局面）で position が startpos を含む
2. `resolveForRow_midGame` - row=3 等で position に moves が含まれる
3. `resolveForRow_negativeRow_clamped` - row<0 で安全にデフォルト値を返す

**ハイライト座標テスト:**
4. `resolveForRow_previousMoveCoordinates` - 直前の指し手の座標が fileTo/rankTo に反映される
5. `resolveForRow_initialPosition_noHighlight` - row=0 で fileTo/rankTo が 0

**USI指し手テスト:**
6. `resolveForRow_lastUsiMove_midGame` - 中盤局面で最後のUSI指し手が返る
7. `resolveForRow_initialPosition_emptyUsiMove` - 初期局面で lastUsiMove が空

**分岐局面テスト:**
8. `resolveForRow_branchPosition_useCurrentSfen` - currentSfenStr が設定されている場合はそちらを優先

**エッジケーステスト:**
9. `resolveForRow_allInputsNull` - 全入力 nullptr でクラッシュしない
10. `resolveForRow_emptyLists` - 空リストでクラッシュしない
11. `resolveForRow_beyondListSize` - リストサイズを超える row で安全に処理

テンプレート:
```cpp
/// @file tst_consideration_resolver.cpp
/// @brief 検討モード局面解決テスト

#include <QtTest>

#include "considerationpositionresolver.h"
#include "shogimove.h"

class TestConsiderationResolver : public QObject
{
    Q_OBJECT

private:
    /// テスト用のデータセットを準備
    struct TestData {
        QStringList positionStrList;
        QStringList gameUsiMoves;
        QList<ShogiMove> gameMoves;
        QString startSfenStr;
        QStringList sfenRecord;
        QString currentSfenStr;
    };

    /// 平手から3手進んだテストデータ
    TestData makeSampleData()
    {
        TestData d;
        d.startSfenStr = QStringLiteral("startpos");
        d.gameUsiMoves = {
            QStringLiteral("7g7f"),
            QStringLiteral("3c3d"),
            QStringLiteral("2g2f")
        };
        d.positionStrList = {
            QStringLiteral("position startpos"),
            QStringLiteral("position startpos moves 7g7f"),
            QStringLiteral("position startpos moves 7g7f 3c3d"),
            QStringLiteral("position startpos moves 7g7f 3c3d 2g2f"),
        };
        d.sfenRecord = {
            QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1"),
            QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/2P6/PP1PPPPPP/1B5R1/LNSGKGSNL w - 2"),
            QStringLiteral("lnsgkgsnl/1r5b1/pppppp1pp/6p2/9/2P6/PP1PPPPPP/1B5R1/LNSGKGSNL b - 3"),
            QStringLiteral("lnsgkgsnl/1r5b1/pppppp1pp/6p2/9/2P4P1/PP1PPPP1P/1B5R1/LNSGKGSNL w - 4"),
        };
        return d;
    }

    ConsiderationPositionResolver::Inputs makeInputs(const TestData& d)
    {
        ConsiderationPositionResolver::Inputs inputs;
        inputs.positionStrList = &d.positionStrList;
        inputs.gameUsiMoves = &d.gameUsiMoves;
        inputs.gameMoves = &d.gameMoves;
        inputs.startSfenStr = &d.startSfenStr;
        inputs.sfenRecord = &d.sfenRecord;
        if (!d.currentSfenStr.isEmpty())
            inputs.currentSfenStr = &d.currentSfenStr;
        return inputs;
    }

private slots:
    void resolveForRow_initialPosition();
    void resolveForRow_midGame();
    void resolveForRow_negativeRow_clamped();
    void resolveForRow_previousMoveCoordinates();
    void resolveForRow_initialPosition_noHighlight();
    void resolveForRow_lastUsiMove_midGame();
    void resolveForRow_initialPosition_emptyUsiMove();
    void resolveForRow_allInputsNull();
    void resolveForRow_emptyLists();
    void resolveForRow_beyondListSize();
};
```

実装指針:
- `Inputs` 構造体の各ポインタにはテスト用ローカル変数のアドレスを渡す
- `resolveForRow()` の返り値 `UpdateParams` の `position`, `previousFileTo`, `previousRankTo`, `lastUsiMove` を QCOMPARE/QVERIFY で検証
- null安全性テストでは `Inputs` の全ポインタを nullptr にして resolveForRow を呼び、クラッシュしないことを確認
- `KifuLoadCoordinator*`, `KifuRecordListModel*`, `KifuBranchTree*`, `KifuNavigationState*` は nullptr でも基本的な動作をテストできる

### Step 2: CMakeLists.txt にテスト登録

`tests/CMakeLists.txt` に以下を追加:

```cmake
# ============================================================
# Unit: ConsiderationPositionResolver テスト
# ============================================================
add_shogi_test(tst_consideration_resolver
    tst_consideration_resolver.cpp
    ${TEST_STUBS}
    ${SRC}/kifu/considerationpositionresolver.cpp
    ${SRC}/core/shogimove.cpp
    ${SRC}/core/shogiutils.cpp
    ${SRC}/core/shogiboard.cpp
    ${SRC}/core/shogiboard_edit.cpp
    ${SRC}/core/shogiboard_sfen.cpp
)
```

コンパイルエラーが出た場合、ConsiderationPositionResolver が参照する型のヘッダーインクルードパスを確認すること。

### Step 3: ビルド・テスト実行
```bash
cmake -B build -S .
cmake --build build -j4
ctest --test-dir build --output-on-failure -R tst_consideration_resolver
```

### Step 4: 全テスト実行
```bash
ctest --test-dir build --output-on-failure
```

## 受け入れ基準
- 基本的な局面解決テスト 3件以上
- ハイライト座標・USI指し手テスト 4件以上
- null安全性・エッジケーステスト 3件以上
- ビルド成功（warning なし）
- 既存テスト全件成功
