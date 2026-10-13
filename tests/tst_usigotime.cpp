/// @file tst_usigotime.cpp
/// @brief USI の go に載せる時間を求める純粋関数（UsiGoTimeCalculator）のテスト

#include <QtTest>

#include "timecontrol_test_helper.h"
#include "usigotime.h"

using TestTime::rules;

class TestUsiGoTime : public QObject
{
    Q_OBJECT

private slots:
    /// 加算方式: 予算（btime + 加算）から余裕を引き、加算分は binc/winc で伝える。
    /// 余裕を引くと加算分に届かないときは、手番側の加算を減らして btime + binc が予算を超えないようにする
    void incrementBudgetIsSplitIntoBinc()
    {
        UsiGoTimeInput in;
        in.timeControl = rules(0, 0, 0, 0, 10, 7);
        in.btimeMs = 0;
        in.wtimeMs = 10000;
        in.sideToMove = Turn::Black;
        const UsiGoTime t = UsiGoTimeCalculator::compute(in);
        QVERIFY(!t.useByoyomi);
        QCOMPARE(t.btime, 0LL);
        QCOMPARE(t.binc, 10000LL - UsiGoTimeCalculator::kEngineReserveMs);
        // 手番でない側はそのまま
        QCOMPARE(t.wtime, 10000LL);
        QCOMPARE(t.winc, 7000LL);
    }

    void incrementWithMainTimeKeepsFullIncrement()
    {
        UsiGoTimeInput in;
        in.timeControl = rules(300, 300, 0, 0, 10, 10);
        in.btimeMs = 300000;
        in.wtimeMs = 300000;
        in.sideToMove = Turn::White;
        const UsiGoTime t = UsiGoTimeCalculator::compute(in);
        QCOMPARE(t.wtime, 300000LL - UsiGoTimeCalculator::kEngineReserveMs);
        QCOMPARE(t.winc, 10000LL);
        QCOMPARE(t.btime, 300000LL);
        QCOMPARE(t.binc, 10000LL);
        QCOMPARE(UsiGoTimeCalculator::budgetMs(in), 310000LL);
    }

    /// 先後で秒読みが違うときは、手番側の秒読みを送る
    void asymmetricByoyomiUsesSideToMove()
    {
        UsiGoTimeInput in;
        in.timeControl = rules(0, 0, 0, 5, 0, 0);
        in.sideToMove = Turn::White;
        QCOMPARE(UsiGoTimeCalculator::compute(in).byoyomi, 5000LL - UsiGoTimeCalculator::kEngineReserveMs);
        in.sideToMove = Turn::Black;
        const UsiGoTime black = UsiGoTimeCalculator::compute(in);
        QVERIFY(black.useByoyomi);
        QCOMPARE(black.byoyomi, 0LL);
    }

    /// 秒読みの途中なら、残りの秒読みから求める
    void remainingByoyomiIsUsed()
    {
        UsiGoTimeInput in;
        in.timeControl = rules(0, 0, 10, 10, 0, 0);
        in.sideToMove = Turn::Black;
        in.moverByoyomiMs = 3000;
        QCOMPARE(UsiGoTimeCalculator::compute(in).byoyomi, 3000LL - UsiGoTimeCalculator::kEngineReserveMs);
    }

    /// CSA の遅延時間は今の手に足し、通信の余裕はエンジンの余裕とは別に差し引く
    void delayAndMarginInByoyomiMode()
    {
        UsiGoTimeInput in;
        in.timeControl = rules(300, 300, 10, 10, 0, 0);
        in.btimeMs = 300000;
        in.wtimeMs = 300000;
        in.sideToMove = Turn::Black;
        in.extraMs = 3000;
        in.marginMs = 2000;
        const UsiGoTime t = UsiGoTimeCalculator::compute(in);
        QCOMPARE(t.byoyomi, 10000LL + 3000 - 2000 - UsiGoTimeCalculator::kEngineReserveMs);
        QCOMPARE(t.btime, 300000LL);
        QCOMPARE(UsiGoTimeCalculator::budgetMs(in), 313000LL);
    }

    void delayAndMarginInIncrementMode()
    {
        UsiGoTimeInput in;
        in.timeControl = rules(60, 60, 0, 0, 5, 5);
        in.btimeMs = 60000;
        in.wtimeMs = 60000;
        in.sideToMove = Turn::Black;
        in.extraMs = 2000;
        in.marginMs = 1000;
        const UsiGoTime t = UsiGoTimeCalculator::compute(in);
        // 予算 60000 + 5000 + 2000 から 1000 と 250 を引き、加算分 5000 を binc で伝える
        QCOMPARE(t.binc, 5000LL);
        QCOMPARE(t.btime, 60000LL + 2000 - 1000 - UsiGoTimeCalculator::kEngineReserveMs);
    }

    /// 秒読みと加算が両方あれば秒読みを使い、加算は送らない
    void byoyomiWinsOverIncrement()
    {
        UsiGoTimeInput in;
        in.timeControl = rules(60, 60, 10, 10, 5, 5);
        in.btimeMs = 60000;
        in.wtimeMs = 60000;
        const UsiGoTime t = UsiGoTimeCalculator::compute(in);
        QVERIFY(t.useByoyomi);
        QCOMPARE(t.binc, 0LL);
        QCOMPARE(t.winc, 0LL);
    }

    /// 短い設定でも、余裕は予算の半分までしか引かない
    void shortBudgetKeepsHalf()
    {
        UsiGoTimeInput in;
        in.timeControl = rules(0, 0, 0, 0, 0, 0);
        in.timeControl.black.byoyomiMs = 300;
        in.sideToMove = Turn::Black;
        QCOMPARE(UsiGoTimeCalculator::compute(in).byoyomi, 150LL);
    }

    void commandText()
    {
        UsiGoTime t;
        t.btime = 1;
        t.wtime = 2;
        t.byoyomi = 3;
        t.binc = 4;
        t.winc = 5;
        t.useByoyomi = true;
        QCOMPARE(t.toCommand(false), QStringLiteral("go btime 1 wtime 2 byoyomi 3"));
        t.useByoyomi = false;
        QCOMPARE(t.toCommand(true), QStringLiteral("go ponder btime 1 wtime 2 binc 4 winc 5"));
    }
};

QTEST_MAIN(TestUsiGoTime)
#include "tst_usigotime.moc"
