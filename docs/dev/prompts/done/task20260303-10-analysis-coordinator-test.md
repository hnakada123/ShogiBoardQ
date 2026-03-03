# Task 20260303-10: 棋譜解析テスト強化（AnalysisCoordinator + AnalysisResultHandler）

## 概要
AnalysisCoordinator のエンジン応答処理と AnalysisResultHandler の結果蓄積・確定ロジックのテストを追加する。現状は AnalysisFlowController の状態遷移テスト(17件)のみで、実際の解析データ処理パスのテストが不足。

## 優先度
High

## 背景
- `AnalysisCoordinator` は QObject で、`requestSendUsiCommand` シグナル経由でエンジンと疎結合。モック不要でテスト可能
- `AnalysisResultHandler` は非QObject で、一時結果の保持→確定処理のロジックをテストできる
- `parseInfoUSI()` は private static だが、`onEngineInfoLine()` 経由で間接テスト可能
- 棋譜解析はユーザーの主要機能だが、解析データの正確性を検証するテストがない

## 対象ファイル

### 参照（変更不要だが理解が必要）
- `src/analysis/analysiscoordinator.h` - コーディネータ定義（Deps, Options, Mode, ParsedInfo）
- `src/analysis/analysiscoordinator.cpp` - コーディネータ実装（297行）
- `src/analysis/analysisresulthandler.h` - 結果ハンドラ定義（Refs構造体、updatePending/commitPendingResult）
- `src/analysis/analysisresulthandler.cpp` - 結果ハンドラ実装（523行）
- `tests/tst_analysisflow.cpp` - 既存テスト確認

### 新規作成
1. `tests/tst_analysis_coordinator.cpp` - テストファイル

### 修正対象
2. `tests/CMakeLists.txt` - テスト登録追加

## 実装手順

### Step 1: テストファイル新規作成

`tests/tst_analysis_coordinator.cpp` を新規作成する。

テストケース方針:

**AnalysisCoordinator 基本テスト:**
1. `initialState_isIdle` - 初期状態で mode==Idle, currentPly==-1
2. `startAnalyzeRange_emitsPositionPrepared` - 範囲解析開始で positionPrepared シグナルが発火
3. `startAnalyzeSingle_emitsPositionPrepared` - 単一局面解析で positionPrepared シグナルが発火
4. `stop_returnsToIdle` - stop() 後に解析が中断される
5. `startWhileRunning_ignored` - 解析中の再start は無視される

**AnalysisCoordinator エンジン応答処理テスト:**
6. `onEngineInfoLine_scorecp_emitsProgress` - `info depth 10 score cp 100 pv 7g7f` でanalysisProgress発火
7. `onEngineInfoLine_scoremate_emitsProgress` - `info depth 20 score mate 5 pv ...` で mate 情報付き発火
8. `onEngineInfoLine_noScore_noEmit` - score 情報なしの info 行で progress 未発火
9. `onEngineBestmove_emitsAnalysisFinished` - 単一局面で bestmove 受信時に analysisFinished(SinglePosition) 発火
10. `onEngineBestmove_range_advancesToNextPly` - 範囲解析で bestmove 後に次の ply へ進む

**AnalysisCoordinator USI コマンド送信テスト:**
11. `sendGoCommand_emitsRequestSendUsiCommand` - sendGoCommand で "position" と "go" コマンドが送信される
12. `setOptions_multiPV_sendSetoption` - multiPV>1 時に setoption コマンドが送信される

**AnalysisResultHandler テスト:**
13. `updatePending_storesData` - updatePending でデータが保持される
14. `commitPendingResult_updatesLastCommitted` - commitPendingResult で lastCommittedPly/ScoreCp が更新
15. `reset_clearsAllPending` - reset で一時結果がクリア
16. `extractUsiMoveFromKanji_basicMove` - "☗７六歩(77)" → "7g7f" 等の変換
17. `extractUsiMoveFromKanji_drop` - "☗５五角打" → 打ち駒の変換
18. `extractUsiMoveFromKanji_emptyInput` - 空文字列で空文字列を返す

テンプレート:
```cpp
/// @file tst_analysis_coordinator.cpp
/// @brief 棋譜解析コーディネータ・結果ハンドラテスト

#include <QtTest>
#include <QSignalSpy>

#include "analysiscoordinator.h"
#include "analysisresulthandler.h"

class TestAnalysisCoordinator : public QObject
{
    Q_OBJECT

private:
    /// テスト用の sfenRecord を作成（平手→7六歩→3四歩...）
    QStringList makeSampleSfenRecord()
    {
        return {
            QStringLiteral("position startpos"),
            QStringLiteral("position startpos moves 7g7f"),
            QStringLiteral("position startpos moves 7g7f 3c3d"),
            QStringLiteral("position startpos moves 7g7f 3c3d 2g2f"),
        };
    }

private slots:
    // --- AnalysisCoordinator 基本 ---
    void initialState_isIdle();
    void startAnalyzeRange_emitsPositionPrepared();
    void startAnalyzeSingle_emitsPositionPrepared();
    void stop_returnsToIdle();

    // --- エンジン応答処理 ---
    void onEngineInfoLine_scorecp_emitsProgress();
    void onEngineInfoLine_scoremate_emitsProgress();
    void onEngineBestmove_single_emitsFinished();

    // --- USI コマンド送信 ---
    void sendGoCommand_emitsRequestSendUsiCommand();

    // --- AnalysisResultHandler ---
    void updatePending_storesData();
    void commitPendingResult_updatesLastCommitted();
    void reset_clearsAllPending();
    void extractUsiMoveFromKanji_basicMove();
    void extractUsiMoveFromKanji_emptyInput();
};
```

実装指針:
- `AnalysisCoordinator` は `Deps{&sfenRecord}` と `Options` を設定して使用
- シグナル検証は `QSignalSpy` を使用
- `requestSendUsiCommand` シグナルを spy して送信されたコマンド文字列を検証
- `AnalysisResultHandler` は `Refs` の各ポインタを nullptr にして、commitPendingResult のモデル更新以外のロジック（lastCommittedPly 等）をテスト
- `extractUsiMoveFromKanji` のテストは実装の漢字パース仕様を確認してから入力値を決定すること

### Step 2: CMakeLists.txt にテスト登録

`tests/CMakeLists.txt` に以下を追加:

```cmake
# ============================================================
# Unit: AnalysisCoordinator + AnalysisResultHandler テスト
# ============================================================
add_shogi_test(tst_analysis_coordinator
    tst_analysis_coordinator.cpp
    ${SRC}/common/logcategories.cpp
    ${SRC}/analysis/analysiscoordinator.cpp
    ${SRC}/analysis/analysisresulthandler.cpp
)
```

コンパイルエラーが出た場合、AnalysisResultHandler が依存する型（KifuAnalysisListModel 等）のスタブまたはソースを追加すること。必要に応じてスタブファイルを新規作成する。

### Step 3: ビルド・テスト実行
```bash
cmake -B build -S .
cmake --build build -j4
ctest --test-dir build --output-on-failure -R tst_analysis_coordinator
```

### Step 4: 全テスト実行
```bash
ctest --test-dir build --output-on-failure
```

## 受け入れ基準
- AnalysisCoordinator のエンジン応答処理テスト 5件以上
- AnalysisResultHandler のデータ蓄積・確定テスト 3件以上
- extractUsiMoveFromKanji のテスト 2件以上
- ビルド成功（warning なし）
- 既存テスト全件成功
