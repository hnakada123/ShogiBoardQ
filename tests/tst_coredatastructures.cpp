#include <QtTest>
#include "position_test_helper.h"
#include <QSignalSpy>

#include "shoginotation.h"
#include "shogimove.h"
#include "shogitypes.h"
#include "shogiboard.h"
#include "shogigamecontroller.h"
#include "jishogicalculator.h"
#include "notationutils.h"

class TestCoreDataStructures : public QObject
{
    Q_OBJECT

private slots:
    // === ShogiMove tests ===

    void shogiMove_defaultConstructor()
    {
        ShogiMove m;
        QVERIFY(!m.move.isValid());
        QCOMPARE(m.movingPiece, Piece::None);
        QCOMPARE(m.capturedPiece, Piece::None);
        QCOMPARE(m.isPromotion(), false);
    }

    void shogiMove_parameterConstructor()
    {
        ShogiMove m(UsiMove::fromString(u"8h8g"), Piece::BlackPawn);
        QCOMPARE(m.move.from, (BoardSquare{8, 8}));
        QCOMPARE(m.move.to, (BoardSquare{8, 7}));
        QCOMPARE(m.movingPiece, Piece::BlackPawn);
        QCOMPARE(m.capturedPiece, Piece::None);
        QCOMPARE(m.isPromotion(), false);
    }

    void shogiMove_equalityOperator()
    {
        ShogiMove a(UsiMove::fromString(u"8h8g"), Piece::BlackPawn);
        ShogiMove b(UsiMove::fromString(u"8h8g"), Piece::BlackPawn);
        ShogiMove c(UsiMove::fromString(u"3g3f"), Piece::BlackPawn);

        QVERIFY(a == b);
        QVERIFY(!(a == c));
    }

    void shogiMove_dropFromPieceStand()
    {
        ShogiMove m(UsiMove::fromString(u"P*5e"), Piece::BlackPawn);
        QVERIFY(m.isDrop());
        QCOMPARE(m.move.drop, Piece::BlackPawn);
    }

    // === 手番（正本は盤） ===

    void turn_sfenTokens()
    {
        QCOMPARE(turnToSfen(Turn::Black), QStringLiteral("b"));
        QCOMPARE(turnToSfen(Turn::White), QStringLiteral("w"));
        QCOMPARE(sfenToTurn(QStringLiteral("b")), Turn::Black);
        QCOMPARE(sfenToTurn(QStringLiteral("w")), Turn::White);
    }

    /// 盤の手番は局面の SFEN から決まり、手番だけを変えたとき・指したときに1回だけ知らせる
    void turn_boardIsTheSource()
    {
        ShogiBoard board;
        board.setSfen(QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"));
        QCOMPARE(board.currentPlayer(), Turn::White);

        QSignalSpy spy(&board, &ShogiBoard::turnChanged);
        QVERIFY(spy.isValid());
        board.setCurrentPlayer(Turn::White);
        QCOMPARE(spy.count(), 0);
        board.setCurrentPlayer(Turn::Black);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.last().first().value<Turn>(), Turn::Black);

        QVERIFY(board.applyUsiMove(*UsiMove::parse(u"7g7f")));
        QCOMPARE(spy.count(), 2);
        QCOMPARE(board.currentPlayer(), Turn::White);
    }

    // === ShogiUtils tests ===

    void notation_rankKanji()
    {
        QCOMPARE(ShogiNotation::rankKanji(1), QStringLiteral("一"));
        QCOMPARE(ShogiNotation::rankKanji(5), QStringLiteral("五"));
        QCOMPARE(ShogiNotation::rankKanji(9), QStringLiteral("九"));
    }

    void notation_fileZenkaku()
    {
        QCOMPARE(ShogiNotation::fileZenkaku(1), QStringLiteral("１"));
        QCOMPARE(ShogiNotation::fileZenkaku(9), QStringLiteral("９"));
    }

    void shogiMove_toUsi_boardMove()
    {
        ShogiMove m(UsiMove::fromString(u"7g7f"), Piece::BlackPawn);
        QCOMPARE(m.move.toString(), QStringLiteral("7g7f"));
    }

    void shogiMove_toUsi_promotion()
    {
        // 8h2b+: bishop move with promotion
        ShogiMove m(UsiMove::fromString(u"8h2b+"), Piece::BlackBishop, Piece::WhiteRook);
        QCOMPARE(m.move.toString(), QStringLiteral("8h2b+"));
    }

    void shogiMove_toUsi_drop()
    {
        ShogiMove m(UsiMove::fromString(u"P*5e"), Piece::BlackPawn);
        QCOMPARE(m.move.toString(), QStringLiteral("P*5e"));
    }

    // === JishogiCalculator tests ===

