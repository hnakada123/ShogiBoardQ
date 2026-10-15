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
        const QMetaMethod log = QMetaMethod::fromSignal(&CsaGameCoordinator::logMessage);
        wiring.wire();
        wiring.wire();
        emit coordinator.logMessage(QStringLiteral("connected"), false);
        QCOMPARE(status.currentMessage(), QStringLiteral("connected"));
        wiring.unwire();
        emit coordinator.logMessage(QStringLiteral("disconnected"), false);
        QCOMPARE(status.currentMessage(), QStringLiteral("connected"));
        wiring.wire();
        QVERIFY(QObject::disconnect(&coordinator, log, &wiring, QMetaMethod()));
        QVERIFY(!QObject::disconnect(&coordinator, log, &wiring, QMetaMethod()));  // 1本だけ
    }

    void replacingCoordinatorDisconnectsOldSender()
    {
        CsaGameCoordinator oldCoordinator, newCoordinator;
        QStatusBar status;
        CsaGameWiring::Dependencies deps;
        deps.coordinator = &oldCoordinator;
        deps.statusBar = &status;
        CsaGameWiring wiring(deps);
        wiring.wire();
        wiring.setCoordinator(&newCoordinator);
        wiring.wire();
        emit oldCoordinator.logMessage(QStringLiteral("old"), false);
        QVERIFY(status.currentMessage().isEmpty());
        emit newCoordinator.logMessage(QStringLiteral("new"), false);
        QCOMPARE(status.currentMessage(), QStringLiteral("new"));
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
        AppActivityState activity;
        QStatusBar status;
        int prepared = 0;
        CsaGameWiring::Dependencies deps;
        deps.coordinator = &coordinator;
        deps.activity = &activity;
        deps.statusBar = &status;
        deps.prepareRecord = [&prepared, &activity](const QString&, const QString&, const QString&) {
            // 棋譜を作り直してから通信対局中にする
            QCOMPARE(activity.activity(), AppActivityState::Activity::Idle);
            ++prepared;
        };
        CsaGameWiring wiring(deps);
        UiStatePolicyManager policy;
        wiring.wire();
        wiring.wireExternalSignals(&policy, nullptr, nullptr);
        wiring.wireExternalSignals(&policy, nullptr, nullptr);
        emit coordinator.gameStarted(QStringLiteral("Black"), QStringLiteral("White"), {});
        QCOMPARE(activity.activity(), AppActivityState::Activity::DuringCsaGame);
        QCOMPARE(policy.currentState(), UiStatePolicyManager::AppState::DuringCsaGame);
        QCOMPARE(prepared, 1);
        QVERIFY(status.currentMessage().contains(QStringLiteral("Black")));
    }
};
QTEST_MAIN(TestWiringCsaGame)
#include "tst_wiring_csagame.moc"
