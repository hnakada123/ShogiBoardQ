# Task 20260303-05: 大型ファイルの段階的分割（P2）

## 概要

500行超のファイルがまだ多い（現在550行超: 10ファイル）。機能スライス単位で分割し、KPI上限を段階的に引き下げる。1ファイルずつ実施し、ビルド・テスト確認を挟む。

## 背景

- `tests/tst_structural_kpi.cpp:598` で `kMaxFilesOver550 = 10`
- `tests/tst_structural_kpi.cpp:630` で `kMaxFilesOver500 = 31`（実測値は現在27程度、KPI値の更新も必要）
- 550行超の上位ファイル:
  1. `src/ui/coordinators/kifudisplaycoordinator.cpp`（592行）
  2. `src/kifu/formats/usentosfenconverter.cpp`（591行）
  3. `src/kifu/kifuapplyservice.cpp`（584行）
  4. `src/dialogs/josekiwindowui.cpp`（581行）
  5. `src/views/shogiview.cpp`（579行）
  6. `src/engine/usiprotocolhandler.cpp`（579行）
  7. `src/analysis/tsumeshogigenerator.cpp`（575行）
  8. `src/kifu/formats/kifexporter.cpp`（561行）
  9. `src/network/csagamecoordinator.cpp`（554行）
  10. `src/ui/coordinators/dialogcoordinator.cpp`（553行）

## 実装方針

- 1ファイルずつ分割する（並列分割しない）
- 分割単位は「イベントハンドラ」「変換処理」「状態同期」等の機能スライス
- 公開APIは維持し、内部実装を分離する
- 分割ごとに `ctest` で全テスト pass を確認

## 実装手順

### 分割対象1: `kifudisplaycoordinator.cpp`（592行）

1. ファイルを読み、責務の境界を特定する
2. イベントハンドラ群、表示更新群、初期化群など、機能スライスで分割先を決定する
3. 分割先のヘッダー/ソースを新規作成
4. `CMakeLists.txt` に新規ファイルを追加（`scripts/update-sources.sh` で生成可能）
5. ビルド・テスト確認

### 分割対象2: `usentosfenconverter.cpp`（591行）

1. 変換処理のフェーズ（パース、変換、出力）で分割可能か検討
2. ヘルパー関数群を別ファイルに抽出する形が有力
3. ビルド・テスト確認（`tst_usenconverter` が pass すること）

### 分割対象3以降

上位ファイルから順に同様の手順で分割する。目標は `kMaxFilesOver550` を 10 → 8 に削減。

### KPI 値の更新

各分割後に `tests/tst_structural_kpi.cpp` の該当する `constexpr` 値を更新する:

- `kMaxFilesOver550` を実測値に合わせて引き下げ
- `kMaxFilesOver500` を実測値に合わせて更新（31 → 実測値）

## 確認手順

各分割ごとに:

```bash
cmake --build build
ctest --test-dir build --output-on-failure
```

- 全テスト pass
- `tst_structural_kpi` の `filesOver550` / `filesOver500` が pass
- 分割対象に関連するユニットテストが pass

## 制約

- CLAUDE.md のコードスタイルに従うこと
- 新規ファイルは適切なディレクトリに配置する（既存のディレクトリ構成に従う）
- 新規ファイルを `CMakeLists.txt` に追加する（`scripts/update-sources.sh` の出力形式に従う）
- `connect()` でラムダ不使用
- 分割は公開APIを維持する（既存の include/connect を壊さない）
- 翻訳更新が必要な場合は `cmake --build build --target update_translations` を実行

## 注意

- このタスクは継続的に実施する。1回のセッションで全ファイルを分割する必要はない
- 優先度は上位ファイル（550行に近いほど優先）
- 分割先のクラス名・ファイル名は既存の命名規則に従う
