# Task 20260303-17: 多言語対応テスト追加

## 概要
LanguageController の言語メニュー状態管理テストを追加する。現状は設定永続化テスト（appSettings_language）のみで、言語コントローラのロジックテストがない。

## 優先度
Low

## 背景
- `LanguageController` は QObject で、3つの QAction（system, japanese, english）の checked 状態を管理
- `updateMenuState()` は SettingsService から現在言語を読み取り、対応する QAction を checked にする
- `changeLanguage()` は private で QMessageBox を表示するため直接テスト困難
- `updateMenuState()` と QAction の checked 状態の同期をテストするのが現実的

## 対象ファイル

### 参照（変更不要だが理解が必要）
- `src/ui/controllers/languagecontroller.h` - コントローラ定義（77行）
- `src/ui/controllers/languagecontroller.cpp` - コントローラ実装（94行）
- `src/services/appsettings.h/.cpp` - 言語設定の読み書き

### 新規作成
1. `tests/tst_language_controller.cpp` - テストファイル

### 修正対象
2. `tests/CMakeLists.txt` - テスト登録追加

## 実装手順

### Step 1: テストファイル新規作成

`tests/tst_language_controller.cpp` を新規作成する。

テストケース方針:

**メニュー状態テスト:**
1. `setActions_createsActionGroup` - setActions 後にアクションが排他グループに属する
2. `updateMenuState_system_checksSystemAction` - 言語設定 "system" で systemAction が checked
3. `updateMenuState_japanese_checksJapaneseAction` - 言語設定 "ja_JP" で japaneseAction が checked
4. `updateMenuState_english_checksEnglishAction` - 言語設定 "en" で englishAction が checked

**初期状態テスト:**
5. `initialState_noActionsSet` - setActions 前はクラッシュしない
6. `setActions_actionsAreExclusive` - 3つのアクションが QActionGroup で排他的

**設定連動テスト:**
7. `updateMenuState_afterSettingsChange` - AppSettings::setLanguage → updateMenuState でメニュー状態が同期

テンプレート:
```cpp
/// @file tst_language_controller.cpp
/// @brief 多言語対応コントローラテスト

#include <QtTest>
#include <QAction>

#include "languagecontroller.h"
#include "appsettings.h"

class TestLanguageController : public QObject
{
    Q_OBJECT

private:
    struct TestSetup {
        LanguageController controller;
        QAction systemAction{QStringLiteral("System")};
        QAction japaneseAction{QStringLiteral("日本語")};
        QAction englishAction{QStringLiteral("English")};

        TestSetup()
        {
            systemAction.setCheckable(true);
            japaneseAction.setCheckable(true);
            englishAction.setCheckable(true);
            controller.setActions(&systemAction, &japaneseAction, &englishAction);
        }
    };

private slots:
    void initTestCase();
    void cleanupTestCase();

    void setActions_createsActionGroup();
    void setActions_actionsAreExclusive();
    void updateMenuState_system_checksSystemAction();
    void updateMenuState_japanese_checksJapaneseAction();
    void updateMenuState_english_checksEnglishAction();
    void updateMenuState_afterSettingsChange();
};

void TestLanguageController::initTestCase()
{
    // テスト用に一時的な設定ファイルを使用
    // AppSettings が QSettings を使うため、組織名/アプリ名を設定
    QCoreApplication::setOrganizationName(QStringLiteral("TestOrg"));
    QCoreApplication::setApplicationName(QStringLiteral("TestApp"));
}

void TestLanguageController::cleanupTestCase()
{
    // テスト用設定をクリーンアップ
}
```

実装指針:
- `LanguageController` は `setActions()` で3つの QAction を受け取る。テスト用に QAction をローカルに作成
- `updateMenuState()` は内部で `AppSettings::language()` を読む。テスト前に `AppSettings::setLanguage()` で値を設定
- `AppSettings` は `QSettings` 経由で INI ファイルに保存するため、テスト用の一時パスを設定するか、テスト後にクリーンアップ
- `changeLanguage()` は private で QMessageBox を表示するため、スロット経由のテストは行わない
- `setActions` 後の QAction::isChecked() を QVERIFY で検証

### Step 2: CMakeLists.txt にテスト登録

`tests/CMakeLists.txt` に以下を追加:

```cmake
# ============================================================
# Unit: LanguageController テスト
# ============================================================
add_shogi_test(tst_language_controller
    tst_language_controller.cpp
    ${SRC}/common/logcategories.cpp
    ${SRC}/ui/controllers/languagecontroller.cpp
    ${SETTINGS_SOURCES}
)
```

### Step 3: ビルド・テスト実行
```bash
cmake -B build -S .
cmake --build build -j4
ctest --test-dir build --output-on-failure -R tst_language_controller
```

### Step 4: 全テスト実行
```bash
ctest --test-dir build --output-on-failure
```

## 受け入れ基準
- メニュー状態テスト（system/japanese/english）3件以上
- アクション排他性テスト 1件以上
- ビルド成功（warning なし）
- 既存テスト全件成功
