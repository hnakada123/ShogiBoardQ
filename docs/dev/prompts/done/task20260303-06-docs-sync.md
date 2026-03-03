# Task 20260303-06: ドキュメント/運用ルールの同期（P3）

## 概要

開発ガイドやドキュメントに旧テスト本数（31本）の記載が残っている。現行は 57 テスト（18 テスト実行ファイル）。実測値とドキュメントを同期し、テスト本数の更新元を一本化する。

## 背景

- 旧テスト本数「31」の記載が残っている文書:
  - `docs/dev/done/code-quality-improvement-plan-2026-02-27-current.md:13` — "テストは `31` 件あり"
  - `docs/dev/done/code-quality-improvement-plan-2026-02-26.md:18-19` — "全 31 件 pass"
  - `docs/dev/done/code-quality-improvement-plan-2026-02-26.md:127` — "31 テスト"
- `docs/dev/test-summary.md` は「18テスト実行ファイル」と記載しているが、テスト関数の総数（57）は記載していない
- `ctest --test-dir build -N` の出力: `Total Tests: 57`

## 実装手順

### Step 1: `docs/dev/test-summary.md` を最新化

1. `ctest --test-dir build -N` の出力でテスト名一覧を取得
2. `docs/dev/test-summary.md` の「テスト結果」セクションを現行の 57 テスト一覧に更新
3. テスト実行ファイル数（18）とテスト関数数（57）の両方を明記する

例:

```
18 テスト実行ファイル、57 テストケース
```

### Step 2: `done/` 配下の旧文書は修正しない

`docs/dev/done/` 配下の文書は過去の時点のスナップショットであり、歴史的記録として正しい。修正対象外とする。

### Step 3: 現行の改善計画を確認

`docs/dev/current-source-improvement-plan-2026-03-03.md:13` の記述:

```
- テストは現行 `build` で `57/57` pass。
```

これは正しい。変更不要。

### Step 4: テスト本数の自動転記（オプション）

`scripts/` に以下のスクリプトを追加して、テスト本数の手動管理を減らす:

```bash
#!/bin/bash
# scripts/update-test-summary.sh
# ctest -N の出力から test-summary.md のテスト数を更新する

BUILD_DIR="${1:-build}"
SUMMARY="docs/dev/test-summary.md"

total=$(ctest --test-dir "$BUILD_DIR" -N 2>/dev/null | grep "Total Tests:" | awk '{print $NF}')
executables=$(ctest --test-dir "$BUILD_DIR" -N 2>/dev/null | grep "Test #" | awk -F: '{print $2}' | awk '{print $1}' | sort -u | wc -l)

echo "Tests: $executables executables, $total test cases"
```

スクリプト追加はオプション（必須ではない）。

### Step 5: `tst_structural_kpi.cpp` の `kMaxFilesOver500` を実測値に更新

現在 `kMaxFilesOver500 = 31`（`tests/tst_structural_kpi.cpp:630`）だが、実測値は 27 程度。実測値に合わせて更新する:

```bash
cmake --build build
ctest --test-dir build --output-on-failure -R tst_structural_kpi -V
```

出力から `files_over_500` の実測値を確認し、`kMaxFilesOver500` を更新する。

## 確認手順

```bash
cmake --build build
ctest --test-dir build --output-on-failure
```

- 全テスト pass
- `docs/dev/test-summary.md` のテスト数が `ctest -N` の出力と一致
- テスト本数の表記差分がない（現行文書間）

## 制約

- `docs/dev/done/` 配下のファイルは変更しない（歴史的記録）
- `docs/dev/test-summary.md` を正とし、テスト数の情報源を一本化する
- 翻訳更新は不要
