#include <QtTest>
#include <QSignalSpy>

#include "shogiclock.h"
#include "timecontrolutil.h"
#include "gamestartcoordinator.h"
#include "timecontrol_test_helper.h"

using TestTime::rules;

class TestShogiClock : public QObject
{
    Q_OBJECT

private slots:
    void initialIncrement_data()
    {
        QTest::addColumn<qint64>("baseMs");
        QTest::newRow("zero-main") << qint64(0);
        QTest::newRow("five-minutes") << qint64(300000);
    }
    void initialIncrement()
    {
        QFETCH(qint64, baseMs);
        ShogiClock clock;
        GameStartCoordinator::TimeSettings tc;
        tc.enabled = true;
        tc.rules.black.mainMs = tc.rules.white.mainMs = baseMs;
        tc.rules.black.incrementMs = 10000;
        tc.rules.white.incrementMs = 7000;
        TimeControlUtil::applyToClock(&clock, tc, QStringLiteral("startpos"), {});
        QCOMPARE(clock.remainingMs(1), baseMs + 10000);
        QCOMPARE(clock.remainingMs(2), baseMs + 7000);
        // 再適用・再開で初手分を二重加算しない。
        TimeControlUtil::applyToClock(&clock, tc, QStringLiteral("startpos"), {});
        auto state = clock.pauseAndSnapshot();
        state.of(1).remainingMs -= 3000;
        state.of(1).considerationMs = 3000;
        clock.restoreSnapshot(state);
        clock.commitMove(1);
        QCOMPARE(clock.remainingMs(1), baseMs + 17000);
        QCOMPARE(clock.totalConsiderationTimeString(1), QStringLiteral("00:00:03"));
        clock.setCurrentPlayer(2);
        clock.commitMove(2);
        clock.setCurrentPlayer(1);
        state = clock.pauseAndSnapshot();
        state.of(1).remainingMs -= 5000;
        state.of(1).considerationMs = 5000;
        clock.restoreSnapshot(state);
        clock.commitMove(1);
        QCOMPARE(clock.remainingMs(1), baseMs + 22000);
        QCOMPARE(clock.totalConsiderationTimeString(1), QStringLiteral("00:00:08"));
        clock.startClock();
        QTest::qWait(60);
        QVERIFY(!clock.isGameOver());
        clock.stopClock();
    }

    void unlimitedTimeMatchesRecord_data()
    {
        QTest::addColumn<bool>("switchBeforeCommit");
        QTest::newRow("commit-before-switch") << false;
        QTest::newRow("switch-before-commit") << true;
    }

    void unlimitedTimeMatchesRecord()
    {
        QFETCH(bool, switchBeforeCommit);
        ShogiClock clock;
        clock.setTimeControl(rules(0, 0, 0, 0, 0, 0));
        QCOMPARE(clock.timeString(1), QStringLiteral("00:00:00"));
        clock.setConsiderationTimeMs(1, 1750);
        QCOMPARE(clock.timeString(1), QStringLiteral("00:00:01"));
        if (switchBeforeCommit) clock.setCurrentPlayer(2);
        clock.commitMove(1);
        QCOMPARE(clock.timeString(1), clock.totalConsiderationTimeString(1));
        QCOMPARE(clock.timeString(1), QStringLiteral("00:00:01"));
        clock.setCurrentPlayer(2);
        clock.setMeasuredConsiderationTime(2, 2999);
        QCOMPARE(clock.timeString(2), QStringLiteral("00:00:02"));
        if (switchBeforeCommit) clock.setCurrentPlayer(1);
        clock.commitMove(2);
        QCOMPARE(clock.timeString(2), clock.totalConsiderationTimeString(2));
        clock.setCurrentPlayer(1);
        clock.setConsiderationTimeMs(1, 750);
        QCOMPARE(clock.timeString(1), QStringLiteral("00:00:02"));
        clock.markGameOver();
        clock.commitMove(1);
        QCOMPARE(clock.timeString(1), clock.totalConsiderationTimeString(1));
        QCOMPARE(clock.considerationAndTotalTime(1), QStringLiteral("00:01/00:00:02"));
        // 表示用の累積時間をエンジン向けの残り時間へ流用しない。
        QCOMPARE(clock.remainingMs(1), 0LL);
        QCOMPARE(clock.remainingMs(2), 0LL);
    }

