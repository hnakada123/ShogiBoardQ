/// @file tst_language_controller.cpp
/// @brief 多言語対応コントローラテスト
///
/// テスト対象:
/// - LanguageController のアクション排他性
/// - updateMenuState() による言語設定→メニュー状態の同期
/// - AppSettings::setLanguage → updateMenuState のラウンドトリップ

#include <QtTest>
#include <QAction>
#include <QActionGroup>
#include <QStandardPaths>
#include <QFile>

#include "languagecontroller.h"
#include "appsettings.h"
#include "settingscommon.h"

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
    void cleanup();

    void setActions_createsActionGroup();
    void setActions_actionsAreExclusive();
    void updateMenuState_system_checksSystemAction();
    void updateMenuState_japanese_checksJapaneseAction();
    void updateMenuState_english_checksEnglishAction();
    void updateMenuState_afterSettingsChange();
    void updateMenuState_unknownLanguage_noneChecked();
};

void TestLanguageController::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    QString path = SettingsCommon::settingsFilePath();
    QFile::remove(path);
}

void TestLanguageController::cleanupTestCase()
{
    QString path = SettingsCommon::settingsFilePath();
    QFile::remove(path);
}

void TestLanguageController::cleanup()
{
    // 各テスト後にデフォルトに戻す
    AppSettings::setLanguage(QStringLiteral("system"));
}

void TestLanguageController::setActions_createsActionGroup()
{
    TestSetup s;

    // setActions 後、各アクションが QActionGroup に属する
    QVERIFY(s.systemAction.actionGroup() != nullptr);
    QVERIFY(s.japaneseAction.actionGroup() != nullptr);
    QVERIFY(s.englishAction.actionGroup() != nullptr);
}

void TestLanguageController::setActions_actionsAreExclusive()
{
    TestSetup s;

    // 3つのアクションが同じ排他グループに属する
    QActionGroup* group = s.systemAction.actionGroup();
    QVERIFY(group != nullptr);
    QCOMPARE(s.japaneseAction.actionGroup(), group);
    QCOMPARE(s.englishAction.actionGroup(), group);
    QVERIFY(group->isExclusive());
}

void TestLanguageController::updateMenuState_system_checksSystemAction()
{
    AppSettings::setLanguage(QStringLiteral("system"));
    TestSetup s;

    // setActions 内で updateMenuState が呼ばれるため、すでに同期済み
    QVERIFY(s.systemAction.isChecked());
    QVERIFY(!s.japaneseAction.isChecked());
    QVERIFY(!s.englishAction.isChecked());
}

void TestLanguageController::updateMenuState_japanese_checksJapaneseAction()
{
    AppSettings::setLanguage(QStringLiteral("ja_JP"));
    TestSetup s;

    QVERIFY(!s.systemAction.isChecked());
    QVERIFY(s.japaneseAction.isChecked());
    QVERIFY(!s.englishAction.isChecked());
}

void TestLanguageController::updateMenuState_english_checksEnglishAction()
{
    AppSettings::setLanguage(QStringLiteral("en"));
    TestSetup s;

    QVERIFY(!s.systemAction.isChecked());
    QVERIFY(!s.japaneseAction.isChecked());
    QVERIFY(s.englishAction.isChecked());
}

void TestLanguageController::updateMenuState_afterSettingsChange()
{
    // 初期状態: system
    AppSettings::setLanguage(QStringLiteral("system"));
    TestSetup s;
    QVERIFY(s.systemAction.isChecked());

    // 設定を変更後に updateMenuState で同期
    AppSettings::setLanguage(QStringLiteral("ja_JP"));
    s.controller.updateMenuState();
    QVERIFY(s.japaneseAction.isChecked());
    QVERIFY(!s.systemAction.isChecked());
    QVERIFY(!s.englishAction.isChecked());

    // さらに英語に変更
    AppSettings::setLanguage(QStringLiteral("en"));
    s.controller.updateMenuState();
    QVERIFY(s.englishAction.isChecked());
    QVERIFY(!s.systemAction.isChecked());
    QVERIFY(!s.japaneseAction.isChecked());
}

void TestLanguageController::updateMenuState_unknownLanguage_noneChecked()
{
    AppSettings::setLanguage(QStringLiteral("fr_FR"));
    TestSetup s;

    // 未知の言語コードの場合、どのアクションも checked にならない
    QVERIFY(!s.systemAction.isChecked());
    QVERIFY(!s.japaneseAction.isChecked());
    QVERIFY(!s.englishAction.isChecked());
}

QTEST_MAIN(TestLanguageController)
#include "tst_language_controller.moc"
