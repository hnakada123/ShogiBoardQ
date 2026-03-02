# Task 08: mainwindow include 削減バッチ（ISSUE-012 / P1）

## 概要

`mainwindow.cpp` の include 数が40で上限に到達している。前方宣言化と実装側 include 移動で 40 → 36 に削減する。

## 現状

- `mainwindow.cpp`: 40 include（KPI上限 `kMaxIncludes = 40`）
- `mainwindow.h`: 8 include

## 手順

### Step 1: include の分析

1. `mainwindow.cpp` の全40 include をリストアップする
2. 各 include について以下を分類:
   - **必須**: `mainwindow.h`, `ui_mainwindow.h`, Qt 基本ヘッダ
   - **前方宣言化可能**: ポインタ/参照のみで使用しているヘッダ
   - **移動可能**: 特定のメソッドでのみ使用しているヘッダ（そのメソッドが他ファイルに移動可能な場合）
   - **不要**: 使用されていないヘッダ

### Step 2: 削減の実施

**手法1: 前方宣言化**
1. `mainwindow.cpp` でポインタ/参照のみで使用しているヘッダを特定
2. ヘッダ側に前方宣言を追加し、`.cpp` の include を削除

**手法2: 実装側への include 移動**
1. 特定のメソッドでのみ使用されるヘッダを特定
2. そのメソッドが呼ぶサービスやコーディネータの `.cpp` に include を移動

**手法3: 未使用 include の削除**
1. 各 include のシンボルが `mainwindow.cpp` 内で実際に使用されているか確認
2. 未使用であれば削除

### Step 3: 目標達成の確認

1. `mainwindow.cpp` の include 数をカウント:
   ```bash
   grep -c "^#include" src/app/mainwindow.cpp
   ```
2. 36以下であることを確認

### Step 4: ビルド・テスト

1. `cmake --build build` でビルド確認
2. テスト実行:
   ```bash
   ctest --test-dir build -R tst_structural_kpi --output-on-failure
   ```

## 完了条件

- `tst_structural_kpi` の `mainwindow_include_count` が 36 以下
- ビルド通過

## 注意事項

- 前方宣言化は `.h` ファイルの include を減らす効果もある
- Qt の `Q_OBJECT` マクロを使うクラスは前方宣言だけでは足りない場合がある
- ISSUE-011 完了後に着手する
- KPI の閾値を更新することを忘れないこと
