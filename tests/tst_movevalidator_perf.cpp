#include <QtTest>
#include <QElapsedTimer>

#include "fastmovevalidator.h"
#include "movevalidator.h"
#include "shogiboard.h"
#include "shogimove.h"

static const QString kHirateSfen =
    QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");

class TestMoveValidatorPerf : public QObject
{
    Q_OBJECT

private slots:
    void benchmarkGenerateLegalMoves()
    {
        ShogiBoard board;
        board.setSfen(kHirateSfen);

        MoveValidator mv;
        FastMoveValidator fv;

        const auto& bd = board.boardData();
        const auto& ps = board.getPieceStand();

        constexpr int N = 10000;

        QElapsedTimer t1;
        t1.start();
        volatile int sink1 = 0;
        for (int i = 0; i < N; ++i) {
            sink1 += mv.generateLegalMoves(MoveValidator::BLACK, bd, ps);
        }
        const qint64 ms1 = t1.elapsed();

        QElapsedTimer t2;
        t2.start();
        volatile int sink2 = 0;
        for (int i = 0; i < N; ++i) {
            sink2 += fv.generateLegalMoves(FastMoveValidator::BLACK, bd, ps);
        }
        const qint64 ms2 = t2.elapsed();

        qInfo() << "[PERF] generateLegalMoves MoveValidator(ms)=" << ms1
                << "FastMoveValidator(ms)=" << ms2
                << "speedup=" << (double)ms1 / (double)ms2
                << "sink=" << sink1 << sink2;

        QVERIFY(ms1 > 0);
        QVERIFY(ms2 > 0);
    }

    void benchmarkIsLegalMove()
    {
        ShogiBoard board;
        board.setSfen(kHirateSfen);

        MoveValidator mv;
        FastMoveValidator fv;

        const auto& bd = board.boardData();
        const auto& ps = board.getPieceStand();

        ShogiMove move(QPoint(6, 6), QPoint(6, 5), QLatin1Char('P'), QLatin1Char(' '), false);

        constexpr int N = 50000;

        QElapsedTimer t1;
        t1.start();
        volatile int sink1 = 0;
        for (int i = 0; i < N; ++i) {
            ShogiMove m = move;
            const auto st = mv.isLegalMove(MoveValidator::BLACK, bd, ps, m);
            sink1 += st.nonPromotingMoveExists ? 1 : 0;
            sink1 += st.promotingMoveExists ? 1 : 0;
        }
        const qint64 ms1 = t1.elapsed();

        QElapsedTimer t2;
        t2.start();
        volatile int sink2 = 0;
        for (int i = 0; i < N; ++i) {
            ShogiMove m = move;
            const auto st = fv.isLegalMove(FastMoveValidator::BLACK, bd, ps, m);
            sink2 += st.nonPromotingMoveExists ? 1 : 0;
            sink2 += st.promotingMoveExists ? 1 : 0;
        }
        const qint64 ms2 = t2.elapsed();

        qInfo() << "[PERF] isLegalMove MoveValidator(ms)=" << ms1
                << "FastMoveValidator(ms)=" << ms2
                << "speedup=" << (double)ms1 / (double)ms2
                << "sink=" << sink1 << sink2;

        QVERIFY(ms1 > 0);
        QVERIFY(ms2 > 0);
    }
};

QTEST_MAIN(TestMoveValidatorPerf)
#include "tst_movevalidator_perf.moc"
