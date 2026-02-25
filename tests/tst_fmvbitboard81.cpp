#include <QtTest>

#include "fmvbitboard81.h"

class TestFmvBitboard81 : public QObject
{
    Q_OBJECT

private slots:
    void setTestClear_basic()
    {
        fmv::Bitboard81 bb;
        QVERIFY(bb.none());

        // sq=0 (lo bit 0)
        bb.set(0);
        QVERIFY(bb.test(0));
        QCOMPARE(bb.count(), 1);
        bb.clear(0);
        QVERIFY(!bb.test(0));
        QVERIFY(bb.none());

        // sq=63 (lo bit 63)
        bb.set(63);
        QVERIFY(bb.test(63));
        QCOMPARE(bb.count(), 1);
        bb.clear(63);
        QVERIFY(bb.none());

        // sq=64 (hi bit 0)
        bb.set(64);
        QVERIFY(bb.test(64));
        QCOMPARE(bb.count(), 1);
        bb.clear(64);
        QVERIFY(bb.none());

        // sq=80 (hi bit 16)
        bb.set(80);
        QVERIFY(bb.test(80));
        QCOMPARE(bb.count(), 1);
        bb.clear(80);
        QVERIFY(bb.none());
    }

    void count_multiple()
    {
        fmv::Bitboard81 bb;
        bb.set(0);
        bb.set(40);
        bb.set(64);
        bb.set(80);
        QCOMPARE(bb.count(), 4);
    }

    void popFirst_iteration()
    {
        fmv::Bitboard81 bb;
        bb.set(5);
        bb.set(70);
        bb.set(30);

        QVector<int> popped;
        while (bb.any()) {
            popped.append(static_cast<int>(bb.popFirst()));
        }
        QCOMPARE(popped.size(), 3);
        QVERIFY(bb.none());
        // popFirst returns lowest bit first
        QCOMPARE(popped[0], 5);
        QCOMPARE(popped[1], 30);
        QCOMPARE(popped[2], 70);
    }

    void operators_and_or_not()
    {
        fmv::Bitboard81 a;
        fmv::Bitboard81 b;
        a.set(10);
        a.set(70);
        b.set(10);
        b.set(50);

        fmv::Bitboard81 andResult = a & b;
        QVERIFY(andResult.test(10));
        QVERIFY(!andResult.test(70));
        QVERIFY(!andResult.test(50));
        QCOMPARE(andResult.count(), 1);

        fmv::Bitboard81 orResult = a | b;
        QVERIFY(orResult.test(10));
        QVERIFY(orResult.test(50));
        QVERIFY(orResult.test(70));
        QCOMPARE(orResult.count(), 3);

        fmv::Bitboard81 notA = ~a;
        QVERIFY(!notA.test(10));
        QVERIFY(!notA.test(70));
        QVERIFY(notA.test(0));
        QVERIFY(notA.test(50));
        QCOMPARE(notA.count(), 81 - 2);
    }

    void anyNone()
    {
        fmv::Bitboard81 bb;
        QVERIFY(bb.none());
        QVERIFY(!bb.any());

        bb.set(40);
        QVERIFY(bb.any());
        QVERIFY(!bb.none());
    }

    void squareBit()
    {
        fmv::Bitboard81 bb = fmv::Bitboard81::squareBit(42);
        QCOMPARE(bb.count(), 1);
        QVERIFY(bb.test(42));
    }
};

QTEST_MAIN(TestFmvBitboard81)
#include "tst_fmvbitboard81.moc"
