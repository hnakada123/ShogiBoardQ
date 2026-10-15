#include <QtTest>

#include "shogimove.h"

class TestShogiMove : public QObject
{
    Q_OBJECT

private slots:
    void defaultConstructor()
    {
        ShogiMove m;
        QVERIFY(!m.move.isValid());
        QCOMPARE(m.movingPiece, Piece::None);
        QCOMPARE(m.capturedPiece, Piece::None);
        QVERIFY(!m.isPromotion());
        QVERIFY(!m.isValid());
    }

    void constructor_keepsMoveAndPieces()
    {
        ShogiMove m(UsiMove::fromString(u"8h2b+"), Piece::BlackBishop, Piece::WhiteRook);
        QCOMPARE(m.move.from, (BoardSquare{8, 8}));
        QCOMPARE(m.move.to, (BoardSquare{2, 2}));
        QCOMPARE(m.movingPiece, Piece::BlackBishop);
        QCOMPARE(m.capturedPiece, Piece::WhiteRook);
        QVERIFY(m.isPromotion());
        QVERIFY(!m.isDrop());
        QVERIFY(m.isValid());
    }

    void mover_followsMovingPieceColor()
    {
        QCOMPARE(ShogiMove(UsiMove::fromString(u"7g7f"), Piece::BlackPawn).mover(), Turn::Black);
        QCOMPARE(ShogiMove(UsiMove::fromString(u"3c3d"), Piece::WhitePawn).mover(), Turn::White);
        QCOMPARE(ShogiMove(UsiMove::fromString(u"P*5e"), Piece::WhitePawn).mover(), Turn::White);
    }

    void equality()
    {
        const ShogiMove a(UsiMove::fromString(u"7g7f"), Piece::BlackPawn);
        QVERIFY(a == ShogiMove(UsiMove::fromString(u"7g7f"), Piece::BlackPawn));
        QVERIFY(a != ShogiMove(UsiMove::fromString(u"7g7e"), Piece::BlackPawn));
        QVERIFY(a != ShogiMove(UsiMove::fromString(u"7g7f"), Piece::BlackLance));
        QVERIFY(a != ShogiMove(UsiMove::fromString(u"7g7f"), Piece::BlackPawn, Piece::WhitePawn));
        QVERIFY(ShogiMove(UsiMove::fromString(u"5c5b"), Piece::BlackPawn)
                != ShogiMove(UsiMove::fromString(u"5c5b+"), Piece::BlackPawn));
    }

    void drop()
    {
        const ShogiMove black(UsiMove::fromString(u"P*5e"), Piece::BlackPawn);
        const ShogiMove white(UsiMove::fromString(u"P*5e"), Piece::WhitePawn);
        QVERIFY(black.isDrop());
        QCOMPARE(black.move.toString(), QStringLiteral("P*5e"));
        QCOMPARE(white.move.toString(), QStringLiteral("P*5e"));
        QVERIFY(black != white);
    }
};

QTEST_MAIN(TestShogiMove)
#include "tst_shogimove.moc"