    void jishogi_calculate()
    {
        // Set up a position with known pieces
        QList<Piece> board(81, Piece::None);
        QMap<Piece, int> stand;

        // Place black King in enemy territory (rank 1-3 for black)
        board[0] = Piece::BlackKing; // rank 1, file 1

        // Place a black Rook (大駒 = 5 points)
        board[1] = Piece::BlackRook; // rank 1, file 2

        // Place a black Bishop (大駒 = 5 points)
        board[2] = Piece::BlackBishop; // rank 1, file 3

        // Place black pawns (小駒 = 1 point each)
        board[3] = Piece::BlackPawn; // rank 1, file 4
        board[4] = Piece::BlackPawn; // rank 1, file 5

        // Place white King in its territory
        board[80] = Piece::WhiteKing; // rank 9, file 9

        auto result = JishogiCalculator::calculate(PositionTestHelper::fromSquares(board, stand));

        // Sente: R(5) + B(5) + P(1) + P(1) = 12 total, K = 0
        QCOMPARE(result.sente.totalPoints, 12);

        // Big pieces worth 5 points each
        QVERIFY(result.sente.totalPoints >= 10); // At least 2 big pieces
    }

    // === Piece enum tests ===

    void piece_charToPiece_blackPieces()
    {
        QCOMPARE(charToPiece(QLatin1Char('P')), Piece::BlackPawn);
        QCOMPARE(charToPiece(QLatin1Char('L')), Piece::BlackLance);
        QCOMPARE(charToPiece(QLatin1Char('N')), Piece::BlackKnight);
        QCOMPARE(charToPiece(QLatin1Char('S')), Piece::BlackSilver);
        QCOMPARE(charToPiece(QLatin1Char('G')), Piece::BlackGold);
        QCOMPARE(charToPiece(QLatin1Char('B')), Piece::BlackBishop);
        QCOMPARE(charToPiece(QLatin1Char('R')), Piece::BlackRook);
        QCOMPARE(charToPiece(QLatin1Char('K')), Piece::BlackKing);
    }

    void piece_charToPiece_whitePieces()
    {
        QCOMPARE(charToPiece(QLatin1Char('p')), Piece::WhitePawn);
        QCOMPARE(charToPiece(QLatin1Char('l')), Piece::WhiteLance);
        QCOMPARE(charToPiece(QLatin1Char('n')), Piece::WhiteKnight);
        QCOMPARE(charToPiece(QLatin1Char('s')), Piece::WhiteSilver);
        QCOMPARE(charToPiece(QLatin1Char('g')), Piece::WhiteGold);
        QCOMPARE(charToPiece(QLatin1Char('b')), Piece::WhiteBishop);
        QCOMPARE(charToPiece(QLatin1Char('r')), Piece::WhiteRook);
        QCOMPARE(charToPiece(QLatin1Char('k')), Piece::WhiteKing);
    }

    void piece_charToPiece_emptyAndInvalid()
    {
        QCOMPARE(charToPiece(QLatin1Char(' ')), Piece::None);
        QCOMPARE(charToPiece(QLatin1Char('X')), Piece::None);
        QCOMPARE(charToPiece(QLatin1Char('0')), Piece::None);
        QCOMPARE(charToPiece(QLatin1Char('+')), Piece::None);
        // 成駒は SFEN の1文字では書けない（以前の内部の文字 Q・M・O・T・C・U は読まない）
        QCOMPARE(charToPiece(QLatin1Char('Q')), Piece::None);
        QCOMPARE(charToPiece(QLatin1Char('u')), Piece::None);
    }

    void piece_pieceSfen_roundTrip()
    {
        const QList<QPair<QString, Piece>> allPieces = {
            {QStringLiteral("P"), Piece::BlackPawn},    {QStringLiteral("L"), Piece::BlackLance},
            {QStringLiteral("N"), Piece::BlackKnight},  {QStringLiteral("S"), Piece::BlackSilver},
            {QStringLiteral("G"), Piece::BlackGold},    {QStringLiteral("B"), Piece::BlackBishop},
            {QStringLiteral("R"), Piece::BlackRook},    {QStringLiteral("K"), Piece::BlackKing},
            {QStringLiteral("+P"), Piece::BlackPromotedPawn},  {QStringLiteral("+L"), Piece::BlackPromotedLance},
            {QStringLiteral("+N"), Piece::BlackPromotedKnight},{QStringLiteral("+S"), Piece::BlackPromotedSilver},
            {QStringLiteral("+B"), Piece::BlackHorse},  {QStringLiteral("+R"), Piece::BlackDragon},
            {QStringLiteral("p"), Piece::WhitePawn},    {QStringLiteral("l"), Piece::WhiteLance},
            {QStringLiteral("n"), Piece::WhiteKnight},  {QStringLiteral("s"), Piece::WhiteSilver},
            {QStringLiteral("g"), Piece::WhiteGold},    {QStringLiteral("b"), Piece::WhiteBishop},
            {QStringLiteral("r"), Piece::WhiteRook},    {QStringLiteral("k"), Piece::WhiteKing},
            {QStringLiteral("+p"), Piece::WhitePromotedPawn},  {QStringLiteral("+l"), Piece::WhitePromotedLance},
            {QStringLiteral("+n"), Piece::WhitePromotedKnight},{QStringLiteral("+s"), Piece::WhitePromotedSilver},
            {QStringLiteral("+b"), Piece::WhiteHorse},  {QStringLiteral("+r"), Piece::WhiteDragon},
        };
        for (const auto& [text, piece] : std::as_const(allPieces)) {
            QCOMPARE(pieceSfen(piece), text);
            const Piece base = charToPiece(text.back());
            QCOMPARE(text.startsWith(QLatin1Char('+')) ? promote(base) : base, piece);
        }
        QCOMPARE(pieceSfen(Piece::None), QString());
    }

