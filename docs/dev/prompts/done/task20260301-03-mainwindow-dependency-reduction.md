# Task 20260301-03: MainWindow 依存ハブ縮退（P1）

## 背景

MainWindow の責務委譲は完了したが、依存保持量（include 49件、raw pointer 42件、friend 7件）が依然として大きく、
変更影響の予測が難しい状態が続いている。

### 現状の数値

| 指標 | 実測値 | 目標 |
|---|---:|---:|
| mainwindow.cpp include 数 | 49 | 40以下 |
| raw pointer 風メンバ | 42 | 35以下 |
| friend class 数 | 7 | 7（新規追加ゼロ） |
| public slots 数 | 13 | 13（新規追加ゼロ） |

## 作業内容

### Phase 1: include 削減の調査

#### Step 1-1: IWYU 分析

mainwindow.cpp の 49 件の include を分析し、以下に分類する:

```bash
# mainwindow.cpp の include 一覧を抽出
grep '#include' src/app/mainwindow.cpp
```

分類カテゴリ:
1. **必須**: mainwindow.h, ui_mainwindow.h, unique_ptr デストラクタ用
2. **前方宣言で代替可能**: ヘッダ内でポインタ/参照のみ使用
3. **Registry/Pipelineに移動可能**: CompositionRoot/ServiceRegistry が使うが MainWindow 自身は不要
4. **削除可能**: 不使用

#### Step 1-2: 前方宣言の活用

`mainwindow.h` で include している型のうち、ポインタ/参照のみ使用しているものを前方宣言に置換する。

```cpp
// Before (mainwindow.h)
#include "someclass.h"

// After (mainwindow.h)
class SomeClass;
```

対応する include は `mainwindow.cpp` に移動する（必要な場合のみ）。

#### Step 1-3: Registry への include 移動

MainWindow が直接使わず、Registry/Pipeline 経由でのみ使用される include を特定し、
対応する Registry/Pipeline の `.cpp` に移動する。

### Phase 2: raw pointer メンバの削減

#### Step 2-1: メンバ変数の用途分析

`mainwindow.h` の全メンバ変数（58件）を調査し、以下に分類する:

1. **MainWindow 直接使用**: public slots や MainWindow メソッドから参照される
2. **Registry 経由のみ使用**: ensure* やwiring 内でのみ参照される
3. **未使用**: リファクタリング残骸

#### Step 2-2: RuntimeRefs への集約

「Registry 経由のみ使用」のメンバは、`MainWindowRuntimeRefs` のサブ構造体に集約し、
MainWindow のメンバ変数から削除する。

**注意**: `friend class` によるアクセスパターンを維持するため、RuntimeRefs 経由で
Registry がアクセスできるようにする。

#### Step 2-3: 未使用メンバの削除

`grep` で全ソースを検索し、MainWindow からのみ参照されず、かつ connect() のターゲットでもない
メンバ変数を特定・削除する。

### Phase 3: 段階的検証

#### Step 3-1: include 削減の検証

```bash
# include 数の確認
grep -c '#include' src/app/mainwindow.cpp

# ビルド確認
cmake --build build
cd build && ctest --output-on-failure
```

#### Step 3-2: KPI 更新

Task 02 で導入した `mainWindowIncludeCount` のしきい値を更新する。

## 実施上の注意

- **一度に大量変更しない**: 5-10 include ずつ削減し、各段階でビルド確認する
- **IWYU pragma: keep は尊重**: unique_ptr デストラクタ用の include は削除しない
- **friend class は増やさない**: 新たな friend を追加する代わりに、API を公開する方向で検討
- **connect() ターゲットの確認**: public slots を削除する前に、全 connect() 呼び出しを検索する

## 完了条件

- [x] mainwindow.cpp の include 数が 40 以下に削減される
- [x] raw pointer 風メンバが 35 以下に削減される
- [x] friend class の新規追加がゼロ
- [x] 全テスト pass
- [x] `tst_structural_kpi` の全KPIが pass
