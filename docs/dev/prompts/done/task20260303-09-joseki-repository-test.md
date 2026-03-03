# Task 20260303-09: 定跡リポジトリ・プレゼンターテスト追加

## 概要
JosekiRepository のCRUD操作と JosekiPresenter の static ユーティリティメソッドのユニットテストを新規追加する。現状は JosekiWindow の設定永続化テストのみで、定跡データ操作のロジックテストが不足。

## 優先度
High

## 背景
- `JosekiRepository` は非QObjectで、定跡データのインメモリCRUD + ファイルI/Oを担当
- `JosekiPresenter` は QObject だが、`normalizeSfen()` や `pieceToKanji()` 等の static メソッドは純粋関数
- YANEURAOU-DB2016 形式の定跡ファイルの読み書きテストがない

## 対象ファイル

### 参照（変更不要だが理解が必要）
- `src/dialogs/josekirepository.h` - リポジトリクラス定義
- `src/dialogs/josekirepository.cpp` - リポジトリ実装（404行）
- `src/dialogs/josekipresenter.h` - プレゼンタークラス定義
- `src/dialogs/josekipresenter.cpp` - プレゼンター実装（288行）
- `src/dialogs/josekiioresult.h` - I/O結果型（JosekiLoadResult, JosekiSaveResult）
- `src/dialogs/josekiwindow.h` - JosekiMove 構造体定義（move, nextMove, value, depth, frequency, comment）

### 新規作成
1. `tests/tst_joseki_repository.cpp` - テストファイル

### 修正対象
2. `tests/CMakeLists.txt` - テスト登録追加

## 実装手順

### Step 1: テストファイル新規作成

`tests/tst_joseki_repository.cpp` を新規作成する。

テストケース方針:

**JosekiRepository CRUD テスト:**
1. `initialState_isEmpty` - 初期状態で isEmpty()==true, positionCount()==0
2. `addMove_addsToPosition` - addMove 後に containsPosition==true, movesForPosition で取得可能
3. `addMove_multipleMovesToSamePosition` - 同一局面に複数手追加
4. `updateMove_changesValues` - updateMove で value/depth/frequency/comment が更新される
5. `deleteMove_removesAtIndex` - deleteMove でインデックス指定削除
6. `removeMoveByUsi_removesMatching` - removeMoveByUsi でUSI指し手一致で削除
7. `clear_resetsAll` - clear で全データクリア
8. `containsPosition_false_forUnknown` - 未登録局面で false
9. `movesForPosition_returnsEmptyForUnknown` - 未登録局面で空リスト

**JosekiRepository ファイルI/O テスト:**
10. `saveAndLoad_roundTrip` - saveToFile → loadFromFile でデータが一致
11. `parseFromFile_invalidPath_failsGracefully` - 存在しないファイルでエラー
12. `serializeToFile_writesExpectedFormat` - 一時ファイルに書き出して内容確認

**JosekiRepository マージ操作テスト:**
13. `registerMergeMove_newEntry_addsWithFrequency1` - 新規マージ登録で頻度1
14. `registerMergeMove_existing_incrementsFrequency` - 既存指し手でマージすると頻度+1
15. `ensureSfenWithPly_registersOnce` - 手数付きSFENが未登録時のみ追加

**JosekiPresenter static メソッドテスト:**
16. `normalizeSfen_removesPlyNumber` - "lnsgkgsnl/... b - 1" → 手数なし正規化
17. `normalizeSfen_alreadyNormalized` - 既に正規化済みならそのまま
18. `pieceToKanji_pawn` - 'P' → "歩"
19. `pieceToKanji_promotedPawn` - 'P' + promoted=true → "と"
20. `pieceToKanji_allPieces` - 全駒種の変換確認
21. `hasDuplicateMove_true` - 同じUSI指し手が既存時 true
22. `hasDuplicateMove_false` - 異なるUSI指し手時 false

テンプレート:
```cpp
/// @file tst_joseki_repository.cpp
/// @brief 定跡リポジトリ・プレゼンターテスト

#include <QtTest>
#include <QTemporaryFile>
#include <QTemporaryDir>

#include "josekirepository.h"
#include "josekipresenter.h"
#include "josekiwindow.h"    // JosekiMove 構造体

class TestJosekiRepository : public QObject
{
    Q_OBJECT

private:
    static JosekiMove makeMove(const QString& usi, int value = 0, int depth = 0,
                               int freq = 1, const QString& comment = {})
    {
        JosekiMove m;
        m.move = usi;
        m.value = value;
        m.depth = depth;
        m.frequency = freq;
        m.comment = comment;
        return m;
    }

    static const QString kHirateSfen;  // "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1"

private slots:
    // --- CRUD ---
    void initialState_isEmpty();
    void addMove_addsToPosition();
    void addMove_multipleMovesToSamePosition();
    void updateMove_changesValues();
    void deleteMove_removesAtIndex();
    void removeMoveByUsi_removesMatching();
    void clear_resetsAll();
    void containsPosition_false_forUnknown();
    void movesForPosition_returnsEmptyForUnknown();

    // --- File I/O ---
    void saveAndLoad_roundTrip();
    void parseFromFile_invalidPath_failsGracefully();

    // --- Merge ---
    void registerMergeMove_newEntry_addsWithFrequency1();
    void registerMergeMove_existing_incrementsFrequency();
    void ensureSfenWithPly_registersOnce();

    // --- JosekiPresenter static ---
    void normalizeSfen_removesPlyNumber();
    void normalizeSfen_alreadyNormalized();
    void pieceToKanji_pawn();
    void pieceToKanji_promotedPawn();
    void pieceToKanji_allPieces();
    void hasDuplicateMove_true();
    void hasDuplicateMove_false();
};
```

注意:
- JosekiPresenter のインスタンステストには `JosekiRepository*` が必要。テスト内でローカルインスタンスを作成して渡す
- ファイルI/Oテストには `QTemporaryDir` を使い、テスト後に自動クリーンアップ
- `JosekiMove` 構造体は `src/dialogs/josekiwindow.h` で定義されている

### Step 2: CMakeLists.txt にテスト登録

`tests/CMakeLists.txt` に以下を追加:

```cmake
# ============================================================
# Unit: JosekiRepository + JosekiPresenter テスト
# ============================================================
add_shogi_test(tst_joseki_repository
    tst_joseki_repository.cpp
    ${TEST_STUBS}
    ${SRC}/dialogs/josekirepository.cpp
    ${SRC}/dialogs/josekipresenter.cpp
    ${SRC}/board/sfenpositiontracer.cpp
    ${SRC}/core/shogimove.cpp
    ${SRC}/core/shogiutils.cpp
    ${SRC}/core/shogiboard.cpp
    ${SRC}/core/shogiboard_edit.cpp
    ${SRC}/core/shogiboard_sfen.cpp
)
```

コンパイルエラーが出た場合、JosekiPresenter が依存する SfenPositionTracer 等の追加ソースを確認して追加すること。

### Step 3: ビルド・テスト実行
```bash
cmake -B build -S .
cmake --build build -j4
ctest --test-dir build --output-on-failure -R tst_joseki_repository
```

### Step 4: 全テスト実行
```bash
ctest --test-dir build --output-on-failure
```

## 受け入れ基準
- JosekiRepository CRUD テスト 9件以上
- ファイルI/O ラウンドトリップテスト 1件以上
- JosekiPresenter static メソッドテスト 5件以上
- ビルド成功（warning なし）
- 既存テスト全件成功