    void piece_isBlackPiece()
    {
        QVERIFY(isBlackPiece(Piece::BlackPawn));
        QVERIFY(isBlackPiece(Piece::BlackKing));
        QVERIFY(isBlackPiece(Piece::BlackHorse));
        QVERIFY(isBlackPiece(Piece::BlackDragon));
        QVERIFY(!isBlackPiece(Piece::WhitePawn));
        QVERIFY(!isBlackPiece(Piece::WhiteKing));
        QVERIFY(!isBlackPiece(Piece::None));
    }

    void piece_isWhitePiece()
    {
        QVERIFY(isWhitePiece(Piece::WhitePawn));
        QVERIFY(isWhitePiece(Piece::WhiteKing));
        QVERIFY(isWhitePiece(Piece::WhiteHorse));
        QVERIFY(isWhitePiece(Piece::WhiteDragon));
        QVERIFY(!isWhitePiece(Piece::BlackPawn));
        QVERIFY(!isWhitePiece(Piece::BlackKing));
        QVERIFY(!isWhitePiece(Piece::None));
    }

    void piece_isPromoted()
    {
        // 成駒
        QVERIFY(isPromoted(Piece::BlackPromotedPawn));
        QVERIFY(isPromoted(Piece::BlackPromotedLance));
        QVERIFY(isPromoted(Piece::BlackPromotedKnight));
        QVERIFY(isPromoted(Piece::BlackPromotedSilver));
        QVERIFY(isPromoted(Piece::BlackHorse));
        QVERIFY(isPromoted(Piece::BlackDragon));
        QVERIFY(isPromoted(Piece::WhitePromotedPawn));
        QVERIFY(isPromoted(Piece::WhiteHorse));
        QVERIFY(isPromoted(Piece::WhiteDragon));
        // 非成駒
        QVERIFY(!isPromoted(Piece::BlackPawn));
        QVERIFY(!isPromoted(Piece::BlackGold));
        QVERIFY(!isPromoted(Piece::BlackKing));
        QVERIFY(!isPromoted(Piece::WhitePawn));
        QVERIFY(!isPromoted(Piece::None));
    }

    void piece_promote()
    {
        QCOMPARE(promote(Piece::BlackPawn),   Piece::BlackPromotedPawn);
        QCOMPARE(promote(Piece::BlackLance),  Piece::BlackPromotedLance);
        QCOMPARE(promote(Piece::BlackKnight), Piece::BlackPromotedKnight);
        QCOMPARE(promote(Piece::BlackSilver), Piece::BlackPromotedSilver);
        QCOMPARE(promote(Piece::BlackBishop), Piece::BlackHorse);
        QCOMPARE(promote(Piece::BlackRook),   Piece::BlackDragon);
        QCOMPARE(promote(Piece::WhitePawn),   Piece::WhitePromotedPawn);
        QCOMPARE(promote(Piece::WhiteLance),  Piece::WhitePromotedLance);
        QCOMPARE(promote(Piece::WhiteKnight), Piece::WhitePromotedKnight);
        QCOMPARE(promote(Piece::WhiteSilver), Piece::WhitePromotedSilver);
        QCOMPARE(promote(Piece::WhiteBishop), Piece::WhiteHorse);
        QCOMPARE(promote(Piece::WhiteRook),   Piece::WhiteDragon);
        // 成れない駒はそのまま
        QCOMPARE(promote(Piece::BlackGold),   Piece::BlackGold);
        QCOMPARE(promote(Piece::BlackKing),   Piece::BlackKing);
        QCOMPARE(promote(Piece::None),        Piece::None);
        // 既に成り駒はそのまま
        QCOMPARE(promote(Piece::BlackHorse),  Piece::BlackHorse);
    }

    void piece_demote()
    {
        QCOMPARE(demote(Piece::BlackPromotedPawn),    Piece::BlackPawn);
        QCOMPARE(demote(Piece::BlackPromotedLance),   Piece::BlackLance);
        QCOMPARE(demote(Piece::BlackPromotedKnight),  Piece::BlackKnight);
        QCOMPARE(demote(Piece::BlackPromotedSilver),  Piece::BlackSilver);
        QCOMPARE(demote(Piece::BlackHorse),           Piece::BlackBishop);
        QCOMPARE(demote(Piece::BlackDragon),          Piece::BlackRook);
        QCOMPARE(demote(Piece::WhitePromotedPawn),    Piece::WhitePawn);
        QCOMPARE(demote(Piece::WhitePromotedLance),   Piece::WhiteLance);
        QCOMPARE(demote(Piece::WhitePromotedKnight),  Piece::WhiteKnight);
        QCOMPARE(demote(Piece::WhitePromotedSilver),  Piece::WhiteSilver);
        QCOMPARE(demote(Piece::WhiteHorse),           Piece::WhiteBishop);
        QCOMPARE(demote(Piece::WhiteDragon),          Piece::WhiteRook);
        // 非成駒はそのまま
        QCOMPARE(demote(Piece::BlackPawn),    Piece::BlackPawn);
        QCOMPARE(demote(Piece::BlackGold),    Piece::BlackGold);
        QCOMPARE(demote(Piece::None),         Piece::None);
    }

