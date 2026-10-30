/// @file tst_usilineparser.cpp
/// @brief UsiLineParser（info・option・id・bestmove・checkmate 行の解析）のユニットテスト

#include <QtTest>
#include <QJsonArray>

#include "usilineparser.h"

class TestUsiLineParser : public QObject
{
    Q_OBJECT

private slots:
    void parsesAllFieldsOfNormalLine()
    {
        const auto info = UsiLineParser::parseInfo(QStringLiteral(
            "info depth 12 seldepth 20 time 1250 nodes 123456 nps 987654 hashfull 42 "
            "score cp -35 currmove 7g7f pv 7g7f 3c3d 2g2f"));
        QVERIFY(info.has_value());
        QCOMPARE(info->depth, 12);
        QCOMPARE(info->seldepth, 20);
        QCOMPARE(info->timeMs, 1250LL);
        QCOMPARE(info->nodes, 123456LL);
        QCOMPARE(info->nps, 987654LL);
        QCOMPARE(info->hashfull, 42);
        QCOMPARE(info->multipv, 1);
        QVERIFY(info->score.isCp());
        QCOMPARE(info->score.value, -35);
        QVERIFY(info->score.bound == UsiScore::Bound::None);
        QCOMPARE(info->currmove, QStringLiteral("7g7f"));
        QCOMPARE(info->pv, QStringList({QStringLiteral("7g7f"), QStringLiteral("3c3d"), QStringLiteral("2g2f")}));
        QVERIFY(info->hasPv());
        QVERIFY(info->string.isEmpty());
    }

    void unsetFieldsKeepDefaults()
    {
        const auto info = UsiLineParser::parseInfo(QStringLiteral("info nodes 100"));
        QVERIFY(info.has_value());
        QCOMPARE(info->nodes, 100LL);
        QCOMPARE(info->depth, -1);
        QCOMPARE(info->seldepth, -1);
        QCOMPARE(info->timeMs, -1LL);
        QCOMPARE(info->nps, -1LL);
        QCOMPARE(info->hashfull, -1);
        QCOMPARE(info->multipv, 1);
        QVERIFY(!info->hasScore());
        QVERIFY(!info->hasPv());

        // "info" だけの行も info として読む
        const auto empty = UsiLineParser::parseInfo(QStringLiteral("info "));
        QVERIFY(empty.has_value());
        QVERIFY(!empty->hasPv());
    }

    void parsesMultipv()
    {
        const auto first = UsiLineParser::parseInfo(QStringLiteral("info depth 10 multipv 1 score cp 100 pv 7g7f 3c3d"));
        QCOMPARE(first->multipv, 1);
        const auto second = UsiLineParser::parseInfo(QStringLiteral("info depth 10 multipv 2 score cp -50 pv 2g2f 8c8d"));
        QCOMPARE(second->multipv, 2);
        QCOMPARE(second->score.value, -50);
        QCOMPARE(second->pv.first(), QStringLiteral("2g2f"));
        const auto large = UsiLineParser::parseInfo(QStringLiteral("info depth 1 multipv 999999 score cp 0 pv 7g7f"));
        QCOMPARE(large->multipv, 999999);
    }

    void parsesMateScores()
    {
        const auto win = UsiLineParser::parseInfo(QStringLiteral("info depth 5 score mate 3 pv G*5b 4a4b 5b4b"));
        QVERIFY(win->score.isMate());
        QCOMPARE(win->score.value, 3);
        QVERIFY(win->score.mateCountKnown);
        QCOMPARE(win->score.mateText(), QStringLiteral("3"));
        QCOMPARE(win->score.mateSign(), 1);
        QCOMPARE(win->pv.size(), 3);

        const auto lose = UsiLineParser::parseInfo(QStringLiteral("info score mate -4 pv 5a4a"));
        QCOMPARE(lose->score.value, -4);
        QCOMPARE(lose->score.mateText(), QStringLiteral("-4"));
        QCOMPARE(lose->score.mateSign(), -1);

        // 手数を省く "mate +" / "mate -"
        const auto plus = UsiLineParser::parseInfo(QStringLiteral("info score mate + pv 2b3c"));
        QVERIFY(plus->score.isMate());
        QVERIFY(!plus->score.mateCountKnown);
        QCOMPARE(plus->score.value, 0);
        QCOMPARE(plus->score.mateText(), QStringLiteral("+"));
        QCOMPARE(plus->score.mateSign(), 1);
        QCOMPARE(plus->pv, QStringList({QStringLiteral("2b3c")}));

        const auto minus = UsiLineParser::parseInfo(QStringLiteral("info score mate - pv resign"));
        QCOMPARE(minus->score.mateText(), QStringLiteral("-"));
        QCOMPARE(minus->score.mateSign(), -1);
        QCOMPARE(minus->pv, QStringList({QStringLiteral("resign")}));
    }

