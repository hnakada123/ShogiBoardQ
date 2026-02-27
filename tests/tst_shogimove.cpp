#include <QtTest>

#include "shogimove.h"
#include "shogiutils.h"

class TestShogiMove : public QObject
{
    Q_OBJECT

private slots:
    // === Constructors ===

    void defaultConstructor()
    {
        ShogiMove m;
        QCOMPARE(m.fromSquare, QPoint(0, 0));
        QCOMPARE(m.toSquare, QPoint(0, 0));
        QCOMPARE(m.movingPiece, Piece::None);
        QCOMPARE(m.capturedPiece, Piece::None);
        QVERIFY(!m.isPromotion);
    }

    void parameterConstructor()
    {
        ShogiMove m(QPoint(7, 7), QPoint(7, 6), Piece::BlackPawn, Piece::None, false);
        QCOMPARE(m.fromSquare, QPoint(7, 7));
        QCOMPARE(m.toSquare, QPoint(7, 6));
        QCOMPARE(m.movingPiece, Piece::BlackPawn);
        QCOMPARE(m.capturedPiece, Piece::None);
        QVERIFY(!m.isPromotion);
    }

    void parameterConstructor_withCapture()
    {
        ShogiMove m(QPoint(7, 7), QPoint(1, 1), Piece::BlackBishop, Piece::WhiteRook, true);
        QCOMPARE(m.fromSquare, QPoint(7, 7));
        QCOMPARE(m.toSquare, QPoint(1, 1));
        QCOMPARE(m.movingPiece, Piece::BlackBishop);
        QCOMPARE(m.capturedPiece, Piece::WhiteRook);
        QVERIFY(m.isPromotion);
    }

    // === Equality operator ===

    void equality_sameMove()
    {
        ShogiMove a(QPoint(7, 7), QPoint(7, 6), Piece::BlackPawn, Piece::None, false);
        ShogiMove b(QPoint(7, 7), QPoint(7, 6), Piece::BlackPawn, Piece::None, false);
        QVERIFY(a == b);
    }

    void equality_differentFrom()
    {
        ShogiMove a(QPoint(7, 7), QPoint(7, 6), Piece::BlackPawn, Piece::None, false);
        ShogiMove b(QPoint(6, 6), QPoint(7, 6), Piece::BlackPawn, Piece::None, false);
        QVERIFY(!(a == b));
    }

    void equality_differentTo()
    {
        ShogiMove a(QPoint(7, 7), QPoint(7, 6), Piece::BlackPawn, Piece::None, false);
        ShogiMove b(QPoint(7, 7), QPoint(7, 5), Piece::BlackPawn, Piece::None, false);
        QVERIFY(!(a == b));
    }

    void equality_differentPiece()
    {
        ShogiMove a(QPoint(4, 4), QPoint(4, 3), Piece::BlackPawn, Piece::None, false);
        ShogiMove b(QPoint(4, 4), QPoint(4, 3), Piece::BlackLance, Piece::None, false);
        QVERIFY(!(a == b));
    }

    void equality_differentPromotion()
    {
        ShogiMove a(QPoint(4, 2), QPoint(4, 1), Piece::BlackPawn, Piece::None, false);
        ShogiMove b(QPoint(4, 2), QPoint(4, 1), Piece::BlackPawn, Piece::None, true);
        QVERIFY(!(a == b));
    }

    void equality_differentCapture()
    {
        ShogiMove a(QPoint(4, 4), QPoint(4, 3), Piece::BlackRook, Piece::None, false);
        ShogiMove b(QPoint(4, 4), QPoint(4, 3), Piece::BlackRook, Piece::WhitePawn, false);
        QVERIFY(!(a == b));
    }

    // === Drop from piece stand ===

    void dropMove_blackPieceStand()
    {
        // 先手駒台からの打ち: fromSquare.x() == 9
        ShogiMove m(QPoint(9, 0), QPoint(4, 4), Piece::BlackPawn, Piece::None, false);
        QCOMPARE(m.fromSquare.x(), 9);
        QVERIFY(m.fromSquare.x() >= 9);
    }

    void dropMove_whitePieceStand()
    {
        // 後手駒台からの打ち: fromSquare.x() == 10
        ShogiMove m(QPoint(10, 0), QPoint(4, 4), Piece::WhitePawn, Piece::None, false);
        QCOMPARE(m.fromSquare.x(), 10);
        QVERIFY(m.fromSquare.x() >= 9);
    }

    // === USI conversion (via ShogiUtils) ===

    void usiConversion_normalMove()
    {
        ShogiMove m(QPoint(6, 6), QPoint(6, 5), Piece::BlackPawn, Piece::None, false);
        QCOMPARE(ShogiUtils::moveToUsi(m), QStringLiteral("7g7f"));
    }

    void usiConversion_promotionMove()
    {
        ShogiMove m(QPoint(7, 7), QPoint(1, 1), Piece::BlackBishop, Piece::WhiteRook, true);
        QCOMPARE(ShogiUtils::moveToUsi(m), QStringLiteral("8h2b+"));
    }

    void usiConversion_dropMove()
    {
        ShogiMove m(QPoint(9, 0), QPoint(4, 4), Piece::BlackPawn, Piece::None, false);
        QCOMPARE(ShogiUtils::moveToUsi(m), QStringLiteral("P*5e"));
    }

    void usiConversion_cornerMove()
    {
        // 1a → 9i (corner to corner)
        ShogiMove m(QPoint(0, 0), QPoint(8, 8), Piece::BlackBishop, Piece::None, false);
        QCOMPARE(ShogiUtils::moveToUsi(m), QStringLiteral("1a9i"));
    }

    void usiConversion_whiteDropMove()
    {
        // 後手駒台からの打ち
        ShogiMove m(QPoint(10, 0), QPoint(4, 4), Piece::WhitePawn, Piece::None, false);
        QCOMPARE(ShogiUtils::moveToUsi(m), QStringLiteral("P*5e"));
    }
};

QTEST_MAIN(TestShogiMove)
#include "tst_shogimove.moc"