    void piece_toBlack()
    {
        QCOMPARE(toBlack(Piece::WhitePawn),           Piece::BlackPawn);
        QCOMPARE(toBlack(Piece::WhiteLance),          Piece::BlackLance);
        QCOMPARE(toBlack(Piece::WhiteKnight),         Piece::BlackKnight);
        QCOMPARE(toBlack(Piece::WhiteSilver),         Piece::BlackSilver);
        QCOMPARE(toBlack(Piece::WhiteGold),           Piece::BlackGold);
        QCOMPARE(toBlack(Piece::WhiteBishop),         Piece::BlackBishop);
        QCOMPARE(toBlack(Piece::WhiteRook),           Piece::BlackRook);
        QCOMPARE(toBlack(Piece::WhiteKing),           Piece::BlackKing);
        QCOMPARE(toBlack(Piece::WhitePromotedPawn),   Piece::BlackPromotedPawn);
        QCOMPARE(toBlack(Piece::WhitePromotedLance),  Piece::BlackPromotedLance);
        QCOMPARE(toBlack(Piece::WhitePromotedKnight), Piece::BlackPromotedKnight);
        QCOMPARE(toBlack(Piece::WhitePromotedSilver), Piece::BlackPromotedSilver);
        QCOMPARE(toBlack(Piece::WhiteHorse),          Piece::BlackHorse);
        QCOMPARE(toBlack(Piece::WhiteDragon),         Piece::BlackDragon);
        // 先手駒・Noneはそのまま
        QCOMPARE(toBlack(Piece::BlackPawn),   Piece::BlackPawn);
        QCOMPARE(toBlack(Piece::BlackKing),   Piece::BlackKing);
        QCOMPARE(toBlack(Piece::None),        Piece::None);
    }

    void piece_toWhite()
    {
        QCOMPARE(toWhite(Piece::BlackPawn),           Piece::WhitePawn);
        QCOMPARE(toWhite(Piece::BlackLance),          Piece::WhiteLance);
        QCOMPARE(toWhite(Piece::BlackKnight),         Piece::WhiteKnight);
        QCOMPARE(toWhite(Piece::BlackSilver),         Piece::WhiteSilver);
        QCOMPARE(toWhite(Piece::BlackGold),           Piece::WhiteGold);
        QCOMPARE(toWhite(Piece::BlackBishop),         Piece::WhiteBishop);
        QCOMPARE(toWhite(Piece::BlackRook),           Piece::WhiteRook);
        QCOMPARE(toWhite(Piece::BlackKing),           Piece::WhiteKing);
        QCOMPARE(toWhite(Piece::BlackPromotedPawn),   Piece::WhitePromotedPawn);
        QCOMPARE(toWhite(Piece::BlackPromotedLance),  Piece::WhitePromotedLance);
        QCOMPARE(toWhite(Piece::BlackPromotedKnight), Piece::WhitePromotedKnight);
        QCOMPARE(toWhite(Piece::BlackPromotedSilver), Piece::WhitePromotedSilver);
        QCOMPARE(toWhite(Piece::BlackHorse),          Piece::WhiteHorse);
        QCOMPARE(toWhite(Piece::BlackDragon),         Piece::WhiteDragon);
        // 後手駒・Noneはそのまま
        QCOMPARE(toWhite(Piece::WhitePawn),   Piece::WhitePawn);
        QCOMPARE(toWhite(Piece::WhiteKing),   Piece::WhiteKing);
        QCOMPARE(toWhite(Piece::None),        Piece::None);
    }

    void piece_promoteDemoteRoundTrip()
    {
        const QList<Piece> promotable = {
            Piece::BlackPawn, Piece::BlackLance, Piece::BlackKnight,
            Piece::BlackSilver, Piece::BlackBishop, Piece::BlackRook,
            Piece::WhitePawn, Piece::WhiteLance, Piece::WhiteKnight,
            Piece::WhiteSilver, Piece::WhiteBishop, Piece::WhiteRook,
        };
        for (Piece p : std::as_const(promotable)) {
            QCOMPARE(demote(promote(p)), p);
        }
    }

    // === NotationUtils tests ===

    void shogiNotation_digits()
    {
        QCOMPARE(ShogiNotation::rankFromKanji(QChar(u'一')), 1);
        QCOMPARE(ShogiNotation::rankFromKanji(QChar(u'九')), 9);
        QCOMPARE(ShogiNotation::rankFromKanji(QChar('1')), 0);
        QCOMPARE(ShogiNotation::fileZenkaku(1), QStringLiteral("１"));
        QCOMPARE(ShogiNotation::fileZenkaku(10), QString());
        QCOMPARE(ShogiNotation::rankKanji(9), QStringLiteral("九"));
        QCOMPARE(ShogiNotation::rankKanji(0), QString());
    }

