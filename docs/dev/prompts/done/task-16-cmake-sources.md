# Task 16: CMake ソース明示列挙への移行

`CMakeLists.txt` の `file(GLOB_RECURSE)` を `target_sources()` による明示的なファイルリストに置き換えてください。

## 背景

現在は `file(GLOB_RECURSE SRC_* CONFIGURE_DEPENDS src/<module>/*.cpp)` で12モジュールのソースを自動収集している。ファイルの追加・削除がレビューで見えにくく、意図しないファイルの混入リスクがある。

## 作業内容

### Step 1: 現在のソースファイル一覧の取得

各モジュールの `.cpp` ファイルと `.h` ファイルを列挙するスクリプトを作成:

`scripts/update-sources.sh`（新規）:
```bash
#!/bin/bash
# CMakeLists.txt のソースリストを更新するヘルパー
# 使い方: ./scripts/update-sources.sh > /tmp/sources.txt
# 出力を CMakeLists.txt にコピーする

for dir in app core game kifu analysis engine network navigation board ui views widgets dialogs models services common; do
    echo "# src/${dir}/"
    find "src/${dir}" -name '*.cpp' -o -name '*.h' | sort
    echo ""
done
```

### Step 2: CMakeLists.txt の変更

`file(GLOB_RECURSE ...)` を `set()` + `target_sources()` に置換:

```cmake
# Before
file(GLOB_RECURSE SRC_CORE CONFIGURE_DEPENDS src/core/*.cpp)

# After
set(SRC_CORE
    src/core/shogiboard.cpp
    src/core/shogiboard.h
    src/core/shogimove.cpp
    src/core/shogimove.h
    # ...
)
target_sources(ShogiBoardQ PRIVATE ${SRC_CORE})
```

### Step 3: ヘッダファイルの扱い

- `.h` ファイルもリストに含める（IDE のプロジェクトツリーに表示されるため）
- AUTOMOC が `.h` を正しく処理できるよう、Q_OBJECT を含むヘッダは必ずリストに含める

### Step 4: ビルド確認

- `cmake -B build -S .` が成功すること
- `cmake --build build` が成功すること
- テストも含めてビルドが通ること

## 制約

- 既存のビルド結果と同一になること
- `AUTOUIC`, `AUTOMOC`, `AUTORCC` の設定は変更しない
- スクリプトは bash で記述（Linux 環境前提）
