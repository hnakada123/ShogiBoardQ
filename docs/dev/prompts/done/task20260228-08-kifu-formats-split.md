# Task 08: kifu/formats 大型ファイル分割

## 概要

`src/kifu/formats/` 配下の大型ファイルを「parse → normalize → apply」の3段構成に整理し、600行以下に削減する。

## 前提条件

- なし（他タスクと並行可能）

## 現状

対象ファイル:
- `src/kifu/formats/csaexporter.cpp`: 758行（ISSUE-055）
- `src/kifu/formats/jkfexporter.cpp`: 735行（ISSUE-055）
- `src/kifu/formats/parsecommon.cpp`: 641行（ISSUE-055）
- `src/kifu/kifuapplyservice.cpp`: 638行（ISSUE-055）

関連（分割済み）:
- `src/kifu/kifuexportcontroller.cpp`: 654行（933→654、進行中）

## 実施内容

### Step 1: csaexporter.cpp の分割

1. ファイルを読み込み、責務を分析する
2. エクスポートロジックを以下に分離:
   - **CSAフォーマッタ** — CSA形式の文字列生成ロジック
   - **CSAエクスポータ** — ファイル出力・ストリーム管理
3. `src/kifu/formats/csaformatter.h/.cpp` を新規作成
4. 元ファイルを 600行以下に削減

### Step 2: jkfexporter.cpp の分割

1. 同様にファイルを読み込み、責務を分析
2. JKF/JSON 生成ロジックを分離:
   - **JKFフォーマッタ** — JKF JSON構造の生成
   - **JKFエクスポータ** — 出力管理
3. `src/kifu/formats/jkfformatter.h/.cpp` を新規作成

### Step 3: parsecommon.cpp の分割

1. 共通パース関数群の責務を分析
2. 機能カテゴリ（座標変換、駒名解析、手番解析等）で分離
3. 適切な粒度で新ファイルに分割

### Step 4: kifuapplyservice.cpp の分割

1. 棋譜適用サービスの責務を分析
2. 適用ロジックと検証ロジックを分離

### Step 5: KPI更新

1. `tst_structural_kpi.cpp` の `knownLargeFiles()` で上限値を更新
2. 600行以下になったものは例外リストから削除
3. CMakeLists.txt に新規ファイルを追加

## 完了条件

- 4ファイル全てが 650行以下（目標: 600行以下）
- 新規ファイルがそれぞれ 600行以下
- ビルド成功
- 全テスト通過
- 棋譜の読み込み・保存・エクスポート機能が維持されていること

## 注意事項

- パーサー/エクスポータは変換の正確性が重要。分割時にロジックを変更しないこと
- `parsecommon.cpp` の関数は複数のパーサーから参照されるため、インターフェースを変えない
- 新規クラスは非QObject でよい（ユーティリティ的な責務の場合）
- 翻訳文字列（`tr()`）を含む場合は翻訳更新が必要
