# Task 05: tst_structural_kpi 閾値更新と例外リスト縮小反映

## 概要

Task 01〜03 で分割されたファイルの実測値を反映し、`tst_structural_kpi.cpp` の例外リスト上限値を更新する。
分割済みファイルが 600行以下になったものは例外リストから削除する。

## 前提条件

- Task 01（ServiceRegistry再分割）完了後
- Task 02（engineregistrationdialog分割）完了後
- Task 03（josekimovedialog分割）完了後
- いずれか1つ以上が完了していれば着手可能

## 現状

- `tests/tst_structural_kpi.cpp` の `knownLargeFiles()` に 32件の例外登録
- うち TODO 管理対象は 24件
- 上限値は 2026-02-28 実測値と一致

## 実施内容

### Step 1: 実測値の収集

```bash
find src -type f \( -name '*.cpp' -o -name '*.h' \) -print0 | xargs -0 wc -l | sed '$d' | awk '$1>600' | sort -rn
```

### Step 2: 例外リストの更新

`tst_structural_kpi.cpp` の `knownLargeFiles()` を更新:

1. 分割により 600行以下になったファイルをリストから削除
2. 縮小したファイルの上限値を実測値に更新（10行マージン以内）
3. 新規作成ファイルが 600行を超える場合はリストに追加（Issue番号付き）

### Step 3: KPI閾値の更新

Task 01 が完了している場合:
- `serviceRegistryEnsureLimit()` の `maxEnsureMethods` を新しい実測値に更新

### Step 4: テスト実行

```bash
cmake --build build
ctest --test-dir build --output-on-failure
QT_QPA_PLATFORM=offscreen build/tests/tst_structural_kpi -o -,txt
```

## 完了条件

- `knownLargeFiles()` の全エントリが実測値と整合
- 例外リスト件数が削減されている（目標: 32 → 29以下）
- 全テスト通過
- `shrunkFiles` 警告が出力されないこと

## 注意事項

- 例外リストから削除するファイルは、安定して 600行以下であることを確認
- 新しい上限値は実測値 + 0〜5行の範囲に設定（過度なマージンを避ける）
- CI の `translation-check` ジョブの `THRESHOLD` も必要に応じて更新
