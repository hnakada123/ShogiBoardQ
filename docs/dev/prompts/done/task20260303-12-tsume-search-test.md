# Task 20260303-12: 詰み探索テスト強化

## 概要
詰み探索フローの中核ロジック（TsumePositionUtil 統合テストと USI checkmate 応答の end-to-end 検証）を追加する。現状は UIステートポリシーテストと USI プロトコル単体テストのみ。

## 優先度
Medium

## 背景
- `TsumeSearchFlowController` は `runWithDialog` でダイアログを表示するため直接テストが困難
- ただし内部で使用する `TsumePositionUtil::buildPositionForMate` と `buildPositionWithMoves` の組み合わせ動作はテスト可能
- USI checkmate 応答の解析は `tst_usiprotocolhandler.cpp` にあるが、局面構築→checkmate送信→応答解析の一連フローは未テスト
- Task 08 で TsumePositionUtil の単体テストは追加済みなので、ここではより実践的な統合テストに焦点

## 対象ファイル

### 参照（変更不要だが理解が必要）
- `src/common/tsumepositionutil.h` - 局面構築ユーティリティ
- `src/analysis/tsumesearchflowcontroller.h/.cpp` - 詰み探索フローコントローラ
- `src/engine/usiprotocolhandler.h/.cpp` - USI プロトコルハンドラ（checkmate 応答解析）
- `tests/tst_usiprotocolhandler.cpp` - 既存の checkmate テスト確認

### 新規作成
1. `tests/tst_tsume_search.cpp` - テストファイル

### 修正対象
2. `tests/CMakeLists.txt` - テスト登録追加

## 実装手順

### Step 1: テストファイル新規作成

`tests/tst_tsume_search.cpp` を新規作成する。

テストケース方針:

**TsumePositionUtil 統合テスト（詰み探索向け局面構築）:**
1. `buildForMate_singleKing_forcesAttackerTurn` - 片方の玉のみの局面で攻方手番に強制
2. `buildForMate_tsumePosition_correctFormat` - 実際の詰将棋局面（例: 1手詰め）で正しい position コマンドを生成
3. `buildForMate_midGamePosition_withMoves` - usiMoves ありの中盤局面で moves 付き position を生成
4. `buildForMate_handicapPosition` - 駒落ち局面のSFEN処理

**局面構築→チェックメイトコマンド構築フロー:**
5. `buildCheckmateSendCommand_fromStartpos` - buildPositionWithMoves の結果に "go mate infinite" を付加
6. `buildCheckmateSendCommand_fromSfen` - SFEN局面から checkmate コマンド構築

**詰将棋典型局面テスト:**
7. `tsumePosition_oneMoveMate` - 1手詰め局面で正しい position コマンドが構築できる
8. `tsumePosition_threeMoveMate` - 3手詰め局面で正しい position コマンドが構築できる
9. `tsumePosition_noMatePosition` - 詰みなし局面でも position コマンドは正常に構築される

**エッジケーステスト:**
10. `buildForMate_emptyBoard_returnsEmpty` - 盤面データなしで空文字列
11. `buildForMate_positionStrList_fallback` - sfenRecord が空で positionStrList にフォールバック

テンプレート:
```cpp
/// @file tst_tsume_search.cpp
/// @brief 詰み探索フロー統合テスト

#include <QtTest>

#include "tsumepositionutil.h"

class TestTsumeSearch : public QObject
{
    Q_OBJECT

private:
    // 1手詰め局面: 後手玉が1一、先手金が2一にいて、1二金打で詰み
    static const QString kOneMoveMateBoard;  // 実際の詰め局面SFENを設定

private slots:
    // --- 局面構築統合テスト ---
    void buildForMate_singleKing_forcesAttackerTurn();
    void buildForMate_tsumePosition_correctFormat();
    void buildForMate_midGamePosition_withMoves();
    void buildForMate_handicapPosition();

    // --- チェックメイトコマンド構築 ---
    void buildCheckmateSendCommand_fromStartpos();
    void buildCheckmateSendCommand_fromSfen();

    // --- 典型局面テスト ---
    void tsumePosition_oneMoveMate();
    void tsumePosition_threeMoveMate();
    void tsumePosition_noMatePosition();

    // --- エッジケース ---
    void buildForMate_emptyBoard_returnsEmpty();
    void buildForMate_positionStrList_fallback();
};
```

実装指針:
- 詰将棋のSFEN文字列は手動で用意する。例:
  - 1手詰め: `"3sks3/9/4G4/9/9/9/9/9/9 b G2r2b4g4s4n4l18p 1"` (金打で詰み)
  - ※ 実際の有効な詰め局面SFENは実装時にバリデーションすること
- `buildPositionForMate` の返り値が "position sfen ..." で始まることを確認
- 手番トークン（b/w）が攻方になっていることを確認
- `buildPositionWithMoves` と `buildPositionForMate` の結果が期待するUSIコマンド形式であることを文字列比較

### Step 2: CMakeLists.txt にテスト登録

`tests/CMakeLists.txt` に以下を追加:

```cmake
# ============================================================
# Unit: 詰み探索フロー統合テスト
# ============================================================
add_shogi_test(tst_tsume_search
    tst_tsume_search.cpp
    ${SRC}/common/logcategories.cpp
    ${SRC}/common/tsumepositionutil.cpp
)
```

TsumePositionUtil はヘッダーオンリーに近い（.cpp は 6行のスタブ）ため、依存は最小限。

### Step 3: ビルド・テスト実行
```bash
cmake -B build -S .
cmake --build build -j4
ctest --test-dir build --output-on-failure -R tst_tsume_search
```

### Step 4: 全テスト実行
```bash
ctest --test-dir build --output-on-failure
```

## 受け入れ基準
- TsumePositionUtil 統合テスト 4件以上
- 典型的な詰将棋局面（1手詰め、3手詰め等）のテスト 2件以上
- エッジケーステスト 2件以上
- ビルド成功（warning なし）
- 既存テスト全件成功