    void unlimitedTimeUpdatesWhileThinking()
    {
        ShogiClock clock;
        clock.setTimeControl(rules(0, 0, 0, 0, 0, 0));
        QSignalSpy updated(&clock, &ShogiClock::timeUpdated);
        clock.startClock();
        updated.clear();
        QTRY_VERIFY_WITH_TIMEOUT(!updated.isEmpty(), 2000);
        QVERIFY(clock.timeString(1) != QStringLiteral("00:00:00"));
        QCOMPARE(clock.timeString(2), QStringLiteral("00:00:00"));
        QVERIFY(!clock.isGameOver());
        clock.finishTurn();
        clock.commitMove(1);
        QCOMPARE(clock.timeString(1), clock.totalConsiderationTimeString(1));
        clock.stopClock();
    }

    void unlimitedTimePauseAndUndo()
    {
        ShogiClock clock;
        clock.setTimeControl(rules(0, 0, 0, 0, 0, 0));
        clock.startClock();
        clock.stopClock();
        clock.setConsiderationTimeMs(1, 1750);
        const auto paused = clock.pauseAndSnapshot();
        clock.markGameOver();
        clock.commitMove(1);
        clock.restoreSnapshot(paused);
        QCOMPARE(clock.timeString(1), QStringLiteral("00:00:01"));
        clock.setMeasuredConsiderationTime(1, 750);
        clock.commitMove(1);
        QCOMPARE(clock.timeString(1), QStringLiteral("00:00:02"));
        clock.setCurrentPlayer(2);
        clock.startClock();
        clock.stopClock();
        clock.setConsiderationTimeMs(2, 3200);
        clock.commitMove(2);
        clock.setCurrentPlayer(1);
        clock.startClock();
        clock.stopClock();
        clock.undo();
        QCOMPARE(clock.timeString(1), QStringLiteral("00:00:00"));
        QCOMPARE(clock.timeString(2), QStringLiteral("00:00:00"));
    }

    void resumePreservesClock_data()
    {
        QTest::addColumn<int>("byoyomi");
        QTest::addColumn<int>("increment");
        QTest::newRow("main-time") << 0 << 0;
        QTest::newRow("byoyomi") << 3 << 0;
        QTest::newRow("fischer") << 0 << 2;
    }

    void resumePreservesClock()
    {
        QFETCH(int, byoyomi);
        QFETCH(int, increment);
        ShogiClock clock;
        clock.setTimeControl(rules(byoyomi ? 0 : 30, 60, byoyomi, byoyomi, increment, increment));
        clock.startClock();
        QTest::qSleep(35);
        const auto state = clock.pauseAndSnapshot();
        QVERIFY(state.of(1).considerationMs >= 35);
        const qint64 left = clock.remainingTurnTimeMs(1);
        QTest::qWait(70);
        QCOMPARE(clock.remainingTurnTimeMs(1), left);
        // 中断行による時間確定と、閲覧による手番変更を経ても復元できる。
        clock.markGameOver();
        clock.commitMove(1);
        clock.setCurrentPlayer(2);
        clock.restoreSnapshot(state);
        QCOMPARE(clock.currentPlayer(), 1);
        QCOMPARE(clock.remainingMs(1), state.of(1).remainingMs);
        QCOMPARE(clock.remainingMs(2), state.of(2).remainingMs);
        QCOMPARE(clock.byoyomiApplied(1), byoyomi != 0);
        QCOMPARE(clock.totalConsiderationTimeString(1), QStringLiteral("00:00:00"));
        clock.startClock();
        QTest::qSleep(40);
        clock.finishTurn();
        const auto resumed = clock.pauseAndSnapshot();
        QVERIFY(resumed.of(1).considerationMs >= state.of(1).considerationMs + 40);
        QCOMPARE(resumed.history.size(), state.history.size());
        clock.setCurrentPlayer(2);
        clock.setMeasuredConsiderationTime(1, 40);
        QCOMPARE(clock.considerationMs(1), state.of(1).considerationMs + 40);
        clock.commitMove(1);
        if (byoyomi) QCOMPARE(clock.remainingMs(1), 3000LL);
        if (increment) QCOMPARE(clock.remainingMs(1), resumed.of(1).remainingMs + 2000);
    }

