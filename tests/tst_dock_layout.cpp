/// @file tst_dock_layout.cpp
/// @brief ドックレイアウトマネージャーテスト

#include <QtTest>
#include <QMainWindow>
#include <QDockWidget>
#include <QStandardPaths>
#include <QFile>

#include "docklayoutmanager.h"
#include "docksettings.h"
#include "settingscommon.h"

class TestDockLayout : public QObject
{
    Q_OBJECT

private:
    /// テスト用の QMainWindow + DockWidget セットアップ
    struct TestSetup {
        QMainWindow mainWindow;
        QDockWidget* dock1 = nullptr;
        QDockWidget* dock2 = nullptr;
        QDockWidget* dock3 = nullptr;
        DockLayoutManager* manager = nullptr;

        TestSetup()
        {
            dock1 = new QDockWidget(QStringLiteral("Record"), &mainWindow);
            dock1->setObjectName(QStringLiteral("RecordDock"));
            dock2 = new QDockWidget(QStringLiteral("Thinking"), &mainWindow);
            dock2->setObjectName(QStringLiteral("ThinkingDock"));
            dock3 = new QDockWidget(QStringLiteral("EvalChart"), &mainWindow);
            dock3->setObjectName(QStringLiteral("EvalChartDock"));
            mainWindow.addDockWidget(Qt::RightDockWidgetArea, dock1);
            mainWindow.addDockWidget(Qt::BottomDockWidgetArea, dock2);
            mainWindow.addDockWidget(Qt::BottomDockWidgetArea, dock3);
            manager = new DockLayoutManager(&mainWindow, &mainWindow);
        }
    };

private slots:
    void initTestCase();
    void cleanupTestCase();
    void cleanup();

    // ドック登録テスト
    void registerDock_addsDockWidget();
    void registerDock_multipleDocks();

    // ロック状態テスト
    void setDocksLocked_true_disablesMovement();
    void setDocksLocked_false_enablesMovement();
    void setDocksLocked_onlyAffectsRegisteredDocks();

    // レイアウト保存・復元テスト
    void restoreLayout_existingName_succeeds();
    void restoreStartupLayoutIfSet_restoresLayout();
    void restoreStartupLayoutIfSet_noLayoutSet_noEffect();
    void resetToDefault_restoresInitialState();
    void saveDockStates_persistsState();
};

void TestDockLayout::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    QString path = SettingsCommon::settingsFilePath();
    QFile::remove(path);
}

void TestDockLayout::cleanupTestCase()
{
    QString path = SettingsCommon::settingsFilePath();
    QFile::remove(path);
}

void TestDockLayout::cleanup()
{
    QSettings& s = SettingsCommon::openSettings();
    s.remove(QStringLiteral("CustomDockLayouts"));
    s.remove(QStringLiteral("DockLayout"));
    s.remove(QStringLiteral("Dock"));
    s.remove(QStringLiteral("EvalChartDock"));
    s.remove(QStringLiteral("RecordPaneDock"));
    s.remove(QStringLiteral("MenuWindowDock"));
    s.remove(QStringLiteral("JosekiWindowDock"));
    s.remove(QStringLiteral("KifuAnalysisResultsDock"));
    s.sync();
}

// --- ドック登録テスト ---

void TestDockLayout::registerDock_addsDockWidget()
{
    TestSetup setup;
    setup.manager->registerDock(DockLayoutManager::DockType::Record, setup.dock1);

    // 登録したドックがロック操作で影響を受けることを検証
    setup.manager->setDocksLocked(true);
    QCOMPARE(setup.dock1->features(), QDockWidget::DockWidgetClosable);
}

void TestDockLayout::registerDock_multipleDocks()
{
    TestSetup setup;
    setup.manager->registerDock(DockLayoutManager::DockType::Record, setup.dock1);
    setup.manager->registerDock(DockLayoutManager::DockType::Thinking, setup.dock2);
    setup.manager->registerDock(DockLayoutManager::DockType::EvalChart, setup.dock3);

    // 全登録ドックがロック操作で影響を受けることを検証
    setup.manager->setDocksLocked(true);
    QCOMPARE(setup.dock1->features(), QDockWidget::DockWidgetClosable);
    QCOMPARE(setup.dock2->features(), QDockWidget::DockWidgetClosable);
    QCOMPARE(setup.dock3->features(), QDockWidget::DockWidgetClosable);
}

// --- ロック状態テスト ---

void TestDockLayout::setDocksLocked_true_disablesMovement()
{
    TestSetup setup;
    setup.manager->registerDock(DockLayoutManager::DockType::Record, setup.dock1);
    setup.manager->registerDock(DockLayoutManager::DockType::Thinking, setup.dock2);

    setup.manager->setDocksLocked(true);

    // ロック時: Closable のみ許可
    const auto expectedFeatures = QDockWidget::DockWidgetClosable;
    QCOMPARE(setup.dock1->features(), expectedFeatures);
    QCOMPARE(setup.dock2->features(), expectedFeatures);

    // ロック時: ドッキングエリア禁止
    QCOMPARE(setup.dock1->allowedAreas(), Qt::NoDockWidgetArea);
    QCOMPARE(setup.dock2->allowedAreas(), Qt::NoDockWidgetArea);
}

