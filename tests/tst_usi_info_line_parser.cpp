/// @file tst_usi_info_line_parser.cpp
/// @brief UsiInfoLineParser と ShogiEngineInfoParser（読み筋の表記）のユニットテスト

#include <QtTest>

#include "sfenutils.h"
#include "shogiengineinfoparser.h"
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

    void engineInfoParserShowsResignAsTerminalMove()
    {
        // 詰んだ局面ではエンジンが指し手の代わりに resign を返す。生の USI 文字列を表示しない
        ShogiEngineInfoParser parser;
        parser.setPosition(QStringLiteral("4k4/9/9/9/9/9/9/9/4K4 b - 1"));
        parser.parseEngineOutputAndUpdateState(QStringLiteral("info depth 0 score mate -1 pv resign"));
        QCOMPARE(parser.pvKanjiStr(), QStringLiteral("▲投了"));
        QCOMPARE(parser.pvUsiStr(), QString());

        parser.setPosition(QStringLiteral("4k4/9/9/9/9/9/9/9/4K4 w - 1"));
        parser.parseEngineOutputAndUpdateState(QStringLiteral("info depth 1 score mate 1 pv win"));
        QCOMPARE(parser.pvKanjiStr(), QStringLiteral("△入玉勝ち"));
    }

    // 読み筋は思考を始めた局面の上で指し手を進めて書く（成り・同・取った駒を打つ）
    void engineInfoParserFollowsPositionThroughPv()
    {
        ShogiEngineInfoParser parser;
        parser.setPosition(SfenUtils::hirateSfen());
        parser.parseEngineOutputAndUpdateState(
            QStringLiteral("info depth 5 score cp 30 pv 7g7f 3c3d 8h2b+ 3a2b B*4e"));
        QCOMPARE(parser.pvKanjiStr(),
                 QStringLiteral("▲７六歩(77)△３四歩(33)▲２二角成(88)△同銀(31)▲４五角打"));
        QCOMPARE(parser.pvUsiStr(), QStringLiteral("7g7f 3c3d 8h2b+ 3a2b B*4e"));
        QCOMPARE(parser.searchedHand(), QStringLiteral("▲７六歩(77)"));
        QCOMPARE(parser.searchedUsi(), QStringLiteral("7g7f"));
        QCOMPARE(parser.depth(), QStringLiteral("5"));
        QCOMPARE(parser.scoreCp(), QStringLiteral("30"));

        // 成った駒は成駒の名前で書く
        parser.parseEngineOutputAndUpdateState(QStringLiteral("info depth 5 pv 7g7f 3c3d 8h3c+ 4a3b 3c2b"));
        QCOMPARE(parser.pvKanjiStr(),
                 QStringLiteral("▲７六歩(77)△３四歩(33)▲３三角成(88)△３二金(41)▲２二馬(33)"));

        // 局面は info 行ごとに思考を始めた局面へ戻る
        parser.parseEngineOutputAndUpdateState(QStringLiteral("info depth 6 pv 2g2f 8c8d"));
        QCOMPARE(parser.pvKanjiStr(), QStringLiteral("▲２六歩(27)△８四歩(83)"));
    }

    // 初手の印は局面の手番で決まり、直前の手と同じマスへの初手は「同」と書く
    void engineInfoParserUsesSideToMoveAndPreviousMove()
    {
        ShogiEngineInfoParser parser;
        // ▲７六歩 △３四歩 ▲２二角成 の後（後手番）
        parser.setPosition(QStringLiteral("lnsgkgsnl/1r5+B1/pppppp1pp/6p2/9/2P6/PP1PPPPPP/7R1/LNSGKGSNL w B 4"));
        parser.setPreviousMove(BoardSquare{2, 2});
        parser.parseEngineOutputAndUpdateState(QStringLiteral("info depth 3 currmove 3a2b"));
        QCOMPARE(parser.searchedHand(), QStringLiteral("△同銀(31)"));
        QCOMPARE(parser.searchedUsi(), QStringLiteral("3a2b"));
        QCOMPARE(parser.pvKanjiStr(), QString());

        parser.parseEngineOutputAndUpdateState(QStringLiteral("info depth 3 pv 3a2b B*3c"));
        QCOMPARE(parser.pvKanjiStr(), QStringLiteral("△同銀(31)▲３三角打"));

        // 直前の手がなければ移動先を書く
        parser.setPreviousMove(BoardSquare{});
        parser.parseEngineOutputAndUpdateState(QStringLiteral("info depth 3 pv 3a2b"));
        QCOMPARE(parser.pvKanjiStr(), QStringLiteral("△２二銀(31)"));
    }

    // 局面と合わない指し手（持っていない駒を打つ・相手の駒を動かす・駒のないマスから動かす）で変換をやめる。
    // そこまでの漢字表記は残し、USI形式の読み筋は残さない
    void engineInfoParserStopsAtMoveNotFittingPosition()
    {
        ShogiEngineInfoParser parser;
        parser.setPosition(SfenUtils::hirateSfen());

        parser.parseEngineOutputAndUpdateState(QStringLiteral("info depth 2 pv 7g7f G*5e"));
        QCOMPARE(parser.pvKanjiStr(), QStringLiteral("▲７六歩(77)"));
        QCOMPARE(parser.pvUsiStr(), QString());

        parser.parseEngineOutputAndUpdateState(QStringLiteral("info depth 2 pv 7g7f 7f7e"));
        QCOMPARE(parser.pvKanjiStr(), QStringLiteral("▲７六歩(77)"));
        QCOMPARE(parser.pvUsiStr(), QString());

        parser.parseEngineOutputAndUpdateState(QStringLiteral("info depth 2 pv 5e5d"));
        QCOMPARE(parser.pvKanjiStr(), QString());
        QCOMPARE(parser.pvUsiStr(), QString());
        QCOMPARE(parser.searchedHand(), QString());
    }

    // 末尾の勝率などの指し手でない語はそのまま付け、途中に崩れた指し手があればそこまでにする
    void engineInfoParserHandlesNonMoveTokens()
    {
        ShogiEngineInfoParser parser;
        parser.setPosition(SfenUtils::hirateSfen());

        parser.parseEngineOutputAndUpdateState(QStringLiteral("info depth 2 pv 7g7f 3c3d (57.54%)"));
        QCOMPARE(parser.pvKanjiStr(), QStringLiteral("▲７六歩(77)△３四歩(33) (57.54%)"));
        QCOMPARE(parser.pvUsiStr(), QStringLiteral("7g7f 3c3d"));

        parser.parseEngineOutputAndUpdateState(QStringLiteral("info depth 2 pv 7g7f 7z7f 3c3d"));
        QCOMPARE(parser.pvKanjiStr(), QStringLiteral("▲７六歩(77)"));
        QCOMPARE(parser.pvUsiStr(), QStringLiteral("7g7f"));

        parser.parseEngineOutputAndUpdateState(QStringLiteral("info depth 2 pv 7g7f (57%) 3c3d"));
        QCOMPARE(parser.pvKanjiStr(), QStringLiteral("▲７六歩(77)"));
    }

    // 局面がない・読めないときは、読み筋を日本語にしない（ほかの項目は読む）
    void engineInfoParserWithoutPositionKeepsOtherFields()
    {
        ShogiEngineInfoParser parser;
        parser.parseEngineOutputAndUpdateState(QStringLiteral("info depth 4 nodes 100 pv 7g7f"));
        QCOMPARE(parser.depth(), QStringLiteral("4"));
        QCOMPARE(parser.nodes(), QStringLiteral("100"));
        QCOMPARE(parser.pvKanjiStr(), QString());
        QCOMPARE(parser.pvUsiStr(), QString());

        parser.setPosition(SfenUtils::hirateSfen());
        parser.setPosition(QStringLiteral("not a sfen"));
        parser.parseEngineOutputAndUpdateState(QStringLiteral("info depth 4 pv 7g7f"));
        QCOMPARE(parser.pvKanjiStr(), QString());
    }
};

QTEST_GUILESS_MAIN(TestUsiInfoLineParser)
#include "tst_usi_info_line_parser.moc"
