/// @file tst_pvboardcontroller.cpp
/// @brief PvBoardController のユニットテスト

#include <QtTest>

#include "pvboardcontroller.h"
#include "sfenutils.h"

class TestPvBoardController : public QObject
{
    Q_OBJECT

private slots:
    void dropHighlight_basePositionWhiteToMove_usesWhiteStand();
    void basePositionWithHand_isNotTreatedAsStartpos();
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

QTEST_MAIN(TestPvBoardController)

#include "tst_pvboardcontroller.moc"
