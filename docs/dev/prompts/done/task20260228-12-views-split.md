# Task 12: views 大型ファイル分割

## 概要

`src/views/` 配下の大型ファイルを分割し、描画ロジックの局所性を高める。

## 前提条件

- なし（他タスクと並行可能）

## 現状

対象ファイル:
- `src/views/shogiview_draw.cpp`: 808行（ISSUE-058 描画ロジック分離）
- `src/views/shogiview.cpp`: 802行（ISSUE-058 描画ロジック分離）
- `src/views/shogiview_labels.cpp`: 704行（ISSUE-058 描画ロジック分離）

## 実施内容

### Step 1: ShogiView の責務全体像を把握

`shogiview.h` を読み、ShogiView クラスの全体構造を把握する。
3つの .cpp ファイルが1つのクラスの実装を分割している構造を理解する。

### Step 2: shogiview_draw.cpp の分割

1. 描画メソッド群の責務を分析:
   - **盤面描画** — マス目、罫線、背景
   - **駒描画** — 駒画像の配置
   - **ハイライト描画** — 選択・移動先・最終手のハイライト
   - **矢印描画** — 解析結果の矢印表示
2. 独立性の高い責務を別ファイルに分離:
   - `src/views/shogiview_highlights.cpp` — ハイライト関連描画
   - `src/views/shogiview_arrows.cpp` — 矢印関連描画
   - または適切な粒度で分割

### Step 3: shogiview.cpp の分割

1. イベントハンドラ・初期化・座標変換等の責務を分析
2. 独立性の高い責務を別ファイルに分離:
   - `src/views/shogiview_events.cpp` — マウス/キーイベント処理
   - `src/views/shogiview_coords.cpp` — 座標変換ユーティリティ

### Step 4: shogiview_labels.cpp の分割

1. ラベル描画の責務を分析
2. 段・筋のラベル、対局者名表示、持ち駒表示等で分離

### Step 5: KPI更新

1. `tst_structural_kpi.cpp` の `knownLargeFiles()` で上限値を更新
2. CMakeLists.txt に新規ファイルを追加

## 完了条件

- 3ファイルが 650行以下（目標: 600行以下）
- 新規ファイルがそれぞれ 600行以下
- ビルド成功
- 全テスト通過
- 盤面の描画が正常に動作すること（目視確認推奨）

## 注意事項

- ShogiView は1クラス複数 .cpp の実装分割パターン。新規 .cpp も同クラスのメソッド実装として追加する
- 描画ロジックは `QPainter` を多用するため、ヘルパー関数の引数に `QPainter&` を渡すパターンを維持
- SVG の CJK テキスト配置に関する既知の注意点あり（MEMORY.md の SVG CJK text positioning 参照）
- 座標系の変換ロジックは変更しないよう注意（バグ混入リスクが高い）