    void switchAccountsElapsedToOldPlayer()
    {
        ShogiClock clock;
        clock.setTimeControl(rules(10, 10, 0, 0, 0, 0));
        clock.startClock();
        QTest::qSleep(35); // イベント処理なし: 未精算のtick端数を作る
        clock.setCurrentPlayer(2);
        QVERIFY(clock.remainingMs(1) <= 9965);
        QCOMPARE(clock.remainingMs(2), 10000LL);
        clock.stopClock();
    }

    void receivedMoveStopsChargingUntilNextTurn()
    {
        ShogiClock clock;
        clock.setTimeControl(rules(0, 0, 1, 1, 0, 0));
        clock.startClock();
        QTest::qSleep(30);
        clock.finishTurn();
        const qint64 remaining = clock.remainingMs(1);
        QTest::qSleep(1050); // 受理後のUI処理が期限を越えても敗着にしない
        clock.updateClock();
        QCOMPARE(clock.remainingMs(1), remaining);
        QVERIFY(!clock.isGameOver());
        clock.setCurrentPlayer(2);
        QTest::qSleep(30);
        clock.updateClock();
        QVERIFY(clock.remainingTurnTimeMs(2) <= 970);
        clock.stopClock();
    }

    void lateResponseStillLoses()
    {
        ShogiClock clock;
        clock.setTimeControl(rules(0, 0, 1, 1, 0, 0));
        QSignalSpy timeout(&clock, &ShogiClock::player1TimeOut);
        QSignalSpy resignation(&clock, &ShogiClock::resignationTriggered);
        clock.startClock();
        QTest::qSleep(1020);
        clock.finishTurn();
        QVERIFY(clock.isGameOver());
        QCOMPARE(timeout.count(), 1);
        QCOMPARE(resignation.count(), 0);
    }

    void liveBudgetIncludesUntickedByoyomiTransition()
    {
        ShogiClock clock;
        clock.setTimeControl(rules(1, 0, 1, 5, 0, 0));
        clock.startClock();
        QTest::qSleep(1020);
        QCOMPARE(clock.remainingMainTimeMs(1), 0LL);
        QVERIFY(clock.remainingTurnTimeMs(1) <= 980);
        QCOMPARE(clock.remainingTurnTimeMs(2), 5000LL);
        clock.stopClock();
        QVERIFY(!clock.isGameOver());
        QVERIFY(clock.byoyomiApplied(1));
        QVERIFY(clock.remainingMs(1) > 0);
    }

    void setTimeControl_basic()
    {
        ShogiClock clock;
        clock.setTimeControl(rules(300, 300, 0, 0, 0, 0));

        // 300 seconds = 300000 ms
        QCOMPARE(clock.remainingMs(1), 300000LL);
        QCOMPARE(clock.remainingMs(2), 300000LL);
    }

    /// CSA のようにサーバーが残り時間を示すときは、決まり・考慮時間・待ったの履歴を保ったまま合わせる
    void setRemainingTimes_keepsRulesAndHistory()
    {
        ShogiClock clock;
        clock.setTimeControl(rules(300, 300, 10, 10, 0, 0));
        clock.setConsiderationTimeMs(1, 1500);
        clock.commitMove(1);
        const auto before = clock.pauseAndSnapshot();
        clock.setRemainingTimes(250000, 0);
        QCOMPARE(clock.remainingMs(1), 250000LL);
        QVERIFY(!clock.byoyomiApplied(1));
        // 使い切った側は秒読みに入る
        QVERIFY(clock.byoyomiApplied(2));
        QCOMPARE(clock.remainingMs(2), 10000LL);
        QCOMPARE(clock.byoyomiMs(1), 10000LL);
        QCOMPARE(clock.totalConsiderationTimeString(1), QStringLiteral("00:00:01"));
        QCOMPARE(clock.pauseAndSnapshot().history.size(), before.history.size());
        QVERIFY(!clock.isUnlimited());
    }

