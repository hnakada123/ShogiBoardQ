# Task 16: CMake 小改善まとめ（P3 / §4.4+§4.5+§4.6）

## 概要

3つの小規模な CMake 改善をまとめて実施する。

## 改善項目

### 16-A: GCC 固有警告の有効化（§4.4）

CMakeLists.txt にコメントアウトされている GCC 固有警告:
- `-Wduplicated-cond`
- `-Wduplicated-branches`
- `-Wlogical-op`
- `-Wuseless-cast`
- `-Wold-style-cast`

### 16-B: `threadtypes.h` を CMakeLists.txt に追記（§4.5）

`src/common/threadtypes.h` が `SRC_COMMON` に含まれていない。

### 16-C: `cmake_minimum_required` のポリシー上限指定（§4.6）

```cmake
# Before
cmake_minimum_required(VERSION 3.16)
# After
cmake_minimum_required(VERSION 3.16...3.28)
```

## 手順

### Step 1: GCC 固有警告の有効化

1. `CMakeLists.txt` の該当セクション（94-131行付近）を確認する
2. コメントアウトされた警告を `CMAKE_CXX_COMPILER_ID STREQUAL "GNU"` の条件分岐内で有効化:
   ```cmake
   if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
       target_compile_options(ShogiBoardQ PRIVATE
           -Wduplicated-cond
           -Wduplicated-branches
           -Wlogical-op
           -Wuseless-cast
           -Wold-style-cast
       )
   endif()
   ```
3. `-Wold-style-cast` は Qt のマクロ（`Q_OBJECT` 等）で警告が出る可能性がある。ビルドして確認する

### Step 2: threadtypes.h の追記

1. `CMakeLists.txt` の `SRC_COMMON` セクションを見つける
2. `src/common/threadtypes.h` を追加する

### Step 3: cmake_minimum_required 上限指定

1. `CMakeLists.txt` 冒頭の `cmake_minimum_required` を修正:
   ```cmake
   cmake_minimum_required(VERSION 3.16...3.28)
   ```

### Step 4: ビルド確認

1. `cmake --build build` でビルドが通ることを確認（GCC の場合は警告が出ないことも確認）
2. テストを実行して全パスを確認

## 注意事項

- `-Wold-style-cast` は Qt の `Q_OBJECT` マクロに起因する警告が出る可能性が高い。出る場合はスキップする
- `cmake_minimum_required` の上限 `3.28` は執筆時点の安定版。環境に合わせて調整する
- CI は Ubuntu でビルドしており GCC がデフォルトコンパイラであるため、GCC 警告は CI でも検証される
