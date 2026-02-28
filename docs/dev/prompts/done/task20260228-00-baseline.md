# Task 00: ベースライン計測・記録

## 目的

改善施策の起点となるベースライン（現状計測値）を固定し、以降のPRで参照する基準値を記録する。

## 背景

`docs/dev/future-improvement-analysis-2026-02-28.md` で定義された改善テーマA-Fの進捗を追跡するには、実測値の起点が必要。

## 作業手順

### Step 1: 計測コマンドの実行

以下のコマンドを実行し、結果を記録する。

```bash
# ファイル数
find src -type f \( -name '*.cpp' -o -name '*.h' \) | wc -l

# 総行数
find src -type f \( -name '*.cpp' -o -name '*.h' \) -print0 | xargs -0 wc -l | tail -n 1

# 600行超ファイル一覧
find src -type f \( -name '*.cpp' -o -name '*.h' \) -print0 | xargs -0 wc -l | awk '$1>600 {print $1" "$2}' | sort -nr

# 1000行超ファイル数
find src -type f \( -name '*.cpp' -o -name '*.h' \) -print0 | xargs -0 wc -l | awk '$1>1000' | wc -l

# delete文の数
rg -c '\bdelete\b' src --type cpp | awk -F: '{sum+=$2} END {print sum}'

# lambda connect数
rg 'connect\(.*\[' src --type cpp | wc -l

# テスト数
ctest --test-dir build -N 2>/dev/null | tail -1

# 構造KPIテスト
QT_QPA_PLATFORM=offscreen build/tests/tst_structural_kpi
```

### Step 2: ベースラインファイル作成

`docs/dev/baseline-2026-02-28.md` を以下のフォーマットで作成する。

```markdown
# ベースライン計測（2026-02-28）

## 概要指標
| 指標 | 値 |
|---|---:|
| src/ 内 .cpp/.h ファイル数 | (実測値) |
| src/ 総行数 | (実測値) |
| 600行超ファイル数 | (実測値) |
| 800行超ファイル数 | (実測値) |
| 1000行超ファイル数 | (実測値) |
| テスト数 | (実測値) |
| delete 文 | (実測値) |
| lambda connect | (実測値) |

## 600行超ファイル一覧
(コマンド出力をそのまま記載)

## 1000行超ファイル（分割優先対象）
1. analysisflowcontroller.cpp - (行数)
2. usi.cpp - (行数)
3. evaluationchartwidget.cpp - (行数)
```

### Step 3: 検証

```bash
cmake -B build -S . -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

## 完了条件

- [x] ベースラインファイルが `docs/dev/baseline-2026-02-28.md` にコミットされている
- [x] 全テストが通過している
- [x] 計測値が分析文書 `future-improvement-analysis-2026-02-28.md` の値と整合している

## KPI（この作業自体の）

- 新規ファイル: 1件（baseline記録）
- コード変更: なし
