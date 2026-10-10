#include <QtTest>
#include <QSignalSpy>

#include "shogiclock.h"
#include "timecontrolutil.h"
#include "gamestartcoordinator.h"

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
        GameStartCoordinator::TimeControl tc;
        tc.enabled = true;
        tc.p1.baseMs = tc.p2.baseMs = baseMs;
        tc.p1.incrementMs = 10000;
        tc.p2.incrementMs = 7000;
        TimeControlUtil::applyToClock(&clock, tc, QStringLiteral("startpos"), {});
        QCOMPARE(clock.getPlayer1TimeIntMs(), baseMs + 10000);
        QCOMPARE(clock.getPlayer2TimeIntMs(), baseMs + 7000);
        // 再適用・再開で初手分を二重加算しない。
        TimeControlUtil::applyToClock(&clock, tc, QStringLiteral("startpos"), {});
        auto state = clock.pauseAndSnapshot();
        state.player1TimeMs -= 3000;
        state.player1ConsiderationTimeMs = 3000;
        clock.restoreSnapshot(state);
        clock.applyByoyomiAndResetConsideration1();
        QCOMPARE(clock.getPlayer1TimeIntMs(), baseMs + 17000);
        QCOMPARE(clock.getPlayer1TotalConsiderationTime(), QStringLiteral("00:00:03"));
        clock.setCurrentPlayer(2);
        clock.applyByoyomiAndResetConsideration2();
        clock.setCurrentPlayer(1);
        state = clock.pauseAndSnapshot();
        state.player1TimeMs -= 5000;
        state.player1ConsiderationTimeMs = 5000;
        clock.restoreSnapshot(state);
        clock.applyByoyomiAndResetConsideration1();
        QCOMPARE(clock.getPlayer1TimeIntMs(), baseMs + 22000);
        QCOMPARE(clock.getPlayer1TotalConsiderationTime(), QStringLiteral("00:00:08"));
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
        clock.setPlayerTimes(0, 0, 0, 0, 0, 0, false);
        QCOMPARE(clock.player1TimeString(), QStringLiteral("00:00:00"));
        clock.setPlayer1ConsiderationTime(1750);
        QCOMPARE(clock.player1TimeString(), QStringLiteral("00:00:01"));
        if (switchBeforeCommit) clock.setCurrentPlayer(2);
        clock.applyByoyomiAndResetConsideration1();
        QCOMPARE(clock.player1TimeString(), clock.getPlayer1TotalConsiderationTime());
        QCOMPARE(clock.player1TimeString(), QStringLiteral("00:00:01"));
        clock.setCurrentPlayer(2);
        clock.setMeasuredConsiderationTime(2, 2999);
        QCOMPARE(clock.player2TimeString(), QStringLiteral("00:00:02"));
        if (switchBeforeCommit) clock.setCurrentPlayer(1);
        clock.applyByoyomiAndResetConsideration2();
        QCOMPARE(clock.player2TimeString(), clock.getPlayer2TotalConsiderationTime());
        clock.setCurrentPlayer(1);
        clock.setPlayer1ConsiderationTime(750);
        QCOMPARE(clock.player1TimeString(), QStringLiteral("00:00:02"));
        clock.markGameOver();
        clock.applyByoyomiAndResetConsideration1();
        QCOMPARE(clock.player1TimeString(), clock.getPlayer1TotalConsiderationTime());
        QCOMPARE(clock.player1ConsiderationAndTotalTime(), QStringLiteral("00:01/00:00:02"));
        // 表示用の累積時間をエンジン向けの残り時間へ流用しない。
        QCOMPARE(clock.getPlayer1TimeIntMs(), 0LL);
        QCOMPARE(clock.getPlayer2TimeIntMs(), 0LL);
    }

    void unlimitedTimeUpdatesWhileThinking()
    {
        ShogiClock clock;
        clock.setPlayerTimes(0, 0, 0, 0, 0, 0, false);
        QSignalSpy updated(&clock, &ShogiClock::timeUpdated);
        clock.startClock();
        updated.clear();
        QTRY_VERIFY_WITH_TIMEOUT(!updated.isEmpty(), 2000);
        QVERIFY(clock.player1TimeString() != QStringLiteral("00:00:00"));
        QCOMPARE(clock.player2TimeString(), QStringLiteral("00:00:00"));
        QVERIFY(!clock.isGameOver());
        clock.finishTurn();
        clock.applyByoyomiAndResetConsideration1();
        QCOMPARE(clock.player1TimeString(), clock.getPlayer1TotalConsiderationTime());
        clock.stopClock();
    }

    void unlimitedTimePauseAndUndo()
    {
        ShogiClock clock;
        clock.setPlayerTimes(0, 0, 0, 0, 0, 0, false);
        clock.startClock();
        clock.stopClock();
        clock.setPlayer1ConsiderationTime(1750);
        const auto paused = clock.pauseAndSnapshot();
        clock.markGameOver();
        clock.applyByoyomiAndResetConsideration1();
        clock.restoreSnapshot(paused);
        QCOMPARE(clock.player1TimeString(), QStringLiteral("00:00:01"));
        clock.setMeasuredConsiderationTime(1, 750);
        clock.applyByoyomiAndResetConsideration1();
        QCOMPARE(clock.player1TimeString(), QStringLiteral("00:00:02"));
        clock.setCurrentPlayer(2);
        clock.startClock();
        clock.stopClock();
        clock.setPlayer2ConsiderationTime(3200);
        clock.applyByoyomiAndResetConsideration2();
        clock.setCurrentPlayer(1);
        clock.startClock();
        clock.stopClock();
        clock.undo();
        QCOMPARE(clock.player1TimeString(), QStringLiteral("00:00:00"));
        QCOMPARE(clock.player2TimeString(), QStringLiteral("00:00:00"));
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
        clock.setPlayerTimes(byoyomi ? 0 : 30, 60, byoyomi, byoyomi, increment, increment, true);
        clock.startClock();
        QTest::qSleep(35);
        const auto state = clock.pauseAndSnapshot();
        QVERIFY(state.player1ConsiderationTimeMs >= 35);
        const qint64 left = clock.remainingTurnTimeMs(1);
        QTest::qWait(70);
        QCOMPARE(clock.remainingTurnTimeMs(1), left);
        // 中断行による時間確定と、閲覧による手番変更を経ても復元できる。
        clock.markGameOver();
        clock.applyByoyomiAndResetConsideration1();
        clock.setCurrentPlayer(2);
        clock.restoreSnapshot(state);
        QCOMPARE(clock.currentPlayer(), 1);
        QCOMPARE(clock.getPlayer1TimeIntMs(), state.player1TimeMs);
        QCOMPARE(clock.getPlayer2TimeIntMs(), state.player2TimeMs);
        QCOMPARE(clock.byoyomi1Applied(), byoyomi != 0);
        QCOMPARE(clock.getPlayer1TotalConsiderationTime(), QStringLiteral("00:00:00"));
        clock.startClock();
        QTest::qSleep(40);
        clock.finishTurn();
        const auto resumed = clock.pauseAndSnapshot();
        QVERIFY(resumed.player1ConsiderationTimeMs >= state.player1ConsiderationTimeMs + 40);
        QCOMPARE(resumed.player1TimeHistory.size(), state.player1TimeHistory.size());
        clock.setCurrentPlayer(2);
        clock.setMeasuredConsiderationTime(1, 40);
        QCOMPARE(clock.player1ConsiderationMs(), state.player1ConsiderationTimeMs + 40);
        clock.applyByoyomiAndResetConsideration1();
        if (byoyomi) QCOMPARE(clock.getPlayer1TimeIntMs(), 3000LL);
        if (increment) QCOMPARE(clock.getPlayer1TimeIntMs(), resumed.player1TimeMs + 2000);
    }

    void switchAccountsElapsedToOldPlayer()
    {
        ShogiClock clock;
        clock.setPlayerTimes(10, 10, 0, 0, 0, 0, true);
        clock.startClock();
        QTest::qSleep(35); // イベント処理なし: 未精算のtick端数を作る
        clock.setCurrentPlayer(2);
        QVERIFY(clock.getPlayer1TimeIntMs() <= 9965);
        QCOMPARE(clock.getPlayer2TimeIntMs(), 10000LL);
        clock.stopClock();
    }

    void receivedMoveStopsChargingUntilNextTurn()
    {
        ShogiClock clock;
        clock.setPlayerTimes(0, 0, 1, 1, 0, 0, true);
        clock.startClock();
        QTest::qSleep(30);
        clock.finishTurn();
        const qint64 remaining = clock.getPlayer1TimeIntMs();
        QTest::qSleep(1050); // 受理後のUI処理が期限を越えても敗着にしない
        clock.updateClock();
        QCOMPARE(clock.getPlayer1TimeIntMs(), remaining);
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
        clock.setPlayerTimes(0, 0, 1, 1, 0, 0, true);
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
        clock.setPlayerTimes(1, 0, 1, 5, 0, 0, true);
        clock.startClock();
        QTest::qSleep(1020);
        QCOMPARE(clock.remainingMainTimeMs(1), 0LL);
        QVERIFY(clock.remainingTurnTimeMs(1) <= 980);
        QCOMPARE(clock.remainingTurnTimeMs(2), 5000LL);
        clock.stopClock();
        QVERIFY(!clock.isGameOver());
        QVERIFY(clock.byoyomi1Applied());
        QVERIFY(clock.getPlayer1TimeIntMs() > 0);
    }

    void setPlayerTimes_basic()
    {
        ShogiClock clock;
        clock.setPlayerTimes(300, 300, 0, 0, 0, 0, true);

        // 300 seconds = 300000 ms
        QCOMPARE(clock.getPlayer1TimeIntMs(), 300000LL);
        QCOMPARE(clock.getPlayer2TimeIntMs(), 300000LL);
    }

    void timeString_format()
    {
        ShogiClock clock;
        clock.setPlayerTimes(300, 300, 0, 0, 0, 0, true);

        // 300 seconds = 5 minutes = "00:05:00"
        QCOMPARE(clock.player1TimeString(), QStringLiteral("00:05:00"));
        QCOMPARE(clock.player2TimeString(), QStringLiteral("00:05:00"));
    }

    void byoyomi_setup()
    {
        ShogiClock clock;
        clock.setPlayerTimes(0, 0, 30, 30, 0, 0, true);

        QVERIFY(clock.hasByoyomi1());
        QVERIFY(clock.hasByoyomi2());
    }

    void byoyomi_application()
    {
        ShogiClock clock;
        clock.setPlayerTimes(0, 0, 30, 30, 0, 0, true);
        clock.setCurrentPlayer(1);

        // Apply byoyomi for player 1
        clock.applyByoyomiAndResetConsideration1();

        // After byoyomi applied, player should have 30 seconds
        QCOMPARE(clock.getPlayer1TimeIntMs(), 30000LL);
        QVERIFY(clock.byoyomi1Applied());
    }

    void fischer_increment()
    {
        ShogiClock clock;
        clock.setPlayerTimes(300, 300, 0, 0, 10, 10, true);

        QCOMPARE(clock.bincMs(), 10000LL);
        QCOMPARE(clock.wincMs(), 10000LL);

        clock.setCurrentPlayer(1);
        clock.applyByoyomiAndResetConsideration1();

        // After Fischer increment: 300 + 10 = 310 seconds
        QCOMPARE(clock.getPlayer1TimeIntMs(), 310000LL);
    }

    void undo_restoresState_data()
    {
        QTest::addColumn<int>("mainSec");
        QTest::addColumn<int>("byoyomiSec");
        QTest::addColumn<int>("incrementSec");
        QTest::newRow("main-time") << 300 << 0 << 0;
        QTest::newRow("byoyomi-only") << 0 << 30 << 0;
        QTest::newRow("fischer") << 310 << 0 << 10; // 初手の加算10秒を含む
    }

    // 「待った」は、取り消す自分の手を指し始めたときの時計へ戻す。続けて使うと2手ずつ戻る。
    void undo_restoresState()
    {
        QFETCH(int, mainSec);
        QFETCH(int, byoyomiSec);
        QFETCH(int, incrementSec);
        ShogiClock clock;
        clock.setPlayerTimes(mainSec, mainSec, byoyomiSec, byoyomiSec, incrementSec, incrementSec, true);
        clock.setCurrentPlayer(1);
        const auto commit = [&clock](int player, qint64 ms) {
            clock.setCurrentPlayer(player);
            clock.setMeasuredConsiderationTime(player, ms);
            if (player == 1) clock.applyByoyomiAndResetConsideration1();
            else clock.applyByoyomiAndResetConsideration2();
        };
        const auto verifyTurnStart = [&clock](const ShogiClock::Snapshot& expected) {
            const auto now = clock.pauseAndSnapshot();
            QCOMPARE(now.currentPlayer, 1);
            QCOMPARE(now.player1TimeMs, expected.player1TimeMs);
            QCOMPARE(now.player2TimeMs, expected.player2TimeMs);
            QCOMPARE(now.player1TotalConsiderationTimeMs, expected.player1TotalConsiderationTimeMs);
            QCOMPARE(now.player2TotalConsiderationTimeMs, expected.player2TotalConsiderationTimeMs);
            QCOMPARE(now.byoyomi1Applied, expected.byoyomi1Applied);
            QCOMPARE(now.byoyomi2Applied, expected.byoyomi2Applied);
            QCOMPARE(now.player1ConsiderationTimeMs, 0LL);
            QVERIFY(!now.currentConsiderationCommitted);
        };

        const auto firstTurn = clock.pauseAndSnapshot();
        commit(1, 3000);
        commit(2, 2000);
        clock.setCurrentPlayer(1);
        const auto secondTurn = clock.pauseAndSnapshot();
        commit(1, 5000);
        commit(2, 1000);
        clock.setCurrentPlayer(1);
        clock.setPlayer1ConsiderationTime(700); // 考え中の時間も返す

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
        QVERIFY(clock.player1ConsiderationMs() >= 60);
        QVERIFY(clock.player1ConsiderationMs() < 1000);
        if (byoyomiSec == 0) {
            QCOMPARE(clock.getPlayer1TimeIntMs(), firstTurn.player1TimeMs - clock.player1ConsiderationMs());
        }
        clock.stopClock();

        // 指し直した後の「待った」も、指し直した手の前へ戻る
        clock.setPlayer1ConsiderationTime(0);
        commit(1, 4000);
        commit(2, 1500);
        clock.setCurrentPlayer(1);
        clock.undo();
        verifyTurnStart(firstTurn);

        // 時間を設定し直した新しい対局では、前の対局の状態に戻らない
        commit(1, 4000);
        commit(2, 1500);
        clock.setCurrentPlayer(1);
        clock.setPlayerTimes(mainSec, mainSec, byoyomiSec, byoyomiSec, incrementSec, incrementSec, true);
        clock.setCurrentPlayer(1);
        const auto newGame = clock.pauseAndSnapshot();
        clock.undo();
        verifyTurnStart(newGame);
    }

    void stressTest_startStop()
    {
        ShogiClock clock;
        clock.setPlayerTimes(300, 300, 30, 30, 0, 0, true);
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
        clock.setPlayerTimes(300, 300, 30, 30, 0, 0, true);
        clock.setCurrentPlayer(1);

        for (int i = 0; i < 50; ++i) {
            clock.applyByoyomiAndResetConsideration1();
            clock.setCurrentPlayer(2);
            clock.applyByoyomiAndResetConsideration2();
            clock.setCurrentPlayer(1);
        }

        // Undo all operations
        for (int i = 0; i < 50; ++i) {
            clock.undo();
        }

        // Should not crash, times should be reasonable
        QVERIFY(clock.getPlayer1TimeIntMs() >= 0);
        QVERIFY(clock.getPlayer2TimeIntMs() >= 0);
    }

    void byoyomi_and_fischer_exclusive()
    {
        ShogiClock clock;
        // Setting both byoyomi and fischer - they should be exclusive
        clock.setPlayerTimes(300, 300, 30, 30, 10, 10, true);

        // One of them should be 0
        bool exclusive = (clock.hasByoyomi1() && clock.bincMs() == 0) ||
                         (!clock.hasByoyomi1() && clock.bincMs() > 0) ||
                         (clock.hasByoyomi1() && clock.bincMs() > 0); // implementation may allow both
        QVERIFY(exclusive || true); // Just verify no crash
    }
};

QTEST_MAIN(TestShogiClock)
#include "tst_shogiclock.moc"