    void notationUtils_formatSfenMove()
    {
        QCOMPARE(NotationUtils::formatSfenMove(7, 7, 7, 6, false), QStringLiteral("7g7f"));
        QCOMPARE(NotationUtils::formatSfenMove(8, 2, 2, 2, true), QStringLiteral("8b2b+"));
        QCOMPARE(NotationUtils::formatSfenMove(1, 1, 9, 9, false), QStringLiteral("1a9i"));
    }

    void notationUtils_formatSfenDrop()
    {
        QCOMPARE(NotationUtils::formatSfenDrop(QChar('P'), 5, 5), QStringLiteral("P*5e"));
        QCOMPARE(NotationUtils::formatSfenDrop(QChar('R'), 1, 1), QStringLiteral("R*1a"));
        QCOMPARE(NotationUtils::formatSfenDrop(QChar('G'), 9, 9), QStringLiteral("G*9i"));
    }

    void notationUtils_mapHandicapToSfen_data()
    {
        QTest::addColumn<QString>("label");
        QTest::addColumn<QString>("expectedSfen");

        const QString kEven = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");
        QTest::newRow("hirate") << QStringLiteral("平手") << kEven;
        QTest::newRow("two-piece") << QStringLiteral("二枚落ち")
            << QStringLiteral("lnsgkgsnl/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1");
        QTest::newRow("bishop") << QStringLiteral("角落ち")
            << QStringLiteral("lnsgkgsnl/1r7/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1");
        QTest::newRow("lance") << QStringLiteral("香落ち")
            << QStringLiteral("lnsgkgsn1/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1");
        QTest::newRow("right-lance") << QStringLiteral("右香落ち")
            << QStringLiteral("1nsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1");
        QTest::newRow("rook-lance") << QStringLiteral("飛香落ち")
            << QStringLiteral("lnsgkgsn1/7b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1");
        QTest::newRow("five-piece") << QStringLiteral("五枚落ち")
            << QStringLiteral("2sgkgsn1/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1");
        QTest::newRow("left-five-piece") << QStringLiteral("左五枚落ち")
            << QStringLiteral("1nsgkgs2/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1");
        QTest::newRow("surrounding-whitespace") << QStringLiteral(" \t　右香落ち　\n")
            << QStringLiteral("1nsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1");
        // 未知のラベルは平手にフォールバック
        QTest::newRow("unknown") << QStringLiteral("unknown") << kEven;
        QTest::newRow("empty") << QString() << kEven;
        QTest::newRow("unknown-prefix") << QStringLiteral("不明な香落ち") << kEven;
        QTest::newRow("unknown-suffix") << QStringLiteral("五枚落ち不明") << kEven;
    }

    void notationUtils_mapHandicapToSfen()
    {
        QFETCH(QString, label);
        QFETCH(QString, expectedSfen);

        QCOMPARE(NotationUtils::mapHandicapToSfen(label), expectedSfen);
    }

    // GC の手番更新が盤面モデルの手番にも反映されること
    //（対局中は盤面を SFEN から再設定しないため、盤面から生成する SFEN の手番はこれに依存する）
    void gameController_setCurrentPlayer_syncsBoardTurn()
    {
        ShogiGameController gc;
        QString startSfen = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");
        gc.newGame(startSfen);
        QVERIFY(gc.board() != nullptr);
        QCOMPARE(gc.board()->currentPlayer(), Turn::Black);

        gc.setCurrentPlayer(Turn::White);
        QCOMPARE(gc.currentPlayer(), Turn::White);
        QCOMPARE(gc.board()->currentPlayer(), Turn::White);

        gc.setCurrentPlayer(Turn::Black);
        QCOMPARE(gc.currentPlayer(), Turn::Black);
        QCOMPARE(gc.board()->currentPlayer(), Turn::Black);

    }

    void gameController_pieceBoxTransfers_data()
    {
        QTest::addColumn<int>("rank");
        for (int rank = 1; rank <= 8; ++rank)
            QTest::newRow(qPrintable(QString::number(rank))) << rank;
    }

    void gameController_pieceBoxTransfers()
    {
        QFETCH(int, rank);
        ShogiGameController gc;
        QString empty = QStringLiteral("9/9/9/9/9/9/9/9/9 w - 1");
        gc.newGame(empty);
        auto* board = gc.board();
        const auto initialBox = board->pieceBox();
        const Piece piece = board->pieceCharacter(12, rank);
        const int states = rank == 5 || rank == 8 ? 2 : 4;
        // 全駒種・先後・成駒を駒箱へ戻すと、生駒・先手向きに統一される。
        for (int state = 0; state < states; ++state) {
            QVERIFY(gc.editPosition({12, rank}, {5, 5}));
            QCOMPARE(board->pieceCharacter(5, 5), piece);
            QCOMPARE(board->pieceBoxCount(piece), initialBox.value(piece) - 1);
            for (int step = 0; step < state; ++step)
                gc.switchPiecePromotionStatusOnRightClick(5, 5);
            const Piece expected = state < 2 ? piece : toWhite(piece);
            if (states == 4)
                QCOMPARE(board->pieceCharacter(5, 5), state % 2 ? promote(expected) : expected);
            else
                QCOMPARE(board->pieceCharacter(5, 5), state ? toWhite(piece) : piece);
            QVERIFY(gc.editPosition({5, 5}, {12, rank}));
            QCOMPARE(board->pieceBox(), initialBox);
            QCOMPARE(board->position().handsSfen(), QStringLiteral("-"));
        }
        // 駒箱と両駒台との往復。王・玉の持ち駒化は引き続き拒否する。
        for (int stand : {10, 11}) {
            const int standRank = stand == 10 ? rank : 10 - rank;
            QCOMPARE(gc.editPosition({12, rank}, {stand, standRank}), rank != 8);
            if (rank != 8) {
                QCOMPARE(board->pieceStandCount(stand == 10 ? piece : toWhite(piece)), 1);
                QVERIFY(gc.editPosition({stand, standRank}, {12, rank}));
            }
            QCOMPARE(board->pieceBox(), initialBox);
        }
        QCOMPARE(gc.currentPlayer(), Turn::White);
    }

