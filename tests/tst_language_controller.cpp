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
#include <QLabel>
#include <QMessageBox>
#include <QTimer>

#include "languagecontroller.h"
#include "appsettings.h"
#include "settingscommon.h"

class TestLanguageController : public QObject
{
    Q_OBJECT

private:
    int m_unwrappedLines = -1;

    /// 表示中の「棋譜表記の読み方」で、すべての行が本文の幅に収まっていれば行数を記録して閉じる
    void inspectNotationHelp()
    {
        auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
        if (!box) {
            QTimer::singleShot(10, this, &TestLanguageController::inspectNotationHelp);
            return;
        }
        auto* label = box->findChild<QLabel*>(QStringLiteral("qt_msgbox_label"));
        int lines = 0;
        bool fits = label != nullptr;
        for (const QString& line : label ? label->text().split(QLatin1Char('\n')) : QStringList()) {
            fits = fits && label->fontMetrics().horizontalAdvance(line) <= label->contentsRect().width();
            ++lines;
        }
        m_unwrappedLines = fits ? lines : 0;
        box->accept();
    }

    struct TestSetup {
        LanguageController controller;
        QAction systemAction{QStringLiteral("System")};
        QAction japaneseAction{QStringLiteral("日本語")};
        QAction englishAction{QStringLiteral("English")};
        QAction simplifiedAction{QStringLiteral("简体中文")};
        QAction traditionalAction{QStringLiteral("繁體中文")};

        TestSetup()
        {
            systemAction.setCheckable(true);
            japaneseAction.setCheckable(true);
            englishAction.setCheckable(true);
            simplifiedAction.setCheckable(true);
            traditionalAction.setCheckable(true);
            controller.setActions(&systemAction, &japaneseAction, &englishAction, &simplifiedAction, &traditionalAction);
        }
    };

private slots:
    void initTestCase();
    void cleanupTestCase();
    void cleanup();

    void chineseLanguagesAreExclusive()
    {
        TestSetup setup;
        setup.simplifiedAction.trigger();
        QCOMPARE(AppSettings::language(), QStringLiteral("zh_CN"));
        QVERIFY(setup.simplifiedAction.isChecked());
        setup.traditionalAction.trigger();
        QCOMPARE(AppSettings::language(), QStringLiteral("zh_TW"));
        QVERIFY(setup.traditionalAction.isChecked());
        QVERIFY(!setup.simplifiedAction.isChecked());
        setup.englishAction.trigger();
        QVERIFY(!setup.traditionalAction.isChecked());
    }
    /// 「棋譜表記の読み方」は、記号と説明が別の行に分かれないよう各行を折り返さずに表示する
    void notationHelpKeepsLinesUnwrapped()
    {
        LanguageController controller;
        QAction automatic, japanese, western, origin, help;
        controller.setNotationActions(&automatic, &japanese, &western, &origin, &help);
        m_unwrappedLines = -1;
        QTimer::singleShot(0, this, &TestLanguageController::inspectNotationHelp);
        help.trigger();
        QVERIFY(m_unwrappedLines > 5);
    }
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
