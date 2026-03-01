# Task 06: fmvlegalcore.cpp 分割（core層）

## 概要

`src/core/fmvlegalcore.cpp`（930行、プロジェクト最大）を「手生成」「合法性フィルタ」「打ち/成り判定」に分解し、各ファイルを600行以下にする。

## 前提条件

- なし（core層は他モジュールへの依存が少なく独立して実施可能）

## 現状

- `src/core/fmvlegalcore.cpp`: 930行（プロジェクト最大の単一ファイル）
- #include: 4（低結合）
- 依存: `fmvlegalcore.h`, `fmvattacks.h`, `<cctype>`, `<cstdlib>`
- 匿名名前空間のヘルパー関数11個（14〜311行）
- LegalCore パブリックメソッド6個（316〜433行）
- LegalCore プライベートメソッド8個（437〜928行）

## 分析

責務は以下の3カテゴリに分かれる:

### カテゴリ1: 合法手生成（MoveGeneration）
- 匿名名前空間:
  - `addBoardMoves()` — 方向別移動候補追加
  - `generatePieceMoves()` — 駒種別候補生成ディスパッチ（~85行）
  - `handTypeToPieceType()` — 持ち駒→駒種変換
- LegalCore:
  - `generateLegalMoves()` — 全合法手生成
  - `generateEvasionMoves()` — 王手回避手生成（~148行）
  - `generateNonEvasionMoves()` — 通常手生成
  - `generateDropMoves()` — 打ち手生成

### カテゴリ2: 合法性判定（LegalityCheck）
- 匿名名前空間:
  - `inBoard()` — 盤面範囲チェック
  - `charToPieceType()` — 文字→駒種変換
- LegalCore:
  - `isPseudoLegal()` — 疑似合法判定（~225行、最大メソッド）
  - `ownKingInCheck()` — 自王手チェック
  - `isLegalAfterDoUndo()` — do/undo法の合法判定
  - `checkMove()` — 単手合法判定
  - `countLegalMoves()` — 合法手数カウント
  - `countChecksToKing()` — 王手数カウント
  - `tryApplyLegalMove()` — 合法手適用
  - `undoAppliedMove()` — 手の取り消し
  - `hasAnyLegalMove()` — 合法手存在判定

### カテゴリ3: 打ち/成り判定（DropPromotionRules）
- 匿名名前空間:
  - `isPromotable()` — 成り可能判定
  - `isPromotionZone()` — 成り圏判定
  - `isMandatoryPromotion()` — 強制成り判定
  - `isDropDeadSquare()` — 行き所なし判定
  - `hasPawnOnFile()` — 二歩判定
  - `givesDirectPawnCheck()` — 打ち歩王手判定
- LegalCore:
  - `isPawnDropMate()` — 打ち歩詰め判定

## 実施内容

### Step 1: 分割構造の確認

1. `fmvlegalcore.cpp` を全文読み込み、上記3カテゴリの妥当性を確認
2. カテゴリ間のメソッド呼び出し依存を特定
3. 匿名名前空間のヘルパーがどのカテゴリから呼ばれるかマッピング

### Step 2: DropPromotionRules の抽出

1. `src/core/fmvdroppromotionrules.h/.cpp` を作成
2. カテゴリ3の匿名名前空間関数と `isPawnDropMate()` を移動
3. 他カテゴリから呼ばれるヘルパー（`isPromotionZone`, `isMandatoryPromotion` 等）は共有ヘッダに公開関数として配置するか、`fmvlegalcore_common.h` にまとめる

### Step 3: MoveGeneration の抽出

1. `src/core/fmvmovegeneration.h/.cpp` を作成
2. カテゴリ1のメソッドを移動
3. `LegalCore` からの呼び出しは `FmvMoveGenerator` インスタンスへの委譲に変更
4. または `LegalCore` の private メソッドとして別 .cpp ファイルに分離する方式（クラス分割なし）

### Step 4: LegalCore 本体のスリム化

1. 残った `fmvlegalcore.cpp` にはカテゴリ2のみ残す
2. カテゴリ2の `isPseudoLegal()` が225行と巨大なため、駒種別のサブ関数に分解を検討

### Step 5: ビルドとテスト

1. CMakeLists.txt に新規ファイルを追加
2. `tst_structural_kpi.cpp` の `knownLargeFiles()` を更新
3. ビルド確認
4. 全テスト通過確認

## 完了条件

- `fmvlegalcore.cpp` が 600行以下
- 新規ファイルがそれぞれ 600行以下
- 800行超ファイル: 1件減少（930→分割後各600以下）
- ビルド成功
- 全テスト通過

## 注意事項

- core層は Qt GUI に非依存。QObject を使わない純粋 C++ クラス
- 匿名名前空間の関数は分割先で再度匿名名前空間に入れる（外部公開が不要なら）
- 性能クリティカルなコードのため、関数呼び出しオーバーヘッドに注意（インライン化を検討）
- `fmvattacks.h` の `attackersTo` 関数は合法性判定で使用される共有依存
- `isPseudoLegal()` の225行は内部分解（private ヘルパー化）も検討するが、分割先ファイルでの行数次第
