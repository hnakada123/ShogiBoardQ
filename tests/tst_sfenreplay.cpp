#include <QtTest>

#include "usimove.h"
#include "sfenutils.h"
#include "shogiboard.h"
#include "sfenreplay.h"
#include "shogimove.h"
#include "test_replay.h"

static const QString kHirateSfen =
    QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");

/// 開始局面から USI の手を順に指す処理（SfenReplay）と、盤（ShogiBoard）の SFEN の読み書き
class TestSfenReplay : public QObject
{
    Q_OBJECT

private:
    static Piece pieceAt(const QString& sfen, int file, int rank)
    {
        ShogiBoard board;
        board.setSfen(sfen);
        return board.pieceAt(BoardSquare{file, rank});
    }

private slots:
    void replayReportsFailureWithoutInventingAPosition()
    {
        const QStringList moves{QStringLiteral("7g7f"), QStringLiteral("3c3d"), QStringLiteral("7g7e")};
        const auto result = SfenReplay::replay(SfenUtils::hirateSfen(), moves);
        QVERIFY(!result.success());
        QCOMPARE(result.failedPly, 3);
        QCOMPARE(result.moves.size(), 2);
        QVERIFY(result.error.contains(QStringLiteral("Source square is empty")));
        QVERIFY(TestReplay::buildSfenRecord(SfenUtils::hirateSfen(), moves, false).isEmpty());
        QVERIFY(TestReplay::buildGameMoves(SfenUtils::hirateSfen(), moves).isEmpty());
        const auto invalidStart = SfenReplay::replay(QStringLiteral("invalid"), {});
        QCOMPARE(invalidStart.failedPly, 0);
        QVERIFY(invalidStart.moves.isEmpty());
        QVERIFY(!invalidStart.error.isEmpty());
    }

    /// 持ち駒は標準の並び（先手の飛角金銀桂香歩、続いて後手）で書く
    void handsUseStandardOrder()
    {
        ShogiBoard board;
        board.setSfen(QStringLiteral("4k4/9/9/9/9/9/9/9/4K4 b bPBp 1"));
        QCOMPARE(board.toSfen(), QStringLiteral("4k4/9/9/9/9/9/9/9/4K4 b BPbp 1"));

        // 角換わりの後、両者が角と歩を持つ局面の持ち駒の並び
        const QStringList moves = {QStringLiteral("7g7f"), QStringLiteral("3c3d"), QStringLiteral("8h2b+"),
                                   QStringLiteral("3a2b"), QStringLiteral("2g2f"), QStringLiteral("8c8d"),
                                   QStringLiteral("2f2e"), QStringLiteral("8d8e"), QStringLiteral("2e2d"),
                                   QStringLiteral("2c2d"), QStringLiteral("2h2d"), QStringLiteral("8e8f"),
                                   QStringLiteral("8g8f"), QStringLiteral("8b8f")};
        const QStringList record = TestReplay::buildSfenRecord(SfenUtils::hirateSfen(), moves, false);
        QCOMPARE(record.size(), moves.size() + 1);
        QCOMPARE(record.last().section(QLatin1Char(' '), 2, 2), QStringLiteral("B2Pb2p"));  // 以前は "Bb2P2p" と書いていた
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

    void sfenRoundTrip()
    {
        ShogiBoard board;
        board.setSfen(kHirateSfen);
        QCOMPARE(board.toSfen(), kHirateSfen);
    }

    void sfenFromPositionCommand_startposMoves()
    {
        const QString sfen = SfenReplay::sfenFromPositionCommand(QStringLiteral("position startpos moves 7g7f"));

        QVERIFY(!sfen.isEmpty());
        QVERIFY(sfen.contains(QStringLiteral(" w ")));
        QCOMPARE(pieceAt(sfen, 7, 6), Piece::BlackPawn);
    }

    void sfenFromPositionCommand_sfenKeepsTurn()
    {
        const QString base = QStringLiteral("4k4/9/9/9/9/9/9/9/9 b P 1");
        const QString sfen = SfenReplay::sfenFromPositionCommand(QStringLiteral("position sfen %1").arg(base));

        QCOMPARE(sfen, base);
    }

    void sfenFromPositionCommand_sfenMovesKeepMoverSide()
    {
        const QString base = QStringLiteral("4k4/9/9/9/9/9/9/9/9 b P 1");
        const QString sfen = SfenReplay::sfenFromPositionCommand(
            QStringLiteral("position sfen %1 moves P*5e").arg(base));

        QVERIFY(!sfen.isEmpty());
        QVERIFY(sfen.contains(QStringLiteral(" w ")));
        QCOMPARE(pieceAt(sfen, 5, 5), Piece::BlackPawn);
    }

    void positionAfter_pawnAdvance()
    {
        const QString sfen = SfenReplay::positionAfter(kHirateSfen, QStringLiteral("7g7f"));

        QCOMPARE(pieceAt(sfen, 7, 7), Piece::None);
        QCOMPARE(pieceAt(sfen, 7, 6), Piece::BlackPawn);
        QCOMPARE(sfen.section(QLatin1Char(' '), 1, 1), QStringLiteral("w"));
        QCOMPARE(sfen.section(QLatin1Char(' '), 3, 3), QStringLiteral("2"));
    }

    void positionAfter_bishopPromote()
    {
        const QStringList record = TestReplay::buildSfenRecord(
            kHirateSfen, {QStringLiteral("7g7f"), QStringLiteral("3c3d"), QStringLiteral("8h2b+")}, false);
        QCOMPARE(record.size(), 4);

        QCOMPARE(pieceAt(record.last(), 2, 2), Piece::BlackHorse);
        QCOMPARE(pieceAt(record.last(), 8, 8), Piece::None);
    }

    void positionAfter_drop()
    {
        const QString sfen = SfenReplay::positionAfter(
            QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b P 1"), QStringLiteral("P*5e"));

        QCOMPARE(pieceAt(sfen, 5, 5), Piece::BlackPawn);
    }

    void positionAfter_rejectsEmptySource()
    {
        QVERIFY(SfenReplay::positionAfter(kHirateSfen, QStringLiteral("5e5d")).isEmpty());
        QVERIFY(SfenReplay::positionAfter(kHirateSfen, QStringLiteral("none")).isEmpty());
    }

    void buildGameMoves()
    {
        const QStringList usiMoves = {QStringLiteral("7g7f"), QStringLiteral("3c3d"), QStringLiteral("2g2f")};
        const QList<ShogiMove> gameMoves = TestReplay::buildGameMoves(kHirateSfen, usiMoves);
        QCOMPARE(gameMoves.size(), 3);

        QCOMPARE(gameMoves[0].move.toString(), QStringLiteral("7g7f"));
        QCOMPARE(gameMoves[0].movingPiece, Piece::BlackPawn);
        QCOMPARE(gameMoves[1].movingPiece, Piece::WhitePawn);
    }

    void buildSfenRecord()
    {
        const QStringList record =
            TestReplay::buildSfenRecord(kHirateSfen, {QStringLiteral("7g7f"), QStringLiteral("3c3d")}, false);
        // 開始局面と2手の後の局面
        QCOMPARE(record.size(), 3);
        QCOMPARE(record[0], kHirateSfen);
        QVERIFY(record[1] != record[2]);
    }
};

QTEST_MAIN(TestSfenReplay)
#include "tst_sfenreplay.moc"