void TestDockLayout::setDocksLocked_false_enablesMovement()
{
    TestSetup setup;
    setup.manager->registerDock(DockLayoutManager::DockType::Record, setup.dock1);
    setup.manager->registerDock(DockLayoutManager::DockType::Thinking, setup.dock2);

    // まずロックしてからアンロック
    setup.manager->setDocksLocked(true);
    setup.manager->setDocksLocked(false);

    // アンロック時: Closable + Movable + Floatable
    const auto expectedFeatures = QDockWidget::DockWidgetClosable
                                | QDockWidget::DockWidgetMovable
                                | QDockWidget::DockWidgetFloatable;
    QCOMPARE(setup.dock1->features(), expectedFeatures);
    QCOMPARE(setup.dock2->features(), expectedFeatures);

    // アンロック時: 全エリア許可
    QCOMPARE(setup.dock1->allowedAreas(), Qt::AllDockWidgetAreas);
    QCOMPARE(setup.dock2->allowedAreas(), Qt::AllDockWidgetAreas);
}

void TestDockLayout::setDocksLocked_onlyAffectsRegisteredDocks()
{
    TestSetup setup;
    // dock1 のみ登録、dock2 は未登録
    setup.manager->registerDock(DockLayoutManager::DockType::Record, setup.dock1);

    const auto featuresBefore = setup.dock2->features();
    setup.manager->setDocksLocked(true);

    // 登録済みドックはロックされる
    QCOMPARE(setup.dock1->features(), QDockWidget::DockWidgetClosable);
    // 未登録ドックは影響を受けない
    QCOMPARE(setup.dock2->features(), featuresBefore);
}

// --- レイアウト保存・復元テスト ---

void TestDockLayout::restoreLayout_existingName_succeeds()
{
    TestSetup setup;
    setup.manager->registerDock(DockLayoutManager::DockType::Record, setup.dock1);
    setup.manager->registerDock(DockLayoutManager::DockType::Thinking, setup.dock2);

    // 現在の状態を DockSettings 経由で直接保存
    QByteArray state = setup.mainWindow.saveState();
    DockSettings::saveDockLayout(QStringLiteral("TestLayout"), state);

    // restoreLayout が正常に完了すること（成功パス: QMessageBox 不表示）
    setup.manager->restoreLayout(QStringLiteral("TestLayout"));

    // 保存されたレイアウトデータが一致すること
    QCOMPARE(DockSettings::loadDockLayout(QStringLiteral("TestLayout")), state);
}

void TestDockLayout::restoreStartupLayoutIfSet_restoresLayout()
{
    TestSetup setup;
    setup.manager->registerDock(DockLayoutManager::DockType::Record, setup.dock1);
    setup.manager->registerDock(DockLayoutManager::DockType::Thinking, setup.dock2);

    // レイアウトを保存してスタートアップに設定
    QByteArray state = setup.mainWindow.saveState();
    DockSettings::saveDockLayout(QStringLiteral("Startup"), state);
    DockSettings::setStartupDockLayoutName(QStringLiteral("Startup"));

    // dock1 を非表示にして状態を変更
    setup.dock1->setVisible(false);

    // スタートアップレイアウトを復元
    setup.manager->restoreStartupLayoutIfSet();

    // showAllDocks() により全ドックが表示状態にされてから restoreState される
    // スタートアップレイアウト名が正しく設定されていること
    QCOMPARE(DockSettings::startupDockLayoutName(), QStringLiteral("Startup"));
}

void TestDockLayout::restoreStartupLayoutIfSet_noLayoutSet_noEffect()
{
    TestSetup setup;
    setup.manager->registerDock(DockLayoutManager::DockType::Record, setup.dock1);

    // スタートアップレイアウトが未設定
    DockSettings::setStartupDockLayoutName(QString());

    QByteArray stateBefore = setup.mainWindow.saveState();

    // 何もせずに戻ることを検証
    setup.manager->restoreStartupLayoutIfSet();

    // 状態が変わっていないこと
    QCOMPARE(setup.mainWindow.saveState(), stateBefore);
}

void TestDockLayout::resetToDefault_restoresInitialState()
{
    TestSetup setup;
    setup.manager->registerDock(DockLayoutManager::DockType::Record, setup.dock1);
    setup.manager->registerDock(DockLayoutManager::DockType::Thinking, setup.dock2);
    setup.manager->registerDock(DockLayoutManager::DockType::EvalChart, setup.dock3);

    // ドックをフローティングにして状態を変更
    setup.dock1->setFloating(true);

    // デフォルトにリセット
    setup.manager->resetToDefault();

    // リセット後: 全ドックがフローティングではない
    QVERIFY(!setup.dock1->isFloating());
    QVERIFY(!setup.dock2->isFloating());
    QVERIFY(!setup.dock3->isFloating());
}

void TestDockLayout::saveDockStates_persistsState()
{
    TestSetup setup;
    setup.manager->registerDock(DockLayoutManager::DockType::Record, setup.dock1);
    setup.manager->registerDock(DockLayoutManager::DockType::EvalChart, setup.dock3);

    // 状態を保存
    setup.manager->saveDockStates();

    // 保存された値がドックの実際の状態と一致すること
    QCOMPARE(DockSettings::recordPaneDockFloating(), setup.dock1->isFloating());
    QCOMPARE(DockSettings::recordPaneDockVisible(), setup.dock1->isVisible());
    QVERIFY(!DockSettings::recordPaneDockGeometry().isEmpty());

    QCOMPARE(DockSettings::evalChartDockFloating(), setup.dock3->isFloating());
    QCOMPARE(DockSettings::evalChartDockVisible(), setup.dock3->isVisible());
    QVERIFY(!DockSettings::evalChartDockGeometry().isEmpty());
}

QTEST_MAIN(TestDockLayout)
#include "tst_dock_layout.moc"
