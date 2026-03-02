# Task 03: macOS バンドル識別子 + バージョン番号統一（P0+P1 / §4.1+§4.2）

## 概要

macOS バンドル識別子がプレースホルダ `com.example.ShogiBoardQ` のまま。また `project(VERSION 0.1)` と `APP_VERSION "2026.02.25"` でバージョン番号が二重管理されている。

## 現状

```cmake
project(ShogiBoardQ VERSION 0.1 LANGUAGES CXX)
set(APP_VERSION "2026.02.25")

set_target_properties(ShogiBoardQ PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER com.example.ShogiBoardQ
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}                           # "0.1"
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}  # "0.1"
)
```

## 手順

### Step 1: バンドル識別子の修正

1. `CMakeLists.txt` の `MACOSX_BUNDLE_GUI_IDENTIFIER` を適切な逆DNS識別子に変更する
   - GitHub リポジトリ所有者に基づく識別子を設定（例: `io.github.<owner>.ShogiBoardQ`）
   - ユーザーに確認が必要な場合は、コメントで TODO を残す

### Step 2: バージョン番号の統一

1. `APP_VERSION` の CalVer を正とし、`project(VERSION)` と `MACOSX_BUNDLE_*_VERSION` を統一する
2. 方法A（推奨）: `MACOSX_BUNDLE_*_VERSION` を `APP_VERSION` から明示設定:
   ```cmake
   set_target_properties(ShogiBoardQ PROPERTIES
       MACOSX_BUNDLE_BUNDLE_VERSION ${APP_VERSION}
       MACOSX_BUNDLE_SHORT_VERSION_STRING ${APP_VERSION}
   )
   ```
3. `APP_VERSION` が使用されている他の箇所（`target_compile_definitions` 等）に影響がないことを確認する

### Step 3: ビルド確認

1. `cmake --build build` でビルドが通ることを確認

## 注意事項

- `project(VERSION)` は CMake の内部変数（`PROJECT_VERSION_MAJOR` 等）にも影響するため、他の箇所で使っていないか確認すること
- バンドル識別子の変更は既存ユーザーの設定ファイルパスに影響する可能性がある（macOS の `~/Library/Preferences` 等）
