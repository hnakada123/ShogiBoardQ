/// @file tst_csagamesession.cpp
/// @brief CsaGameSession が通信対局の司令塔のシグナルを画面の部品へ届け、動作状態を切り替えることを確かめる

#include <QtTest>
#include <QLabel>
#include <QMessageBox>
#include <QTemporaryDir>
#include <QStatusBar>
#include "csagamecoordinator.h"
#include "csawaitingdialog.h"
#include "csagamesession.h"
#include "matchrecordsink.h"
#include "recordentry.h"
#include "notificationpresenter.h"
#include "uistatepolicymanager.h"

namespace {

/// 書かれた棋譜の行を数える受け手
} // namespace

class TestCsaGameSession : public QObject
{
    Q_OBJECT
    QTemporaryDir m_config;
private slots:
    void initTestCase()
    {
        QVERIFY(m_config.isValid());
        qputenv("SHOGIBOARDQ_CONFIG_HOME", m_config.path().toUtf8());
    }

    /// 司令塔のログはステータスバーに出る
    void logMessageReachesStatusBar()
    {
        CsaGameCoordinator coordinator;
        AppActivityState activity;
        QStatusBar status;
        CsaGameSession::Deps deps;
        deps.coordinator = &coordinator;
        deps.activity = &activity;
        deps.statusBar = &status;
        CsaGameSession session(deps);
        emit coordinator.logMessage(QStringLiteral("connected"), false);
        QCOMPARE(status.currentMessage(), QStringLiteral("connected"));
    }

    /// エラー・指し手の強調は中継せず、通知サービス・盤の操作へ1本ずつ直接つながる
    void coordinatorSignalsReachReceiversDirectly()
    {
        CsaGameCoordinator coordinator;
        AppActivityState activity;
        NotificationPresenter notifications({});
        CsaGameSession::Deps deps;
        deps.coordinator = &coordinator;
        deps.activity = &activity;
        deps.notifications = &notifications;
        CsaGameSession session(deps);

        const QMetaMethod error = QMetaMethod::fromSignal(&CsaGameCoordinator::errorOccurred);
        QVERIFY(!QObject::disconnect(&coordinator, error, &session, QMetaMethod()));        // 中継しない
        QVERIFY(QObject::disconnect(&coordinator, error, &notifications, QMetaMethod()));
        QVERIFY(!QObject::disconnect(&coordinator, error, &notifications, QMetaMethod()));  // 1本だけ
    }

    /// サーバーから受け取った行は HTML として描かない（リンク・ローカルファイルの画像を出させない。セキュリティ監査 S4）
    void serverTextIsShownAsPlainText()
    {
        CsaGameCoordinator coordinator;
        AppActivityState activity;
        NotificationPresenter notifications({});
        CsaGameSession::Deps deps;
        deps.coordinator = &coordinator;
        deps.activity = &activity;
        deps.notifications = &notifications;
        CsaGameSession session(deps);

        const QString line = QStringLiteral("REJECT:<a href=\"file:///tmp/x\">click</a>");
        Qt::TextFormat format = Qt::AutoText;
        QString shown;
        QTimer::singleShot(0, this, [&format, &shown]() {
            auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
            if (!box) return;
            format = box->textFormat();
            shown = box->text();
            box->accept();
        });
        emit coordinator.errorOccurred(line);
        QCOMPARE(format, Qt::PlainText);
        QCOMPARE(shown, line);

        CsaWaitingDialog waiting(nullptr);
        auto* detail = waiting.findChild<QLabel*>(QStringLiteral("csaWaitingDetail"));
        QVERIFY(detail);
        QCOMPARE(detail->textFormat(), Qt::PlainText);
    }

    /// 対局が始まると通信対局中にし、UI もそれに従う。棋譜（行の追記）は GameSession が受け、ここでは受けない
    void startAndEndSwitchActivity()
    {
        CsaGameCoordinator coordinator;
        AppActivityState activity;
        QStatusBar status;
        CsaGameSession::Deps deps;
        deps.coordinator = &coordinator;
        deps.activity = &activity;
        deps.statusBar = &status;
        CsaGameSession session(deps);
        UiStatePolicyManager::Deps policyDeps;
        policyDeps.activity = &activity;
        UiStatePolicyManager policy(policyDeps);

        const QMetaMethod record = QMetaMethod::fromSignal(&CsaGameCoordinator::recordEntryReady);
        QVERIFY(!QObject::disconnect(&coordinator, record, &session, QMetaMethod()));

        emit coordinator.gameStarted(QStringLiteral("Black"), QStringLiteral("White"), {RecordEntry{}});
        QCOMPARE(activity.activity(), AppActivityState::Activity::DuringCsaGame);
        QVERIFY(session.isActive());
        QCOMPARE(policy.currentState(), UiStatePolicyManager::Activity::DuringCsaGame);
        QVERIFY(status.currentMessage().contains(QStringLiteral("Black")));
    }
};
QTEST_MAIN(TestCsaGameSession)
#include "tst_csagamesession.moc"
