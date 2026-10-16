/// @file tst_csagamesession.cpp
/// @brief CsaGameSession が通信対局の司令塔のシグナルを画面の部品へ届け、動作状態を切り替えることを確かめる

#include <QtTest>
#include <QTemporaryDir>
#include <QStatusBar>
#include "boardinteractioncontroller.h"
#include "csagamecoordinator.h"
#include "csagamesession.h"
#include "matchrecordsink.h"
#include "recordentry.h"
#include "uinotificationservice.h"
#include "uistatepolicymanager.h"

namespace {

/// 書かれた棋譜の行を数える受け手
class CountingSink final : public MatchRecordSink {
public:
    bool canAppendRecord() const override { return true; }
    void appendRecord(const RecordEntry&) override { ++appended; }
    bool canUndoRecords(int) const override { return false; }
    void undoRecords(int) override {}

    int appended = 0;
};

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
        CountingSink sink;
        QStatusBar status;
        CsaGameSession::Deps deps;
        deps.coordinator = &coordinator;
        deps.activity = &activity;
        deps.recordSink = &sink;
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
        CountingSink sink;
        BoardInteractionController board(nullptr, nullptr);
        UiNotificationService notifications({});
        CsaGameSession::Deps deps;
        deps.coordinator = &coordinator;
        deps.activity = &activity;
        deps.recordSink = &sink;
        deps.boardController = &board;
        deps.notifications = &notifications;
        CsaGameSession session(deps);

        const QMetaMethod error = QMetaMethod::fromSignal(&CsaGameCoordinator::errorOccurred);
        const QMetaMethod highlight = QMetaMethod::fromSignal(&CsaGameCoordinator::moveHighlightRequested);
        QVERIFY(!QObject::disconnect(&coordinator, error, &session, QMetaMethod()));        // 中継しない
        QVERIFY(QObject::disconnect(&coordinator, error, &notifications, QMetaMethod()));
        QVERIFY(!QObject::disconnect(&coordinator, error, &notifications, QMetaMethod()));  // 1本だけ
        QVERIFY(QObject::disconnect(&coordinator, highlight, &board, QMetaMethod()));
        QVERIFY(!QObject::disconnect(&coordinator, highlight, &board, QMetaMethod()));
    }

    /// 対局が始まると棋譜を作り直してから通信対局中にし、UI もそれに従う。終局で待機に戻る
    void startAndEndSwitchActivity()
    {
        CsaGameCoordinator coordinator;
        AppActivityState activity;
        CountingSink sink;
        QStatusBar status;
        int prepared = 0;
        CsaGameSession::Deps deps;
        deps.coordinator = &coordinator;
        deps.activity = &activity;
        deps.recordSink = &sink;
        deps.statusBar = &status;
        deps.prepareRecord = [&prepared, &activity](const QString&, const QString&, const QString&, const TimeControl&) {
            QCOMPARE(activity.activity(), AppActivityState::Activity::Idle);
            ++prepared;
        };
        CsaGameSession session(deps);
        UiStatePolicyManager::Deps policyDeps;
        policyDeps.activity = &activity;
        UiStatePolicyManager policy(policyDeps);

        emit coordinator.gameStarted(QStringLiteral("Black"), QStringLiteral("White"), {RecordEntry{}});
        QCOMPARE(activity.activity(), AppActivityState::Activity::DuringCsaGame);
        QVERIFY(session.isActive());
        QCOMPARE(policy.currentState(), UiStatePolicyManager::AppState::DuringCsaGame);
        QCOMPARE(prepared, 1);
        QCOMPARE(sink.appended, 1);  // 再開局面の既存手順
        QVERIFY(status.currentMessage().contains(QStringLiteral("Black")));
    }
};
QTEST_MAIN(TestCsaGameSession)
#include "tst_csagamesession.moc"
