# Task 05: `-Werror` オプション追加（P1 / §4.3）

## 概要

コンパイラ警告が有効だが `-Werror` がないため、警告がビルドを失敗させない。CI で有効化するためのオプションを追加する。

## 現状

`CMakeLists.txt` で `-Wall -Wextra -Wpedantic -Wshadow -Wconversion` 等が設定されているが `-Werror` がない。

## 手順

### Step 1: CMake オプションの追加

1. `CMakeLists.txt` に `ENABLE_WERROR` オプションを追加:
   ```cmake
   option(ENABLE_WERROR "Treat compiler warnings as errors" OFF)
   ```
2. 既存の警告フラグ設定セクションの後に条件分岐を追加:
   ```cmake
   if(ENABLE_WERROR)
       target_compile_options(ShogiBoardQ PRIVATE -Werror)
   endif()
   ```

### Step 2: CI の設定更新

1. `.github/workflows/ci.yml` の `build-and-test` ジョブに `-DENABLE_WERROR=ON` を追加
2. 他のジョブ（`build-debug` 等）にも必要に応じて追加

### Step 3: ローカルビルドで警告がないことを確認

1. `cmake -B build -S . -DENABLE_WERROR=ON` でビルドし、既存の警告でエラーにならないことを確認
2. 警告が出る場合は、そのファイルの警告を修正する

## 注意事項

- ローカル開発ではデフォルト OFF のため、開発者の作業フローには影響しない
- 既存の警告を先に修正しないと CI が通らなくなるため、Step 3 の確認が重要
- MSVC の場合は `/WX` だが、現時点では Linux CI のみなので `-Werror` で十分