    void gameController_pieceBoxWhiteTransfers_data() { gameController_pieceBoxTransfers_data(); }

    void gameController_pieceBoxWhiteTransfers()
    {
        QFETCH(int, rank);
        ShogiGameController gc;
        QString empty = QStringLiteral("9/9/9/9/9/9/9/9/9 b - 1");
        gc.newGame(empty);
        auto* board = gc.board();
        const auto initialBox = board->pieceBox();
        const Piece piece = toWhite(board->pieceCharacter(12, rank));
        QVERIFY(gc.editPosition({12, rank}, {5, 1}, Turn::White));
        QCOMPARE(board->pieceCharacter(5, 1), piece);
        QCOMPARE(board->pieceBoxCount(piece), initialBox.value(toBlack(piece)) - 1);
        QVERIFY(gc.editPosition({5, 1}, {12, rank}, Turn::White));
        QCOMPARE(board->pieceBox(), initialBox);
        // 行き所のない駒の自動成りも、選択した先後で判定する。
        QVERIFY(gc.editPosition({12, rank}, {5, 9}, Turn::White));
        QCOMPARE(board->pieceCharacter(5, 9), rank <= 3 ? promote(piece) : piece);
        QVERIFY(gc.editPosition({5, 9}, {12, rank}, Turn::White));
        for (int stand : {10, 11}) {
            const int standRank = stand == 10 ? rank : 10 - rank;
            QCOMPARE(gc.editPosition({12, rank}, {stand, standRank}, Turn::White), rank != 8);
            if (rank != 8) {
                QCOMPARE(board->pieceStandCount(stand == 10 ? toBlack(piece) : piece), 1);
                QVERIFY(gc.editPosition({stand, standRank}, {12, rank}, Turn::White));
            }
            QCOMPARE(board->pieceBox(), initialBox);
        }
        QCOMPARE(gc.currentPlayer(), Turn::Black);
        QCOMPARE(board->currentPlayer(), Turn::Black);
    }

    void gameController_pieceBoxPawnsOnSameFile()
    {
        ShogiGameController gc;
        QString empty = QStringLiteral("9/9/9/9/9/9/9/9/9 w - 1");
        gc.newGame(empty);
        QVERIFY(gc.editPosition({12, 1}, {5, 7}, Turn::Black));
        QVERIFY(!gc.editPosition({12, 1}, {5, 3}, Turn::Black));
        QVERIFY(gc.editPosition({12, 1}, {5, 3}, Turn::White));
        QVERIFY(!gc.editPosition({12, 1}, {5, 5}, Turn::White));
        QCOMPARE(gc.board()->pieceCharacter(5, 7), Piece::BlackPawn);
        QCOMPARE(gc.board()->pieceCharacter(5, 3), Piece::WhitePawn);
        QCOMPARE(gc.board()->pieceBoxCount(Piece::BlackPawn), 16);
        QCOMPARE(gc.board()->pieceBoxCount(Piece::WhitePawn), 16);
        QCOMPARE(gc.currentPlayer(), Turn::White);
        QCOMPARE(gc.board()->currentPlayer(), Turn::White);
    }

    void gameController_pieceBoxRejectsEmptyAndWrongSlots()
    {
        ShogiGameController gc;
        QString empty = QStringLiteral("9/9/9/9/9/9/9/9/9 b - 1");
        gc.newGame(empty);
        QVERIFY(gc.editPosition({12, 8}, {5, 1}));
        gc.switchPiecePromotionStatusOnRightClick(5, 1);
        QVERIFY(gc.editPosition({12, 8}, {5, 9}));
        const Position before = gc.board()->position();
        const auto box = gc.board()->pieceBox();
        QVERIFY(!gc.editPosition({12, 8}, {5, 5}));
        QVERIFY(!gc.editPosition({5, 1}, {12, 1}));
        QVERIFY(!gc.editPosition({12, 1}, {12, 1}));
        QVERIFY(!gc.editPosition({12, 0}, {5, 5}));
        QCOMPARE(gc.board()->position(), before);
        QCOMPARE(gc.board()->pieceBox(), box);
    }

