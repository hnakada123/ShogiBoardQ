# Task 20260303-16: メニューウィンドウテスト追加

## 概要
MenuWindow のお気に入り管理ロジックとカテゴリ表示のウィジェットテストを追加する。現状は設定永続化テストとアクション有効/無効テストのみ。

## 優先度
Low

## 背景
- `MenuWindow` は QWidget サブクラスで、カテゴリータブ + お気に入りタブ + ボタングリッドのUI
- お気に入りの追加・削除・並替えロジックはテスト可能
- `setCategories`, `setFavorites`, `favorites()` のラウンドトリップテストが主要ターゲット
- QAction ベースのため、テスト用に QAction を作成して渡す必要がある

## 対象ファイル

### 参照（変更不要だが理解が必要）
- `src/dialogs/menuwindow.h` - ウィンドウ定義（231行）
- `src/dialogs/menuwindow.cpp` - ウィンドウ実装（374行）

### 新規作成
1. `tests/tst_menu_window.cpp` - テストファイル

### 修正対象
2. `tests/CMakeLists.txt` - テスト登録追加

## 実装手順

### Step 1: テストファイル新規作成

`tests/tst_menu_window.cpp` を新規作成する。

テストケース方針:

**お気に入りラウンドトリップテスト:**
1. `setFavorites_roundTrip` - setFavorites で設定した値が favorites() で取得できる
2. `setFavorites_empty` - 空リストで設定しても正常動作
3. `setFavorites_unknownAction_skipped` - 未登録アクション名はスキップされる

**シグナルテスト:**
4. `favoritesChanged_emittedOnChange` - お気に入り変更時に favoritesChanged シグナル発火
5. `actionTriggered_emittedOnButtonClick` - ボタンクリックで actionTriggered シグナル発火

**カテゴリ表示テスト:**
6. `setCategories_createsTabs` - カテゴリ設定でタブが作成される
7. `setCategories_empty_noTabs` - 空カテゴリでタブなし

テンプレート:
```cpp
/// @file tst_menu_window.cpp
/// @brief メニューウィンドウテスト

#include <QtTest>
#include <QSignalSpy>
#include <QAction>

#include "menuwindow.h"

class TestMenuWindow : public QObject
{
    Q_OBJECT

private:
    /// テスト用アクションを作成
    QList<QAction*> makeTestActions(QObject* parent)
    {
        QList<QAction*> actions;
        for (const auto& name : {
            QStringLiteral("actionNew"),
            QStringLiteral("actionOpen"),
            QStringLiteral("actionSave"),
            QStringLiteral("actionClose")
        }) {
            auto* action = new QAction(name, parent);
            action->setObjectName(name);
            actions.append(action);
        }
        return actions;
    }

private slots:
    void setFavorites_roundTrip();
    void setFavorites_empty();
    void favoritesChanged_emittedOnChange();
    void setCategories_createsTabs();
};
```

実装指針:
- MenuWindow のコンストラクタシグネチャを確認してインスタンス化
- CategoryInfo 構造体の定義を確認して setCategories に渡すデータを準備
- QAction は `setObjectName()` でアクション名を設定（MenuWindow が objectName で検索するため）
- お気に入りテストでは setFavorites → favorites() のラウンドトリップを QCOMPARE で検証
- favoritesChanged シグナルは QSignalSpy で検証
- MenuWindow のヘッダーを読んで CategoryInfo の定義と public API を確認すること

### Step 2: CMakeLists.txt にテスト登録

`tests/CMakeLists.txt` に以下を追加:

```cmake
# ============================================================
# Unit: MenuWindow テスト
# ============================================================
add_shogi_test(tst_menu_window
    tst_menu_window.cpp
    ${SRC}/common/logcategories.cpp
    ${SRC}/dialogs/menuwindow.cpp
    ${SETTINGS_SOURCES}
)
```

MenuWindow が依存するカスタムウィジェット（MenuButtonWidget 等）のソースも必要。コンパイルエラーに応じて追加すること。

### Step 3: ビルド・テスト実行
```bash
cmake -B build -S .
cmake --build build -j4
ctest --test-dir build --output-on-failure -R tst_menu_window
```

### Step 4: 全テスト実行
```bash
ctest --test-dir build --output-on-failure
```

## 受け入れ基準
- お気に入りラウンドトリップテスト 2件以上
- シグナルまたはカテゴリテスト 1件以上
- ビルド成功（warning なし）
- 既存テスト全件成功