    // 手番側から見た値を先手から見た値に直すのは UsiScore::blackView だけ（後手から見た値だけを反転する）
    void blackViewFlipsOnlyWhitesScore()
    {
        const UsiScore cp = UsiScore::cp(120, UsiScore::Bound::Lower);
        QCOMPARE(cp.blackView(Turn::Black), cp);
        const UsiScore flipped = cp.blackView(Turn::White);
        QCOMPARE(flipped.value, -120);
        QVERIFY(flipped.bound == UsiScore::Bound::Upper);

        const UsiScore mate = UsiScore::mate(3).blackView(Turn::White);
        QCOMPARE(mate.value, -3);
        QCOMPARE(mate.mateSign(), -1);

        // viewedBy は逆の直し（先手から見た値 → その側から見た値）
        QCOMPARE(flipped.viewedBy(Turn::White), cp);
        QCOMPARE(cp.viewedBy(Turn::Black), cp);
    }

    void parsesBounds()
    {
        const auto lower = UsiLineParser::parseInfo(QStringLiteral("info depth 8 score cp 120 lowerbound nodes 10 pv 2g2f"));
        QVERIFY(lower->score.bound == UsiScore::Bound::Lower);
        QCOMPARE(lower->score.value, 120);
        QCOMPARE(lower->nodes, 10LL);
        QCOMPARE(lower->pv.size(), 1);

        const auto upper = UsiLineParser::parseInfo(QStringLiteral("info score cp -5 upperbound pv 8c8d"));
        QVERIFY(upper->score.bound == UsiScore::Bound::Upper);
        QCOMPARE(upper->score.value, -5);

        const auto mateBound = UsiLineParser::parseInfo(QStringLiteral("info score mate 5 lowerbound"));
        QVERIFY(mateBound->score.isMate());
        QVERIFY(mateBound->score.bound == UsiScore::Bound::Lower);
    }

    void infoStringKeepsRestOfLine()
    {
        // string の後ろは行末まで本文。中に pv などの語があっても読み筋にしない
        const auto str = UsiLineParser::parseInfo(QStringLiteral("info string hello world pv 7g7f score cp 3"));
        QVERIFY(str.has_value());
        QCOMPARE(str->string, QStringLiteral("hello world pv 7g7f score cp 3"));
        QVERIFY(!str->hasPv());
        QVERIFY(!str->hasScore());

        const auto afterFields = UsiLineParser::parseInfo(QStringLiteral("info depth 3 string evaluation started"));
        QCOMPARE(afterFields->depth, 3);
        QCOMPARE(afterFields->string, QStringLiteral("evaluation started"));
    }