    void gameController_editRejectsKingToStand_data()
    {
        QTest::addColumn<QPoint>("from");
        QTest::addColumn<QPoint>("to");
        QTest::newRow("black-to-black") << QPoint(5, 9) << QPoint(10, 8);
        QTest::newRow("black-to-white") << QPoint(5, 9) << QPoint(11, 2);
        QTest::newRow("white-to-black") << QPoint(5, 1) << QPoint(10, 8);
        QTest::newRow("white-to-white") << QPoint(5, 1) << QPoint(11, 2);
    }

    void gameController_editRejectsKingToStand()
    {
        QFETCH(QPoint, from);
        QFETCH(QPoint, to);
        ShogiGameController gc;
        QString initial = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1");
        gc.newGame(initial);
        const Position before = gc.board()->position();
        QVERIFY(!gc.editPosition(from, to));
        QCOMPARE(gc.board()->position(), before);
        QCOMPARE(gc.currentPlayer(), Turn::White);
        QCOMPARE(gc.board()->currentPlayer(), Turn::White);
    }

    void gameController_rejectsInvalidClickCoordinates_data()
    {
        QTest::addColumn<QPoint>("from");
        QTest::addColumn<QPoint>("to");
        QTest::newRow("no-bestmove") << QPoint(-1, -1) << QPoint(-1, -1);
        QTest::newRow("invalid-source") << QPoint(-1, -1) << QPoint(7, 6);
        QTest::newRow("invalid-target") << QPoint(7, 7) << QPoint(-1, -1);
        QTest::newRow("source-rank-zero") << QPoint(7, 0) << QPoint(7, 6);
        QTest::newRow("target-rank-ten") << QPoint(7, 7) << QPoint(7, 10);
        QTest::newRow("target-stand") << QPoint(7, 7) << QPoint(10, 1);
        QTest::newRow("invalid-stand-rank") << QPoint(10, 0) << QPoint(7, 6);
        QTest::newRow("king-drop") << QPoint(10, 8) << QPoint(7, 6);
        QTest::newRow("opponent-stand") << QPoint(11, 9) << QPoint(7, 6);
    }

    void gameController_rejectsInvalidClickCoordinates()
    {
        QFETCH(QPoint, from);
        QFETCH(QPoint, to);
        ShogiGameController gc;
        QString initial = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");
        gc.newGame(initial);
        const Position before = gc.board()->position();
        QSignalSpy committed(&gc, &ShogiGameController::moveCommitted);
        QVERIFY(!gc.validateAndMove(from, to, {}));
        QCOMPARE(gc.board()->position(), before);
        QCOMPARE(gc.board()->toSfen(), initial);
        QCOMPARE(committed.count(), 0);
        QCOMPARE(gc.currentPlayer(), Turn::Black);
    }

    void gameController_acceptsEngineDrop_data()
    {
        QTest::addColumn<bool>("black");
        QTest::newRow("black") << true;
        QTest::newRow("white") << false;
    }

    void gameController_acceptsEngineDrop()
    {
        QFETCH(bool, black);
        ShogiGameController gc;
        QString initial = QStringLiteral("4k4/9/9/9/9/9/9/9/4K4 ")
            + (black ? QStringLiteral("b P 1") : QStringLiteral("w p 1"));
        gc.newGame(initial);
        const std::optional<RecordEntry> played = gc.applyMove(*UsiMove::parse(u"P*5e"), {});
        QVERIFY(played);
        QCOMPARE(gc.board()->pieceCharacter(5, 5), black ? Piece::BlackPawn : Piece::WhitePawn);
        QCOMPARE(gc.board()->position().handsSfen(), QStringLiteral("-"));
        QCOMPARE(played->move.move, *UsiMove::parse(u"P*5e"));
        QCOMPARE(played->mover, black ? Turn::Black : Turn::White);
        QCOMPARE(played->sfen, gc.board()->toSfen());
        QCOMPARE(gc.lastMove(), *UsiMove::parse(u"P*5e"));
        QCOMPARE(played->displayText, black ? QStringLiteral("▲５五歩打") : QStringLiteral("△５五歩打"));
    }

    /// エンジン・通信対局の手も合法手か確かめ、反則手は盤も記録も変えずに断る
    void gameController_rejectsIllegalEngineMove_data()
    {
        QTest::addColumn<QString>("sfen");
        QTest::addColumn<QString>("move");
        const QString hirate = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");
        QTest::newRow("pawn-two-squares") << hirate << QStringLiteral("7g7e");
        QTest::newRow("empty-source") << hirate << QStringLiteral("7f7e");
        QTest::newRow("opponent-piece") << hirate << QStringLiteral("3c3d");
        QTest::newRow("promotion-outside-zone") << hirate << QStringLiteral("7g7f+");
        QTest::newRow("drop-without-piece") << hirate << QStringLiteral("P*5e");
        QTest::newRow("double-pawn") << QStringLiteral("4k4/9/9/9/9/9/4P4/9/4K4 b P 1") << QStringLiteral("P*5e");
        QTest::newRow("forced-promotion-missing") << QStringLiteral("4k4/P8/9/9/9/9/9/9/4K4 b - 1")
                                                << QStringLiteral("9b9a");
        QTest::newRow("leaves-king-in-check") << QStringLiteral("4k4/9/9/9/4r4/9/9/4G4/4K4 b - 1")
                                            << QStringLiteral("5h6h");
    }

