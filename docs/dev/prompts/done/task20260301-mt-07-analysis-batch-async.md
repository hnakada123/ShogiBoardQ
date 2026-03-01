# Task 20260301-mt-07: 一括解析の非同期化（P1）

## 背景

棋譜の一括解析は「各局面でエンジンに `go` を送り、`bestmove` を待つ」を繰り返す。100手の棋譜を30秒/手で解析すると約50分間UIが凍結する。Task 06 で導入したエンジンワーカースレッドを活用し、解析ループを非同期化する。

### 対象ファイル

| ファイル | 行 | メソッド | 処理内容 |
|---------|-----|---------|---------|
| `src/analysis/analysiscoordinator.cpp` | 44-70 | `startAnalyzeRange()` | 解析範囲のループ実行 |
| `src/engine/usi_communication.cpp` | 66-113 | `executeAnalysisCommunication()` | `waitForBestMove()` / `keepWaitingForBestMove()` |
| `src/analysis/tsumesearchflowcontroller.cpp` | 21-52 | `runWithDialog()` | 詰み探索の同期実行 |

## 作業内容

### Step 1: 現状の解析フローを把握

以下のファイルを読み、一括解析の全体フローを理解する:

- `src/analysis/analysiscoordinator.h/.cpp`
- `src/analysis/analysisflowcontroller.h/.cpp`（既にファイル分割済み）
- `src/engine/usi_communication.cpp`
- `src/analysis/tsumesearchflowcontroller.h/.cpp`

確認ポイント:
- 解析ループの開始/終了/キャンセルの制御フロー
- 各局面の解析結果（`AnalysisResult` 等）の型と内容
- 進捗通知の現在の仕組み（シグナル/プログレスダイアログ）
- 解析結果がどこに保存されるか（モデル/リスト）

### Step 2: 非同期解析ループの設計

```
メインスレッド                         エンジンワーカースレッド
     |                                        |
     |--- startRangeAnalysis(range) -------->|  (requestId含む)
     |                                        |
     |                                        |-- for each ply in range:
     |<-- analysisProgress(ply, total) ------|      send "position" + "go"
     |<-- positionAnalyzed(ply, result) -----|      waitForBestMove()
     |                                        |
     |--- cancelAnalysis() ----------------->|  (キャンセル要求)
     |                                        |-- send "stop"
     |<-- analysisFinished(completed) -------|
```

### Step 3: AnalysisRequest / AnalysisPositionResult 構造体の定義

解析リクエストと結果の値型を定義する:

```cpp
// src/analysis/analysisrequest.h（既存の型があれば拡張）

struct RangeAnalysisRequest {
    quint64 requestId = 0;
    QList<QString> positions;     // 各局面のSFEN position コマンド
    QString goCommand;            // "go depth ..." or "go nodes ..."
    int startPly = 0;
    int endPly = 0;
};

struct AnalysisPositionResult {
    int ply = 0;
    QString bestMove;
    QString ponderMove;
    int scoreValue = 0;           // 評価値
    QString scoreMate;            // 詰み手数（あれば）
    QList<QString> pvMoves;       // 読み筋
    int depth = 0;
    int multiPvIndex = 0;
};
```

### Step 4: AnalysisCoordinator の非同期化

`AnalysisCoordinator` を改修し、同期ループを非同期シグナル駆動に変更する:

1. **`startAnalyzeRangeAsync()`** メソッドを追加:
   - 解析対象の局面リストを準備（値コピー）
   - エンジンワーカーに最初の局面の `sendGoAsync()` を送信
   - 現在の解析位置（ply index）を保持

2. **`onPositionAnalyzed()`** スロットを追加:
   - エンジンワーカーの `asyncBestMoveReceived` を受けて呼ばれる
   - 結果をメインスレッドで解析結果モデルに格納
   - 進捗を `analysisProgress(current, total)` で通知
   - 次の局面があれば `sendGoAsync()` を再度呼び出し
   - 全局面完了なら `analysisFinished()` をemit

3. **`cancelAnalysis()`** の改修:
   - `std::atomic_bool` キャンセルフラグを設定
   - エンジンワーカーに `sendStop()` を送信
   - 現在進行中の結果は破棄

### Step 5: 進捗表示とキャンセルUI

- `QProgressDialog` でキャンセルボタン付きの進捗表示
- `analysisProgress(current, total)` で `QProgressDialog::setValue()` を更新
- キャンセルボタン押下で `cancelAnalysis()` を呼び出し
- 既存のプログレスダイアログ実装を確認し、非同期版に適合させる

### Step 6: 詰み探索（TsumeSearch）の非同期化

`src/analysis/tsumesearchflowcontroller.cpp` の `runWithDialog()` を改修:

1. 詰み探索もエンジンワーカーの `sendGoAsync()` を使用
2. 結果を `asyncBestMoveReceived` で受信
3. ダイアログのプログレス更新はメインスレッドで

### Step 7: 検討モード（ConsiderationMode）との整合

検討モードは既に `QTimer::singleShot` ベースで動作している場合がある。
エンジンワーカースレッド経由に統一するか、既存パターンを維持するか確認する。

- `src/analysis/considerationflowcontroller.h/.cpp` を確認
- 検討モードではリアルタイムの `info` 行表示が重要なので、`infoLineReceived` の転送がスムーズであること

### Step 8: ビルド・テスト確認

```bash
cmake --build build
cd build && ctest --output-on-failure
```

**手動テスト項目**:
1. 棋譜の一括解析（範囲指定）が完了すること
2. 解析中にUI操作が可能なこと（ウィンドウ移動、タブ切替）
3. 解析中のキャンセルが機能すること
4. 進捗表示が正しく更新されること
5. 解析結果が正しくグラフ/テーブルに反映されること
6. 詰み探索が動作すること
7. 検討モードが動作すること

## 注意事項

- 解析結果のモデル更新（`QAbstractItemModel` 系）はメインスレッドで行うこと
- 解析中に棋譜のナビゲーション（手を進める/戻す）が発生した場合の動作を考慮する
- `multiPV` 設定がある場合、1局面で複数の `info` + 1つの `bestmove` が返る

## 完了条件

- [ ] 一括解析が非同期で動作する（UIが凍結しない）
- [ ] キャンセル機能が動作する
- [ ] 進捗表示が正しく更新される
- [ ] 解析結果がモデルに正しく格納される
- [ ] 詰み探索が非同期で動作する
- [ ] 検討モードとの整合性が取れている
- [ ] 全テスト pass