    void toleratesMalformedInput()
    {
        // info でない行
        QVERIFY(!UsiLineParser::parseInfo(QString()).has_value());
        QVERIFY(!UsiLineParser::parseInfo(QStringLiteral("bestmove 7g7f ponder 3c3d")).has_value());
        QVERIFY(!UsiLineParser::parseInfo(QStringLiteral("infox depth 3")).has_value());

        // 数でない値は読まずに飛ばし、後ろの項目は読む
        const auto badDepth = UsiLineParser::parseInfo(QStringLiteral("info depth abc nodes 7 pv 7g7f"));
        QCOMPARE(badDepth->depth, -1);
        QCOMPARE(badDepth->nodes, 7LL);
        QCOMPARE(badDepth->pv.size(), 1);

        // 値のない項目・読めない score
        const auto truncated = UsiLineParser::parseInfo(QStringLiteral("info depth"));
        QCOMPARE(truncated->depth, -1);
        const auto badScore = UsiLineParser::parseInfo(QStringLiteral("info score cp x depth 4"));
        QVERIFY(!badScore->hasScore());
        QCOMPARE(badScore->depth, 4);
        const auto shortScore = UsiLineParser::parseInfo(QStringLiteral("info score cp"));
        QVERIFY(!shortScore->hasScore());

        // pv の語がないときは読み筋なし
        const auto emptyPv = UsiLineParser::parseInfo(QStringLiteral("info depth 2 pv"));
        QVERIFY(!emptyPv->hasPv());

        // 改行・タブ・連続する空白
        const auto spaced = UsiLineParser::parseInfo(QStringLiteral("info  depth\t9   pv 7g7f  3c3d\r\n"));
        QCOMPARE(spaced->depth, 9);
        QCOMPARE(spaced->pv, QStringList({QStringLiteral("7g7f"), QStringLiteral("3c3d")}));

        // 知らない語（currmovenumber など）は飛ばす
        const auto unknown = UsiLineParser::parseInfo(QStringLiteral("info currmove 7g7f currmovenumber 1 depth 2"));
        QCOMPARE(unknown->currmove, QStringLiteral("7g7f"));
        QCOMPARE(unknown->depth, 2);
        QVERIFY(!unknown->hasPv());
    }

    void jsonOmitsUnsetFields()
    {
        const QJsonObject json = UsiLineParser::parseInfo(QStringLiteral("info depth 3 score cp 10 pv 7g7f"))->toJson();
        QCOMPARE(json.value(QStringLiteral("depth")).toInt(), 3);
        QCOMPARE(json.value(QStringLiteral("score_cp")).toInt(), 10);
        QCOMPARE(json.value(QStringLiteral("multipv")).toInt(), 1);
        QVERIFY(!json.contains(QStringLiteral("nodes")));
        QVERIFY(!json.contains(QStringLiteral("score_mate")));
        QVERIFY(!json.contains(QStringLiteral("bound")));
        QCOMPARE(json.value(QStringLiteral("pv")).toArray().size(), 1);

        const QJsonObject mate = UsiLineParser::parseInfo(QStringLiteral("info score mate + upperbound"))->toJson();
        QCOMPARE(mate.value(QStringLiteral("score_mate")).toInt(), 0);
        QCOMPARE(mate.value(QStringLiteral("bound")).toString(), QStringLiteral("upper"));
    }

    void parsesOptionDeclaration()
    {
        const auto ponder = UsiLineParser::parseOption(
            QStringLiteral("option name USI_Ponder type check default false"));
        QVERIFY(ponder.has_value());
        QCOMPARE(ponder->name, QStringLiteral("USI_Ponder"));
        QCOMPARE(ponder->type, QStringLiteral("check"));

        // 名前に空白が入るオプション
        const auto spaced = UsiLineParser::parseOption(
            QStringLiteral("option name Book File type string default book.db\n"));
        QCOMPARE(spaced->name, QStringLiteral("Book File"));
        QCOMPARE(spaced->type, QStringLiteral("string"));

        QVERIFY(!UsiLineParser::parseOption(QStringLiteral("option name NoType")).has_value());
        QVERIFY(!UsiLineParser::parseOption(QStringLiteral("option name  type spin")).has_value());
        QVERIFY(!UsiLineParser::parseOption(QStringLiteral("id name Engine")).has_value());
    }

    void parsesIdName()
    {
        QCOMPARE(UsiLineParser::parseIdName(QStringLiteral("id name Hayanagi 1.0\r\n")).value_or(QString()),
                 QStringLiteral("Hayanagi 1.0"));
        QVERIFY(!UsiLineParser::parseIdName(QStringLiteral("id author someone")).has_value());
        QVERIFY(!UsiLineParser::parseIdName(QStringLiteral("id name ")).has_value());
        QVERIFY(!UsiLineParser::parseIdName(QStringLiteral("info string id name x")).has_value());
    }
};

QTEST_GUILESS_MAIN(TestUsiLineParser)
#include "tst_usilineparser.moc"
