# KPI しきい値根拠

`tests/tst_structural_kpi.cpp` で検証される構造KPIの閾値一覧と設定根拠。

## 既存KPI

| KPI | 閾値 | 設定日 | 根拠 |
|---|---|---|---|
| files_over_600 (fileLineLimits) | 例外リスト方式 | — | 全ファイル600行以下達成。例外リストは空 |
| files_over_1000 (largeFileCount) | 0 | — | 全ファイル600行以下達成 |
| mw_friend_classes | 7 | — | MainWindow friend class 数の上限 |
| mw_ensure_methods | 0 | — | MainWindow ensure* 全廃（CompositionRoot移譲済み） |
| sr_ensure_methods | 14 | — | ServiceRegistry ensure* 上限 |
| mw_public_slots | 13 | — | MainWindow public slots 数の上限 |
| delete_count | 1 | — | flowlayout.cpp の1件のみ許容 |
| lambda_connect_count | 0 | — | lambda connect 全廃 |
| old_style_connections | 0 | — | SIGNAL/SLOT マクロ全廃 |
| SubRegistry ensure* | 各上限 | — | FoundationRegistry:15, Game:6, GameSession:6, GameWiring:4, Kifu:8 |

## 新規KPI（2026-03-01 導入）

| KPI | 閾値 | 設定日 | 根拠 |
|---|---|---|---|
| files_over_550 | 10 | 2026-03-02 | 実測値。ISSUE-013で3ファイル分割により削減 |
| files_over_500 | 31 | 2026-03-01 | 実測値。段階的削減目標 |
| mainwindow_include_count | 36 | 2026-03-02 | 実測値。依存削減の追跡用 |
| long_functions_over_100 | — | 2026-03-01 | 監視のみ（ゲートなし）。100行超関数をログ出力 |

## 閾値更新ルール

- 閾値はリファクタリング時の **ラチェット** として機能する
- ファイル行数・メソッド数が削減されたら、閾値も下げる（後退を防止）
- 新たな例外を追加する場合は、削減計画の Issue 番号を記載する

## 運用ルール

### 値の正とする場所
- `tests/tst_structural_kpi.cpp` の実装値を **Single Source of Truth** とする
- 本文書は参照用ドキュメントとして同期を維持する

### 閾値変更時の手順
1. `tests/tst_structural_kpi.cpp` の閾値を変更する
2. 本文書の対応する値を同時に更新する
3. PR 説明に変更理由を記載する

### 緩和（閾値を上げる）場合
- 一時的な緩和であっても、変更理由を PR 説明に必ず記載する
- 削減計画の Issue 番号がある場合はリンクする
