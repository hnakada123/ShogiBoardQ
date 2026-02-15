#include <QtTest>
#include <QSignalSpy>

#include "shogiboard.h"
#include "shogimove.h"

static const QString kHirateSfen =
    QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");

class TestShogiBoard : public QObject
{
    Q_OBJECT

private slots:
    // === SFEN I/O ===

    void setSfen_hirate()
    {
        ShogiBoard board;
        board.setSfen(kHirateSfen);

        const auto& data = board.boardData();
        QCOMPARE(data.size(), 81);

        // Rank 1 (index 0-8): l n s g k g s n l
        QCOMPARE(data[0], QLatin1Char('l'));
        QCOMPARE(data[4], QLatin1Char('k'));
        QCOMPARE(data[8], QLatin1Char('l'));

        // Rank 7 (index 54-62): P P P P P P P P P
        for (int i = 54; i <= 62; ++i) {
            QCOMPARE(data[i], QLatin1Char('P'));
        }

        // Rank 9 (index 72-80): L N S G K G S N L
        QCOMPARE(data[72], QLatin1Char('L'));
        QCOMPARE(data[76], QLatin1Char('K'));
        QCOMPARE(data[80], QLatin1Char('L'));
    }

    void sfen_roundTrip_hirate()
    {
        ShogiBoard board;
        board.setSfen(kHirateSfen);
        QString out = board.convertBoardToSfen();
        QCOMPARE(out, QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL"));
    }

    void sfen_roundTrip_midgame()
    {
        // A midgame position with some pieces moved
        QString midSfen = QStringLiteral("lnsgkgsnl/1r5b1/pppppp1pp/6p2/9/2P6/PP1PPPPPP/1B5R1/LNSGKGSNL w - 2");
        ShogiBoard board;
        board.setSfen(midSfen);
        QString out = board.convertBoardToSfen();
        QCOMPARE(out, QStringLiteral("lnsgkgsnl/1r5b1/pppppp1pp/6p2/9/2P6/PP1PPPPPP/1B5R1/LNSGKGSNL"));
    }

    void sfen_promotedPieces()
    {
        // Position with promoted pieces
        // +P at rank 4, file 3 (SFEN: 6+P2 means 6 empty then +P then 2 empty)
        QString sfen = QStringLiteral("lnsgkgsnl/1r5b1/pppppp1pp/6+P2/9/9/PP1PPPPPP/1B5R1/LNSGKGSNL w - 2");
        ShogiBoard board;
        board.setSfen(sfen);

        const auto& data = board.boardData();
        // SFEN file order is 9→1, so 6+P2 = files 9-4 empty, file 3 has +P, files 2-1 empty
        // index = (rank-1)*9 + (file-1) = (4-1)*9 + (3-1) = 29
        QCOMPARE(data[29], QLatin1Char('Q')); // +P → internal 'Q'
    }

    void convertStandToSfen()
    {
        ShogiBoard board;
        board.setSfen(kHirateSfen);
        // Initially no pieces on stand
        QCOMPARE(board.convertStandToSfen(), QStringLiteral("-"));
    }

    void convertStandToSfen_withPieces()
    {
        // Position with pieces on stand
        QString sfen = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b 3P 1");
        ShogiBoard board;
        board.setSfen(sfen);
        QString stand = board.convertStandToSfen();
        QVERIFY(stand.contains(QLatin1Char('P')));
    }

    // === Piece Movement ===

    void movePiece_basic()
    {
        ShogiBoard board;
        board.setSfen(kHirateSfen);

        // Move pawn from 7g (file=7, rank=7) to 7f (file=7, rank=6)
        board.movePieceToSquare(QLatin1Char('P'), 7, 7, 7, 6, false);

        const auto& data = board.boardData();
        // 7g should be empty: index = (7-1)*9 + (7-1) = 60
        QCOMPARE(data[60], QLatin1Char(' '));
        // 7f should have P: index = (6-1)*9 + (7-1) = 51
        QCOMPARE(data[51], QLatin1Char('P'));
    }

    void movePiece_withPromotion()
    {
        ShogiBoard board;
        // Set up a position where P can promote
        QString sfen = QStringLiteral("lnsgkgsnl/1r5b1/ppppPpppp/9/9/9/PPPP1PPPP/1B5R1/LNSGKGSNL b - 1");
        board.setSfen(sfen);

        // Move P from rank 3 to rank 2 with promotion
        board.movePieceToSquare(QLatin1Char('P'), 5, 3, 5, 2, true);

        const auto& data = board.boardData();
        // 5b (file=5, rank=2): index = (2-1)*9 + (5-1) = 13
        QCOMPARE(data[13], QLatin1Char('Q')); // Promoted pawn
    }

    // === Piece Stand ===

    void addPieceToStand_normalPiece()
    {
        ShogiBoard board;
        board.setSfen(kHirateSfen);
        int before = board.getPieceStand().value(QLatin1Char('P'), 0);
        board.addPieceToStand(QLatin1Char('p')); // Capture opponent's pawn
        int after = board.getPieceStand().value(QLatin1Char('P'), 0);
        QCOMPARE(after, before + 1);
    }

    void addPieceToStand_promotedPiece()
    {
        ShogiBoard board;
        board.setSfen(kHirateSfen);
        int before = board.getPieceStand().value(QLatin1Char('P'), 0);
        // Capture promoted pawn 'q' (opponent's +P) → should convert to 'P'
        board.addPieceToStand(QLatin1Char('q'));
        int after = board.getPieceStand().value(QLatin1Char('P'), 0);
        QCOMPARE(after, before + 1);
    }

    void incrementDecrementStand()
    {
        ShogiBoard board;
        board.setSfen(kHirateSfen);

        int before = board.getPieceStand().value(QLatin1Char('P'), 0);
        board.incrementPieceOnStand(QLatin1Char('P'));
        QCOMPARE(board.getPieceStand().value(QLatin1Char('P'), 0), before + 1);

        board.decrementPieceOnStand(QLatin1Char('P'));
        QCOMPARE(board.getPieceStand().value(QLatin1Char('P'), 0), before);
    }

    // === Board Operations ===

    void resetGameBoard()
    {
        ShogiBoard board;
        board.setSfen(kHirateSfen);
        board.resetGameBoard();

        const auto& data = board.boardData();
        // All squares should be empty
        for (int i = 0; i < 81; ++i) {
            QCOMPARE(data[i], QLatin1Char(' '));
        }
    }

    void flipSides()
    {
        // Use a clearly asymmetric position:
        // K at rank1 file9, P at rank2 file9, k at rank9 file1
        ShogiBoard board;
        board.setSfen(QStringLiteral("K8/P8/9/9/9/9/9/9/8k b - 1"));

        const auto& data = board.boardData();
        // P at rank2 file9: index = (2-1)*9 + (9-1) = 17
        QCOMPARE(data[17], QLatin1Char('P'));
        // rank8 file1: index = (8-1)*9 + (1-1) = 63, should be empty
        QCOMPARE(data[63], QLatin1Char(' '));

        board.flipSides();

        // After 180° rotation + case swap:
        // P at [17] → p at [80-17] = [63]
        QCOMPARE(board.boardData()[63], QLatin1Char('p'));
        // Original [17] should now be empty
        QCOMPARE(board.boardData()[17], QLatin1Char(' '));
    }

    // === Signals ===

    void signal_boardReset()
    {
        ShogiBoard board;
        QSignalSpy spy(&board, &ShogiBoard::boardReset);
        QVERIFY(spy.isValid());

        board.setSfen(kHirateSfen);
        QVERIFY(spy.count() >= 1);
    }

    void signal_dataChanged()
    {
        ShogiBoard board;
        board.setSfen(kHirateSfen);

        QSignalSpy spy(&board, &ShogiBoard::dataChanged);
        QVERIFY(spy.isValid());

        board.movePieceToSquare(QLatin1Char('P'), 7, 7, 7, 6, false);
        QVERIFY(spy.count() >= 1);
    }
};

QTEST_MAIN(TestShogiBoard)
#include "tst_shogiboard.moc"
