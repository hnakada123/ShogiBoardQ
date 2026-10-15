#include <QtTest>
#include <QTemporaryDir>
#include <QStatusBar>
#include "boardinteractioncontroller.h"
#include "csagamecoordinator.h"
#include "csagamewiring.h"
#include "uinotificationservice.h"
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

    /// 何度配線しても1回だけ届き、外せば届かない
    void repeatedWiringDeliversOnce()
    {
        CsaGameCoordinator coordinator;
        QStatusBar status;
        CsaGameWiring::Dependencies deps;
        deps.coordinator = &coordinator;
        deps.statusBar = &status;
        CsaGameWiring wiring(deps);
        QSignalSpy moves(&wiring, &CsaGameWiring::moveAppliedToBoard);
        wiring.wire();
        wiring.wire();
        emit coordinator.logMessage(QStringLiteral("connected"), false);
        emit coordinator.moveAppliedToBoard();
        QCOMPARE(status.currentMessage(), QStringLiteral("connected"));
        QCOMPARE(moves.count(), 1);
        wiring.unwire();
        emit coordinator.logMessage(QStringLiteral("disconnected"), false);
        emit coordinator.moveAppliedToBoard();
        QCOMPARE(status.currentMessage(), QStringLiteral("connected"));
        QCOMPARE(moves.count(), 1);
        wiring.wire();
        emit coordinator.moveAppliedToBoard();
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
        emit oldCoordinator.moveAppliedToBoard();
        QCOMPARE(moves.count(), 0);
        emit newCoordinator.moveAppliedToBoard();
        QCOMPARE(moves.count(), 1);
    }

    /// エラー・指し手ハイライトは中継せず、通知サービス・盤の操作へ1本ずつ直接つながる
    void coordinatorSignalsReachReceiversDirectly()
    {
        CsaGameCoordinator coordinator;
        BoardInteractionController board(nullptr, nullptr);
        UiNotificationService notifications;
        CsaGameWiring::Dependencies deps;
        deps.coordinator = &coordinator;
        deps.boardController = &board;
        CsaGameWiring wiring(deps);
        wiring.wire();
        wiring.wireExternalSignals(nullptr, nullptr, &notifications);
        wiring.wireExternalSignals(nullptr, nullptr, &notifications);

        const QMetaMethod error = QMetaMethod::fromSignal(&CsaGameCoordinator::errorOccurred);
        const QMetaMethod highlight = QMetaMethod::fromSignal(&CsaGameCoordinator::moveHighlightRequested);
        QVERIFY(!QObject::disconnect(&coordinator, error, &wiring, QMetaMethod()));        // 中継しない
        QVERIFY(QObject::disconnect(&coordinator, error, &notifications, QMetaMethod()));
        QVERIFY(!QObject::disconnect(&coordinator, error, &notifications, QMetaMethod()));  // 1本だけ
        QVERIFY(QObject::disconnect(&coordinator, highlight, &board, QMetaMethod()));
        QVERIFY(!QObject::disconnect(&coordinator, highlight, &board, QMetaMethod()));
    }

    void startUpdatesModeAndNavigationOnce()
    {
        CsaGameCoordinator coordinator;
        PlayMode mode = PlayMode::NotStarted;
        QStatusBar status;
        int prepared = 0;
        CsaGameWiring::Dependencies deps;
        deps.coordinator = &coordinator;
        deps.playMode = &mode;
        deps.changePlayMode = [&mode, &prepared](PlayMode value) {
            QCOMPARE(prepared, 1);
            mode = value;
        };
        deps.statusBar = &status;
        deps.prepareRecord = [&prepared, &mode](const QString&, const QString&, const QString&) {
            QCOMPARE(mode, PlayMode::NotStarted);
            ++prepared;
        };
        CsaGameWiring wiring(deps);
        UiStatePolicyManager policy;
        wiring.wire();
        wiring.wireExternalSignals(&policy, nullptr, nullptr);
        wiring.wireExternalSignals(&policy, nullptr, nullptr);
        emit coordinator.gameStarted(QStringLiteral("Black"), QStringLiteral("White"), {});
        QCOMPARE(mode, PlayMode::CsaNetworkMode);
        QCOMPARE(policy.currentState(), UiStatePolicyManager::AppState::DuringCsaGame);
        QCOMPARE(prepared, 1);
        QVERIFY(status.currentMessage().contains(QStringLiteral("Black")));
    }
};
QTEST_MAIN(TestWiringCsaGame)
#include "tst_wiring_csagame.moc"
