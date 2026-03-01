# Task 12: analysisflowcontroller.cpp 分割（analysis層）

## 概要

`src/analysis/analysisflowcontroller.cpp`（676行）から大型メソッドを分離し、600行以下にする。

## 前提条件

- なし（analysis層は独立性が高い）

## 現状

- `src/analysis/analysisflowcontroller.cpp`: 676行
- `src/analysis/analysisflowcontroller.h`: 156行
- #include: 14
- シグナル: `analysisStopped`, `analysisProgressReported`, `analysisResultRowSelected`
- 2つの巨大メソッド: `start()`（~197行）と `onPositionPrepared()`（~173行）

## 分析

メソッドのカテゴリ:

### カテゴリ1: ライフサイクル（~35行）
- コンストラクタ、デストラクタ

### カテゴリ2: 開始フロー（~230行）
- `start()`（44〜240行、~197行）— バリデーション → キャッシュ → AnalysisCoordinator生成 → シグナル接続 → エンジン起動 → 解析開始
- `applyDialogOptions()`（270〜304行）— ダイアログ設定の変換

### カテゴリ3: 手ごとの盤面同期（~175行）
- `onPositionPrepared()`（356〜528行、~173行）— 最も複雑: SFEN→盤面データ変換、USI指し手抽出、漢字座標パース、進捗通知、goコマンド送信

### カテゴリ4: 結果処理スロット（~50行）
- `onUsiCommLogChanged()`, `onBestMoveReceived()`, `onInfoLineReceived()`, `onThinkingInfoUpdated()`, `onAnalysisProgress()`

### カテゴリ5: 完了・ダイアログ（~115行）
- `onAnalysisFinished()` — 解析完了処理
- `runWithDialog()`（570〜655行、~85行）— ダイアログ表示 + 内部Usi生成 + start()呼び出し
- `onResultRowDoubleClicked()`, `onEngineError()`

## 実施内容

### Step 1: 盤面同期処理の分離

`onPositionPrepared()` が173行と最大:

1. `src/analysis/analysisflowcontroller_position.cpp` を作成（実装ファイル分割）
2. `onPositionPrepared()` を移動
3. 必要に応じて内部ヘルパーメソッドに分解

### Step 2: start() の整理（必要に応じて）

Step 1 で 600行以下になるか確認:
- 676 - 173 = ~503行 → 600行以下達成

達成できない場合は `start()` の一部（AnalysisCoordinator 生成・配線部分）も分離。

### Step 3: ビルドとテスト

1. CMakeLists.txt に新規ファイルを追加
2. `tst_structural_kpi.cpp` の `knownLargeFiles()` を更新
3. ビルド確認
4. 全テスト通過確認

## 完了条件

- `analysisflowcontroller.cpp` が 600行以下
- 分割後ファイルが 600行以下
- ビルド成功
- 全テスト通過

## 注意事項

- `AnalysisFlowController` は QObject 派生のため **実装ファイル分割** を採用
- `onPositionPrepared()` 内のSFEN→盤面変換ロジックは `ShogiBoard` のAPIを直接使用しているため、盤面操作の共通化は本タスクの範囲外
- `.h` は変更なし
