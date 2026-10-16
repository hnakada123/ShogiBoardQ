/// @file tst_dock_layout.cpp
/// @brief ドックの表（DockRegistry）とレイアウトマネージャーのテスト

#include <QtTest>
#include <QMainWindow>
#include <QDockWidget>
#include <QTabBar>
#include <QStandardPaths>
#include <QFile>

#include "docklayoutmanager.h"
#include "dockregistry.h"
#include "layoutboundarywidget.h"
#include "docksettings.h"
#include "settingscommon.h"

class TestDockLayout : public QObject
{
    Q_OBJECT

private:
    using Id = DockRegistry::DockId;

    /// テスト用の QMainWindow と、棋譜・思考・評価値グラフの3つのドック
    struct TestSetup {
        QMainWindow mainWindow;
        DockRegistry* registry = nullptr;
        QDockWidget* dock1 = nullptr;  ///< 棋譜（右）
        QDockWidget* dock2 = nullptr;  ///< 思考（下）
        QDockWidget* dock3 = nullptr;  ///< 評価値グラフ（下。思考のタブに重なる）
        DockLayoutManager* manager = nullptr;

        TestSetup()
        {
            registry = new DockRegistry(&mainWindow, nullptr, &mainWindow);
            DockRegistry::Contents contents{};
            for (Id id : {Id::Record, Id::Thinking, Id::EvalChart})
                contents.at(static_cast<std::size_t>(id)) = new QWidget(&mainWindow);
            registry->createAll(contents);
            dock1 = registry->dock(Id::Record);
            dock2 = registry->dock(Id::Thinking);
            dock3 = registry->dock(Id::EvalChart);
            manager = new DockLayoutManager(&mainWindow, registry, &mainWindow);
        }
    };

    /// すべてのドックを作った QMainWindow
    struct FullSetup {
        QMainWindow window;
        DockRegistry* registry = nullptr;

        FullSetup()
        {
            window.setCentralWidget(new QWidget(&window));
            registry = new DockRegistry(&window, nullptr, &window);
            DockRegistry::Contents contents{};
            for (auto& content : contents) content = new QWidget(&window);
            registry->createAll(contents);
        }

        /// 思考のドックを含む下段のタブの題名（current には前面のタブ）
        QStringList bottomTabs(QString* current = nullptr) const
        {
            QStringList tabs;
            const QString thinking = DockRegistry::title(Id::Thinking);
            const auto bars = window.findChildren<QTabBar*>();
            for (QTabBar* bar : bars) {
                QStringList texts;
                for (int i = 0; i < bar->count(); ++i) texts << bar->tabText(i);
                if (!texts.contains(thinking)) continue;
                tabs = texts;
                if (current) *current = bar->tabText(bar->currentIndex());
            }
            return tabs;
        }
    };

    /// 表の下段のドックの題名（表の順。hidden も含めるなら includeHidden）
    static QStringList bottomTitles(bool includeHidden)
    {
        QStringList titles;
        for (const auto& spec : DockRegistry::specs())
            if (spec.area == Qt::BottomDockWidgetArea && (includeHidden || spec.defaultVisible))
                titles << DockRegistry::title(spec.id);
        return titles;
    }

private slots:
    void initTestCase();
    void cleanupTestCase();
    void cleanup();

    // ドックの作成テスト
    void createAll_createsOnlyGivenDocks();
    void createAll_restoresSavedStateOfEveryDock();
    void createAll_matchesDefaultArrangement();

    // ロック状態テスト
    void setDocksLocked_true_disablesMovement();
    void setDocksLocked_false_enablesMovement();
    void setDocksLocked_onlyAffectsRegistryDocks();

    // レイアウト保存・復元テスト
    void restoreLayout_existingName_succeeds();
    void restoreStartupLayoutIfSet_restoresLayout();
    void restoreStartupLayoutIfSet_noLayoutSet_noEffect();
    void resetToDefault_restoresInitialState();
    void resetToDefault_resetsAnalysisResults();
    void resetToDefault_matchesStartupArrangement();
    void setDocksLocked_persistsState();
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
    for (const auto& spec : DockRegistry::specs()) s.remove(QString::fromLatin1(spec.settingsGroup));
    s.sync();
}

// --- ドック登録テスト ---

