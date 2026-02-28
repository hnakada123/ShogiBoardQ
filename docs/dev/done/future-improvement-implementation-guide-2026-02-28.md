# 今後改善分析の適用手順書（2026-02-28）

対象文書: `docs/dev/future-improvement-analysis-2026-02-28.md`  
本書の目的: 上記分析内容を、実際のソースコード変更に落とし込むための実行手順を定義する。

---

## 1. 前提と運用ルール

1. 1PR 1目的で進める（巨大分割・所有権整理・CI拡張を混ぜない）。
2. 1PRあたりの変更ファイル数は原則 15 以下。
3. すべてのPRで以下を必須実行する。

```bash
cmake -B build -S . -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
QT_QPA_PLATFORM=offscreen build/tests/tst_structural_kpi
QT_QPA_PLATFORM=offscreen build/tests/tst_layer_dependencies
```

4. 構造KPIの例外上限を変更する場合は、同PRで実測値と理由を記録する。

---

## 2. 実施の全体順序（推奨）

1. Phase 0: ベースライン固定（計測・課題登録）
2. Phase 1: 巨大ファイルの重点分割（A）
3. Phase 2: MainWindow周辺の依存密度低減（B）
4. Phase 3: 所有権モデル最終統一（C）
5. Phase 4: app層テスト補完（D）
6. Phase 5: CI品質ゲート拡張（E）
7. Phase 6: 棋譜/通信仕様境界の整理（F）

---

## 3. Phase 0（初週）: ベースライン固定

### 3.1 作業手順

1. 現在値を計測し、作業開始時点として保存する。

```bash
find src -type f \( -name '*.cpp' -o -name '*.h' \) | wc -l
find src -type f \( -name '*.cpp' -o -name '*.h' \) -print0 | xargs -0 wc -l | tail -n 1
find src -type f \( -name '*.cpp' -o -name '*.h' \) -print0 | xargs -0 wc -l | awk '$1>600 {print $1" "$2}' | sort -nr | head -n 40
QT_QPA_PLATFORM=offscreen build/tests/tst_structural_kpi
```

2. 下記テンプレートで `docs/dev/prompts/done/` または `docs/dev/` に計測ログを追加する。
3. 改善タスクを Issue 化し、テーマA-Fと紐づける。

### 3.2 完了条件

1. ベースライン記録がコミットされている。
2. 以降のPRが参照する Issue 番号が確定している。

---

## 4. Phase 1（1-4週）: 巨大ファイル分割（テーマA）

## 4.1 対象優先順位

1. `src/analysis/analysisflowcontroller.cpp`
2. `src/engine/usi.cpp`
3. `src/widgets/evaluationchartwidget.cpp`
4. `src/kifu/kifuexportcontroller.cpp`
5. `src/core/fmvlegalcore.cpp`
6. `src/engine/usiprotocolhandler.cpp`

## 4.2 1ファイル分割の標準手順

1. 対象ファイルの責務を3分類する。
- 例: 入力受理
- 例: 状態更新
- 例: 出力反映

2. 抽出先クラスを先にヘッダで定義する。
- `src/<module>/...service.h`
- `src/<module>/...service.cpp`

3. 既存クラスには `Deps` または明示 setter で依存注入する。
4. 動作を変えずに転送実装へ置換する。
5. 単体テストを更新または追加する。
- 既存の関連テストがあれば先に拡張
- 無ければ `tests/tst_<target>_*.cpp` を新規追加

6. `tests/tst_structural_kpi.cpp` の例外上限を実測へ合わせる。
- 対象ファイルが縮小した場合は上限値を必ず下げる。

## 4.3 完了条件

1. 対象ファイルの行数が分割前より 20%以上減る。
2. 既存テスト全通過。
3. 構造KPI例外値が最新化されている。

---

## 5. Phase 2（3-6週）: MainWindow依存密度低減（テーマB）

## 5.1 主対象ファイル

1. `src/app/mainwindow.h`
2. `src/app/mainwindow.cpp`
3. `src/app/mainwindowserviceregistry.h`
4. `src/app/mainwindowanalysisregistry.cpp`
5. `src/app/mainwindowboardregistry.cpp`
6. `src/app/mainwindowgameregistry.cpp`
7. `src/app/mainwindowkifuregistry.cpp`
8. `src/app/mainwindowuiregistry.cpp`
9. `src/app/mainwindowruntimerefs.h`

## 5.2 具体手順

1. `ensure*` 呼び出しグラフを作成する。

```bash
rg -n "^\\s*void ensure" src/app/mainwindowserviceregistry.h
rg -n "ensure[A-Za-z0-9_]+\\(" src/app/mainwindow*.cpp
```

2. 呼び出しの密な塊を子レジストリへ再分割する。
- 例: `MainWindowGameRegistry`, `MainWindowUiRegistry` など

3. `MainWindowRuntimeRefs` を用途別に分割する。
- 例: `GameRuntimeRefs`, `KifuRuntimeRefs`, `UiRuntimeRefs`

4. `MainWindow` から直接保持している非UIメンバを段階移管する。
- 優先: 状態参照・サービス参照
- 後回し: Qt Widgetそのもの

5. `tests/tst_structural_kpi.cpp` に以下を追加または調整する。
- `ensure*` 件数上限の監視
- `MainWindow` 生ポインタ件数の監視（必要なら新テスト追加）

