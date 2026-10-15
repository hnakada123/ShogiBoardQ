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

    void trySetSfen_readsHandCountsAndReplacesStand()
    {
        ShogiBoard board;
        QVERIFY(board.trySetSfen(QStringLiteral("4k4/9/9/9/9/9/9/9/4K4 w 2BG 5")));

        // 2桁の枚数、同じ駒の繰り返し（足す）、先後の駒。前の局面の持駒は残らない
        QVERIFY(board.trySetSfen(QStringLiteral("4k4/9/9/9/9/9/9/9/4K4 b S2P3P10p 1")));
        QCOMPARE(board.pieceStandCount(Piece::BlackSilver), 1);
        QCOMPARE(board.pieceStandCount(Piece::BlackPawn), 5);
        QCOMPARE(board.pieceStandCount(Piece::WhitePawn), 10);
        QCOMPARE(board.pieceStandCount(Piece::BlackBishop), 0);
        QCOMPARE(board.pieceStandCount(Piece::BlackGold), 0);
        QCOMPARE(board.pieceStand().size(), 16);
        QCOMPARE(board.convertStandToSfen(), QStringLiteral("S5P10p"));
        QCOMPARE(board.currentPlayer(), Turn::Black);

        QVERIFY(board.trySetSfen(QStringLiteral("4k4/9/9/9/9/9/9/9/4K4 w - 1")));
        QCOMPARE(board.convertStandToSfen(), QStringLiteral("-"));
        QCOMPARE(board.currentPlayer(), Turn::White);
    }

    void trySetSfen_rejectsInvalidAndKeepsPosition_data()
    {
        QTest::addColumn<QString>("sfen");
        QTest::newRow("black-pawn-rank1") << QStringLiteral("P3k4/9/9/9/9/9/9/9/4K4 b - 1");
        QTest::newRow("black-lance-rank1") << QStringLiteral("L3k4/9/9/9/9/9/9/9/4K4 b - 1");
        QTest::newRow("black-knight-rank2") << QStringLiteral("4k4/N8/9/9/9/9/9/9/4K4 b - 1");
        QTest::newRow("white-pawn-rank9") << QStringLiteral("4k4/9/9/9/9/9/9/9/p3K4 b - 1");
        QTest::newRow("white-knight-rank8") << QStringLiteral("4k4/9/9/9/9/9/9/n8/4K4 b - 1");
        QTest::newRow("promoted-gold") << QStringLiteral("4k4/9/9/9/9/9/9/9/+G3K4 b - 1");
        QTest::newRow("plus-at-rank-end") << QStringLiteral("4k3+/9/9/9/9/9/9/9/4K4 b - 1");
        QTest::newRow("unknown-letter") << QStringLiteral("4k4/9/9/4X4/9/9/9/9/4K4 b - 1");
        QTest::newRow("internal-promoted-letter") << QStringLiteral("4k4/9/9/9/4Q4/9/9/9/4K4 b - 1");
        QTest::newRow("internal-promoted-letter-white") << QStringLiteral("4k4/9/9/9/4c4/9/9/9/4K4 b - 1");
        QTest::newRow("zero-empty") << QStringLiteral("4k4/09/9/9/9/9/9/9/4K4 b - 1");
        QTest::newRow("fullwidth-digit") << QStringLiteral("4k4/\uFF19/9/9/9/9/9/9/4K4 b - 1");
        QTest::newRow("ten-squares") << QStringLiteral("4k5/9/9/9/9/9/9/9/4K4 b - 1");
        QTest::newRow("eight-squares") << QStringLiteral("4k3/9/9/9/9/9/9/9/4K4 b - 1");
        QTest::newRow("eight-ranks") << QStringLiteral("4k4/9/9/9/9/9/9/4K4 b - 1");
        QTest::newRow("ten-ranks") << QStringLiteral("4k4/9/9/9/9/9/9/9/4K4/9 b - 1");
        QTest::newRow("empty-rank") << QStringLiteral("4k4//9/9/9/9/9/9/4K4 b - 1");
        QTest::newRow("hand-three-digits") << QStringLiteral("4k4/9/9/9/9/9/9/9/4K4 b 123P 1");
        QTest::newRow("hand-count-at-end") << QStringLiteral("4k4/9/9/9/9/9/9/9/4K4 b P2 1");
        QTest::newRow("hand-promoted") << QStringLiteral("4k4/9/9/9/9/9/9/9/4K4 b +P 1");
        QTest::newRow("turn") << QStringLiteral("4k4/9/9/9/9/9/9/9/4K4 x - 1");
        QTest::newRow("move-number") << QStringLiteral("4k4/9/9/9/9/9/9/9/4K4 b - 0");
        QTest::newRow("five-fields") << QStringLiteral("4k4/9/9/9/9/9/9/9/4K4 b - 1 1");
    }

    void trySetSfen_rejectsInvalidAndKeepsPosition()
    {
        QFETCH(QString, sfen);
        static const QString kBefore = QStringLiteral("lnsgk2nl/1r4gs1/p1pppp1pp/6p2/1p5P1/2P6/PP1PPPP1P/1BG4R1/LN1SKGSNL w Bb 12");

        ShogiBoard board;
        QVERIFY(board.trySetSfen(kBefore));
        QSignalSpy resetSpy(&board, &ShogiBoard::boardReset);

        QVERIFY(!board.trySetSfen(sfen));
        QCOMPARE(resetSpy.count(), 0);
        QCOMPARE(board.convertBoardToSfen(), kBefore.section(QLatin1Char(' '), 0, 0));
        QCOMPARE(board.convertStandToSfen(), QStringLiteral("Bb"));
        QCOMPARE(board.currentPlayer(), Turn::White);
        QCOMPARE(board.pieceStand().size(), 16);
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
        int before = board.pieceStand().value(Piece::BlackPawn, 0);
        board.addPieceToStand(Piece::WhitePawn); // Capture opponent's pawn
        int after = board.pieceStand().value(Piece::BlackPawn, 0);
        QCOMPARE(after, before + 1);
    }

    void addPieceToStand_promotedPiece()
    {
        ShogiBoard board;
        board.setSfen(kHirateSfen);
        int before = board.pieceStand().value(Piece::BlackPawn, 0);
        // Capture promoted pawn (opponent's +P) -> should convert to P
        board.addPieceToStand(Piece::WhitePromotedPawn);
        int after = board.pieceStand().value(Piece::BlackPawn, 0);
        QCOMPARE(after, before + 1);
    }

    void incrementDecrementStand()
    {
        ShogiBoard board;
        board.setSfen(kHirateSfen);

        int before = board.pieceStand().value(Piece::BlackPawn, 0);
        board.incrementPieceOnStand(Piece::BlackPawn);
        QCOMPARE(board.pieceStand().value(Piece::BlackPawn, 0), before + 1);

        board.decrementPieceOnStand(Piece::BlackPawn);
        QCOMPARE(board.pieceStand().value(Piece::BlackPawn, 0), before);
    }

    // === Board Operations ===

    void resetGameBoard_data()
    {
        QTest::addColumn<QString>("sfen");
        QTest::newRow("hirate") << kHirateSfen;
        QTest::newRow("moved-kings") << QStringLiteral("9/3k5/9/9/9/9/9/5K3/9 w Pp 1");
        QTest::newRow("tsume") << QStringLiteral("4k4/9/9/9/9/9/9/9/9 b R 1");
    }

    void resetGameBoard()
    {
        QFETCH(QString, sfen);
        ShogiBoard board;
        board.setSfen(sfen);
        const auto turn = board.currentPlayer();
        board.resetGameBoard();

        QCOMPARE(board.convertBoardToSfen(), QStringLiteral("9/9/9/9/9/9/9/9/9"));
        QCOMPARE(board.pieceStandCount(Piece::BlackKing), 0);
        QCOMPARE(board.pieceStandCount(Piece::WhiteKing), 0);
        QCOMPARE(board.convertStandToSfen(), QStringLiteral("-"));
        const QMap<Piece, int> allPieces = {
            {Piece::BlackPawn, 18}, {Piece::BlackLance, 4}, {Piece::BlackKnight, 4},
            {Piece::BlackSilver, 4}, {Piece::BlackGold, 4}, {Piece::BlackBishop, 2},
            {Piece::BlackRook, 2}, {Piece::BlackKing, 2}};
        QCOMPARE(board.pieceBox(), allPieces);
        QCOMPARE(board.currentPlayer(), turn);
        ShogiBoard restored;
        restored.setSfen(board.convertBoardToSfen() + QStringLiteral(" b ")
                        + board.convertStandToSfen() + QStringLiteral(" 1"));
        QCOMPARE(restored.boardData(), board.boardData());
        QCOMPARE(restored.pieceStand(), board.pieceStand());
        QCOMPARE(restored.pieceBox(), board.pieceBox());
    }

    void pieceBox_countsUnusedPieces()
    {
        ShogiBoard board;
        board.setSfen(kHirateSfen);
        for (int count : board.pieceBox()) QCOMPARE(count, 0);
        // 成駒・先後・持ち駒を同じ駒種として数える。
        board.setSfen(QStringLiteral("4k4/9/9/4+p4/9/4+R4/9/9/4K4 w 2Pr 1"));
        QCOMPARE(board.pieceBoxCount(Piece::BlackPawn), 15);
        QCOMPARE(board.pieceBoxCount(Piece::WhitePromotedPawn), 15);
        QCOMPARE(board.pieceBoxCount(Piece::BlackRook), 0);
        QCOMPARE(board.pieceBoxCount(Piece::BlackKing), 0);
        // SFENの往復でも未使用の玉を復元できる。
        board.setSfen(QStringLiteral("4k4/9/9/9/9/9/9/9/9 b R 1"));
        QCOMPARE(board.pieceBoxCount(Piece::BlackKing), 1);
        board.flipSides();
        QCOMPARE(board.pieceBoxCount(Piece::BlackKing), 1);
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

    // 駒台の枚数が変わったときは standChanged で知らせる（盤面の部分的な描き直しに使う）
    void signal_standChanged()
    {
        ShogiBoard board;
        board.setSfen(kHirateSfen);

        QSignalSpy spy(&board, &ShogiBoard::standChanged);
        QVERIFY(spy.isValid());

        board.addStandPiece(Piece::BlackPawn);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).value<Piece>(), Piece::BlackPawn);

        QVERIFY(board.consumeStandPiece(Piece::BlackPawn));
        QCOMPARE(spy.count(), 2);
        // 変わらなかったときは知らせない
        QVERIFY(!board.consumeStandPiece(Piece::BlackPawn));
        QCOMPARE(spy.count(), 2);

        // 駒を取ると、取った側の駒台が増える
        board.setSfen(QStringLiteral("lnsgkgsnl/1r5b1/pppppp1pp/6p2/9/2P6/PP1PPPPPP/1B5R1/LNSGKGSNL b - 1"));
        spy.clear();
        board.updateBoardAndPieceStand(Piece::BlackBishop, Piece::WhiteBishop, 8, 8, 2, 2, false);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).value<Piece>(), Piece::BlackBishop);
        QCOMPARE(board.pieceStandCount(Piece::BlackBishop), 1);
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
        QCOMPARE(board.pieceCharacter(5, 1), Piece::WhiteKing);
        // file=5, rank=9 should be black King
        QCOMPARE(board.pieceCharacter(5, 9), Piece::BlackKing);
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

    // === toSfen ===

    void toSfen_usesTurnAndMoveNumber()
    {
        ShogiBoard board;
        board.setSfen(kHirateSfen);
        QCOMPARE(board.toSfen(), kHirateSfen);
        board.setCurrentPlayer(Turn::White);
        board.setMoveNumber(12);
        QCOMPARE(board.toSfen(), QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 12"));
    }

    /// 指すと手番が替わり、手数が1つ進む
    void applyUsiMove_advancesTurnAndMoveNumber()
    {
        ShogiBoard board;
        board.setSfen(kHirateSfen);
        QVERIFY(board.applyUsiMove(*UsiMove::parse(QStringLiteral("7g7f"))));
        QCOMPARE(board.currentPlayer(), Turn::White);
        QCOMPARE(board.moveNumber(), 2);
        QVERIFY(board.toSfen().endsWith(QStringLiteral(" w - 2")));
    }

    // === Mandatory promotion ===

    void movePiece_mandatoryPromotion_blackPawnRank1()
    {
        // 先手歩が1段目に移動 → 自動で成り
        QString sfen = QStringLiteral("4k4/4P4/9/9/9/9/9/9/4K4 b - 1");
        ShogiBoard board;
        board.setSfen(sfen);
        board.movePieceToSquare(Piece::BlackPawn, 5, 2, 5, 1, false);
        QCOMPARE(board.pieceCharacter(5, 1), Piece::BlackPromotedPawn);
    }

    void movePiece_mandatoryPromotion_blackKnightRank2()
    {
        // 先手桂が2段目に移動 → 自動で成り
        QString sfen = QStringLiteral("4k4/9/4N4/9/9/9/9/9/4K4 b - 1");
        ShogiBoard board;
        board.setSfen(sfen);
        board.movePieceToSquare(Piece::BlackKnight, 5, 3, 4, 1, false);
        QCOMPARE(board.pieceCharacter(4, 1), Piece::BlackPromotedKnight);
    }

    void movePiece_mandatoryPromotion_whitePawnRank9()
    {
        // 後手歩が9段目に移動 → 自動で成り
        QString sfen = QStringLiteral("4K4/9/9/9/9/9/9/4p4/4k4 w - 1");
        ShogiBoard board;
        board.setSfen(sfen);
        board.movePieceToSquare(Piece::WhitePawn, 5, 8, 5, 9, false);
        QCOMPARE(board.pieceCharacter(5, 9), Piece::WhitePromotedPawn);
    }

    // === pieceAt / setPieceAt ===

    void pieceAt_readsBoardSquares()
    {
        ShogiBoard board;
        board.setSfen(kHirateSfen);
        QCOMPARE(board.pieceAt(BoardSquare{7, 7}), Piece::BlackPawn);
        QCOMPARE(board.pieceAt(BoardSquare{2, 2}), Piece::WhiteBishop);
        QCOMPARE(board.pieceAt(BoardSquare{5, 5}), Piece::None);
        QCOMPARE(board.pieceAt(BoardSquare{0, 5}), Piece::None);
        QCOMPARE(board.pieceAt(BoardSquare{5, 10}), Piece::None);
    }

    // 局面の組み立て用。成りの補正も駒台の増減もしない
    void setPieceAt_placesPieceAsGiven()
    {
        ShogiBoard board;
        QSignalSpy changed(&board, &ShogiBoard::dataChanged);
        board.setPieceAt(BoardSquare{5, 1}, Piece::BlackPawn);
        QCOMPARE(board.pieceAt(BoardSquare{5, 1}), Piece::BlackPawn);
        QCOMPARE(changed.count(), 1);
        board.setPieceAt(BoardSquare{10, 1}, Piece::BlackGold);
        QCOMPARE(changed.count(), 1);
        QCOMPARE(board.pieceStandCount(Piece::BlackPawn), 0);
        QCOMPARE(board.convertBoardToSfen(), QStringLiteral("4P4/9/9/9/9/9/9/9/9"));
    }

    // === copyPositionFrom ===

    void copyPositionFrom_copiesBoardStandTurnAndIsIndependent()
    {
        const QString sfen = QStringLiteral("lnsgkgsnl/1r5+B1/pppppp1pp/6p2/9/2P6/PP1PPPPPP/7R1/LNSGKGSNL w B 4");
        ShogiBoard source;
        source.setSfen(sfen);
        ShogiBoard copy;
        copy.setSfen(QStringLiteral("4k4/9/9/9/9/9/9/9/4K4 b 2P 9"));
        QSignalSpy reset(&copy, &ShogiBoard::boardReset);

        copy.copyPositionFrom(source);
        QCOMPARE(reset.count(), 1);
        QCOMPARE(copy.boardData(), source.boardData());
        QCOMPARE(copy.pieceStand(), source.pieceStand());
        QCOMPARE(copy.currentPlayer(), Turn::White);
        QCOMPARE(copy.convertBoardToSfen() + QStringLiteral(" w ") + copy.convertStandToSfen(),
                 sfen.left(sfen.lastIndexOf(QLatin1Char(' '))));

        // 写した後で進めても、写し元は変わらない
        QVERIFY(copy.applyUsiMove(*UsiMove::parse(u"3a2b"), Turn::White));
        QVERIFY(copy.applyUsiMove(*UsiMove::parse(u"B*4e"), Turn::Black));
        QCOMPARE(source.pieceAt(BoardSquare{2, 2}), Piece::BlackHorse);
        QCOMPARE(source.pieceStandCount(Piece::BlackBishop), 1);
        QCOMPARE(copy.pieceStandCount(Piece::BlackBishop), 0);
        QCOMPARE(copy.pieceStandCount(Piece::WhiteBishop), 1);
    }

    // === 駒台の表の共有 ===

    void newBoards_keepStandChangesToThemselves()
    {
        // 作ったばかりの盤は0枚の駒台の表を共有する。どの書き換え方でも、ほかの盤には及ばない
        ShogiBoard added;
        ShogiBoard loaded;
        ShogiBoard emptyLoaded;
        added.addStandPiece(Piece::BlackPawn, 3);
        QVERIFY(loaded.trySetSfen(QStringLiteral("4k4/9/9/9/9/9/9/9/4K4 b R2p 1")));
        QVERIFY(emptyLoaded.trySetSfen(QStringLiteral("4k4/9/9/9/9/9/9/9/4K4 b - 1")));

        QCOMPARE(added.convertStandToSfen(), QStringLiteral("3P"));
        QCOMPARE(loaded.convertStandToSfen(), QStringLiteral("R2p"));
        QCOMPARE(emptyLoaded.convertStandToSfen(), QStringLiteral("-"));

        added.initStand();
        QCOMPARE(added.convertStandToSfen(), QStringLiteral("-"));
        QCOMPARE(loaded.convertStandToSfen(), QStringLiteral("R2p"));

        const ShogiBoard fresh;
        QCOMPARE(fresh.pieceStand().size(), 16);
        QCOMPARE(fresh.convertStandToSfen(), QStringLiteral("-"));
        QCOMPARE(fresh.pieceStand(), added.pieceStand());
    }

    // === initStand ===

    void initStand_clearsAll()
    {
        ShogiBoard board;
        // Add some pieces to stand
        board.incrementPieceOnStand(Piece::BlackPawn);
        board.incrementPieceOnStand(Piece::BlackPawn);
        QCOMPARE(board.pieceStand().value(Piece::BlackPawn, 0), 2);

        board.initStand();
        QCOMPARE(board.pieceStand().value(Piece::BlackPawn, 0), 0);
    }
};

QTEST_MAIN(TestShogiBoard)
#include "tst_shogiboard.moc"