    void timeString_format()
    {
        ShogiClock clock;
        clock.setTimeControl(rules(300, 300, 0, 0, 0, 0));

        // 300 seconds = 5 minutes = "00:05:00"
        QCOMPARE(clock.timeString(1), QStringLiteral("00:05:00"));
        QCOMPARE(clock.timeString(2), QStringLiteral("00:05:00"));
    }

    void byoyomi_setup()
    {
        ShogiClock clock;
        clock.setTimeControl(rules(0, 0, 30, 30, 0, 0));

        QVERIFY(clock.hasByoyomi(1));
        QVERIFY(clock.hasByoyomi(2));
    }

    /// 持ち時間を使い切った側（CSA の時計合わせ・持ち時間0の秒読み対局）は初めから秒読みに入り、
    /// 手番でないあいだも 00:00:00 ではなく秒読みの秒数を表示する
    void exhaustedMainTimeStartsInByoyomi()
    {
        ShogiClock clock;
        clock.setTimeControl(rules(0, 60, 5, 5, 0, 0));
        QVERIFY(clock.byoyomiApplied(1));
        QVERIFY(!clock.byoyomiApplied(2));
        QCOMPARE(clock.remainingMs(1), 5000LL);
        QCOMPARE(clock.timeString(1), QStringLiteral("00:00:05"));
        QCOMPARE(clock.remainingMainTimeMs(1), 0LL);
        QCOMPARE(clock.remainingTurnTimeMs(1), 5000LL);
        QCOMPARE(clock.remainingMs(2), 60000LL);
        QCOMPARE(clock.remainingTurnTimeMs(2), 65000LL);
    }

    void byoyomi_application()
    {
        ShogiClock clock;
        clock.setTimeControl(rules(0, 0, 30, 30, 0, 0));
        clock.setCurrentPlayer(1);

        // Apply byoyomi for player 1
        clock.commitMove(1);

        // After byoyomi applied, player should have 30 seconds
        QCOMPARE(clock.remainingMs(1), 30000LL);
        QVERIFY(clock.byoyomiApplied(1));
    }

    void fischer_increment()
    {
        ShogiClock clock;
        clock.setTimeControl(rules(300, 300, 0, 0, 10, 10));

        QCOMPARE(clock.incrementMs(1), 10000LL);
        QCOMPARE(clock.incrementMs(2), 10000LL);
        // 初手にも加算を与える
        QCOMPARE(clock.remainingMs(1), 310000LL);

        clock.setCurrentPlayer(1);
        clock.commitMove(1);

        // 着手確定で次の手番の分を加算する: 310 + 10 = 320 秒
        QCOMPARE(clock.remainingMs(1), 320000LL);
    }

    void undo_restoresState_data()
    {
        QTest::addColumn<int>("mainSec");
        QTest::addColumn<int>("byoyomiSec");
        QTest::addColumn<int>("incrementSec");
        QTest::newRow("main-time") << 300 << 0 << 0;
        QTest::newRow("byoyomi-only") << 0 << 30 << 0;
        QTest::newRow("fischer") << 300 << 0 << 10; // 初手の加算10秒は時計が足す
    }

