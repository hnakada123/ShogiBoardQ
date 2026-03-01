# Task 07: shogiboard.cpp 分割（core層）

## 概要

`src/core/shogiboard.cpp`（822行）を「盤面状態」「SFEN変換」「駒台処理」に分割し、各ファイルを600行以下にする。

## 前提条件

- Task 06（fmvlegalcore分割）完了推奨（同ディレクトリの分割パターンを再利用）

## 現状

- `src/core/shogiboard.cpp`: 822行（800行超の2ファイルの一つ）
- `src/core/shogiboard.h`: 139行
- #include: 3（低結合）
- シグナル: `boardReset()`, `dataChanged(int, int)`

## 分析

責務は以下の4カテゴリに分かれる:

### カテゴリ1: 盤面状態管理（~150行）
- 初期化: `ShogiBoard()`, `initBoard()`, `initStand()`
- アクセス: `boardData()`, `getPieceStand()`, `currentPlayer()`, `getPieceCharacter()`
- 操作: `setDataInternal()`, `setData()`, `movePieceToSquare()`

### カテゴリ2: SFEN入力/出力（~385行）— 最大グループ
- 入力: `validateAndConvertSfenBoardStr()`(60行), `setPieceStandFromSfen()`(80行), `setPiecePlacementFromSfen()`(40行), `parseSfen()`(static), `setSfen()`
- 出力: `convertPieceToSfen()`, `convertBoardToSfen()`, `convertStandToSfen()`, `addSfenRecord()`

### カテゴリ3: 駒台操作（~60行）
- `convertPieceChar()`, `addPieceToStand()`, `decrementPieceOnStand()`, `isPieceAvailableOnStand()`, `convertPromotedPieceToOriginal()`, `incrementPieceOnStand()`

### カテゴリ4: 盤面編集・デバッグ（~135行）
- 編集: `updateBoardAndPieceStand()`, `setInitialPieceStandValues()`, `resetGameBoard()`, `flipSides()`, `promoteOrDemotePiece()`(85行)
- デバッグ: `printPlayerPieces()`, `printPieceStand()`, `printPieceCount()`

## 実施内容

### Step 1: SFEN処理の分離

カテゴリ2が最大（~385行）のため優先分離:

1. `src/core/shogiboard_sfen.cpp` を作成（同一クラスの実装分割方式）
2. SFEN入力/出力メソッドを移動
3. `ShogiBoard` クラス自体は分割しない（QObject でありシグナルを持つため）
4. `.cpp` ファイルの分割のみで `.h` は変更なし

### Step 2: 盤面編集の分離

1. `src/core/shogiboard_edit.cpp` を作成
2. カテゴリ4のメソッドを移動
3. `promoteOrDemotePiece()`（85行）が最大だが、これ以上の内部分解は不要

### Step 3: 本体のスリム化

1. `shogiboard.cpp` にはカテゴリ1（盤面状態）とカテゴリ3（駒台操作）を残す
2. 結果: ~210行

### Step 4: ビルドとテスト

1. CMakeLists.txt に新規ファイルを追加
2. `tst_structural_kpi.cpp` の `knownLargeFiles()` から `shogiboard.cpp` を削除
3. ビルド確認
4. 全テスト通過確認

## 完了条件

- `shogiboard.cpp` が 300行以下
- `shogiboard_sfen.cpp` が 400行以下
- `shogiboard_edit.cpp` が 200行以下
- 800行超ファイル: 1件減少
- ビルド成功
- 全テスト通過

## 注意事項

- `ShogiBoard` は QObject 派生でシグナルを持つため、クラス分割ではなく **実装ファイル分割**（同一クラスの .cpp を複数に分ける）を採用する
- `.h` は変更不要
- `parseSfen()` は static メソッドだが ShogiBoard のメンバとして残す
- core層は Qt GUI 非依存だが `QObject`（for シグナル/スロット）に依存
