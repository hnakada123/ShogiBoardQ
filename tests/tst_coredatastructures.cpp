#include <QtTest>
#include <QSignalSpy>

#include "shogimove.h"
#include "turnmanager.h"
#include "shogiutils.h"
#include "jishogicalculator.h"

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
        QCOMPARE(m.movingPiece, QLatin1Char(' '));
        QCOMPARE(m.capturedPiece, QLatin1Char(' '));
        QCOMPARE(m.isPromotion, false);
    }

    void shogiMove_parameterConstructor()
    {
        ShogiMove m(QPoint(7, 7), QPoint(7, 6), QLatin1Char('P'), QLatin1Char(' '), false);
        QCOMPARE(m.fromSquare, QPoint(7, 7));
        QCOMPARE(m.toSquare, QPoint(7, 6));
        QCOMPARE(m.movingPiece, QLatin1Char('P'));
        QCOMPARE(m.capturedPiece, QLatin1Char(' '));
        QCOMPARE(m.isPromotion, false);
    }

    void shogiMove_equalityOperator()
    {
        ShogiMove a(QPoint(7, 7), QPoint(7, 6), QLatin1Char('P'), QLatin1Char(' '), false);
        ShogiMove b(QPoint(7, 7), QPoint(7, 6), QLatin1Char('P'), QLatin1Char(' '), false);
        ShogiMove c(QPoint(2, 6), QPoint(2, 5), QLatin1Char('P'), QLatin1Char(' '), false);

        QVERIFY(a == b);
        QVERIFY(!(a == c));
    }

    void shogiMove_dropFromPieceStand()
    {
        // 先手駒台: fromSquare.x() >= 9
        ShogiMove m(QPoint(9, 0), QPoint(4, 4), QLatin1Char('P'), QLatin1Char(' '), false);
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
        ShogiMove m(QPoint(6, 6), QPoint(6, 5), QLatin1Char('P'), QLatin1Char(' '), false);
        QCOMPARE(ShogiUtils::moveToUsi(m), QStringLiteral("7g7f"));
    }

    void shogiUtils_moveToUsi_promotion()
    {
        // 8h2b+: bishop move with promotion
        ShogiMove m(QPoint(7, 7), QPoint(1, 1), QLatin1Char('B'), QLatin1Char('r'), true);
        QCOMPARE(ShogiUtils::moveToUsi(m), QStringLiteral("8h2b+"));
    }

    void shogiUtils_moveToUsi_drop()
    {
        // P*5e: pawn drop from black's hand (x=9) to 5e (4,4)
        ShogiMove m(QPoint(9, 0), QPoint(4, 4), QLatin1Char('P'), QLatin1Char(' '), false);
        QCOMPARE(ShogiUtils::moveToUsi(m), QStringLiteral("P*5e"));
    }

    // === JishogiCalculator tests ===

    void jishogi_calculate()
    {
        // Set up a position with known pieces
        QVector<QChar> board(81, QLatin1Char(' '));
        QMap<QChar, int> stand;

        // Place black King in enemy territory (rank 1-3 for black)
        board[0] = QLatin1Char('K'); // rank 1, file 1

        // Place a black Rook (大駒 = 5 points)
        board[1] = QLatin1Char('R'); // rank 1, file 2

        // Place a black Bishop (大駒 = 5 points)
        board[2] = QLatin1Char('B'); // rank 1, file 3

        // Place black pawns (小駒 = 1 point each)
        board[3] = QLatin1Char('P'); // rank 1, file 4
        board[4] = QLatin1Char('P'); // rank 1, file 5

        // Place white King in its territory
        board[80] = QLatin1Char('k'); // rank 9, file 9

        auto result = JishogiCalculator::calculate(board, stand);

        // Sente: R(5) + B(5) + P(1) + P(1) = 12 total, K = 0
        QCOMPARE(result.sente.totalPoints, 12);

        // Big pieces worth 5 points each
        QVERIFY(result.sente.totalPoints >= 10); // At least 2 big pieces
    }
};

QTEST_MAIN(TestCoreDataStructures)
#include "tst_coredatastructures.moc"