## 5.3 完了条件

1. `MainWindowServiceRegistry` の `ensure*` 件数が目標に向かって減少。
2. `mainwindow.h` の生ポインタ件数が減少。
3. 起動・終了・新規対局開始フローに回帰なし。

---

## 6. Phase 3（4-7週）: 所有権モデル最終統一（テーマC）

## 6.1 対象抽出

```bash
rg -n "\\bnew\\b|\\bdelete\\b" src
```

## 6.2 具体手順

1. `delete` 使用箇所を一覧化し、分類する。
- `QObject` 親子で代替可能
- `std::unique_ptr` で代替可能
- 代替困難（例外扱い）

2. 代替可能箇所を順次置換する。
3. 代替困難箇所には理由コメントを追加する。
- 破棄順依存
- 外部API制約
- ライフサイクル境界の都合

4. 変更後に `tst_structural_kpi` の `deleteCount` を確認する。

## 6.3 完了条件

1. `delete` 件数が段階的に減少。
2. 例外 `delete` 箇所に理由コメントがある。
3. クラッシュ・ダングリングがない（通常テスト + 必要時手動確認）。

---

## 7. Phase 4（5-9週）: app層テスト補完（テーマD）

## 7.1 追加対象の最小セット

1. 起動フロー（Startup pipeline）
2. 終了フロー（Shutdown pipeline）
3. 対局開始フロー（GameSessionOrchestrator）
4. 棋譜ロードフロー（KifuFileController / KifuLoadCoordinator）
5. 分岐ナビ更新フロー（BranchNavigationWiring）
6. UI状態遷移（UiStatePolicyManager）

## 7.2 具体手順

1. `tests/test_stubs_*.cpp` を再利用して依存を最小化する。
2. 1フロー1テストファイルで追加する。
- 命名例: `tests/tst_app_startup_pipeline.cpp`

3. 「シグナル入力 -> 期待スロット/状態変化」の契約を検証する。
4. CI実行時間を監視し、重すぎるケースは分離する。

## 7.3 完了条件

1. app層テストが最低6本追加されている。
2. 重要フローの回帰を自動検出できる。

---

## 8. Phase 5（6-10週）: CI品質ゲート拡張（テーマE）

## 8.1 主対象ファイル

1. `.github/workflows/ci.yml`

## 8.2 具体手順

1. `sanitize` ジョブを追加する。
- `-DCMAKE_BUILD_TYPE=RelWithDebInfo`
- `-fsanitize=address,undefined`（環境に合わせて調整）

2. 実行負荷を抑えるため、以下のいずれかで運用する。
- `workflow_dispatch` 限定
- ラベル付きPR限定
- nightly限定

3. `tst_structural_kpi` の結果をアーティファクト化する。
4. GitHub Step Summary に前回比較を出す（可能なら）。

## 8.3 完了条件

1. SanitizerジョブがCIで実行できる。
2. 失敗時に原因追跡できるログが残る。

---

## 9. Phase 6（8-12週）: 棋譜/通信仕様境界整理（テーマF）

## 9.1 主対象ファイル群

1. `src/kifu/formats/*.cpp`
2. `src/engine/usiprotocolhandler.cpp`
3. `src/network/csaclient.cpp`
4. `src/network/csagamecoordinator.cpp`
5. `tests/tst_*converter*.cpp`
6. `tests/tst_usiprotocolhandler.cpp`
7. `tests/tst_csaprotocol.cpp`

## 9.2 具体手順

1. 変換系の責務を `Parse -> Normalize -> Apply` に明示分離する。
2. 異常系テーブルテストを追加する。
- 欠損
- 不正手
- 文字コード/フォーマット境界
- 上限値/下限値

3. USI/CSA のエラーコード・ログ出力方針を統一する。
4. 仕様変更時は `docs/dev` に仕様差分を記録する。

## 9.3 完了条件

1. 変換処理の重複が減少している。
2. 異常系ケースが追加され、回帰耐性が上がっている。

---

## 10. PRテンプレート（本施策専用）

```md
## 目的
- （テーマA-Fのどれか）

## 変更内容
1.
2.

## 非目的
- 動作仕様の変更なし

## KPI変化
- 600行超ファイル数: X -> Y
- 対象ファイル行数: A -> B
- delete件数: C -> D
- テスト数: E -> F

## 検証
- [ ] cmake --build build
- [ ] ctest --test-dir build --output-on-failure
- [ ] QT_QPA_PLATFORM=offscreen build/tests/tst_structural_kpi
- [ ] QT_QPA_PLATFORM=offscreen build/tests/tst_layer_dependencies
```

---

## 11. 直近2スプリントの推奨着手順

1. `analysisflowcontroller.cpp` 分割（Phase 1）
2. `MainWindowServiceRegistry` の `ensure*` グラフ化と再分割設計（Phase 2）
3. app起動/終了テストを1本ずつ追加（Phase 4）
4. CI sanitize ジョブを `workflow_dispatch` で導入（Phase 5）

---

## 12. 補足

本手順書は、実装順序とレビュー観点を固定化するための運用文書である。  
数値目標（KPI）は固定値ではなく、`tst_structural_kpi` の実測に基づいて段階的に更新する。
