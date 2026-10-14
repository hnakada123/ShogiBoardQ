/// @file tst_pvnotationformatter.cpp
/// @brief PvNotationFormatter（info の読み筋・探索手の漢字表記）のユニットテスト

#include <QtTest>

#include "sfenutils.h"
#include "pvnotationformatter.h"
#include "usiinfoparser.h"

namespace {
/// info 行を解析して formatter で漢字表記にする（解析できない行は空の info）
void format(PvNotationFormatter& formatter, const QString& line)
{
    formatter.format(UsiInfoParser::parse(line).value_or(UsiInfo{}));
}
} // namespace

class TestPvNotationFormatter : public QObject
{
    Q_OBJECT

private slots:
    void formatterShowsResignAsTerminalMove()
    {
        // 詰んだ局面ではエンジンが指し手の代わりに resign を返す。生の USI 文字列を表示しない
        PvNotationFormatter parser;
        parser.setPosition(QStringLiteral("4k4/9/9/9/9/9/9/9/4K4 b - 1"));
        format(parser, QStringLiteral("info depth 0 score mate -1 pv resign"));
        QCOMPARE(parser.pvKanjiStr(), QStringLiteral("▲投了"));
        QCOMPARE(parser.pvUsiStr(), QString());

        parser.setPosition(QStringLiteral("4k4/9/9/9/9/9/9/9/4K4 w - 1"));
        format(parser, QStringLiteral("info depth 1 score mate 1 pv win"));
        QCOMPARE(parser.pvKanjiStr(), QStringLiteral("△入玉勝ち"));
    }

    // 読み筋は思考を始めた局面の上で指し手を進めて書く（成り・同・取った駒を打つ）
    void formatterFollowsPositionThroughPv()
    {
        PvNotationFormatter parser;
        parser.setPosition(SfenUtils::hirateSfen());
        format(parser, 
            QStringLiteral("info depth 5 score cp 30 pv 7g7f 3c3d 8h2b+ 3a2b B*4e"));
        QCOMPARE(parser.pvKanjiStr(),
                 QStringLiteral("▲７六歩(77)△３四歩(33)▲２二角成(88)△同銀(31)▲４五角打"));
        QCOMPARE(parser.pvUsiStr(), QStringLiteral("7g7f 3c3d 8h2b+ 3a2b B*4e"));
        QCOMPARE(parser.searchedHand(), QStringLiteral("▲７六歩(77)"));
        QCOMPARE(parser.searchedUsi(), QStringLiteral("7g7f"));

        // 成った駒は成駒の名前で書く
        format(parser, QStringLiteral("info depth 5 pv 7g7f 3c3d 8h3c+ 4a3b 3c2b"));
        QCOMPARE(parser.pvKanjiStr(),
                 QStringLiteral("▲７六歩(77)△３四歩(33)▲３三角成(88)△３二金(41)▲２二馬(33)"));

        // 局面は info 行ごとに思考を始めた局面へ戻る
        format(parser, QStringLiteral("info depth 6 pv 2g2f 8c8d"));
        QCOMPARE(parser.pvKanjiStr(), QStringLiteral("▲２六歩(27)△８四歩(83)"));
    }

    // 初手の印は局面の手番で決まり、直前の手と同じマスへの初手は「同」と書く
    void formatterUsesSideToMoveAndPreviousMove()
    {
        PvNotationFormatter parser;
        // ▲７六歩 △３四歩 ▲２二角成 の後（後手番）
        parser.setPosition(QStringLiteral("lnsgkgsnl/1r5+B1/pppppp1pp/6p2/9/2P6/PP1PPPPPP/7R1/LNSGKGSNL w B 4"));
        parser.setPreviousMove(BoardSquare{2, 2});
        format(parser, QStringLiteral("info depth 3 currmove 3a2b"));
        QCOMPARE(parser.searchedHand(), QStringLiteral("△同銀(31)"));
        QCOMPARE(parser.searchedUsi(), QStringLiteral("3a2b"));
        QCOMPARE(parser.pvKanjiStr(), QString());

        format(parser, QStringLiteral("info depth 3 pv 3a2b B*3c"));
        QCOMPARE(parser.pvKanjiStr(), QStringLiteral("△同銀(31)▲３三角打"));

        // 直前の手がなければ移動先を書く
        parser.setPreviousMove(BoardSquare{});
        format(parser, QStringLiteral("info depth 3 pv 3a2b"));
        QCOMPARE(parser.pvKanjiStr(), QStringLiteral("△２二銀(31)"));
    }

    // 局面と合わない指し手（持っていない駒を打つ・相手の駒を動かす・駒のないマスから動かす）で変換をやめる。
    // そこまでの漢字表記は残し、USI形式の読み筋は残さない
    void formatterStopsAtMoveNotFittingPosition()
    {
        PvNotationFormatter parser;
        parser.setPosition(SfenUtils::hirateSfen());

        format(parser, QStringLiteral("info depth 2 pv 7g7f G*5e"));
        QCOMPARE(parser.pvKanjiStr(), QStringLiteral("▲７六歩(77)"));
        QCOMPARE(parser.pvUsiStr(), QString());

        format(parser, QStringLiteral("info depth 2 pv 7g7f 7f7e"));
        QCOMPARE(parser.pvKanjiStr(), QStringLiteral("▲７六歩(77)"));
        QCOMPARE(parser.pvUsiStr(), QString());

        format(parser, QStringLiteral("info depth 2 pv 5e5d"));
        QCOMPARE(parser.pvKanjiStr(), QString());
        QCOMPARE(parser.pvUsiStr(), QString());
        QCOMPARE(parser.searchedHand(), QString());
    }

    // 末尾の勝率などの指し手でない語はそのまま付け、途中に崩れた指し手があればそこまでにする
    void formatterHandlesNonMoveTokens()
    {
        PvNotationFormatter parser;
        parser.setPosition(SfenUtils::hirateSfen());

        format(parser, QStringLiteral("info depth 2 pv 7g7f 3c3d (57.54%)"));
        QCOMPARE(parser.pvKanjiStr(), QStringLiteral("▲７六歩(77)△３四歩(33) (57.54%)"));
        QCOMPARE(parser.pvUsiStr(), QStringLiteral("7g7f 3c3d"));

        format(parser, QStringLiteral("info depth 2 pv 7g7f 7z7f 3c3d"));
        QCOMPARE(parser.pvKanjiStr(), QStringLiteral("▲７六歩(77)"));
        QCOMPARE(parser.pvUsiStr(), QStringLiteral("7g7f"));

        format(parser, QStringLiteral("info depth 2 pv 7g7f (57%) 3c3d"));
        QCOMPARE(parser.pvKanjiStr(), QStringLiteral("▲７六歩(77)"));
    }

    // 局面がない・読めないときは、読み筋を日本語にしない
    void formatterWithoutPositionLeavesPvEmpty()
    {
        PvNotationFormatter parser;
        format(parser, QStringLiteral("info depth 4 nodes 100 pv 7g7f"));
        QCOMPARE(parser.pvKanjiStr(), QString());
        QCOMPARE(parser.pvUsiStr(), QString());

        parser.setPosition(SfenUtils::hirateSfen());
        parser.setPosition(QStringLiteral("not a sfen"));
        format(parser, QStringLiteral("info depth 4 pv 7g7f"));
        QCOMPARE(parser.pvKanjiStr(), QString());
    }
};

QTEST_GUILESS_MAIN(TestPvNotationFormatter)
#include "tst_pvnotationformatter.moc"
