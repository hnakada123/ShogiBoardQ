/// @file tst_wiring_analysistab.cpp
/// @brief AnalysisTabWiring の実行時配線テスト

#include <QtTest>

#include "analysistabwiring.h"
#include "engineanalysistab.h"
#include "branchnavigationwiring.h"
#include "commentcoordinator.h"
#include "pvclickcontroller.h"
#include "usicommandcontroller.h"
#include "considerationwiring.h"

namespace AnalysisTabWiringTracker {
extern bool branchNavigationCalled;
extern int branchRow;
extern int branchPly;

extern bool commentUpdatedCalled;
extern int commentMoveIndex;
extern QString commentText;

extern bool pvClickedCalled;
extern int pvEngineIndex;
extern int pvRow;

extern bool usiCommandCalled;
extern int usiTarget;
extern QString usiCommand;

extern bool considerationDialogCalled;
extern bool engineSettingsCalled;
extern int engineSettingsNumber;
extern QString engineSettingsName;

extern bool engineChangedCalled;
extern int changedEngineIndex;
extern QString changedEngineName;

void reset();
} // namespace AnalysisTabWiringTracker

class TestWiringAnalysisTab : public QObject
{
    Q_OBJECT

private slots:
    void buildUiAndWire_isIdempotent_andForwardsBranchNodeSignal();
    void wireExternalSignals_connectsBranchNavigation();
    void wireExternalSignals_connectsCommentCoordinator();
    void wireExternalSignals_connectsPvClickController();
    void wireExternalSignals_connectsUsiCommandController();
    void wireExternalSignals_connectsConsiderationWiring();
};

void TestWiringAnalysisTab::buildUiAndWire_isIdempotent_andForwardsBranchNodeSignal()
{
    QWidget parent;
    AnalysisTabWiring::Deps deps;
    deps.centralParent = &parent;

    AnalysisTabWiring wiring(deps);
    QSignalSpy forwardingSpy(&wiring, &AnalysisTabWiring::branchNodeActivated);

    EngineAnalysisTab* first = wiring.buildUiAndWire();
    EngineAnalysisTab* second = wiring.buildUiAndWire();

    QCOMPARE(first, second);
    QCOMPARE(wiring.analysisTab(), first);
    QCOMPARE(wiring.tab(), nullptr);
    QVERIFY(wiring.thinking1() != nullptr);
    QVERIFY(wiring.thinking2() != nullptr);

    QVERIFY(QMetaObject::invokeMethod(first, "branchNodeActivated",
                                      Qt::DirectConnection,
                                      Q_ARG(int, 3),
                                      Q_ARG(int, 18)));
    QCOMPARE(forwardingSpy.count(), 1);
    const QList<QVariant> args = forwardingSpy.takeFirst();
    QCOMPARE(args.at(0).toInt(), 3);
    QCOMPARE(args.at(1).toInt(), 18);
}

void TestWiringAnalysisTab::wireExternalSignals_connectsBranchNavigation()
{
    QWidget parent;
    AnalysisTabWiring::Deps wiringDeps;
    wiringDeps.centralParent = &parent;
    AnalysisTabWiring wiring(wiringDeps);
    EngineAnalysisTab* tab = wiring.buildUiAndWire();

    BranchNavigationWiring branchNavigation;
    AnalysisTabWiring::ExternalSignalDeps deps;
    deps.branchNavigationWiring = &branchNavigation;

    AnalysisTabWiringTracker::reset();
    wiring.wireExternalSignals(deps);

    QVERIFY(QMetaObject::invokeMethod(tab, "branchNodeActivated",
                                      Qt::DirectConnection,
                                      Q_ARG(int, 4),
                                      Q_ARG(int, 22)));
    QVERIFY(AnalysisTabWiringTracker::branchNavigationCalled);
    QCOMPARE(AnalysisTabWiringTracker::branchRow, 4);
    QCOMPARE(AnalysisTabWiringTracker::branchPly, 22);
}

void TestWiringAnalysisTab::wireExternalSignals_connectsCommentCoordinator()
{
    QWidget parent;
    AnalysisTabWiring::Deps wiringDeps;
    wiringDeps.centralParent = &parent;
    AnalysisTabWiring wiring(wiringDeps);
    EngineAnalysisTab* tab = wiring.buildUiAndWire();

    CommentCoordinator commentCoordinator;
    AnalysisTabWiring::ExternalSignalDeps deps;
    deps.commentCoordinator = &commentCoordinator;

    AnalysisTabWiringTracker::reset();
    wiring.wireExternalSignals(deps);

    QVERIFY(QMetaObject::invokeMethod(tab, "commentUpdated",
                                      Qt::DirectConnection,
                                      Q_ARG(int, 8),
                                      Q_ARG(QString, QStringLiteral("comment"))));
    QVERIFY(AnalysisTabWiringTracker::commentUpdatedCalled);
    QCOMPARE(AnalysisTabWiringTracker::commentMoveIndex, 8);
    QCOMPARE(AnalysisTabWiringTracker::commentText, QStringLiteral("comment"));
}

