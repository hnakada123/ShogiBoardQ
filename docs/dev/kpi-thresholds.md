# KPI しきい値根拠

`tests/tst_structural_kpi.cpp` で検証される構造KPIの閾値一覧と設定根拠。

## 既存KPI

| KPI | 閾値 | 設定日 | 根拠 |
|---|---|---|---|
| files_over_1000 (fileLineLimits) | 例外リスト方式（既定1000行） | 2026-10-13 | 600行の上限は、責務と関係のない機械的なファイル分割（`_session.cpp` など）を生み、設計の見直しを妨げたので1000行へ上げた。例外リストは空 |
| files_over_1000 (largeFileCount) | 0 | — | 1000行を超えるファイルは作らない |
| mw_friend_classes | 0 | 2026-10-15 | 部品は MainWindowComposer が持ち、MainWindow の非公開メンバを外から触らない |
| mw_ensure_methods | 0 | — | MainWindow に ensure* を置かない |
| app_ensure_methods (appLayerHasNoLazyEnsure) | 0 | 2026-10-15 | 部品は合成ルートが起動時に依存順で作る。`src/app/*.h` に遅延生成の ensure* を置かない |
| testsDoNotRedefinePrivate | 0 | 2026-10-15 | テストは `#define private public` を使わず、`MainWindow::composer()` などの公開 API で検証する |
| mw_public_slots | 0 | 2026-10-13 | MainWindow public slots 数の上限（実測 0） |
| delete_count | 1 | — | flowlayout.cpp の1件のみ許容 |
| lambda_connect_count | 0 | — | lambda connect 全廃 |
| old_style_connections | 0 | — | SIGNAL/SLOT マクロ全廃 |

## 新規KPI（2026-03-01 導入）

| KPI | 閾値 | 設定日 | 根拠 |
|---|---|---|---|
| mainwindow_include_count | 14 | 2026-10-15 | 実測 5。部品の組み立てを MainWindowComposer に移した |
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
