#include <QtTest>

#include "usimove.h"
#include "sfenutils.h"
#include "shogiboard.h"
#include "sfenpositiontracer.h"
#include "shogimove.h"

static const QString kHirateSfen =
    QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");

class TestSfenTracer : public QObject
{
    Q_OBJECT

private slots:
    void replayReportsFailureWithoutInventingAPosition()
    {
        const QStringList moves{QStringLiteral("7g7f"), QStringLiteral("3c3d"), QStringLiteral("7g7e")};
        const auto result = SfenPositionTracer::replay(SfenUtils::hirateSfen(), moves);
        QVERIFY(!result.success());
        QCOMPARE(result.failedPly, 3);
        QCOMPARE(result.moves.size(), 2);
        QVERIFY(result.error.contains(QStringLiteral("Source square is empty")));
        QVERIFY(SfenPositionTracer::buildSfenRecord(SfenUtils::hirateSfen(), moves, false).isEmpty());
        QVERIFY(SfenPositionTracer::buildGameMoves(SfenUtils::hirateSfen(), moves).isEmpty());
        const auto invalidStart = SfenPositionTracer::replay(QStringLiteral("invalid"), {});
        QCOMPARE(invalidStart.failedPly, 0);
        QVERIFY(invalidStart.moves.isEmpty());
        QVERIFY(!invalidStart.error.isEmpty());
    }

    /// 持ち駒は標準の並び（先手の飛角金銀桂香歩、続いて後手）で書き、盤と同じ SFEN になる
    void handsUseStandardOrderLikeBoard()
    {
        const QString sfen = QStringLiteral("4k4/9/9/9/9/9/9/9/4K4 b bPBp 1");
        SfenPositionTracer tracer;
        QVERIFY(tracer.setFromSfen(sfen));
        QCOMPARE(tracer.toSfenString(), QStringLiteral("4k4/9/9/9/9/9/9/9/4K4 b BPbp 1"));
        ShogiBoard board;
        board.setSfen(sfen);
        QCOMPARE(board.convertStandToSfen(), QStringLiteral("BPbp"));

        // 角換わりの後、両者が角と歩を持つ局面を盤と追跡の両方で作り、同じ SFEN になる
        const QStringList moves = {QStringLiteral("7g7f"), QStringLiteral("3c3d"), QStringLiteral("8h2b+"),
                                   QStringLiteral("3a2b"), QStringLiteral("2g2f"), QStringLiteral("8c8d"),
                                   QStringLiteral("2f2e"), QStringLiteral("8d8e"), QStringLiteral("2e2d"),
                                   QStringLiteral("2c2d"), QStringLiteral("2h2d"), QStringLiteral("8e8f"),
                                   QStringLiteral("8g8f"), QStringLiteral("8b8f")};
        const QStringList record = SfenPositionTracer::buildSfenRecord(SfenUtils::hirateSfen(), moves, false);
        ShogiBoard played;
        played.setSfen(SfenUtils::hirateSfen());
        Turn mover = Turn::Black;
        for (const QString& move : moves) {
            QVERIFY(played.applyUsiMove(*UsiMove::parse(move), mover));
            mover = (mover == Turn::Black) ? Turn::White : Turn::Black;
        }
        QCOMPARE(played.convertStandToSfen(), QStringLiteral("B2Pb2p"));  // 以前は "Bb2P2p" と書いていた
        QCOMPARE(record.last().section(QLatin1Char(' '), 2, 2), played.convertStandToSfen());
        QCOMPARE(record.last().section(QLatin1Char(' '), 0, 0), played.convertBoardToSfen());
    }

    /// 比較用のキーは持ち駒の並びをそろえる（以前のこのアプリが書いた先後交互の並びも同じ局面になる）
    void sfenKeyCanonicalizesHands()
    {
        QCOMPARE(SfenUtils::canonicalHands(QStringLiteral("BbPp")), QStringLiteral("BPbp"));
        QCOMPARE(SfenUtils::canonicalHands(QStringLiteral("p2PrB")), QStringLiteral("B2Prp"));
        QCOMPARE(SfenUtils::canonicalHands(QStringLiteral("10p")), QStringLiteral("10p"));
        QCOMPARE(SfenUtils::canonicalHands(QStringLiteral("-")), QStringLiteral("-"));
        QCOMPARE(SfenUtils::canonicalHands(QStringLiteral("Kx")), QStringLiteral("Kx"));
        QCOMPARE(SfenUtils::normalizeSfenKey(QStringLiteral("4k4/9/9/9/9/9/9/9/4K4 w BbPp 30")),
                 SfenUtils::normalizeSfenKey(QStringLiteral("position sfen 4k4/9/9/9/9/9/9/9/4K4 w BPbp 1")));
    }

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

    void sfenFromPositionCommand_startposMoves()
    {
        const QString sfen = SfenPositionTracer::sfenFromPositionCommand(
            QStringLiteral("position startpos moves 7g7f"));

        QVERIFY(!sfen.isEmpty());
        QVERIFY(sfen.contains(QStringLiteral(" w ")));

        SfenPositionTracer tracer;
        QVERIFY(tracer.setFromSfen(sfen));
        QCOMPARE(tracer.tokenAtFileRank(7, QLatin1Char('f')), QStringLiteral("P"));
    }

    void sfenFromPositionCommand_sfenKeepsTurn()
    {
        const QString base = QStringLiteral("4k4/9/9/9/9/9/9/9/9 b P 1");
        const QString sfen = SfenPositionTracer::sfenFromPositionCommand(
            QStringLiteral("position sfen %1").arg(base));

        QCOMPARE(sfen, base);
    }

    void sfenFromPositionCommand_sfenMovesKeepMoverSide()
    {
        const QString base = QStringLiteral("4k4/9/9/9/9/9/9/9/9 b P 1");
        const QString sfen = SfenPositionTracer::sfenFromPositionCommand(
            QStringLiteral("position sfen %1 moves P*5e").arg(base));

        QVERIFY(!sfen.isEmpty());
        QVERIFY(sfen.contains(QStringLiteral(" w ")));

        SfenPositionTracer tracer;
        QVERIFY(tracer.setFromSfen(sfen));
        QCOMPARE(tracer.tokenAtFileRank(5, QLatin1Char('e')), QStringLiteral("P"));
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
        QVERIFY(tracer.setFromSfen(sfen));
        // First make some setup moves
        QVERIFY(tracer.applyUsiMove(QStringLiteral("7g7f")));
        QVERIFY(tracer.applyUsiMove(QStringLiteral("3c3d")));
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
        QVERIFY(tracer.setFromSfen(sfen));
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
