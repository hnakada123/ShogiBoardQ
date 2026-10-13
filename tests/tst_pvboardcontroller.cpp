/// @file tst_pvboardcontroller.cpp
/// @brief PvBoardController のユニットテスト

#include <QtTest>

#include "boardcoordinates.h"
#include "pvboardcontroller.h"
#include "sfenutils.h"

class TestPvBoardController : public QObject
{
    Q_OBJECT

private slots:
    void dropHighlight_basePositionWhiteToMove_usesWhiteStand();
    void basePositionWithHand_isNotTreatedAsStartpos();
    void diffHighlight_dropUsesStandOfDroppingSide();
    void diffHighlight_captureUsesSourceAndDestination();
    void diffHighlight_unreadablePreviousPositionGivesNoHighlight();
};

void TestPvBoardController::dropHighlight_basePositionWhiteToMove_usesWhiteStand()
{
    const QString baseSfen = QStringLiteral("%1 w Pp 1").arg(SfenUtils::hirateBoardSfen());

    PvBoardController controller(baseSfen, {});
    controller.setLastMove(QStringLiteral("P*5e"));

    const PvHighlightCoords coords = controller.computeHighlightCoords();

    QVERIFY(coords.valid);
    QVERIFY(coords.hasFrom);
    QVERIFY(coords.hasTo);
    QCOMPARE(coords.fromFile, 11);
    QCOMPARE(coords.fromRank, 9);
    QCOMPARE(coords.toFile, 5);
    QCOMPARE(coords.toRank, 5);
}

void TestPvBoardController::basePositionWithHand_isNotTreatedAsStartpos()
{
    const QString baseSfen = QStringLiteral("%1 b Pp 1").arg(SfenUtils::hirateBoardSfen());

    PvBoardController controller(baseSfen, {QStringLiteral("7g7f")});
    const PvHighlightCoords coords = controller.computeHighlightCoords();

    QVERIFY(!coords.valid);
}

// 直前の手がなく直前の局面だけがあるときは、2つの局面の盤の差分から移動元・移動先を決める
void TestPvBoardController::diffHighlight_dropUsesStandOfDroppingSide()
{
    PvBoardController controller(QStringLiteral("4k4/9/9/9/4g4/9/9/9/4K4 b - 2"), {});
    controller.setPrevSfen(QStringLiteral("4k4/9/9/9/9/9/9/9/4K4 w g 1"));

    const PvHighlightCoords coords = controller.computeHighlightCoords();

    QVERIFY(coords.valid);
    QVERIFY(coords.hasFrom);
    QCOMPARE(QPoint(coords.fromFile, coords.fromRank), BoardCoordinates::standSquareOf(Piece::WhiteGold));
    QVERIFY(coords.hasTo);
    QCOMPARE(coords.toFile, 5);
    QCOMPARE(coords.toRank, 5);
}

void TestPvBoardController::diffHighlight_captureUsesSourceAndDestination()
{
    PvBoardController controller(QStringLiteral("4k4/9/9/4R4/9/9/9/9/4K4 w P 2"), {});
    controller.setPrevSfen(QStringLiteral("4k4/9/9/4p4/9/4R4/9/9/4K4 b - 1"));

    const PvHighlightCoords coords = controller.computeHighlightCoords();

    QVERIFY(coords.valid);
    QVERIFY(coords.hasFrom);
    QCOMPARE(coords.fromFile, 5);
    QCOMPARE(coords.fromRank, 6);
    QVERIFY(coords.hasTo);
    QCOMPARE(coords.toFile, 5);
    QCOMPARE(coords.toRank, 4);
}

void TestPvBoardController::diffHighlight_unreadablePreviousPositionGivesNoHighlight()
{
    PvBoardController controller(QStringLiteral("4k4/9/9/9/4g4/9/9/9/4K4 b - 2"), {});
    controller.setPrevSfen(QStringLiteral("4k4/9/9/9/9/9/9/4K4 w g 1"));

    QVERIFY(!controller.computeHighlightCoords().valid);
}

QTEST_MAIN(TestPvBoardController)

#include "tst_pvboardcontroller.moc"
