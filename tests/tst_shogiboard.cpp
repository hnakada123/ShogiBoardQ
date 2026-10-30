#include <QtTest>
#include <QSignalSpy>

#include "shogiboard.h"
#include "shogiposition.h"
#include "shogimove.h"
#include "usimove.h"

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

        const auto& data = board.position().squares();
        QCOMPARE(static_cast<int>(data.size()), 81);

        // Rank 1 (index 0-8): l n s g k g s n l
        QCOMPARE(data[0], Piece::WhiteLance);
        QCOMPARE(data[4], Piece::WhiteKing);
        QCOMPARE(data[8], Piece::WhiteLance);

        // Rank 7 (index 54-62): P P P P P P P P P
        for (std::size_t i = 54; i <= 62; ++i) {
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
        QString out = board.position().boardSfen();
        QCOMPARE(out, QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL"));
    }

    void sfen_roundTrip_midgame()
    {
        // A midgame position with some pieces moved
        QString midSfen = QStringLiteral("lnsgkgsnl/1r5b1/pppppp1pp/6p2/9/2P6/PP1PPPPPP/1B5R1/LNSGKGSNL w - 2");
        ShogiBoard board;
        board.setSfen(midSfen);
        QString out = board.position().boardSfen();
        QCOMPARE(out, QStringLiteral("lnsgkgsnl/1r5b1/pppppp1pp/6p2/9/2P6/PP1PPPPPP/1B5R1/LNSGKGSNL"));
    }

    void sfen_promotedPieces()
    {
        // Position with promoted pieces
        // +P at rank 4, file 3 (SFEN: 6+P2 means 6 empty then +P then 2 empty)
        QString sfen = QStringLiteral("lnsgkgsnl/1r5b1/pppppp1pp/6+P2/9/9/PP1PPPPPP/1B5R1/LNSGKGSNL w - 2");
        ShogiBoard board;
        board.setSfen(sfen);

        const auto& data = board.position().squares();
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
        QCOMPARE(board.position().handsSfen(), QStringLiteral("S5P10p"));
        QCOMPARE(board.currentPlayer(), Turn::Black);

        QVERIFY(board.trySetSfen(QStringLiteral("4k4/9/9/9/9/9/9/9/4K4 w - 1")));
        QCOMPARE(board.position().handsSfen(), QStringLiteral("-"));
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
        QCOMPARE(board.position().boardSfen(), kBefore.section(QLatin1Char(' '), 0, 0));
        QCOMPARE(board.position().handsSfen(), QStringLiteral("Bb"));
        QCOMPARE(board.currentPlayer(), Turn::White);
    }

    void convertStandToSfen()
    {
        ShogiBoard board;
        board.setSfen(kHirateSfen);
        // Initially no pieces on stand
        QCOMPARE(board.position().handsSfen(), QStringLiteral("-"));
    }

    void convertStandToSfen_withPieces()
    {
        // Position with pieces on stand
        QString sfen = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b 3P 1");
        ShogiBoard board;
        board.setSfen(sfen);
        QString stand = board.position().handsSfen();
        QVERIFY(stand.contains(QLatin1Char('P')));
    }

    // === 局面の置き換え（replacePosition） ===

    /// 変わったマス・駒台・手番だけを知らせる（局面を丸ごと替える setPosition は boardReset）
    void replacePosition_notifiesOnlyChanges()
    {
        ShogiBoard board;
        board.setSfen(kHirateSfen);
        QSignalSpy reset(&board, &ShogiBoard::boardReset);
        QSignalSpy data(&board, &ShogiBoard::dataChanged);
        QSignalSpy stand(&board, &ShogiBoard::standChanged);
        QSignalSpy turn(&board, &ShogiBoard::turnChanged);

        Position next = board.position();
        QVERIFY(next.apply(*UsiMove::parse(u"7g7f")));
        board.replacePosition(next);
        QCOMPARE(board.position(), next);
        QCOMPARE(reset.count(), 0);
        QCOMPARE(data.count(), 2);
        QCOMPARE(data.at(0).at(0).toInt(), 7);
        QCOMPARE(data.at(0).at(1).toInt(), 6);
        QCOMPARE(data.at(1).at(0).toInt(), 7);
        QCOMPARE(data.at(1).at(1).toInt(), 7);
        QCOMPARE(stand.count(), 0);
        QCOMPARE(turn.count(), 1);
        QCOMPARE(turn.at(0).at(0).value<Turn>(), Turn::White);

        // 同じ局面なら何も知らせない
        board.replacePosition(next);
        QCOMPARE(data.count(), 2);
        QCOMPARE(turn.count(), 1);
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

    // 駒台の枚数が変わったときは standChanged で知らせる（盤面の部分的な描き直しに使う）
    void signal_standChanged()
    {
        ShogiBoard board;
        board.setSfen(kHirateSfen);

        QSignalSpy spy(&board, &ShogiBoard::standChanged);
        QVERIFY(spy.isValid());

        Position next = board.position();
        next.addToHand(Piece::BlackPawn);
        board.replacePosition(next);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).value<Piece>(), Piece::BlackPawn);

        // 駒を取ると、取った側の駒台が増える
        board.setSfen(QStringLiteral("lnsgkgsnl/1r5b1/pppppp1pp/6p2/9/2P6/PP1PPPPPP/1B5R1/LNSGKGSNL b - 1"));
        spy.clear();
        next = board.position();
        QVERIFY(next.apply(*UsiMove::parse(u"8h2b")));
        board.replacePosition(next);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).value<Piece>(), Piece::BlackBishop);
        QCOMPARE(board.pieceStandCount(Piece::BlackBishop), 1);
    }

    // === Position::fromSfen ===

    void parseSfen_valid()
    {
        auto result = Position::fromSfen(kHirateSfen);
        QVERIFY(result.has_value());
        QCOMPARE(result->sideToMove(), Turn::Black);
        QCOMPARE(result->moveNumber(), 1);
        QCOMPARE(result->handsSfen(), QStringLiteral("-"));
        QCOMPARE(result->toSfen(), kHirateSfen);
        QCOMPARE(*result, Position::hirate());
    }

    void parseSfen_whiteTurn()
    {
        auto result = Position::fromSfen(
            QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 2"));
        QVERIFY(result.has_value());
        QCOMPARE(result->sideToMove(), Turn::White);
        QCOMPARE(result->moveNumber(), 2);
    }

    void parseSfen_invalid_tooFewParts()
    {
        QVERIFY(!Position::fromSfen(QStringLiteral("lnsgkgsnl/1r5b1 b")).has_value());
    }

    void parseSfen_invalid_turn()
    {
        QVERIFY(!Position::fromSfen(
            QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL x - 1")).has_value());
    }

    void parseSfen_invalid_moveNumber()
    {
        QVERIFY(!Position::fromSfen(
            QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 0")).has_value());
    }

    /// "startpos"・"position ..."・手数を省いた局面も読み、moves 以降は指して進める
    void parseSfenLike_positionCommands()
    {
        QCOMPARE(Position::fromSfenLike(u"startpos")->toSfen(), kHirateSfen);
        QCOMPARE(Position::fromSfenLike(u"position startpos moves 7g7f 3c3d")->toSfen(),
                 QStringLiteral("lnsgkgsnl/1r5b1/pppppp1pp/6p2/9/2P6/PP1PPPPPP/1B5R1/LNSGKGSNL b - 3"));
        QCOMPARE(Position::fromSfenLike(u"4k4/9/9/9/9/9/9/9/4K4 w G")->toSfen(),
                 QStringLiteral("4k4/9/9/9/9/9/9/9/4K4 w G 1"));
        QVERIFY(!Position::fromSfenLike(u"position startpos moves 5e5d").has_value());
    }

    /// 駒の値は色と種類の計算で成る・戻す・先後を入れ替える
    void pieceArithmetic()
    {
        QCOMPARE(promote(Piece::BlackPawn), Piece::BlackPromotedPawn);
        QCOMPARE(promote(Piece::WhiteSilver), Piece::WhitePromotedSilver);
        QCOMPARE(promote(Piece::BlackBishop), Piece::BlackHorse);
        QCOMPARE(promote(Piece::WhiteRook), Piece::WhiteDragon);
        QCOMPARE(promote(Piece::BlackGold), Piece::BlackGold);
        QCOMPARE(promote(Piece::WhiteKing), Piece::WhiteKing);
        QCOMPARE(promote(Piece::BlackHorse), Piece::BlackHorse);
        QCOMPARE(demote(Piece::WhiteDragon), Piece::WhiteRook);
        QCOMPARE(demote(Piece::BlackPromotedKnight), Piece::BlackKnight);
        QCOMPARE(demote(Piece::BlackGold), Piece::BlackGold);
        QCOMPARE(flipSide(Piece::BlackHorse), Piece::WhiteHorse);
        QCOMPARE(capturedToHand(Piece::WhiteDragon), Piece::BlackRook);
        QCOMPARE(pieceSfen(Piece::WhitePromotedLance), QStringLiteral("+l"));
        QCOMPARE(charToPiece(QLatin1Char('Q')), Piece::None);
        QVERIFY(!isBlackPiece(Piece::None));
        QVERIFY(!isWhitePiece(Piece::None));
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
        QString out = board.position().boardSfen();
        QCOMPARE(out, QStringLiteral("lnsgkgsnl/1r7/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL"));
    }

    void sfen_handicapNimaiOchi()
    {
        // 二枚落ち: 上手（後手）の飛車と角がない
        QString sfen = QStringLiteral("lnsgkgsnl/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1");
        ShogiBoard board;
        board.setSfen(sfen);
        const auto& data = board.position().squares();
        // Rank 2 (index 9-17) should all be None
        for (std::size_t i = 9; i <= 17; ++i) {
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
        QString stand = board.position().handsSfen();
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

    /// 指した後の局面に置き換えると、手番が替わり、手数が1つ進む
    void replacePosition_advancesTurnAndMoveNumber()
    {
        ShogiBoard board;
        board.setSfen(kHirateSfen);
        Position next = board.position();
        QVERIFY(next.apply(*UsiMove::parse(QStringLiteral("7g7f"))));
        board.replacePosition(next);
        QCOMPARE(board.currentPlayer(), Turn::White);
        QCOMPARE(board.moveNumber(), 2);
        QVERIFY(board.toSfen().endsWith(QStringLiteral(" w - 2")));
    }

    // === pieceAt ===

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

    // === copyPositionFrom ===

    void copyPositionFrom_copiesBoardStandTurnAndIsIndependent()
    {
        const QString sfen = QStringLiteral("lnsgkgsnl/1r5+B1/pppppp1pp/6p2/9/2P6/PP1PPPPPP/7R1/LNSGKGSNL w B 4");
        ShogiBoard source;
        source.setSfen(sfen);
        ShogiBoard copy;
        copy.setSfen(QStringLiteral("4k4/9/9/9/9/9/9/9/4K4 b 2P 9"));
        QSignalSpy reset(&copy, &ShogiBoard::boardReset);

        copy.setPosition(source.position());
        QCOMPARE(reset.count(), 1);
        QCOMPARE(copy.position(), source.position());
        QCOMPARE(copy.currentPlayer(), Turn::White);
        QCOMPARE(copy.position().boardSfen() + QStringLiteral(" w ") + copy.position().handsSfen(),
                 sfen.left(sfen.lastIndexOf(QLatin1Char(' '))));

        // 写した後で進めても、写し元は変わらない
        Position next = copy.position();
        QVERIFY(next.applyAs(*UsiMove::parse(u"3a2b"), Turn::White));
        QVERIFY(next.applyAs(*UsiMove::parse(u"B*4e"), Turn::Black));
        copy.replacePosition(next);
        QCOMPARE(source.pieceAt(BoardSquare{2, 2}), Piece::BlackHorse);
        QCOMPARE(source.pieceStandCount(Piece::BlackBishop), 1);
        QCOMPARE(copy.pieceStandCount(Piece::BlackBishop), 0);
        QCOMPARE(copy.pieceStandCount(Piece::WhiteBishop), 1);
    }

    // === 局面は値 ===

    void boardsKeepStandChangesToThemselves()
    {
        // どの書き換え方でも、ほかの盤には及ばない
        ShogiBoard added;
        ShogiBoard loaded;
        ShogiBoard emptyLoaded;
        Position withPawns = added.position();
        withPawns.addToHand(Piece::BlackPawn, 3);
        added.replacePosition(withPawns);
        QVERIFY(loaded.trySetSfen(QStringLiteral("4k4/9/9/9/9/9/9/9/4K4 b R2p 1")));
        QVERIFY(emptyLoaded.trySetSfen(QStringLiteral("4k4/9/9/9/9/9/9/9/4K4 b - 1")));

        QCOMPARE(added.position().handsSfen(), QStringLiteral("3P"));
        QCOMPARE(loaded.position().handsSfen(), QStringLiteral("R2p"));
        QCOMPARE(emptyLoaded.position().handsSfen(), QStringLiteral("-"));

        const ShogiBoard fresh;
        QCOMPARE(fresh.position().handsSfen(), QStringLiteral("-"));
        QVERIFY(fresh.position().handsEmpty());
    }
};

QTEST_MAIN(TestShogiBoard)
#include "tst_shogiboard.moc"
