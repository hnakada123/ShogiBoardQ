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
        QCOMPARE(data[0], Piece::WhiteLance);
        QCOMPARE(data[4], Piece::WhiteKing);
        QCOMPARE(data[8], Piece::WhiteLance);

        // Rank 7 (index 54-62): P P P P P P P P P
        for (int i = 54; i <= 62; ++i) {
            QCOMPARE(data[i], Piece::BlackPawn);
        }

        // Rank 9 (index 72-80): L N S G K G S N L
        QCOMPARE(data[72], Piece::BlackLance);
        QCOMPARE(data[76], Piece::BlackKing);
        QCOMPARE(data[80], Piece::BlackLance);
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
        // SFEN file order is 9->1, so 6+P2 = files 9-4 empty, file 3 has +P, files 2-1 empty
        // index = (rank-1)*9 + (file-1) = (4-1)*9 + (3-1) = 29
        QCOMPARE(data[29], Piece::BlackPromotedPawn); // +P -> internal 'Q'
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
        board.movePieceToSquare(Piece::BlackPawn, 7, 7, 7, 6, false);

        const auto& data = board.boardData();
        // 7g should be empty: index = (7-1)*9 + (7-1) = 60
        QCOMPARE(data[60], Piece::None);
        // 7f should have P: index = (6-1)*9 + (7-1) = 51
        QCOMPARE(data[51], Piece::BlackPawn);
    }

    void movePiece_withPromotion()
    {
        ShogiBoard board;
        // Set up a position where P can promote
        QString sfen = QStringLiteral("lnsgkgsnl/1r5b1/ppppPpppp/9/9/9/PPPP1PPPP/1B5R1/LNSGKGSNL b - 1");
        board.setSfen(sfen);

        // Move P from rank 3 to rank 2 with promotion
        board.movePieceToSquare(Piece::BlackPawn, 5, 3, 5, 2, true);

        const auto& data = board.boardData();
        // 5b (file=5, rank=2): index = (2-1)*9 + (5-1) = 13
        QCOMPARE(data[13], Piece::BlackPromotedPawn); // Promoted pawn
    }

    // === Piece Stand ===

    void addPieceToStand_normalPiece()
    {
        ShogiBoard board;
        board.setSfen(kHirateSfen);
        int before = board.getPieceStand().value(Piece::BlackPawn, 0);
        board.addPieceToStand(Piece::WhitePawn); // Capture opponent's pawn
        int after = board.getPieceStand().value(Piece::BlackPawn, 0);
        QCOMPARE(after, before + 1);
    }

    void addPieceToStand_promotedPiece()
    {
        ShogiBoard board;
        board.setSfen(kHirateSfen);
        int before = board.getPieceStand().value(Piece::BlackPawn, 0);
        // Capture promoted pawn (opponent's +P) -> should convert to P
        board.addPieceToStand(Piece::WhitePromotedPawn);
        int after = board.getPieceStand().value(Piece::BlackPawn, 0);
        QCOMPARE(after, before + 1);
    }

    void incrementDecrementStand()
    {
        ShogiBoard board;
        board.setSfen(kHirateSfen);

        int before = board.getPieceStand().value(Piece::BlackPawn, 0);
        board.incrementPieceOnStand(Piece::BlackPawn);
        QCOMPARE(board.getPieceStand().value(Piece::BlackPawn, 0), before + 1);

        board.decrementPieceOnStand(Piece::BlackPawn);
        QCOMPARE(board.getPieceStand().value(Piece::BlackPawn, 0), before);
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
            QCOMPARE(data[i], Piece::None);
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
        QCOMPARE(data[17], Piece::BlackPawn);
        // rank8 file1: index = (8-1)*9 + (1-1) = 63, should be empty
        QCOMPARE(data[63], Piece::None);

        board.flipSides();

        // After 180 rotation + case swap:
        // P at [17] -> p at [80-17] = [63]
        QCOMPARE(board.boardData()[63], Piece::WhitePawn);
        // Original [17] should now be empty
        QCOMPARE(board.boardData()[17], Piece::None);
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

        board.movePieceToSquare(Piece::BlackPawn, 7, 7, 7, 6, false);
        QVERIFY(spy.count() >= 1);
    }

    // === parseSfen (static) ===

    void parseSfen_valid()
    {
        auto result = ShogiBoard::parseSfen(kHirateSfen);
        QVERIFY(result.has_value());
        QCOMPARE(result->turn, Turn::Black);
        QCOMPARE(result->moveNumber, 1);
        QCOMPARE(result->stand, QStringLiteral("-"));
    }

    void parseSfen_whiteTurn()
    {
        auto result = ShogiBoard::parseSfen(
            QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 2"));
        QVERIFY(result.has_value());
        QCOMPARE(result->turn, Turn::White);
        QCOMPARE(result->moveNumber, 2);
    }

    void parseSfen_invalid_tooFewParts()
    {
        auto result = ShogiBoard::parseSfen(QStringLiteral("lnsgkgsnl/1r5b1 b"));
        QVERIFY(!result.has_value());
    }

    void parseSfen_invalid_turn()
    {
        auto result = ShogiBoard::parseSfen(
            QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL x - 1"));
        QVERIFY(!result.has_value());
    }

    void parseSfen_invalid_moveNumber()
    {
        auto result = ShogiBoard::parseSfen(
            QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 0"));
        QVERIFY(!result.has_value());
    }

    // === currentPlayer ===

    void currentPlayer_afterSfen()
    {
        ShogiBoard board;
        board.setSfen(kHirateSfen);
        QCOMPARE(board.currentPlayer(), Turn::Black);

        board.setSfen(QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 2"));
        QCOMPARE(board.currentPlayer(), Turn::White);
    }

    // === Handicap positions ===

    void sfen_handicapKakuOchi()
    {
        // 角落ち: 上手（後手）の角がない
        QString sfen = QStringLiteral("lnsgkgsnl/1r7/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1");
        ShogiBoard board;
        board.setSfen(sfen);
        QString out = board.convertBoardToSfen();
        QCOMPARE(out, QStringLiteral("lnsgkgsnl/1r7/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL"));
    }

    void sfen_handicapNimaiOchi()
    {
        // 二枚落ち: 上手（後手）の飛車と角がない
        QString sfen = QStringLiteral("lnsgkgsnl/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1");
        ShogiBoard board;
        board.setSfen(sfen);
        const auto& data = board.boardData();
        // Rank 2 (index 9-17) should all be None
        for (int i = 9; i <= 17; ++i) {
            QCOMPARE(data[i], Piece::None);
        }
    }

    // === Piece Stand with multiple pieces ===

    void convertStandToSfen_multipleTypes()
    {
        // Position with rook and 2 pawns on stand
        QString sfen = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b R2P 1");
        ShogiBoard board;
        board.setSfen(sfen);
        QString stand = board.convertStandToSfen();
        QVERIFY(stand.contains(QLatin1Char('R')));
        QVERIFY(stand.contains(QLatin1Char('P')));
    }

    // === getPieceCharacter ===

    void getPieceCharacter_boardSquare()
    {
        ShogiBoard board;
        board.setSfen(kHirateSfen);
        // file=5, rank=1 should be white King (5a in SFEN terms)
        QCOMPARE(board.getPieceCharacter(5, 1), Piece::WhiteKing);
        // file=5, rank=9 should be black King
        QCOMPARE(board.getPieceCharacter(5, 9), Piece::BlackKing);
    }

    // === isPieceAvailableOnStand ===

    void isPieceAvailableOnStand_boardSquare()
    {
        ShogiBoard board;
        board.setSfen(kHirateSfen);
        // For board squares (file 1-9), should always return true
        QVERIFY(board.isPieceAvailableOnStand(Piece::BlackPawn, 5));
    }

    void isPieceAvailableOnStand_emptyStand()
    {
        ShogiBoard board;
        board.setSfen(kHirateSfen);
        // No pieces on stand initially
        QVERIFY(!board.isPieceAvailableOnStand(Piece::BlackPawn, 10));
    }

    void isPieceAvailableOnStand_withPieces()
    {
        // 歩が駒台にある
        QString sfen = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b P 1");
        ShogiBoard board;
        board.setSfen(sfen);
        QVERIFY(board.isPieceAvailableOnStand(Piece::BlackPawn, 10));
    }

    // === addSfenRecord ===

    void addSfenRecord_basic()
    {
        ShogiBoard board;
        board.setSfen(kHirateSfen);
        QStringList history;
        board.addSfenRecord(Turn::White, 0, &history);
        QCOMPARE(history.size(), 1);
        QVERIFY(history[0].contains(QStringLiteral("w")));
        QVERIFY(history[0].endsWith(QStringLiteral("1")));
    }

    // === Mandatory promotion ===

    void movePiece_mandatoryPromotion_blackPawnRank1()
    {
        // 先手歩が1段目に移動 → 自動で成り
        QString sfen = QStringLiteral("4k4/4P4/9/9/9/9/9/9/4K4 b - 1");
        ShogiBoard board;
        board.setSfen(sfen);
        board.movePieceToSquare(Piece::BlackPawn, 5, 2, 5, 1, false);
        QCOMPARE(board.getPieceCharacter(5, 1), Piece::BlackPromotedPawn);
    }

    void movePiece_mandatoryPromotion_blackKnightRank2()
    {
        // 先手桂が2段目に移動 → 自動で成り
        QString sfen = QStringLiteral("4k4/9/4N4/9/9/9/9/9/4K4 b - 1");
        ShogiBoard board;
        board.setSfen(sfen);
        board.movePieceToSquare(Piece::BlackKnight, 5, 3, 4, 1, false);
        QCOMPARE(board.getPieceCharacter(4, 1), Piece::BlackPromotedKnight);
    }

    void movePiece_mandatoryPromotion_whitePawnRank9()
    {
        // 後手歩が9段目に移動 → 自動で成り
        QString sfen = QStringLiteral("4K4/9/9/9/9/9/9/4p4/4k4 w - 1");
        ShogiBoard board;
        board.setSfen(sfen);
        board.movePieceToSquare(Piece::WhitePawn, 5, 8, 5, 9, false);
        QCOMPARE(board.getPieceCharacter(5, 9), Piece::WhitePromotedPawn);
    }

    // === initStand ===

    void initStand_clearsAll()
    {
        ShogiBoard board;
        // Add some pieces to stand
        board.incrementPieceOnStand(Piece::BlackPawn);
        board.incrementPieceOnStand(Piece::BlackPawn);
        QCOMPARE(board.getPieceStand().value(Piece::BlackPawn, 0), 2);

        board.initStand();
        QCOMPARE(board.getPieceStand().value(Piece::BlackPawn, 0), 0);
    }
};

QTEST_MAIN(TestShogiBoard)
#include "tst_shogiboard.moc"
