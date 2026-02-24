# Task 13: SettingsService キー一元管理

`SettingsService` の設定キー文字列を定数として一元管理するヘッダを作成してください。

## 背景

現在、設定キー（`"MainWindow/size"`, `"EngineList/count"` 等）が `settingsservice.cpp` 内のリテラル文字列として散在している。キー名の変更や一覧把握が困難。

## 作業内容

### Step 1: 現状把握

`src/services/settingsservice.cpp` を読み、全ての `s.value("...")` / `s.setValue("...")` で使われているキー文字列を抽出する。

### Step 2: キー定数ヘッダの作成

`src/services/settingskeys.h`（新規）に全キーを `inline constexpr` 定数として定義:

```cpp
#pragma once

namespace SettingsKeys {

// MainWindow
inline constexpr char kMainWindowSize[] = "MainWindow/size";
inline constexpr char kMainWindowPos[] = "MainWindow/pos";

// Engine
inline constexpr char kEngineListCount[] = "EngineList/count";
// ... 以下同様

// Settings version
inline constexpr char kSettingsVersion[] = "General/settingsVersion";

} // namespace SettingsKeys
```

命名規則: `k` + グループ名 + 項目名（CamelCase）

### Step 3: settingsservice.cpp の更新

リテラル文字列を定数参照に置換:
```cpp
// Before
return s.value("MainWindow/size", QSize(800, 600)).toSize();

// After
return s.value(SettingsKeys::kMainWindowSize, QSize(800, 600)).toSize();
```

### Step 4: 設定バージョンの導入

- `settingskeys.h` に現在のバージョン番号を定義: `inline constexpr int kCurrentSettingsVersion = 1;`
- `settingsservice.cpp` に `migrateSettingsIfNeeded()` のスケルトンを追加（現バージョンでは何もしない）
- アプリ起動時に呼び出すフックポイントをコメントで記載

## 制約

- CLAUDE.md のコードスタイルに準拠
- 既存の動作は一切変更しない（リテラル→定数の置換のみ）
- 全既存テストが通ること
