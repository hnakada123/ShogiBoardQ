/// @file tst_pvline.cpp
/// @brief 読み筋盤の読み筋 PvLine のテスト（各手数の局面・手の表記・直前の手のハイライト）

#include <QtTest>

#include "boardcoordinates.h"
#include "kifupresentation.h"
#include "pvline.h"
#include "sfenutils.h"

class TestPvLine : public QObject
{
    Q_OBJECT

private:
    static QPoint from(const PvLine::Highlight& highlight) { return highlight.from.value_or(QPoint()); }
    static QPoint to(const PvLine::Highlight& highlight) { return highlight.to.value_or(QPoint()); }
    static PvLine lineOf(const QString& sfen, const QStringList& moves) { return PvLine(PvText::play(sfen, moves)); }

private slots:
    void init() { KifuPresentation::configure(QStringLiteral("ja_JP"), QStringLiteral("japanese"), false); }

    /// 各手数の局面は手数を数え、指せない手の手前で打ち切る
    void positionsCountPliesAndStopAtUnplayableMove()
    {
        const PvLine line = lineOf(SfenUtils::hirateSfen(),
                          {QStringLiteral("7g7f"), QStringLiteral("3c3d"), QStringLiteral("5e5d"), QStringLiteral("2g2f")});
        QCOMPARE(line.moveCount(), 2);
        const QStringList positions = line.positions();
        QCOMPARE(positions.size(), 3);
        QCOMPARE(positions.at(0), SfenUtils::hirateSfen());
        QCOMPARE(positions.at(2), QStringLiteral("lnsgkgsnl/1r5b1/pppppp1pp/6p2/9/2P6/PP1PPPPPP/1B5R1/LNSGKGSNL b - 3"));
    }

    /// 日本語の手の表記は局面と指し手から作り、「同」も付ける
    void japaneseMoveTextUsesSameSquare()
    {
        const PvLine line = lineOf(QStringLiteral("4k4/9/4p4/9/9/9/4P4/4R4/4K4 b - 1"),
                          {QStringLiteral("5g5f"), QStringLiteral("5c5d"), QStringLiteral("5f5e"), QStringLiteral("5d5e")});
        QCOMPARE(line.moveText(0), QString());
        QCOMPARE(line.moveText(1), QStringLiteral("▲５六歩(57)"));
        QCOMPARE(line.moveText(4), QStringLiteral("△同歩(54)"));
        QCOMPARE(line.moveText(5), QString());
    }

    void westernMoveText()
    {
        KifuPresentation::configure(QStringLiteral("en"), QStringLiteral("western"), false);
        const PvLine line = lineOf(SfenUtils::hirateSfen(), {QStringLiteral("7g7f")});
        QCOMPARE(line.moveText(1), QStringLiteral("▲P-7f"));
    }

    /// 読み筋の各手のハイライトは、その手の移動元（駒打ちは打った側の駒台）と移動先
    void moveHighlightUsesMoverStand()
    {
        const PvLine line = lineOf(QStringLiteral("4k4/9/9/9/9/9/9/9/4K4 w P 1"), {QStringLiteral("5a4a"), QStringLiteral("P*5e")});
        QCOMPARE(from(line.highlight(1)), QPoint(5, 1));
        QCOMPARE(to(line.highlight(1)), QPoint(4, 1));
        QCOMPARE(from(line.highlight(2)), BoardCoordinates::standSquareOf(Piece::BlackPawn));
        QCOMPARE(to(line.highlight(2)), QPoint(5, 5));
        QVERIFY(!line.highlight(3).to);
    }

    /// 開始局面に至った駒打ちは、開始局面の手番の相手が打った手（後手番なら先手の駒台）
    void arrivalDropUsesStandOfPreviousMover()
    {
        PvLine line = lineOf(QStringLiteral("%1 w Pp 1").arg(SfenUtils::hirateBoardSfen()), {});
        line.setArrival(QStringLiteral("P*5e"), QString());
        const PvLine::Highlight highlight = line.highlight(0);
        QCOMPARE(from(highlight), BoardCoordinates::standSquareOf(Piece::BlackPawn));
        QCOMPARE(to(highlight), QPoint(5, 5));
    }

    /// 開始局面に至った手が分からなければハイライトしない（平手の開始局面で読み筋の初手を出さない）
    void noArrivalGivesNoHighlight()
    {
        const PvLine line = lineOf(SfenUtils::hirateSfen(), {QStringLiteral("7g7f")});
        QVERIFY(!line.highlight(0).from);
        QVERIFY(!line.highlight(0).to);
    }

    // 直前の手がなく直前の局面だけがあるときは、2つの局面の盤の差分から移動元・移動先を決める
    void diffHighlight_dropUsesStandOfDroppingSide()
    {
        PvLine line = lineOf(QStringLiteral("4k4/9/9/9/4g4/9/9/9/4K4 b - 2"), {});
        line.setArrival(QString(), QStringLiteral("4k4/9/9/9/9/9/9/9/4K4 w g 1"));
        QCOMPARE(from(line.highlight(0)), BoardCoordinates::standSquareOf(Piece::WhiteGold));
        QCOMPARE(to(line.highlight(0)), QPoint(5, 5));
    }

    void diffHighlight_captureUsesSourceAndDestination()
    {
        PvLine line = lineOf(QStringLiteral("4k4/9/9/4R4/9/9/9/9/4K4 w P 2"), {});
        line.setArrival(QString(), QStringLiteral("4k4/9/9/4p4/9/4R4/9/9/4K4 b - 1"));
        QCOMPARE(from(line.highlight(0)), QPoint(5, 6));
        QCOMPARE(to(line.highlight(0)), QPoint(5, 4));
    }

    void diffHighlight_boardMoveOffCenterKeepsFileAndRank()
    {
        // 5筋以外のマスで、筋の向きと段を取り違えないこと（7七→7六）
        PvLine line = lineOf(QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/2P6/PP1PPPPPP/1B5R1/LNSGKGSNL w - 2"), {});
        line.setArrival(QString(), SfenUtils::hirateSfen());
        QCOMPARE(from(line.highlight(0)), QPoint(7, 7));
        QCOMPARE(to(line.highlight(0)), QPoint(7, 6));
    }

    void diffHighlight_unreadablePreviousPositionGivesNoHighlight()
    {
        PvLine line = lineOf(QStringLiteral("4k4/9/9/9/4g4/9/9/9/4K4 b - 2"), {});
        line.setArrival(QString(), QStringLiteral("4k4/9/9/9/9/9/9/4K4 w g 1"));
        QVERIFY(!line.highlight(0).to);
    }
};

QTEST_MAIN(TestPvLine)

#include "tst_pvline.moc"
