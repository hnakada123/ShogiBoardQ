/// @file tst_usimove.cpp
/// @brief USI の指し手の値型・表示座標との対応・盤への適用のテスト

#include <QtTest>

#include "boardcoordinates.h"
#include "shogiboard.h"
#include "shogimove.h"
#include "usimove.h"

class TestUsiMove : public QObject
{
    Q_OBJECT

private slots:
    void parse_roundTrips_data()
    {
        QTest::addColumn<QString>("text");
        QTest::newRow("move") << QStringLiteral("7g7f");
        QTest::newRow("promote") << QStringLiteral("8h2b+");
        QTest::newRow("drop") << QStringLiteral("P*5e");
        QTest::newRow("rook-drop") << QStringLiteral("R*1a");
    }

    void parse_roundTrips()
    {
        QFETCH(QString, text);
        const auto move = UsiMove::parse(text);
        QVERIFY(move);
        QVERIFY(move->isValid());
        QCOMPARE(move->toString(), text);
    }

    void parse_readsParts()
    {
        const auto move = UsiMove::parse(u" 8h2b+ ");
        QVERIFY(move);
        QCOMPARE(move->from, (BoardSquare{8, 8}));
        QCOMPARE(move->to, (BoardSquare{2, 2}));
        QVERIFY(move->promote);
        QVERIFY(!move->isDrop());

        const auto drop = UsiMove::parse(u"g*5e");
        QVERIFY(drop);
        QCOMPARE(drop->drop, Piece::BlackGold);
        QCOMPARE(drop->dropPieceFor(Turn::White), Piece::WhiteGold);
        QCOMPARE(drop->toString(), QStringLiteral("G*5e"));
    }

    void parse_rejectsMalformed_data()
    {
        QTest::addColumn<QString>("text");
        QTest::newRow("empty") << QString();
        QTest::newRow("short") << QStringLiteral("7g7");
        QTest::newRow("file-zero") << QStringLiteral("0g7f");
        QTest::newRow("rank-j") << QStringLiteral("7j7f");
        QTest::newRow("same-square") << QStringLiteral("7g7g");
        QTest::newRow("bad-suffix") << QStringLiteral("7g7f=");
        QTest::newRow("king-drop") << QStringLiteral("K*5e");
        QTest::newRow("promoted-drop") << QStringLiteral("P*5e+");
        QTest::newRow("special") << QStringLiteral("resign");
        QTest::newRow("csa") << QStringLiteral("+7776FU");
    }

    void parse_rejectsMalformed()
    {
        QFETCH(QString, text);
        QVERIFY(!UsiMove::parse(text));
    }

    void notation_squares()
    {
        QCOMPARE(UsiNotation::squareText({1, 1}), QStringLiteral("1a"));
        QCOMPARE(UsiNotation::squareText({9, 9}), QStringLiteral("9i"));
        QVERIFY(UsiNotation::squareText({0, 1}).isEmpty());
        QCOMPARE(UsiNotation::parseSquare(u"5e"), std::optional<BoardSquare>(BoardSquare{5, 5}));
        QVERIFY(!UsiNotation::parseSquare(u"5E"));
        QVERIFY(!UsiNotation::parseSquare(u"5"));
    }

    /// 駒台の段と駒の対応は表示・クリック・記録で同じ
    void coordinates_standRoundTrip()
    {
        for (const Piece piece : {Piece::BlackPawn, Piece::BlackLance, Piece::BlackKnight, Piece::BlackSilver,
                                  Piece::BlackGold, Piece::BlackBishop, Piece::BlackRook, Piece::BlackKing,
                                  Piece::WhitePawn, Piece::WhiteLance, Piece::WhiteKnight, Piece::WhiteSilver,
                                  Piece::WhiteGold, Piece::WhiteBishop, Piece::WhiteRook, Piece::WhiteKing}) {
            const QPoint square = BoardCoordinates::standSquareOf(piece);
            QVERIFY(!square.isNull());
            QCOMPARE(BoardCoordinates::standPieceAt(square.x(), square.y()), piece);
        }
        QCOMPARE(BoardCoordinates::standSquareOf(Piece::BlackPawn), QPoint(10, 1));
        QCOMPARE(BoardCoordinates::standSquareOf(Piece::WhitePawn), QPoint(11, 9));
        QCOMPARE(BoardCoordinates::standSquareOf(Piece::WhiteHorse), QPoint(11, 4));
        QCOMPARE(BoardCoordinates::standPieceAt(10, 9), Piece::None);
    }

    void coordinates_fromClicks()
    {
        const auto move = BoardCoordinates::usiMoveFromSquares(QPoint(7, 7), QPoint(7, 6), false);
        QVERIFY(move);
        QCOMPARE(move->toString(), QStringLiteral("7g7f"));
        const auto drop = BoardCoordinates::usiMoveFromSquares(QPoint(11, 3), QPoint(5, 5), true);
        QVERIFY(drop);
        QCOMPARE(drop->toString(), QStringLiteral("R*5e"));
        QCOMPARE(BoardCoordinates::fromSquare(*drop, Turn::White), QPoint(11, 3));
        QCOMPARE(BoardCoordinates::fromSquare(*drop, Turn::Black), QPoint(10, 7));
        QVERIFY(!BoardCoordinates::usiMoveFromSquares(QPoint(10, 8), QPoint(5, 5), false));  // 玉は打てない
        QVERIFY(!BoardCoordinates::usiMoveFromSquares(QPoint(7, 7), QPoint(10, 1), false));  // 駒台へは指せない
    }

    void board_appliesMoves()
    {
        ShogiBoard board;
        board.setSfen(QStringLiteral("4k4/9/9/9/9/9/9/1B7/4K4 b P 1"));
        QVERIFY(board.applyUsiMove(*UsiMove::parse(u"P*5e"), Turn::Black));
        QCOMPARE(board.pieceCharacter(5, 5), Piece::BlackPawn);
        QCOMPARE(board.pieceStandCount(Piece::BlackPawn), 0);
        QVERIFY(!board.applyUsiMove(*UsiMove::parse(u"P*5d"), Turn::Black));  // 駒台に歩がない
        QVERIFY(board.applyUsiMove(*UsiMove::parse(u"8h2b+"), Turn::Black));
        QCOMPARE(board.pieceCharacter(2, 2), Piece::BlackHorse);
        QVERIFY(!board.applyUsiMove(*UsiMove::parse(u"8h7g"), Turn::Black));  // 移動元が空
        QVERIFY(board.applyUsiMove(*UsiMove::parse(u"5a5b"), Turn::White));
        QVERIFY(board.applyUsiMove(*UsiMove::parse(u"2b5b"), Turn::Black));  // 玉を取る（盤への適用は合法性を見ない）
        QCOMPARE(board.pieceStandCount(Piece::BlackKing), 1);
    }

    void shogiMove_convertsBothWays()
    {
        const UsiMove drop = *UsiMove::parse(u"S*4d");
        const ShogiMove recorded(drop, Piece::WhiteSilver);
        QCOMPARE(recorded.mover(), Turn::White);
        QVERIFY(recorded.isDrop());
        QCOMPARE(recorded.move, drop);

        const UsiMove promote = *UsiMove::parse(u"2c2b+");
        const ShogiMove moved(promote, Piece::BlackPawn, Piece::WhiteBishop);
        QVERIFY(moved.isPromotion());
        QCOMPARE(moved.move, promote);
    }
};

QTEST_MAIN(TestUsiMove)
#include "tst_usimove.moc"