void TestWiringAnalysisTab::wireExternalSignals_connectsPvClickController()
{
    QWidget parent;
    AnalysisTabWiring::Deps wiringDeps;
    wiringDeps.centralParent = &parent;
    AnalysisTabWiring wiring(wiringDeps);
    EngineAnalysisTab* tab = wiring.buildUiAndWire();

    PvClickController pvClickController;
    AnalysisTabWiring::ExternalSignalDeps deps;
    deps.pvClickController = &pvClickController;

    AnalysisTabWiringTracker::reset();
    wiring.wireExternalSignals(deps);

    QVERIFY(QMetaObject::invokeMethod(tab, "pvRowClicked",
                                      Qt::DirectConnection,
                                      Q_ARG(int, 1),
                                      Q_ARG(int, 6)));
    QVERIFY(AnalysisTabWiringTracker::pvClickedCalled);
    QCOMPARE(AnalysisTabWiringTracker::pvEngineIndex, 1);
    QCOMPARE(AnalysisTabWiringTracker::pvRow, 6);
}

void TestWiringAnalysisTab::wireExternalSignals_connectsUsiCommandController()
{
    QWidget parent;
    AnalysisTabWiring::Deps wiringDeps;
    wiringDeps.centralParent = &parent;
    AnalysisTabWiring wiring(wiringDeps);
    EngineAnalysisTab* tab = wiring.buildUiAndWire();

    UsiCommandController usiCommandController;
    AnalysisTabWiring::ExternalSignalDeps deps;
    deps.usiCommandController = &usiCommandController;

    AnalysisTabWiringTracker::reset();
    wiring.wireExternalSignals(deps);

    QVERIFY(QMetaObject::invokeMethod(tab, "usiCommandRequested",
                                      Qt::DirectConnection,
                                      Q_ARG(int, 2),
                                      Q_ARG(QString, QStringLiteral("isready"))));
    QVERIFY(AnalysisTabWiringTracker::usiCommandCalled);
    QCOMPARE(AnalysisTabWiringTracker::usiTarget, 2);
    QCOMPARE(AnalysisTabWiringTracker::usiCommand, QStringLiteral("isready"));
}

void TestWiringAnalysisTab::wireExternalSignals_connectsConsiderationWiring()
{
    QWidget parent;
    AnalysisTabWiring::Deps wiringDeps;
    wiringDeps.centralParent = &parent;
    AnalysisTabWiring wiring(wiringDeps);
    EngineAnalysisTab* tab = wiring.buildUiAndWire();

    ConsiderationWiring considerationWiring(ConsiderationWiring::Deps{});
    AnalysisTabWiring::ExternalSignalDeps deps;
    deps.considerationWiring = &considerationWiring;

    AnalysisTabWiringTracker::reset();
    wiring.wireExternalSignals(deps);

    QVERIFY(QMetaObject::invokeMethod(tab, "startConsiderationRequested", Qt::DirectConnection));
    QVERIFY(QMetaObject::invokeMethod(tab, "engineSettingsRequested",
                                      Qt::DirectConnection,
                                      Q_ARG(int, 1),
                                      Q_ARG(QString, QStringLiteral("Engine-A"))));
    QVERIFY(QMetaObject::invokeMethod(tab, "considerationEngineChanged",
                                      Qt::DirectConnection,
                                      Q_ARG(int, 2),
                                      Q_ARG(QString, QStringLiteral("Engine-B"))));

    QVERIFY(AnalysisTabWiringTracker::considerationDialogCalled);
    QVERIFY(AnalysisTabWiringTracker::engineSettingsCalled);
    QCOMPARE(AnalysisTabWiringTracker::engineSettingsNumber, 1);
    QCOMPARE(AnalysisTabWiringTracker::engineSettingsName, QStringLiteral("Engine-A"));
    QVERIFY(AnalysisTabWiringTracker::engineChangedCalled);
    QCOMPARE(AnalysisTabWiringTracker::changedEngineIndex, 2);
    QCOMPARE(AnalysisTabWiringTracker::changedEngineName, QStringLiteral("Engine-B"));
}

QTEST_MAIN(TestWiringAnalysisTab)

#include "tst_wiring_analysistab.moc"