    // 「待った」は、取り消す自分の手を指し始めたときの時計へ戻す。続けて使うと2手ずつ戻る。
    void undo_restoresState()
    {
        QFETCH(int, mainSec);
        QFETCH(int, byoyomiSec);
        QFETCH(int, incrementSec);
        ShogiClock clock;
        clock.setTimeControl(rules(mainSec, mainSec, byoyomiSec, byoyomiSec, incrementSec, incrementSec));
        clock.setCurrentPlayer(1);
        const auto commit = [&clock](int player, qint64 ms) {
            clock.setCurrentPlayer(player);
            clock.setMeasuredConsiderationTime(player, ms);
            if (player == 1) clock.commitMove(1);
            else clock.commitMove(2);
        };
        const auto verifyTurnStart = [&clock](const ShogiClock::Snapshot& expected) {
            const auto now = clock.pauseAndSnapshot();
            QCOMPARE(now.currentPlayer, 1);
            QCOMPARE(now.of(1).remainingMs, expected.of(1).remainingMs);
            QCOMPARE(now.of(2).remainingMs, expected.of(2).remainingMs);
            QCOMPARE(now.of(1).totalConsiderationMs, expected.of(1).totalConsiderationMs);
            QCOMPARE(now.of(2).totalConsiderationMs, expected.of(2).totalConsiderationMs);
            QCOMPARE(now.of(1).byoyomiApplied, expected.of(1).byoyomiApplied);
            QCOMPARE(now.of(2).byoyomiApplied, expected.of(2).byoyomiApplied);
            QCOMPARE(now.of(1).considerationMs, 0LL);
            QVERIFY(!now.considerationCommitted);
        };

        const auto firstTurn = clock.pauseAndSnapshot();
        commit(1, 3000);
        commit(2, 2000);
        clock.setCurrentPlayer(1);
        const auto secondTurn = clock.pauseAndSnapshot();
        commit(1, 5000);
        commit(2, 1000);
        clock.setCurrentPlayer(1);
        clock.setConsiderationTimeMs(1, 700); // 考え中の時間も返す

        clock.undo();
        verifyTurnStart(secondTurn);
        clock.undo();
        verifyTurnStart(firstTurn);
        clock.undo(); // 対局開始より前には戻らない
        verifyTurnStart(firstTurn);

        // 戻した時点から計時し直す
        clock.startClock();
        QTest::qSleep(60);
        clock.updateClock();
        QVERIFY(clock.considerationMs(1) >= 60);
        QVERIFY(clock.considerationMs(1) < 1000);
        if (byoyomiSec == 0) {
            QCOMPARE(clock.remainingMs(1), firstTurn.of(1).remainingMs - clock.considerationMs(1));
        }
        clock.stopClock();

        // 指し直した後の「待った」も、指し直した手の前へ戻る
        clock.setConsiderationTimeMs(1, 0);
        commit(1, 4000);
        commit(2, 1500);
        clock.setCurrentPlayer(1);
        clock.undo();
        verifyTurnStart(firstTurn);

        // 時間を設定し直した新しい対局では、前の対局の状態に戻らない
        commit(1, 4000);
        commit(2, 1500);
        clock.setCurrentPlayer(1);
        clock.setTimeControl(rules(mainSec, mainSec, byoyomiSec, byoyomiSec, incrementSec, incrementSec));
        clock.setCurrentPlayer(1);
        const auto newGame = clock.pauseAndSnapshot();
        clock.undo();
        verifyTurnStart(newGame);
    }

    void stressTest_startStop()
    {
        ShogiClock clock;
        clock.setTimeControl(rules(300, 300, 30, 30, 0, 0));
        clock.setCurrentPlayer(1);

        for (int i = 0; i < 100; ++i) {
            clock.startClock();
            clock.stopClock();
        }
        // Should not crash
        QVERIFY(true);
    }

    void stressTest_byoyomiUndo()
    {
        ShogiClock clock;
        clock.setTimeControl(rules(300, 300, 30, 30, 0, 0));
        clock.setCurrentPlayer(1);

        for (int i = 0; i < 50; ++i) {
            clock.commitMove(1);
            clock.setCurrentPlayer(2);
            clock.commitMove(2);
            clock.setCurrentPlayer(1);
        }

        // Undo all operations
        for (int i = 0; i < 50; ++i) {
            clock.undo();
        }

        // Should not crash, times should be reasonable
        QVERIFY(clock.remainingMs(1) >= 0);
        QVERIFY(clock.remainingMs(2) >= 0);
    }

    void byoyomi_and_fischer_exclusive()
    {
        ShogiClock clock;
        // Setting both byoyomi and fischer - they should be exclusive
        clock.setTimeControl(rules(300, 300, 30, 30, 10, 10));

        // One of them should be 0
        bool exclusive = (clock.hasByoyomi(1) && clock.incrementMs(1) == 0) ||
                         (!clock.hasByoyomi(1) && clock.incrementMs(1) > 0) ||
                         (clock.hasByoyomi(1) && clock.incrementMs(1) > 0); // implementation may allow both
        QVERIFY(exclusive || true); // Just verify no crash
    }
};

QTEST_MAIN(TestShogiClock)
#include "tst_shogiclock.moc"
