# Task 11: kifuexportcontroller.cpp 分割（kifu層）

## 概要

`src/kifu/kifuexportcontroller.cpp`（654行）からクリップボードコピー群を分離し、600行以下にする。

## 前提条件

- なし（kifu層は独立性が高い）

## 現状

- `src/kifu/kifuexportcontroller.cpp`: 654行
- `src/kifu/kifuexportcontroller.h`: 227行
- #include: 17
- シグナル: `statusMessage(const QString&, int)`
- 9つの `copyXxxToClipboard()` メソッドが全体の約半分を占める

## 分析

メソッドのカテゴリ:

### カテゴリ1: セットアップ・ユーティリティ（~80行）
- コンストラクタ/デストラクタ、`setDependencies()`, `setPrepareCallback()`, `updateState()`
- `resolveUsiMoves()`, `buildExportContext()`, `isCurrentlyPlaying()`, `currentPly()`

### カテゴリ2: ファイル保存（~160行）
- `saveToFile()`（~63行）— 全フォーマット生成 + ダイアログ
- `overwriteFile()`（~50行）— 上書き保存
- `autoSaveToDir()` — 自動保存

### カテゴリ3: クリップボードコピー（~310行）— 最大グループ
- `copyKifToClipboard()`, `copyKi2ToClipboard()`, `copyCsaToClipboard()`, `copyUsiToClipboard()`, `copyUsiCurrentToClipboard()`, `copyJkfToClipboard()`, `copyUsenToClipboard()`, `copySfenToClipboard()`, `copyBodToClipboard()`
- 各メソッドは ~30行で、パターンが類似（フォーマット生成 → クリップボードセット → ステータスバー通知）

### カテゴリ4: ヘルパー（~50行）
- `getCurrentPositionData()`, `generateBodText()`
- `gameMovesToUsiMoves()`(static), `sfenRecordToUsiMoves()`

## 実施内容

### Step 1: クリップボードコピーの分離

1. `src/kifu/kifuexportclipboard.h/.cpp` を作成
2. カテゴリ3の9メソッドを移動
3. 新クラス `KifuExportClipboard` は `KifuExportController` からの委譲先として機能
4. 依存は `KifuExportController::Deps` のサブセットを持つ
5. `statusMessage` シグナルの転送を設定

### Step 2: 本体のスリム化

1. `KifuExportController` にはカテゴリ1, 2, 4を残す
2. クリップボード系は `m_clipboard->copyXxxToClipboard()` で委譲
3. 結果: ~290行

### Step 3: ビルドとテスト

1. CMakeLists.txt に新規ファイルを追加
2. `tst_structural_kpi.cpp` の `knownLargeFiles()` から `kifuexportcontroller.cpp` を削除
3. ビルド確認
4. 全テスト通過確認

## 完了条件

- `kifuexportcontroller.cpp` が 350行以下
- `kifuexportclipboard.cpp` が 400行以下
- ビルド成功
- 全テスト通過

## 注意事項

- 9つの `copyXxxToClipboard()` はパターンが類似しているため、共通ヘルパー化も検討可能。ただし各フォーマットの生成ロジックが異なるため、過度な抽象化は避ける
- `statusMessage` シグナルの転送: `KifuExportClipboard` をQObject派生にしてシグナルを持たせるか、callback 方式にするか選択する
- `connect()` にラムダを使わないこと