void TestDockLayout::createAll_createsOnlyGivenDocks()
{
    TestSetup setup;
    QCOMPARE(setup.registry->docks(), QList<QDockWidget*>({setup.dock1, setup.dock2, setup.dock3}));
    QVERIFY(!setup.registry->dock(Id::Menu));
    QCOMPARE(setup.dock1->objectName(), QStringLiteral("RecordPaneDock"));
    QCOMPARE(setup.dock2->windowTitle(), DockRegistry::title(Id::Thinking));
    QCOMPARE(setup.mainWindow.dockWidgetArea(setup.dock1), Qt::RightDockWidgetArea);
    QCOMPARE(setup.mainWindow.dockWidgetArea(setup.dock3), Qt::BottomDockWidgetArea);
    // 中身の大きさが頻繁に変わる思考のドックは、配置の計算を区切る部品で包む
    QVERIFY(qobject_cast<LayoutBoundaryWidget*>(setup.dock2->widget()));
    QVERIFY(!qobject_cast<LayoutBoundaryWidget*>(setup.dock3->widget()));

    // ロック操作は作ったドックすべてに効く
    setup.manager->setDocksLocked(true);
    for (QDockWidget* dock : {setup.dock1, setup.dock2, setup.dock3})
        QCOMPARE(dock->features(), QDockWidget::DockWidgetClosable);
}

/// 浮動・表示・位置は、下段のパネルも含めてすべてのドックで保存して戻す
void TestDockLayout::createAll_restoresSavedStateOfEveryDock()
{
    {
        TestSetup setup;
        setup.mainWindow.show();
        QVERIFY(QTest::qWaitForWindowExposed(&setup.mainWindow));
        setup.dock2->hide();
        setup.dock3->setFloating(true);
        setup.manager->saveDockStates();
    }
    const DockSettings::DockState thinking = DockSettings::dockState(QStringLiteral("ThinkingDock"), true);
    QVERIFY(!thinking.visible);
    QVERIFY(DockSettings::dockState(QStringLiteral("EvalChartDock"), true).floating);

    TestSetup restored;
    restored.mainWindow.show();
    QVERIFY(QTest::qWaitForWindowExposed(&restored.mainWindow));
    QVERIFY(restored.dock2->isHidden());
    QVERIFY(restored.dock3->isFloating());
    QVERIFY(restored.dock1->isVisible());
}

/// 起動時の配置は「レイアウトを元に戻す」と同じ（どちらも表から作る）
void TestDockLayout::createAll_matchesDefaultArrangement()
{
    FullSetup setup;
    setup.window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&setup.window));
    QString current;
    QCOMPARE(setup.bottomTabs(&current), bottomTitles(false));
    QCOMPARE(current, DockRegistry::title(Id::Thinking));
    QVERIFY(setup.registry->dock(Id::Menu)->isHidden());
    QCOMPARE(setup.window.dockWidgetArea(setup.registry->dock(Id::Menu)), Qt::RightDockWidgetArea);
}

// --- ロック状態テスト ---

