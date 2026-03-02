# Task 02: services 層の views 直接依存を除去（ISSUE-002 / P0）

## 概要

`appsettings.cpp` と `uinotificationservice.cpp` が `ShogiView` に直接依存している。プリミティブ値APIとインターフェース化で依存を切る。

## 現状

### appsettings.cpp
```cpp
#include "shogiview.h"  // src/views/shogiview.h

// saveWindowAndBoard(QWidget* mainWindow, ShogiView* view) で
// view->squareSize() を呼び出して QSettings に保存
```

### uinotificationservice.cpp
```cpp
#include "shogiview.h"  // src/views/shogiview.h

// m_deps.shogiView->setErrorOccurred(true) を呼び出し
```

`tst_layer_dependencies` のルール: `src/services` → `views/` は禁止。

## 手順

### Step 1: AppSettings の ShogiView 依存除去

1. `saveWindowAndBoard()` のシグネチャを変更:
   ```cpp
   // Before
   void saveWindowAndBoard(QWidget* mainWindow, ShogiView* view);
   // After
   void saveWindowAndBoard(QWidget* mainWindow, int squareSize);
   ```
2. 呼び出し元で `view->squareSize()` を取得し、`int` として渡す
3. `appsettings.h` の `ShogiView` 前方宣言も削除する
4. `appsettings.cpp` の `#include "shogiview.h"` を削除する

### Step 2: UiNotificationService の ShogiView 依存除去

1. `Deps` 構造体から `ShogiView*` を除去する
2. `setErrorOccurred(true)` の呼び出しをコールバック（`std::function<void(bool)>`）に置き換える:
   ```cpp
   struct Deps {
       // ShogiView* shogiView;  // 削除
       std::function<void(bool)> setErrorOccurred;  // 追加
   };
   ```
3. 依存注入元（`MainWindowCompositionRoot` 等）で `[view](bool v){ view->setErrorOccurred(v); }` を設定する
4. `uinotificationservice.cpp` の `#include "shogiview.h"` を削除する

### Step 3: ビルド・テスト

1. `cmake --build build` でビルド確認
2. レイヤ依存テストを実行:
   ```bash
   ctest --test-dir build -R tst_layer_dependencies --output-on-failure
   ```
3. 関連テスト:
   ```bash
   ctest --test-dir build -R "tst_layer_dependencies|tst_settings_roundtrip|tst_app_error_handling" --output-on-failure
   ```

## 完了条件

- `tst_layer_dependencies` で `src/services` 違反が 0 件
- 設定保存・エラー表示の既存挙動維持

## 注意事項

- `appsettings.h` の変更は広範に影響する可能性がある。シグネチャ変更の呼び出し元を全て更新すること
- UiNotificationService の Deps は CLAUDE.md の Deps パターンに従うこと
- ISSUE-001 と並行実施可能
