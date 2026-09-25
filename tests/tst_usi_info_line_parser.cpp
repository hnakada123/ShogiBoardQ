/// @file tst_usi_info_line_parser.cpp
/// @brief UsiInfoLineParser のユニットテスト

#include <QtTest>

#include "usiinfolineparser.h"

class TestUsiInfoLineParser : public QObject
{
    Q_OBJECT

private slots:
    void parsesDepthScorePv()
    {
        const UsiInfoLine info = UsiInfoLineParser::parse(
            QStringLiteral("info depth 12 seldepth 20 multipv 2 score cp -35 nodes 123456 nps 987654 time 1250 hashfull 42 pv 7g7f 3c3d 2g2f"));
        QCOMPARE(info.depth, 12);
        QCOMPARE(info.seldepth, 20);
        QCOMPARE(info.multipv, 2);
        QVERIFY(info.scoreCp.has_value());
        QCOMPARE(*info.scoreCp, -35);
        QVERIFY(!info.scoreMate.has_value());
        QCOMPARE(info.nodes, 123456LL);
        QCOMPARE(info.nps, 987654LL);
        QCOMPARE(info.timeMs, 1250LL);
        QCOMPARE(info.hashfull, 42);
        QCOMPARE(info.pv, QStringList({QStringLiteral("7g7f"), QStringLiteral("3c3d"), QStringLiteral("2g2f")}));
        QVERIFY(info.hasPv());
        QVERIFY(info.bound.isEmpty());
    }

    void parsesMateAndBounds()
    {
        UsiInfoLine mate = UsiInfoLineParser::parse(QStringLiteral("info depth 5 score mate 3 pv G*5b 4a4b 5b4b"));
        QVERIFY(mate.scoreMate.has_value());
        QCOMPARE(*mate.scoreMate, 3);
        QCOMPARE(mate.pv.size(), 3);

        UsiInfoLine lower = UsiInfoLineParser::parse(QStringLiteral("info depth 8 score cp 120 lowerbound nodes 10 pv 2g2f"));
        QCOMPARE(lower.bound, QStringLiteral("lower"));
        QCOMPARE(*lower.scoreCp, 120);
        QCOMPARE(lower.nodes, 10LL);

        UsiInfoLine upper = UsiInfoLineParser::parse(QStringLiteral("info score cp -5 upperbound pv 8c8d"));
        QCOMPARE(upper.bound, QStringLiteral("upper"));
        QCOMPARE(upper.multipv, 1);
    }

    void ignoresNonInfoAndStringLines()
    {
        const UsiInfoLine other = UsiInfoLineParser::parse(QStringLiteral("bestmove 7g7f ponder 3c3d"));
        QVERIFY(!other.hasPv());
        QCOMPARE(other.depth, -1);

        const UsiInfoLine str = UsiInfoLineParser::parse(QStringLiteral("info string hello world pv not a move"));
        QVERIFY(!str.hasPv());
        QCOMPARE(str.string, QStringLiteral("hello world pv not a move"));

        const UsiInfoLine curr = UsiInfoLineParser::parse(QStringLiteral("info currmove 7g7f currmovenumber 1"));
        QCOMPARE(curr.currmove, QStringLiteral("7g7f"));
        QVERIFY(!curr.hasPv());
    }

    void jsonOmitsUnsetFields()
    {
        const QJsonObject json = UsiInfoLineParser::parse(QStringLiteral("info depth 3 score cp 10 pv 7g7f")).toJson();
        QCOMPARE(json.value(QStringLiteral("depth")).toInt(), 3);
        QCOMPARE(json.value(QStringLiteral("score_cp")).toInt(), 10);
        QVERIFY(!json.contains(QStringLiteral("nodes")));
        QVERIFY(!json.contains(QStringLiteral("score_mate")));
        QCOMPARE(json.value(QStringLiteral("pv")).toArray().size(), 1);
    }
};

QTEST_GUILESS_MAIN(TestUsiInfoLineParser)
#include "tst_usi_info_line_parser.moc"
