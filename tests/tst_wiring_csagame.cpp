#include <QtTest>
#include <QTemporaryDir>
#include <QStatusBar>
#include "csagamecoordinator.h"
#include "csagamewiring.h"
#include "uistatepolicymanager.h"

/// 実際の送受信オブジェクトで再配線・切断・状態遷移を検証する。
class TestWiringCsaGame : public QObject
{
    Q_OBJECT
    QTemporaryDir m_config;
private slots:
    void initTestCase()
    {
        QVERIFY(m_config.isValid());
        qputenv("SHOGIBOARDQ_CONFIG_HOME", m_config.path().toUtf8());
    }
    void repeatedWiringDeliversOnce()
    {
        CsaGameCoordinator coordinator;
        CsaGameWiring::Dependencies deps;
        deps.coordinator = &coordinator;
        CsaGameWiring wiring(deps);
        QSignalSpy errors(&wiring, &CsaGameWiring::errorMessageRequested);
        QSignalSpy moves(&wiring, &CsaGameWiring::moveAppliedToBoard);
        wiring.wire();
        wiring.wire();
        coordinator.errorOccurred(QStringLiteral("connection failed"));
        coordinator.moveAppliedToBoard();
        QCOMPARE(errors.count(), 1);
        QCOMPARE(errors.first().first().toString(), QStringLiteral("connection failed"));
        QCOMPARE(moves.count(), 1);
        wiring.unwire();
        coordinator.errorOccurred(QStringLiteral("disconnected"));
        coordinator.moveAppliedToBoard();
        QCOMPARE(errors.count(), 1);
        QCOMPARE(moves.count(), 1);
        wiring.wire();
        coordinator.moveAppliedToBoard();
        QCOMPARE(moves.count(), 2);
    }
    void replacingCoordinatorDisconnectsOldSender()
    {
        CsaGameCoordinator oldCoordinator, newCoordinator;
        CsaGameWiring::Dependencies deps;
        deps.coordinator = &oldCoordinator;
        CsaGameWiring wiring(deps);
        QSignalSpy moves(&wiring, &CsaGameWiring::moveAppliedToBoard);
        wiring.wire();
        wiring.setCoordinator(&newCoordinator);
        wiring.wire();
        oldCoordinator.moveAppliedToBoard();
        QCOMPARE(moves.count(), 0);
        newCoordinator.moveAppliedToBoard();
        QCOMPARE(moves.count(), 1);
    }
    void startUpdatesModeAndNavigationOnce()
    {
        CsaGameCoordinator coordinator;
        PlayMode mode = PlayMode::NotStarted;
        QStatusBar status;
        CsaGameWiring::Dependencies deps;
        deps.coordinator = &coordinator;
        deps.playMode = &mode;
        deps.statusBar = &status;
        CsaGameWiring wiring(deps);
        UiStatePolicyManager policy;
        wiring.wire();
        wiring.wireExternalSignals(&policy, nullptr, nullptr);
        wiring.wireExternalSignals(&policy, nullptr, nullptr);
        QSignalSpy navigation(&wiring, &CsaGameWiring::disableNavigationRequested);
        coordinator.gameStarted(QStringLiteral("Black"), QStringLiteral("White"), {});
        QCOMPARE(mode, PlayMode::CsaNetworkMode);
        QCOMPARE(policy.currentState(), UiStatePolicyManager::AppState::DuringCsaGame);
        QCOMPARE(navigation.count(), 1);
        QVERIFY(status.currentMessage().contains(QStringLiteral("Black")));
        wiring.enableNavigationRequested();
        QCOMPARE(policy.currentState(), UiStatePolicyManager::AppState::Idle);
    }
};
QTEST_MAIN(TestWiringCsaGame)
#include "tst_wiring_csagame.moc"