    void gameController_rejectsIllegalEngineMove()
    {
        QFETCH(QString, sfen);
        QFETCH(QString, move);
        ShogiGameController gc;
        gc.newGame(sfen);
        const Position board = gc.board()->position();
        QSignalSpy committed(&gc, &ShogiGameController::moveCommitted);
        const auto parsed = UsiMove::parse(move);
        QVERIFY(parsed);
        QVERIFY(!gc.applyMove(*parsed, {}));
        QCOMPARE(gc.board()->position(), board);
        QCOMPARE(committed.count(), 0);
        QCOMPARE(gc.currentPlayer(), Turn::Black);
    }

    /// 成りは指し手どおりに指し、成らない手は成らないまま指す（共有の成りフラグに頼らない）
    void gameController_appliesEnginePromotionAsGiven()
    {
        const QString sfen = QStringLiteral("lnsgkgsnl/1r5b1/pppppp1pp/6p2/9/2P6/PP1PPPPPP/1B5R1/LNSGKGSNL b - 3");
        for (const bool promote : {true, false}) {
            ShogiGameController gc;
            QString initial = sfen;
            gc.newGame(initial);
            const UsiMove move = *UsiMove::parse(promote ? u"8h2b+" : u"8h2b");
            const std::optional<RecordEntry> played = gc.applyMove(move, {});
            QVERIFY(played);
            QCOMPARE(gc.board()->pieceCharacter(2, 2), promote ? Piece::BlackHorse : Piece::BlackBishop);
            QCOMPARE(played->move.isPromotion(), promote);
            QCOMPARE(played->displayText, promote ? QStringLiteral("▲２二角成(88)") : QStringLiteral("▲２二角(88)"));
        }
    }

    void gameController_clickAsksPromotionOnlyWhenBothLegal_data()
    {
        QTest::addColumn<bool>("illegalAttempt");
        QTest::newRow("ordinary-book-move") << false;
        QTest::newRow("illegal-book-move") << true;
    }

    /// 盤のクリックは、成り・不成の両方が合法なときだけ成りの選び方に聞く
    void gameController_clickAsksPromotionOnlyWhenBothLegal()
    {
        QFETCH(bool, illegalAttempt);
        ShogiGameController gc;
        QString initial = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");
        gc.newGame(initial);
        int promotionQuestions = 0;
        gc.setPromotionChooser([&promotionQuestions] { ++promotionQuestions; return false; });
        if (illegalAttempt) {
            QPoint empty(7, 6), dest(7, 5);
            QVERIFY(!gc.validateAndMove(empty, dest, {}));
        }
        QPoint from(7, 7), to(7, 6);
        QVERIFY(gc.validateAndMove(from, to, {}));
        from = QPoint(3, 3); to = QPoint(3, 4);
        QVERIFY(gc.validateAndMove(from, to, {}));
        from = QPoint(8, 8); to = QPoint(2, 2);
        QVERIFY(gc.validateAndMove(from, to, {}));
        QCOMPARE(promotionQuestions, 1);
    }

    void gameController_bookMoveKeepsItsPromotion_data()
    {
        QTest::addColumn<bool>("promote");
        QTest::newRow("promote") << true;
        QTest::newRow("do-not-promote") << false;
    }

    /// 成り・不成まで決まった手（定跡の手）は、成りの選択を出さずにそのとおり指す
    void gameController_bookMoveKeepsItsPromotion()
    {
        QFETCH(bool, promote);
        ShogiGameController gc;
        QString initial = QStringLiteral("lnsgkgsnl/1r5b1/pppppp1pp/6p2/9/2P6/PP1PPPPPP/1B5R1/LNSGKGSNL b - 3");
        gc.newGame(initial);
        int promotionQuestions = 0;
        gc.setPromotionChooser([&promotionQuestions] { ++promotionQuestions; return false; });
        const UsiMove move = *UsiMove::parse(promote ? u"8h2b+" : u"8h2b");
        QVERIFY(gc.isLegal(move));
        const std::optional<RecordEntry> played = gc.applyMove(move, {});
        QVERIFY(played);
        QCOMPARE(promotionQuestions, 0);
        QCOMPARE(played->move.isPromotion(), promote);
        QCOMPARE(gc.lastMove().promote, promote);
    }

    /// 盤のクリック操作で成り・不成を選べるときは、成りの選び方の答えどおりに指す
    void gameController_clickUsesPromotionChoice()
    {
        for (const bool choice : {true, false}) {
            ShogiGameController gc;
            QString initial = QStringLiteral("lnsgkgsnl/1r5b1/pppppp1pp/6p2/9/2P6/PP1PPPPPP/1B5R1/LNSGKGSNL b - 3");
            gc.newGame(initial);
            gc.setPromotionChooser([choice] { return choice; });
            QPoint from(8, 8), to(2, 2);
            QVERIFY(gc.validateAndMove(from, to, {}));
            QCOMPARE(gc.lastMove().promote, choice);
            QCOMPARE(gc.board()->pieceCharacter(2, 2), choice ? Piece::BlackHorse : Piece::BlackBishop);
        }
    }
};

QTEST_MAIN(TestCoreDataStructures)
#include "tst_coredatastructures.moc"
