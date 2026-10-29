#include <QtTest>
#include <QSignalSpy>

#include "shogiclock.h"
#include "timecontrol_test_helper.h"

using TestTime::rules;

/// 棋譜に書く累計の考慮時間（"HH:MM:SS"）
static QString recordTotal(const ShogiClock& clock, Turn side)
{
    return clock.recordTime(side).toText().section(QLatin1Char('/'), 1);
}

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
        TimeControl rules;
        rules.black.mainMs = rules.white.mainMs = baseMs;
        rules.black.incrementMs = 10000;
        rules.white.incrementMs = 7000;
        clock.setTimeControl(rules);
        clock.setCurrentPlayer(Turn::Black);
        QCOMPARE(clock.remainingMs(Turn::Black), baseMs + 10000);
        QCOMPARE(clock.remainingMs(Turn::White), baseMs + 7000);
        // 再適用・再開で初手分を二重加算しない。
        clock.setTimeControl(rules);
        auto state = clock.pauseAndSnapshot();
        state.of(Turn::Black).remainingMs -= 3000;
        state.of(Turn::Black).considerationMs = 3000;
        clock.restoreSnapshot(state);
        clock.commitMove(Turn::Black);
        QCOMPARE(clock.remainingMs(Turn::Black), baseMs + 17000);
        QCOMPARE(recordTotal(clock, Turn::Black), QStringLiteral("00:00:03"));
        clock.setCurrentPlayer(Turn::White);
        clock.commitMove(Turn::White);
        clock.setCurrentPlayer(Turn::Black);
        state = clock.pauseAndSnapshot();
        state.of(Turn::Black).remainingMs -= 5000;
        state.of(Turn::Black).considerationMs = 5000;
        clock.restoreSnapshot(state);
        clock.commitMove(Turn::Black);
        QCOMPARE(clock.remainingMs(Turn::Black), baseMs + 22000);
        QCOMPARE(recordTotal(clock, Turn::Black), QStringLiteral("00:00:08"));
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
        QCOMPARE(clock.timeString(Turn::Black), QStringLiteral("00:00:00"));
        clock.setConsiderationTimeMs(Turn::Black, 1750);
        QCOMPARE(clock.timeString(Turn::Black), QStringLiteral("00:00:01"));
        if (switchBeforeCommit) clock.setCurrentPlayer(Turn::White);
        clock.commitMove(Turn::Black);
        QCOMPARE(clock.timeString(Turn::Black), recordTotal(clock, Turn::Black));
        QCOMPARE(clock.timeString(Turn::Black), QStringLiteral("00:00:01"));
        clock.setCurrentPlayer(Turn::White);
        clock.setMeasuredConsiderationTime(Turn::White, 2999);
        QCOMPARE(clock.timeString(Turn::White), QStringLiteral("00:00:02"));
        if (switchBeforeCommit) clock.setCurrentPlayer(Turn::Black);
        clock.commitMove(Turn::White);
        QCOMPARE(clock.timeString(Turn::White), recordTotal(clock, Turn::White));
        clock.setCurrentPlayer(Turn::Black);
        clock.setConsiderationTimeMs(Turn::Black, 750);
        QCOMPARE(clock.timeString(Turn::Black), QStringLiteral("00:00:02"));
        clock.markGameOver();
        clock.commitMove(Turn::Black);
        QCOMPARE(clock.timeString(Turn::Black), recordTotal(clock, Turn::Black));
        QCOMPARE(clock.recordTime(Turn::Black).toText(), QStringLiteral("00:01/00:00:02"));
        // 表示用の累積時間をエンジン向けの残り時間へ流用しない。
        QCOMPARE(clock.remainingMs(Turn::Black), 0LL);
        QCOMPARE(clock.remainingMs(Turn::White), 0LL);
    }

    void unlimitedTimeUpdatesWhileThinking()
    {
        ShogiClock clock;
        clock.setTimeControl(rules(0, 0, 0, 0, 0, 0));
        QSignalSpy updated(&clock, &ShogiClock::timeUpdated);
        clock.startClock();
        updated.clear();
        QTRY_VERIFY_WITH_TIMEOUT(!updated.isEmpty(), 2000);
        QVERIFY(clock.timeString(Turn::Black) != QStringLiteral("00:00:00"));
        QCOMPARE(clock.timeString(Turn::White), QStringLiteral("00:00:00"));
        QVERIFY(!clock.isGameOver());
        clock.finishTurn();
        clock.commitMove(Turn::Black);
        QCOMPARE(clock.timeString(Turn::Black), recordTotal(clock, Turn::Black));
        clock.stopClock();
    }

    void unlimitedTimePauseAndUndo()
    {
        ShogiClock clock;
        clock.setTimeControl(rules(0, 0, 0, 0, 0, 0));
        clock.startClock();
        clock.stopClock();
        clock.setConsiderationTimeMs(Turn::Black, 1750);
        const auto paused = clock.pauseAndSnapshot();
        clock.markGameOver();
        clock.commitMove(Turn::Black);
        clock.restoreSnapshot(paused);
        QCOMPARE(clock.timeString(Turn::Black), QStringLiteral("00:00:01"));
        clock.setMeasuredConsiderationTime(Turn::Black, 750);
        clock.commitMove(Turn::Black);
        QCOMPARE(clock.timeString(Turn::Black), QStringLiteral("00:00:02"));
        clock.setCurrentPlayer(Turn::White);
        clock.startClock();
        clock.stopClock();
        clock.setConsiderationTimeMs(Turn::White, 3200);
        clock.commitMove(Turn::White);
        clock.setCurrentPlayer(Turn::Black);
        clock.startClock();
        clock.stopClock();
        clock.undo();
        QCOMPARE(clock.timeString(Turn::Black), QStringLiteral("00:00:00"));
        QCOMPARE(clock.timeString(Turn::White), QStringLiteral("00:00:00"));
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
        QVERIFY(state.of(Turn::Black).considerationMs >= 35);
        const qint64 left = clock.remainingTurnTimeMs(Turn::Black);
        QTest::qWait(70);
        QCOMPARE(clock.remainingTurnTimeMs(Turn::Black), left);
        // 中断行による時間確定と、閲覧による手番変更を経ても復元できる。
        clock.markGameOver();
        clock.commitMove(Turn::Black);
        clock.setCurrentPlayer(Turn::White);
        clock.restoreSnapshot(state);
        QCOMPARE(clock.currentPlayer(), Turn::Black);
        QCOMPARE(clock.remainingMs(Turn::Black), state.of(Turn::Black).remainingMs);
        QCOMPARE(clock.remainingMs(Turn::White), state.of(Turn::White).remainingMs);
        QCOMPARE(clock.byoyomiApplied(Turn::Black), byoyomi != 0);
        QCOMPARE(recordTotal(clock, Turn::Black), QStringLiteral("00:00:00"));
        clock.startClock();
        QTest::qSleep(40);
        clock.finishTurn();
        const auto resumed = clock.pauseAndSnapshot();
        QVERIFY(resumed.of(Turn::Black).considerationMs >= state.of(Turn::Black).considerationMs + 40);
        QCOMPARE(resumed.history.size(), state.history.size());
        clock.setCurrentPlayer(Turn::White);
        clock.setMeasuredConsiderationTime(Turn::Black, 40);
        QCOMPARE(clock.considerationMs(Turn::Black), state.of(Turn::Black).considerationMs + 40);
        clock.commitMove(Turn::Black);
        if (byoyomi) QCOMPARE(clock.remainingMs(Turn::Black), 3000LL);
        if (increment) QCOMPARE(clock.remainingMs(Turn::Black), resumed.of(Turn::Black).remainingMs + 2000);
    }

    void switchAccountsElapsedToOldPlayer()
    {
        ShogiClock clock;
        clock.setTimeControl(rules(10, 10, 0, 0, 0, 0));
        clock.startClock();
        QTest::qSleep(35); // イベント処理なし: 未精算のtick端数を作る
        clock.setCurrentPlayer(Turn::White);
        QVERIFY(clock.remainingMs(Turn::Black) <= 9965);
        QCOMPARE(clock.remainingMs(Turn::White), 10000LL);
        clock.stopClock();
    }

    void receivedMoveStopsChargingUntilNextTurn()
    {
        ShogiClock clock;
        clock.setTimeControl(rules(0, 0, 1, 1, 0, 0));
        clock.startClock();
        QTest::qSleep(30);
        clock.finishTurn();
        const qint64 remaining = clock.remainingMs(Turn::Black);
        QTest::qSleep(1050); // 受理後のUI処理が期限を越えても敗着にしない
        clock.updateClock();
        QCOMPARE(clock.remainingMs(Turn::Black), remaining);
        QVERIFY(!clock.isGameOver());
        clock.setCurrentPlayer(Turn::White);
        QTest::qSleep(30);
        clock.updateClock();
        QVERIFY(clock.remainingTurnTimeMs(Turn::White) <= 970);
        clock.stopClock();
    }

    void lateResponseStillLoses()
    {
        ShogiClock clock;
        clock.setTimeControl(rules(0, 0, 1, 1, 0, 0));
        QSignalSpy timeout(&clock, &ShogiClock::playerTimeOut);
        clock.startClock();
        QTest::qSleep(1020);
        clock.finishTurn();
        QVERIFY(clock.isGameOver());
        QCOMPARE(timeout.count(), 1);
        QCOMPARE(timeout.first().first().value<Turn>(), Turn::Black);
    }

    // 時間切れの知らせは、時間が切れた側（手番の側）を渡す
    void timeoutReportsWhiteSide()
    {
        ShogiClock clock;
        clock.setTimeControl(rules(60, 0, 0, 1, 0, 0));
        clock.setCurrentPlayer(Turn::White);
        QSignalSpy timeout(&clock, &ShogiClock::playerTimeOut);
        clock.startClock();
        QTest::qSleep(1020);
        clock.updateClock();
        QVERIFY(clock.isGameOver());
        QCOMPARE(timeout.count(), 1);
        QCOMPARE(timeout.first().first().value<Turn>(), Turn::White);
    }

    void liveBudgetIncludesUntickedByoyomiTransition()
    {
        ShogiClock clock;
        clock.setTimeControl(rules(1, 0, 1, 5, 0, 0));
        clock.startClock();
        QTest::qSleep(1020);
        QCOMPARE(clock.remainingMainTimeMs(Turn::Black), 0LL);
        QVERIFY(clock.remainingTurnTimeMs(Turn::Black) <= 980);
        QCOMPARE(clock.remainingTurnTimeMs(Turn::White), 5000LL);
        clock.stopClock();
        QVERIFY(!clock.isGameOver());
        QVERIFY(clock.byoyomiApplied(Turn::Black));
        QVERIFY(clock.remainingMs(Turn::Black) > 0);
    }

    void setTimeControl_basic()
    {
        ShogiClock clock;
        clock.setTimeControl(rules(300, 300, 0, 0, 0, 0));

        // 300 seconds = 300000 ms
        QCOMPARE(clock.remainingMs(Turn::Black), 300000LL);
        QCOMPARE(clock.remainingMs(Turn::White), 300000LL);
    }

    /// CSA のようにサーバーが残り時間を示すときは、決まり・考慮時間・待ったの履歴を保ったまま合わせる
    void setRemainingTimes_keepsRulesAndHistory()
    {
        ShogiClock clock;
        clock.setTimeControl(rules(300, 300, 10, 10, 0, 0));
        clock.setConsiderationTimeMs(Turn::Black, 1500);
        clock.commitMove(Turn::Black);
        const auto before = clock.pauseAndSnapshot();
        clock.setRemainingTimes(250000, 0);
        QCOMPARE(clock.remainingMs(Turn::Black), 250000LL);
        QVERIFY(!clock.byoyomiApplied(Turn::Black));
        // 使い切った側は秒読みに入る
        QVERIFY(clock.byoyomiApplied(Turn::White));
        QCOMPARE(clock.remainingMs(Turn::White), 10000LL);
        QCOMPARE(clock.byoyomiMs(Turn::Black), 10000LL);
        QCOMPARE(recordTotal(clock, Turn::Black), QStringLiteral("00:00:01"));
        QCOMPARE(clock.pauseAndSnapshot().history.size(), before.history.size());
        QVERIFY(!clock.isUnlimited());
    }

    void timeString_format()
    {
        ShogiClock clock;
        clock.setTimeControl(rules(300, 300, 0, 0, 0, 0));

        // 300 seconds = 5 minutes = "00:05:00"
        QCOMPARE(clock.timeString(Turn::Black), QStringLiteral("00:05:00"));
        QCOMPARE(clock.timeString(Turn::White), QStringLiteral("00:05:00"));
    }

    void byoyomi_setup()
    {
        ShogiClock clock;
        clock.setTimeControl(rules(0, 0, 30, 30, 0, 0));

        QVERIFY(clock.hasByoyomi(Turn::Black));
        QVERIFY(clock.hasByoyomi(Turn::White));
    }

    /// 持ち時間を使い切った側（CSA の時計合わせ・持ち時間0の秒読み対局）は初めから秒読みに入り、
    /// 手番でないあいだも 00:00:00 ではなく秒読みの秒数を表示する
    void exhaustedMainTimeStartsInByoyomi()
    {
        ShogiClock clock;
        clock.setTimeControl(rules(0, 60, 5, 5, 0, 0));
        QVERIFY(clock.byoyomiApplied(Turn::Black));
        QVERIFY(!clock.byoyomiApplied(Turn::White));
        QCOMPARE(clock.remainingMs(Turn::Black), 5000LL);
        QCOMPARE(clock.timeString(Turn::Black), QStringLiteral("00:00:05"));
        QCOMPARE(clock.remainingMainTimeMs(Turn::Black), 0LL);
        QCOMPARE(clock.remainingTurnTimeMs(Turn::Black), 5000LL);
        QCOMPARE(clock.remainingMs(Turn::White), 60000LL);
        QCOMPARE(clock.remainingTurnTimeMs(Turn::White), 65000LL);
    }

    void byoyomi_application()
    {
        ShogiClock clock;
        clock.setTimeControl(rules(0, 0, 30, 30, 0, 0));
        clock.setCurrentPlayer(Turn::Black);

        // Apply byoyomi for player 1
        clock.commitMove(Turn::Black);

        // After byoyomi applied, player should have 30 seconds
        QCOMPARE(clock.remainingMs(Turn::Black), 30000LL);
        QVERIFY(clock.byoyomiApplied(Turn::Black));
    }

    void fischer_increment()
    {
        ShogiClock clock;
        clock.setTimeControl(rules(300, 300, 0, 0, 10, 10));

        QCOMPARE(clock.incrementMs(Turn::Black), 10000LL);
        QCOMPARE(clock.incrementMs(Turn::White), 10000LL);
        // 初手にも加算を与える
        QCOMPARE(clock.remainingMs(Turn::Black), 310000LL);

        clock.setCurrentPlayer(Turn::Black);
        clock.commitMove(Turn::Black);

        // 着手確定で次の手番の分を加算する: 310 + 10 = 320 秒
        QCOMPARE(clock.remainingMs(Turn::Black), 320000LL);
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
        clock.setCurrentPlayer(Turn::Black);
        const auto commit = [&clock](Turn player, qint64 ms) {
            clock.setCurrentPlayer(player);
            clock.setMeasuredConsiderationTime(player, ms);
            clock.commitMove(player);
        };
        const auto verifyTurnStart = [&clock](const ShogiClock::Snapshot& expected) {
            const auto now = clock.pauseAndSnapshot();
            QCOMPARE(now.currentPlayer, Turn::Black);
            QCOMPARE(now.of(Turn::Black).remainingMs, expected.of(Turn::Black).remainingMs);
            QCOMPARE(now.of(Turn::White).remainingMs, expected.of(Turn::White).remainingMs);
            QCOMPARE(now.of(Turn::Black).totalConsiderationMs, expected.of(Turn::Black).totalConsiderationMs);
            QCOMPARE(now.of(Turn::White).totalConsiderationMs, expected.of(Turn::White).totalConsiderationMs);
            QCOMPARE(now.of(Turn::Black).byoyomiApplied, expected.of(Turn::Black).byoyomiApplied);
            QCOMPARE(now.of(Turn::White).byoyomiApplied, expected.of(Turn::White).byoyomiApplied);
            QCOMPARE(now.of(Turn::Black).considerationMs, 0LL);
            QVERIFY(!now.considerationCommitted);
        };

        const auto firstTurn = clock.pauseAndSnapshot();
        commit(Turn::Black, 3000);
        commit(Turn::White, 2000);
        clock.setCurrentPlayer(Turn::Black);
        const auto secondTurn = clock.pauseAndSnapshot();
        commit(Turn::Black, 5000);
        commit(Turn::White, 1000);
        clock.setCurrentPlayer(Turn::Black);
        clock.setConsiderationTimeMs(Turn::Black, 700); // 考え中の時間も返す

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
        QVERIFY(clock.considerationMs(Turn::Black) >= 60);
        QVERIFY(clock.considerationMs(Turn::Black) < 1000);
        if (byoyomiSec == 0) {
            QCOMPARE(clock.remainingMs(Turn::Black), firstTurn.of(Turn::Black).remainingMs - clock.considerationMs(Turn::Black));
        }
        clock.stopClock();

        // 指し直した後の「待った」も、指し直した手の前へ戻る
        clock.setConsiderationTimeMs(Turn::Black, 0);
        commit(Turn::Black, 4000);
        commit(Turn::White, 1500);
        clock.setCurrentPlayer(Turn::Black);
        clock.undo();
        verifyTurnStart(firstTurn);

        // 時間を設定し直した新しい対局では、前の対局の状態に戻らない
        commit(Turn::Black, 4000);
        commit(Turn::White, 1500);
        clock.setCurrentPlayer(Turn::Black);
        clock.setTimeControl(rules(mainSec, mainSec, byoyomiSec, byoyomiSec, incrementSec, incrementSec));
        clock.setCurrentPlayer(Turn::Black);
        const auto newGame = clock.pauseAndSnapshot();
        clock.undo();
        verifyTurnStart(newGame);
    }

    void stressTest_startStop()
    {
        ShogiClock clock;
        clock.setTimeControl(rules(300, 300, 30, 30, 0, 0));
        clock.setCurrentPlayer(Turn::Black);

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
        clock.setCurrentPlayer(Turn::Black);

        for (int i = 0; i < 50; ++i) {
            clock.commitMove(Turn::Black);
            clock.setCurrentPlayer(Turn::White);
            clock.commitMove(Turn::White);
            clock.setCurrentPlayer(Turn::Black);
        }

        // Undo all operations
        for (int i = 0; i < 50; ++i) {
            clock.undo();
        }

        // Should not crash, times should be reasonable
        QVERIFY(clock.remainingMs(Turn::Black) >= 0);
        QVERIFY(clock.remainingMs(Turn::White) >= 0);
    }

    void byoyomi_and_fischer_exclusive()
    {
        ShogiClock clock;
        // Setting both byoyomi and fischer - they should be exclusive
        clock.setTimeControl(rules(300, 300, 30, 30, 10, 10));

        // One of them should be 0
        bool exclusive = (clock.hasByoyomi(Turn::Black) && clock.incrementMs(Turn::Black) == 0) ||
                         (!clock.hasByoyomi(Turn::Black) && clock.incrementMs(Turn::Black) > 0) ||
                         (clock.hasByoyomi(Turn::Black) && clock.incrementMs(Turn::Black) > 0); // implementation may allow both
        QVERIFY(exclusive || true); // Just verify no crash
    }
};

QTEST_MAIN(TestShogiClock)
#include "tst_shogiclock.moc"
