# Task 20260303-15: ドック機能テスト追加

## 概要
DockLayoutManager のレイアウト保存・復元・管理ロジックのテストを追加する。現状は設定永続化テスト（docksLocked, saveDockLayout 等）のみで、マネージャーのビジネスロジックテストがない。

## 優先度
Low

## 背景
- `DockLayoutManager` は QObject で、`QMainWindow*` に依存する
- レイアウト保存・復元は `QMainWindow::saveState()/restoreState()` を使用
- ドック登録、レイアウトの名前付き保存・復元・削除、ロック状態管理を担当
- テスト環境では QMainWindow のインスタンス化が必要だが、offscreen 環境で動作可能

## 対象ファイル

### 参照（変更不要だが理解が必要）
- `src/ui/coordinators/docklayoutmanager.h` - マネージャー定義（154行）
- `src/ui/coordinators/docklayoutmanager.cpp` - マネージャー実装（399行）
- `src/services/docksettings.h/.cpp` - ドック設定永続化

### 新規作成
1. `tests/tst_dock_layout.cpp` - テストファイル

### 修正対象
2. `tests/CMakeLists.txt` - テスト登録追加

## 実装手順

### Step 1: テストファイル新規作成

`tests/tst_dock_layout.cpp` を新規作成する。

テストケース方針:

**ドック登録テスト:**
1. `registerDock_addsDockWidget` - registerDock で QDockWidget が登録される
2. `registerDock_multipleDocks` - 複数ドックの登録

**ロック状態テスト:**
3. `setDocksLocked_true_disablesMovement` - ロック時にドック移動不可
4. `setDocksLocked_false_enablesMovement` - アンロック時にドック移動可能
5. `setDocksLocked_persistsState` - ロック状態が DockSettings に保存される

**レイアウト保存・復元テスト:**
6. `saveLayoutAs_createsNamedLayout` - 名前付きレイアウト保存
7. `restoreLayout_existingName_succeeds` - 保存済みレイアウトの復元
8. `restoreLayout_unknownName_noEffect` - 未知のレイアウト名で副作用なし
9. `deleteLayout_removesNamedLayout` - レイアウト削除

**スタートアップレイアウトテスト:**
10. `setAsStartupLayout_persistsName` - スタートアップレイアウト設定
11. `clearStartupLayout_clearsName` - スタートアップレイアウトクリア

**デフォルトリセットテスト:**
12. `resetToDefault_restoresInitialState` - デフォルトレイアウトへのリセット

テンプレート:
```cpp
/// @file tst_dock_layout.cpp
/// @brief ドックレイアウトマネージャーテスト

#include <QtTest>
#include <QMainWindow>
#include <QDockWidget>

#include "docklayoutmanager.h"
#include "docksettings.h"

class TestDockLayout : public QObject
{
    Q_OBJECT

private:
    /// テスト用の QMainWindow + DockWidget セットアップ
    struct TestSetup {
        QMainWindow mainWindow;
        QDockWidget* dock1 = nullptr;
        QDockWidget* dock2 = nullptr;
        DockLayoutManager* manager = nullptr;

        TestSetup()
        {
            dock1 = new QDockWidget(QStringLiteral("Dock1"), &mainWindow);
            dock2 = new QDockWidget(QStringLiteral("Dock2"), &mainWindow);
            mainWindow.addDockWidget(Qt::LeftDockWidgetArea, dock1);
            mainWindow.addDockWidget(Qt::RightDockWidgetArea, dock2);
            manager = new DockLayoutManager(&mainWindow);
        }
    };

private slots:
    void registerDock_addsDockWidget();
    void setDocksLocked_true_disablesMovement();
    void setDocksLocked_false_enablesMovement();
    void saveLayoutAs_createsNamedLayout();
    void restoreLayout_existingName_succeeds();
    void deleteLayout_removesNamedLayout();
    void setAsStartupLayout_persistsName();
    void clearStartupLayout_clearsName();
    void resetToDefault_restoresInitialState();
};
```

実装指針:
- `QMainWindow` と `QDockWidget` はテスト内でローカルに作成
- `DockLayoutManager` のコンストラクタに `QMainWindow*` を渡す
- `registerDock` は DockType enum を確認して適切な値を使用
- レイアウト保存・復元テストでは、ドック1つを hide → saveLayout → show → restoreLayout → hide 状態の復元を検証
- 設定永続化テストでは `QSettings` を一時ファイルに向けるか、テスト後にクリーンアップ
- `DockLayoutManager` のヘッダーを読んで DockType enum と registerDock のシグネチャを確認すること

### Step 2: CMakeLists.txt にテスト登録

`tests/CMakeLists.txt` に以下を追加:

```cmake
# ============================================================
# Unit: DockLayoutManager テスト
# ============================================================
add_shogi_test(tst_dock_layout
    tst_dock_layout.cpp
    ${SRC}/common/logcategories.cpp
    ${SRC}/ui/coordinators/docklayoutmanager.cpp
    ${SETTINGS_SOURCES}
)
```

### Step 3: ビルド・テスト実行
```bash
cmake -B build -S .
cmake --build build -j4
ctest --test-dir build --output-on-failure -R tst_dock_layout
```

### Step 4: 全テスト実行
```bash
ctest --test-dir build --output-on-failure
```

## 受け入れ基準
- ドック登録テスト 1件以上
- ロック状態テスト 2件以上
- レイアウト保存・復元テスト 3件以上
- ビルド成功（warning なし）
- 既存テスト全件成功
