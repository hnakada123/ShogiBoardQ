# Task 18: ロギングカテゴリの一元管理

各 `.cpp` ファイルに散在する `Q_LOGGING_CATEGORY` 宣言を共通ヘッダに一元化してください。

## 背景

現在、各モジュールの `.cpp` ファイルが個別に `Q_LOGGING_CATEGORY(lcXxx, "shogiboardq.xxx")` を宣言している。カテゴリの一覧把握が困難で、カテゴリ名の重複や不整合が起きやすい。

## 作業内容

### Step 1: 既存カテゴリの一覧化

コードベース全体で `Q_LOGGING_CATEGORY` を検索し、全カテゴリを列挙:
- カテゴリ変数名（`lcCore`, `lcApp` 等）
- カテゴリ文字列（`"shogiboardq.core"` 等）
- 定義されているファイル

### Step 2: 共通ヘッダの作成

`src/common/logcategories.h`（新規）:
```cpp
#pragma once
#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(lcApp)
Q_DECLARE_LOGGING_CATEGORY(lcCore)
Q_DECLARE_LOGGING_CATEGORY(lcGame)
Q_DECLARE_LOGGING_CATEGORY(lcKifu)
Q_DECLARE_LOGGING_CATEGORY(lcEngine)
Q_DECLARE_LOGGING_CATEGORY(lcUi)
Q_DECLARE_LOGGING_CATEGORY(lcNetwork)
// ... 全カテゴリ
```

`src/common/logcategories.cpp`（新規）:
```cpp
#include "logcategories.h"

Q_LOGGING_CATEGORY(lcApp, "shogiboardq.app")
Q_LOGGING_CATEGORY(lcCore, "shogiboardq.core")
Q_LOGGING_CATEGORY(lcGame, "shogiboardq.game")
// ... 全カテゴリ
```

### Step 3: 各ファイルの更新

各 `.cpp` ファイルから:
1. `Q_LOGGING_CATEGORY(...)` 行を削除
2. `#include "logcategories.h"` を追加（既に `#include` がある場合はパスを調整）

### Step 4: テスト用スタブの整理

`tests/test_stubs.cpp` のカテゴリ定義を確認し、テストビルドで `logcategories.cpp` とリンクするか、テスト用に別途スタブを維持するか判断する。

## 制約

- CLAUDE.md のコードスタイルに準拠
- カテゴリ名・変数名は現状を維持（既存コードの変更を最小化）
- 全既存テストが通ること
- ビルドで ODR 違反（同一カテゴリの多重定義）が起きないこと