void TestDockLayout::setDocksLocked_true_disablesMovement()
{
    TestSetup setup;

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

void TestDockLayout::setDocksLocked_onlyAffectsRegistryDocks()
{
    TestSetup setup;
    // 表に無いドックは影響を受けない
    auto* other = new QDockWidget(QStringLiteral("Other"), &setup.mainWindow);
    setup.mainWindow.addDockWidget(Qt::LeftDockWidgetArea, other);
    const auto featuresBefore = other->features();
    setup.manager->setDocksLocked(true);

    QCOMPARE(setup.dock1->features(), QDockWidget::DockWidgetClosable);
    QCOMPARE(other->features(), featuresBefore);
}

// --- レイアウト保存・復元テスト ---

void TestDockLayout::restoreLayout_existingName_succeeds()
{
    TestSetup setup;
    setup.mainWindow.show();
    setup.mainWindow.tabifyDockWidget(setup.dock2, setup.dock3);
    setup.dock1->setFloating(true);
    setup.dock1->setGeometry(40, 60, 320, 240);
    setup.dock3->hide();
    const QRect floatingGeometry = setup.dock1->geometry();

    // 現在の状態を DockSettings 経由で直接保存
    QByteArray state = setup.mainWindow.saveState();
    DockSettings::saveDockLayout(QStringLiteral("TestLayout"), state);

    setup.dock1->setFloating(false);
    setup.mainWindow.addDockWidget(Qt::LeftDockWidgetArea, setup.dock3);
    setup.dock3->show();

    // restoreLayout が正常に完了すること（成功パス: QMessageBox 不表示）
    setup.manager->restoreLayout(QStringLiteral("TestLayout"));

    // 保存されたレイアウトデータが一致すること
    QCOMPARE(DockSettings::loadDockLayout(QStringLiteral("TestLayout")), state);
    QVERIFY(setup.dock1->isFloating());
    QCOMPARE(setup.dock1->geometry(), floatingGeometry);
    QVERIFY(setup.dock3->isHidden());
    // Qt は閉じたタブを tabifiedDockWidgets() の結果に含めない。
    setup.dock3->show();
    QVERIFY(setup.mainWindow.tabifiedDockWidgets(setup.dock2).contains(setup.dock3));
}

void TestDockLayout::restoreStartupLayoutIfSet_restoresLayout()
{
    TestSetup setup;

    // レイアウトを保存してスタートアップに設定
    setup.mainWindow.show();
    setup.dock1->show();
    setup.dock2->setFloating(true);
    QByteArray state = setup.mainWindow.saveState();
    DockSettings::saveDockLayout(QStringLiteral("Startup"), state);
    DockSettings::startupDockLayoutName.set(QStringLiteral("Startup"));

    // dock1 を非表示にして状態を変更
    setup.dock1->setVisible(false);
    setup.dock2->setFloating(false);

    // スタートアップレイアウトを復元
    setup.manager->restoreStartupLayoutIfSet();

    QVERIFY(setup.dock1->isVisible());
    QVERIFY(setup.dock2->isFloating());
    QCOMPARE(DockSettings::startupDockLayoutName(), QStringLiteral("Startup"));
}

void TestDockLayout::restoreStartupLayoutIfSet_noLayoutSet_noEffect()
{
    TestSetup setup;

    // スタートアップレイアウトが未設定
    DockSettings::startupDockLayoutName.set(QString());

    QByteArray stateBefore = setup.mainWindow.saveState();

    // 何もせずに戻ることを検証
    setup.manager->restoreStartupLayoutIfSet();

    // 状態が変わっていないこと
    QCOMPARE(setup.mainWindow.saveState(), stateBefore);
}

void TestDockLayout::resetToDefault_restoresInitialState()
{
    TestSetup setup;

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

    // 状態を保存
    setup.manager->saveDockStates();

    // 保存された値がドックの実際の状態と一致すること
    const auto record = DockSettings::dockState(QStringLiteral("RecordPaneDock"), false);
    QCOMPARE(record.floating, setup.dock1->isFloating());
    QCOMPARE(record.visible, setup.dock1->isVisible());
    QVERIFY(!record.geometry.isEmpty());

    const auto chart = DockSettings::dockState(QStringLiteral("EvalChartDock"), false);
    QCOMPARE(chart.floating, setup.dock3->isFloating());
    QCOMPARE(chart.visible, setup.dock3->isVisible());
    QVERIFY(!chart.geometry.isEmpty());
}

void TestDockLayout::resetToDefault_resetsAnalysisResults()
{
    FullSetup setup;
    DockLayoutManager manager(&setup.window, setup.registry);
    setup.window.show();
    QDockWidget* results = setup.registry->dock(Id::AnalysisResults);
    results->setFloating(true);
    results->show();
    manager.resetToDefault();
    QVERIFY(!results->isFloating());
    QVERIFY(results->isHidden());
    QCOMPARE(setup.window.dockWidgetArea(results), Qt::BottomDockWidgetArea);
}

/// リセット後の配置は起動時と同じになる
void TestDockLayout::resetToDefault_matchesStartupArrangement()
{
    FullSetup setup;
    DockLayoutManager manager(&setup.window, setup.registry);
    // 起動時とは違う配置にする
    const QList<QDockWidget*> docks = setup.registry->docks();
    for (QDockWidget* dock : docks) {
        setup.window.removeDockWidget(dock);
        setup.window.addDockWidget(Qt::LeftDockWidgetArea, dock);
        dock->show();
    }
    setup.window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&setup.window));
    setup.registry->dock(Id::Thinking)->setFloating(true);

    manager.resetToDefault();
    QCoreApplication::processEvents();

    // 右: 棋譜とメニュー（メニューは非表示）
    QDockWidget* record = setup.registry->dock(Id::Record);
    QDockWidget* menu = setup.registry->dock(Id::Menu);
    QCOMPARE(setup.window.dockWidgetArea(record), Qt::RightDockWidgetArea);
    QCOMPARE(setup.window.dockWidgetArea(menu), Qt::RightDockWidgetArea);
    QVERIFY(menu->isHidden());
    // 下: 定跡と棋譜解析も同じタブにまとめて非表示
    for (Id id : {Id::Joseki, Id::AnalysisResults}) {
        QCOMPARE(setup.window.dockWidgetArea(setup.registry->dock(id)), Qt::BottomDockWidgetArea);
        QVERIFY(setup.registry->dock(id)->isHidden());
    }
    QString current;
    QCOMPARE(setup.bottomTabs(&current), bottomTitles(false));
    QCOMPARE(current, DockRegistry::title(Id::Thinking));

    // 非表示のドックも表示すると起動時と同じタブに戻る（非表示のドックはタブの一覧に含まれない）
    for (Id id : {Id::Menu, Id::Joseki, Id::AnalysisResults}) setup.registry->dock(id)->show();
    QCoreApplication::processEvents();
    QVERIFY(setup.window.tabifiedDockWidgets(record).contains(menu));
    QCOMPARE(setup.bottomTabs(), bottomTitles(true));
}

void TestDockLayout::setDocksLocked_persistsState()
{
    TestSetup setup;
    setup.manager->setDocksLocked(true);
    QVERIFY(DockSettings::docksLocked());
    setup.manager->setDocksLocked(false);
    QVERIFY(!DockSettings::docksLocked());
}

QTEST_MAIN(TestDockLayout)
#include "tst_dock_layout.moc"
