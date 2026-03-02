# Task 09: files_over_550 削減バッチ1（ISSUE-013 / P1）

## 概要

550行超ファイルが13個で KPI 上限に到達。責務分割で 13 → 10 に削減する。

## 現状（550行超ファイル一覧）

| ファイル | 行数 |
|---|---:|
| `src/kifu/formats/kiflexer.cpp` | 596 |
| `src/kifu/formats/csalexer.cpp` | 593 |
| `src/ui/coordinators/kifudisplaycoordinator.cpp` | 592 |
| `src/kifu/formats/usentosfenconverter.cpp` | 591 |
| `src/widgets/evaluationchartwidget.cpp` | 590 |
| `src/kifu/kifuapplyservice.cpp` | 584 |
| `src/dialogs/josekiwindowui.cpp` | 581 |
| `src/views/shogiview.cpp` | 579 |
| `src/engine/usiprotocolhandler.cpp` | 579 |
| `src/analysis/tsumeshogigenerator.cpp` | 575 |
| `src/kifu/formats/kifexporter.cpp` | 561 |
| `src/network/csagamecoordinator.cpp` | 554 |
| `src/ui/coordinators/dialogcoordinator.cpp` | 552 |

全て600行未満（上限内）。

## 手順

### Step 1: 削減対象の選定

最も行数が多く、分割しやすいファイルを3つ選ぶ。候補:

1. **`kiflexer.cpp` (596行)**: レキサーなのでステート分割が可能
2. **`csalexer.cpp` (593行)**: 同上
3. **`kifudisplaycoordinator.cpp` (592行)**: コーディネータなので責務分割が自然
4. **`evaluationchartwidget.cpp` (590行)**: 描画ロジックの分離が可能
5. **`csagamecoordinator.cpp` (554行)**: ネットワーク処理の分割

### Step 2: 各ファイルの分割計画

各対象ファイルについて:
1. ファイルを読み、責務の境界を特定する
2. 分割先のクラス/ファイル名を決める
3. 分割後の各ファイルが550行未満になることを確認する

**分割パターン例:**
- レキサー: トークン定義を別ファイルに分離
- コーディネータ: サブ責務をヘルパークラスに抽出
- ウィジェット: 描画ロジックを Renderer/Painter クラスに分離

### Step 3: 分割の実施

1. 対象ファイルごとに新規ファイルを作成
2. `CMakeLists.txt` に新規ファイルを追加
3. include を整理

### Step 4: ビルド・テスト

1. `cmake --build build` でビルド確認
2. テスト実行:
   ```bash
   ctest --test-dir build -R "tst_structural_kpi|tst_kifconverter|tst_csaprotocol|tst_ui_display_consistency" --output-on-failure
   ```
3. `files_over_550 <= 10` を確認

## 完了条件

- `files_over_550 <= 10`
- 既存機能の回帰なし

## 注意事項

- 600行の上限を超えないことが最優先。550行削減は「KPI余裕の確保」が目的
- 無理な分割はコードの可読性を下げるため、自然な責務境界で分割する
- ISSUE-012 完了後に着手する
- KPI の閾値（`kMaxFilesOver550`）を更新すること
