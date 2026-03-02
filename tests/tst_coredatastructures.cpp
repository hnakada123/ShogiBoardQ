#include <QtTest>
#include <QSignalSpy>

#include "shogimove.h"
#include "shogitypes.h"
#include "turnmanager.h"
#include "shogiutils.h"
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
        QCOMPARE(m.fromSquare, QPoint(0, 0));
        QCOMPARE(m.toSquare, QPoint(0, 0));
        QCOMPARE(m.movingPiece, Piece::None);
        QCOMPARE(m.capturedPiece, Piece::None);
        QCOMPARE(m.isPromotion, false);
    }

    void shogiMove_parameterConstructor()
    {
        ShogiMove m(QPoint(7, 7), QPoint(7, 6), Piece::BlackPawn, Piece::None, false);
        QCOMPARE(m.fromSquare, QPoint(7, 7));
        QCOMPARE(m.toSquare, QPoint(7, 6));
        QCOMPARE(m.movingPiece, Piece::BlackPawn);
        QCOMPARE(m.capturedPiece, Piece::None);
        QCOMPARE(m.isPromotion, false);
    }

    void shogiMove_equalityOperator()
    {
        ShogiMove a(QPoint(7, 7), QPoint(7, 6), Piece::BlackPawn, Piece::None, false);
        ShogiMove b(QPoint(7, 7), QPoint(7, 6), Piece::BlackPawn, Piece::None, false);
        ShogiMove c(QPoint(2, 6), QPoint(2, 5), Piece::BlackPawn, Piece::None, false);

        QVERIFY(a == b);
        QVERIFY(!(a == c));
    }

    void shogiMove_dropFromPieceStand()
    {
        // 先手駒台: fromSquare.x() >= 9
        ShogiMove m(QPoint(9, 0), QPoint(4, 4), Piece::BlackPawn, Piece::None, false);
        QVERIFY(m.fromSquare.x() >= 9);
    }

    // === TurnManager tests ===

    void turnManager_initialState()
    {
        TurnManager tm;
        // initial side should be NoPlayer
        QCOMPARE(tm.side(), TurnManager::Side::NoPlayer);
    }

    void turnManager_setPlayer1()
    {
        TurnManager tm;
        tm.set(TurnManager::Side::Player1);
        QCOMPARE(tm.side(), TurnManager::Side::Player1);
        QCOMPARE(tm.toSfenToken(), QStringLiteral("b"));
        QCOMPARE(tm.toClockPlayer(), 1);
    }

    void turnManager_setPlayer2()
    {
        TurnManager tm;
        tm.set(TurnManager::Side::Player2);
        QCOMPARE(tm.side(), TurnManager::Side::Player2);
        QCOMPARE(tm.toSfenToken(), QStringLiteral("w"));
        QCOMPARE(tm.toClockPlayer(), 2);
    }

    void turnManager_toggle()
    {
        TurnManager tm;
        tm.set(TurnManager::Side::Player1);
        tm.toggle();
        QCOMPARE(tm.side(), TurnManager::Side::Player2);
        tm.toggle();
        QCOMPARE(tm.side(), TurnManager::Side::Player1);
    }

    void turnManager_setFromSfenToken()
    {
        TurnManager tm;
        tm.setFromSfenToken(QStringLiteral("w"));
        QCOMPARE(tm.side(), TurnManager::Side::Player2);
        tm.setFromSfenToken(QStringLiteral("b"));
        QCOMPARE(tm.side(), TurnManager::Side::Player1);
    }

    void turnManager_changedSignal()
    {
        TurnManager tm;
        QSignalSpy spy(&tm, &TurnManager::changed);
        QVERIFY(spy.isValid());

        tm.set(TurnManager::Side::Player1);
        QCOMPARE(spy.count(), 1);

        tm.toggle();
        QCOMPARE(spy.count(), 2);
    }

    // === ShogiUtils tests ===

    void shogiUtils_transRankTo()
    {
        QCOMPARE(ShogiUtils::transRankTo(1), QStringLiteral("一"));
        QCOMPARE(ShogiUtils::transRankTo(5), QStringLiteral("五"));
        QCOMPARE(ShogiUtils::transRankTo(9), QStringLiteral("九"));
    }

    void shogiUtils_transFileTo()
    {
        QCOMPARE(ShogiUtils::transFileTo(1), QStringLiteral("１"));
        QCOMPARE(ShogiUtils::transFileTo(9), QStringLiteral("９"));
    }

    void shogiUtils_moveToUsi_boardMove()
    {
        // 7g7f: fromSquare(6,6) 0-indexed → USI file 7, rank g
        ShogiMove m(QPoint(6, 6), QPoint(6, 5), Piece::BlackPawn, Piece::None, false);
        QCOMPARE(ShogiUtils::moveToUsi(m), QStringLiteral("7g7f"));
    }

    void shogiUtils_moveToUsi_promotion()
    {
        // 8h2b+: bishop move with promotion
        ShogiMove m(QPoint(7, 7), QPoint(1, 1), Piece::BlackBishop, Piece::WhiteRook, true);
        QCOMPARE(ShogiUtils::moveToUsi(m), QStringLiteral("8h2b+"));
    }

    void shogiUtils_moveToUsi_drop()
    {
        // P*5e: pawn drop from black's hand (x=9) to 5e (4,4)
        ShogiMove m(QPoint(9, 0), QPoint(4, 4), Piece::BlackPawn, Piece::None, false);
        QCOMPARE(ShogiUtils::moveToUsi(m), QStringLiteral("P*5e"));
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

        auto result = JishogiCalculator::calculate(board, stand);

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
        QCOMPARE(charToPiece(QLatin1Char('Q')), Piece::BlackPromotedPawn);
        QCOMPARE(charToPiece(QLatin1Char('M')), Piece::BlackPromotedLance);
        QCOMPARE(charToPiece(QLatin1Char('O')), Piece::BlackPromotedKnight);
        QCOMPARE(charToPiece(QLatin1Char('T')), Piece::BlackPromotedSilver);
        QCOMPARE(charToPiece(QLatin1Char('C')), Piece::BlackHorse);
        QCOMPARE(charToPiece(QLatin1Char('U')), Piece::BlackDragon);
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
        QCOMPARE(charToPiece(QLatin1Char('q')), Piece::WhitePromotedPawn);
        QCOMPARE(charToPiece(QLatin1Char('m')), Piece::WhitePromotedLance);
        QCOMPARE(charToPiece(QLatin1Char('o')), Piece::WhitePromotedKnight);
        QCOMPARE(charToPiece(QLatin1Char('t')), Piece::WhitePromotedSilver);
        QCOMPARE(charToPiece(QLatin1Char('c')), Piece::WhiteHorse);
        QCOMPARE(charToPiece(QLatin1Char('u')), Piece::WhiteDragon);
    }

    void piece_charToPiece_emptyAndInvalid()
    {
        QCOMPARE(charToPiece(QLatin1Char(' ')), Piece::None);
        QCOMPARE(charToPiece(QLatin1Char('X')), Piece::None);
        QCOMPARE(charToPiece(QLatin1Char('0')), Piece::None);
        QCOMPARE(charToPiece(QLatin1Char('+')), Piece::None);
    }

    void piece_pieceToChar_roundTrip()
    {
        const QList<QPair<QChar, Piece>> allPieces = {
            {QLatin1Char('P'), Piece::BlackPawn},    {QLatin1Char('L'), Piece::BlackLance},
            {QLatin1Char('N'), Piece::BlackKnight},   {QLatin1Char('S'), Piece::BlackSilver},
            {QLatin1Char('G'), Piece::BlackGold},     {QLatin1Char('B'), Piece::BlackBishop},
            {QLatin1Char('R'), Piece::BlackRook},     {QLatin1Char('K'), Piece::BlackKing},
            {QLatin1Char('Q'), Piece::BlackPromotedPawn},  {QLatin1Char('M'), Piece::BlackPromotedLance},
            {QLatin1Char('O'), Piece::BlackPromotedKnight},{QLatin1Char('T'), Piece::BlackPromotedSilver},
            {QLatin1Char('C'), Piece::BlackHorse},    {QLatin1Char('U'), Piece::BlackDragon},
            {QLatin1Char('p'), Piece::WhitePawn},     {QLatin1Char('l'), Piece::WhiteLance},
            {QLatin1Char('n'), Piece::WhiteKnight},   {QLatin1Char('s'), Piece::WhiteSilver},
            {QLatin1Char('g'), Piece::WhiteGold},     {QLatin1Char('b'), Piece::WhiteBishop},
            {QLatin1Char('r'), Piece::WhiteRook},     {QLatin1Char('k'), Piece::WhiteKing},
            {QLatin1Char('q'), Piece::WhitePromotedPawn},  {QLatin1Char('m'), Piece::WhitePromotedLance},
            {QLatin1Char('o'), Piece::WhitePromotedKnight},{QLatin1Char('t'), Piece::WhitePromotedSilver},
            {QLatin1Char('c'), Piece::WhiteHorse},    {QLatin1Char('u'), Piece::WhiteDragon},
            {QLatin1Char(' '), Piece::None},
        };
        for (const auto& [ch, piece] : std::as_const(allPieces)) {
            QCOMPARE(charToPiece(ch), piece);
            QCOMPARE(pieceToChar(piece), ch);
        }
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

    void notationUtils_rankNumToLetter()
    {
        QCOMPARE(NotationUtils::rankNumToLetter(1), QChar('a'));
        QCOMPARE(NotationUtils::rankNumToLetter(5), QChar('e'));
        QCOMPARE(NotationUtils::rankNumToLetter(9), QChar('i'));
        QCOMPARE(NotationUtils::rankNumToLetter(0), QChar());
        QCOMPARE(NotationUtils::rankNumToLetter(10), QChar());
    }

    void notationUtils_rankLetterToNum()
    {
        QCOMPARE(NotationUtils::rankLetterToNum(QChar('a')), 1);
        QCOMPARE(NotationUtils::rankLetterToNum(QChar('e')), 5);
        QCOMPARE(NotationUtils::rankLetterToNum(QChar('i')), 9);
        QCOMPARE(NotationUtils::rankLetterToNum(QChar('z')), 0);
        QCOMPARE(NotationUtils::rankLetterToNum(QChar('A')), 0);
    }

    void notationUtils_kanjiDigitToInt()
    {
        QCOMPARE(NotationUtils::kanjiDigitToInt(QChar(u'一')), 1);
        QCOMPARE(NotationUtils::kanjiDigitToInt(QChar(u'五')), 5);
        QCOMPARE(NotationUtils::kanjiDigitToInt(QChar(u'九')), 9);
        QCOMPARE(NotationUtils::kanjiDigitToInt(QChar('A')), 0);
        QCOMPARE(NotationUtils::kanjiDigitToInt(QChar('1')), 0);
    }

    void notationUtils_intToZenkakuDigit()
    {
        QCOMPARE(NotationUtils::intToZenkakuDigit(1), QStringLiteral("１"));
        QCOMPARE(NotationUtils::intToZenkakuDigit(9), QStringLiteral("９"));
        QCOMPARE(NotationUtils::intToZenkakuDigit(0), QStringLiteral("？"));
        QCOMPARE(NotationUtils::intToZenkakuDigit(10), QStringLiteral("？"));
    }

    void notationUtils_intToKanjiDigit()
    {
        QCOMPARE(NotationUtils::intToKanjiDigit(1), QStringLiteral("一"));
        QCOMPARE(NotationUtils::intToKanjiDigit(9), QStringLiteral("九"));
        QCOMPARE(NotationUtils::intToKanjiDigit(0), QStringLiteral("？"));
        QCOMPARE(NotationUtils::intToKanjiDigit(10), QStringLiteral("？"));
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

    void notationUtils_mapHandicapToSfen()
    {
        const QString kEven = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");
        QCOMPARE(NotationUtils::mapHandicapToSfen(QStringLiteral("平手")), kEven);
        QCOMPARE(NotationUtils::mapHandicapToSfen(QStringLiteral("二枚落ち")),
                 QStringLiteral("lnsgkgsnl/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"));
        QCOMPARE(NotationUtils::mapHandicapToSfen(QStringLiteral("角落ち")),
                 QStringLiteral("lnsgkgsnl/1r7/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"));
        // 未知のラベルは平手にフォールバック
        QCOMPARE(NotationUtils::mapHandicapToSfen(QStringLiteral("unknown")), kEven);
    }

    void notationUtils_rankRoundTrip()
    {
        // rankNumToLetter → rankLetterToNum ラウンドトリップ
        for (int r = 1; r <= 9; ++r) {
            QChar letter = NotationUtils::rankNumToLetter(r);
            QCOMPARE(NotationUtils::rankLetterToNum(letter), r);
        }
        // 逆方向
        for (char c = 'a'; c <= 'i'; ++c) {
            int num = NotationUtils::rankLetterToNum(QChar(c));
            QCOMPARE(NotationUtils::rankNumToLetter(num), QChar(c));
        }
    }
};

QTEST_MAIN(TestCoreDataStructures)
#include "tst_coredatastructures.moc"
