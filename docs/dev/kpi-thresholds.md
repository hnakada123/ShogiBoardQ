# KPI しきい値根拠

`tests/tst_structural_kpi.cpp` で検証される構造KPIの閾値一覧と設定根拠。

## 既存KPI

| KPI | 閾値 | 設定日 | 根拠 |
|---|---|---|---|
| files_over_1000 (fileLineLimits) | 例外リスト方式（既定1000行） | 2026-10-13 | 600行の上限は、責務と関係のない機械的なファイル分割（`_session.cpp` など）を生み、設計の見直しを妨げたので1000行へ上げた。例外リストは空 |
| files_over_1000 (largeFileCount) | 0 | — | 1000行を超えるファイルは作らない |
| mw_friend_classes | 3 | 2026-03-07 | MainWindow friend class 数の上限（ServiceRegistry とサブレジストリ） |
| mw_ensure_methods | 0 | — | MainWindow ensure* 全廃（レジストリへ移譲済み） |
| sr_ensure_methods | 25 | 2026-03-06 | ServiceRegistry ensure* 上限（実測 24） |
| mw_public_slots | 0 | 2026-10-13 | MainWindow public slots 数の上限（実測 0） |
| delete_count | 1 | — | flowlayout.cpp の1件のみ許容 |
| lambda_connect_count | 0 | — | lambda connect 全廃 |
| old_style_connections | 0 | — | SIGNAL/SLOT マクロ全廃 |
| SubRegistry ensure* | 各上限 | 2026-10-13 | FoundationRegistry:15, Kifu:8（どちらも上限ちょうど。増やすときは別の名前にせず、責務を見直す） |

## 新規KPI（2026-03-01 導入）

| KPI | 閾値 | 設定日 | 根拠 |
|---|---|---|---|
| mainwindow_include_count | 14 | 2026-10-13 | 実測 12。CompositionRoot・RuntimeRefs の削除で減った |
| long_functions_over_100 | 60 | 2026-10-13 | 実測 53 件に対するラチェット上限 |

files_over_550・files_over_500 の監視は 2026-10-13 に外した（files_over_1000 の根拠と同じ理由）。

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
