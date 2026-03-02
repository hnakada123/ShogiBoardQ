#include <QtTest>

#include "sfenpositiontracer.h"
#include "shogimove.h"

static const QString kHirateSfen =
    QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");

class TestSfenTracer : public QObject
{
    Q_OBJECT

private slots:
    void resetToStartpos()
    {
        SfenPositionTracer tracer;
        tracer.resetToStartpos();
        QCOMPARE(tracer.toSfenString(), kHirateSfen);
    }

    void setFromSfen_roundTrip()
    {
        SfenPositionTracer tracer;
        QVERIFY(tracer.setFromSfen(kHirateSfen));
        QCOMPARE(tracer.toSfenString(), kHirateSfen);
    }

    void applyUsiMove_pawnAdvance()
    {
        SfenPositionTracer tracer;
        tracer.resetToStartpos();
        QVERIFY(tracer.applyUsiMove(QStringLiteral("7g7f")));

        // 7g should be empty, 7f should have P
        QCOMPARE(tracer.tokenAtFileRank(7, QLatin1Char('g')), QString());
        QCOMPARE(tracer.tokenAtFileRank(7, QLatin1Char('f')), QStringLiteral("P"));
        QVERIFY(!tracer.blackToMove()); // Now white's turn
    }

    void applyUsiMove_bishopPromote()
    {
        SfenPositionTracer tracer;
        // Set up position where B can capture at 2b
        QString sfen = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");
        tracer.setFromSfen(sfen);
        // First make some setup moves
        tracer.applyUsiMove(QStringLiteral("7g7f"));
        tracer.applyUsiMove(QStringLiteral("3c3d"));
        // Now bishop can go to 2b with promotion
        QVERIFY(tracer.applyUsiMove(QStringLiteral("8h2b+")));

        QCOMPARE(tracer.tokenAtFileRank(2, QLatin1Char('b')), QStringLiteral("+B"));
        QCOMPARE(tracer.tokenAtFileRank(8, QLatin1Char('h')), QString()); // 8h empty
    }

    void applyUsiMove_drop()
    {
        SfenPositionTracer tracer;
        // Position with a pawn in hand
        QString sfen = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b P 1");
        tracer.setFromSfen(sfen);
        QVERIFY(tracer.applyUsiMove(QStringLiteral("P*5e")));

        QCOMPARE(tracer.tokenAtFileRank(5, QLatin1Char('e')), QStringLiteral("P"));
    }

    void generateSfensForMoves()
    {
        SfenPositionTracer tracer;
        tracer.resetToStartpos();

        QStringList moves = {
            QStringLiteral("7g7f"),
            QStringLiteral("3c3d"),
            QStringLiteral("2g2f")
        };
        QStringList sfens = tracer.generateSfensForMoves(moves);
        QCOMPARE(sfens.size(), 3);
        // Each SFEN should be non-empty and different
        QVERIFY(!sfens[0].isEmpty());
        QVERIFY(!sfens[1].isEmpty());
        QVERIFY(!sfens[2].isEmpty());
        QVERIFY(sfens[0] != sfens[1]);
    }

    void buildGameMoves()
    {
        QStringList usiMoves = {
            QStringLiteral("7g7f"),
            QStringLiteral("3c3d"),
            QStringLiteral("2g2f")
        };
        QList<ShogiMove> gameMoves = SfenPositionTracer::buildGameMoves(kHirateSfen, usiMoves);
        QCOMPARE(gameMoves.size(), 3);

        // First move: 7g7f → from(6,6) to(6,5) in 0-indexed
        QCOMPARE(gameMoves[0].fromSquare, QPoint(6, 6));
        QCOMPARE(gameMoves[0].toSquare, QPoint(6, 5));
    }

    void buildSfenRecord()
    {
        QStringList usiMoves = {
            QStringLiteral("7g7f"),
            QStringLiteral("3c3d")
        };
        QStringList record = SfenPositionTracer::buildSfenRecord(kHirateSfen, usiMoves, false);
        // Should have initial + 2 moves = 3 entries
        QCOMPARE(record.size(), 3);
        QCOMPARE(record[0], kHirateSfen);
    }
};

QTEST_MAIN(TestSfenTracer)
#include "tst_sfentracer.moc"
