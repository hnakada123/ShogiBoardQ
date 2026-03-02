# Task 01: engine 層の dialogs 直接依存を除去（ISSUE-001 / P0）

## 概要

`src/engine/enginesettingscoordinator.cpp` が `src/dialogs/engineregistrationdialog.h` に直接依存している。ダイアログ起動責務を `ui` 層へ移管する。

## 現状

```cpp
// src/engine/enginesettingscoordinator.cpp (全14行)
#include "enginesettingscoordinator.h"
#include "engineregistrationdialog.h"  // ← src/dialogs/ のファイル

namespace EngineSettingsCoordinator {
void openDialog(QWidget* parent) {
    EngineRegistrationDialog dlg(parent);
    dlg.exec();
}
}
```

`tst_layer_dependencies` のルール: `src/engine` → `dialogs/` は禁止。

## 手順

### Step 1: 移管先の決定

1. `EngineSettingsCoordinator::openDialog()` の呼び出し元を全て洗い出す
2. 移管先候補:
   - `src/ui/coordinators/` に既存のダイアログ起動コーディネータがあればそこに追加
   - `DialogCoordinator` に `openEngineRegistrationDialog()` を追加するのが自然
   - または `src/ui/wiring/` 配下に新規ファイルを作成

### Step 2: engine 層からダイアログ直接依存を除去

**方針A（推奨）: engine 層からダイアログ起動関数を完全に除去**
1. `EngineSettingsCoordinator` の `openDialog()` を UI 層（例: `DialogCoordinator`）に移動
2. 呼び出し元を新しい場所に向ける
3. `enginesettingscoordinator.cpp` と `.h` が不要になれば削除し、`CMakeLists.txt` からも除去

**方針B: コールバック注入**
1. `EngineSettingsCoordinator` にダイアログ起動コールバック（`std::function`）を持たせる
2. 具体的なダイアログ生成は `app/` 層で注入する
3. engine 層から `dialogs/` のincludeを除去

### Step 3: ビルド・テスト

1. `cmake --build build` でビルド確認
2. レイヤ依存テストを実行:
   ```bash
   ctest --test-dir build -R tst_layer_dependencies --output-on-failure
   ```
3. エンジン設定ダイアログが従来通り起動することを手動確認

## 完了条件

- `tst_layer_dependencies` で `src/engine` 違反が 0 件
- エンジン設定ダイアログの動作互換

## 注意事項

- `EngineSettingsCoordinator` はたった14行なので、関数ごと移動が最もシンプル
- 呼び出し元がメニューアクションの場合、`DialogLaunchWiring` や `DialogCoordinator` 経由が適切
