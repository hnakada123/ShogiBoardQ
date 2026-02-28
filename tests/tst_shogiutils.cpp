#include <QtTest>

#include "shogiutils.h"
#include "shogimove.h"

class TestShogiUtils : public QObject
{
    Q_OBJECT

private slots:
    // === transRankTo ===

    void transRankTo_validRange()
    {
        QCOMPARE(ShogiUtils::transRankTo(1), QStringLiteral("一"));
        QCOMPARE(ShogiUtils::transRankTo(2), QStringLiteral("二"));
        QCOMPARE(ShogiUtils::transRankTo(3), QStringLiteral("三"));
        QCOMPARE(ShogiUtils::transRankTo(4), QStringLiteral("四"));
        QCOMPARE(ShogiUtils::transRankTo(5), QStringLiteral("五"));
        QCOMPARE(ShogiUtils::transRankTo(6), QStringLiteral("六"));
        QCOMPARE(ShogiUtils::transRankTo(7), QStringLiteral("七"));
        QCOMPARE(ShogiUtils::transRankTo(8), QStringLiteral("八"));
        QCOMPARE(ShogiUtils::transRankTo(9), QStringLiteral("九"));
    }

    void transRankTo_outOfRange()
    {
        QCOMPARE(ShogiUtils::transRankTo(0), QString());
        QCOMPARE(ShogiUtils::transRankTo(10), QString());
    }

    void transRankTo_sentinel()
    {
        // -1 はセンチネル値
        QCOMPARE(ShogiUtils::transRankTo(-1), QString());
    }

    // === transFileTo ===

    void transFileTo_validRange()
    {
        QCOMPARE(ShogiUtils::transFileTo(1), QStringLiteral("１"));
        QCOMPARE(ShogiUtils::transFileTo(5), QStringLiteral("５"));
        QCOMPARE(ShogiUtils::transFileTo(9), QStringLiteral("９"));
    }

    void transFileTo_outOfRange()
    {
        QCOMPARE(ShogiUtils::transFileTo(0), QString());
        QCOMPARE(ShogiUtils::transFileTo(10), QString());
    }

    void transFileTo_sentinel()
    {
        QCOMPARE(ShogiUtils::transFileTo(-1), QString());
    }

    // === parseFullwidthFile ===

    void parseFullwidthFile_valid()
    {
        QCOMPARE(ShogiUtils::parseFullwidthFile(QChar(0xFF11)), 1); // '１'
        QCOMPARE(ShogiUtils::parseFullwidthFile(QChar(0xFF15)), 5); // '５'
        QCOMPARE(ShogiUtils::parseFullwidthFile(QChar(0xFF19)), 9); // '９'
    }

    void parseFullwidthFile_invalid()
    {
        QCOMPARE(ShogiUtils::parseFullwidthFile(QChar('1')), 0);
        QCOMPARE(ShogiUtils::parseFullwidthFile(QChar('A')), 0);
        QCOMPARE(ShogiUtils::parseFullwidthFile(QChar(0xFF10)), 0); // '０' is out of range
    }

    // === parseKanjiRank ===

    void parseKanjiRank_valid()
    {
        QCOMPARE(ShogiUtils::parseKanjiRank(QChar(u'一')), 1);
        QCOMPARE(ShogiUtils::parseKanjiRank(QChar(u'五')), 5);
        QCOMPARE(ShogiUtils::parseKanjiRank(QChar(u'九')), 9);
    }

    void parseKanjiRank_invalid()
    {
        QCOMPARE(ShogiUtils::parseKanjiRank(QChar('1')), 0);
        QCOMPARE(ShogiUtils::parseKanjiRank(QChar(u'十')), 0);
    }

    // === parseMoveLabel ===

    void parseMoveLabel_sente()
    {
        auto result = ShogiUtils::parseMoveLabel(QStringLiteral("▲７六歩(77)"));
        QVERIFY(result.has_value());
        auto [file, rank] = *result;
        QCOMPARE(file, 7);
        QCOMPARE(rank, 6);
    }

    void parseMoveLabel_gote()
    {
        auto result = ShogiUtils::parseMoveLabel(QStringLiteral("△３四歩(33)"));
        QVERIFY(result.has_value());
        auto [file, rank] = *result;
        QCOMPARE(file, 3);
        QCOMPARE(rank, 4);
    }

    void parseMoveLabel_doNotation()
    {
        // 「同」で始まる場合は nullopt を返す
        auto result = ShogiUtils::parseMoveLabel(QStringLiteral("▲同　銀(31)"));
        QVERIFY(!result.has_value());
    }

    void parseMoveLabel_noMark()
    {
        // ▲も△もない場合
        auto result = ShogiUtils::parseMoveLabel(QStringLiteral("７六歩"));
        QVERIFY(!result.has_value());
    }

    // === moveToUsi ===

    void moveToUsi_invalidCoordinates()
    {
        // 範囲外の移動先座標
        ShogiMove m(QPoint(0, 0), QPoint(-1, 0), Piece::BlackPawn, Piece::None, false);
        QCOMPARE(ShogiUtils::moveToUsi(m), QString());
    }

    // === Game timer ===

    void gameEpoch_initialState()
    {
        // 開始前は0を返す（最初のテストで呼ばれた場合）
        // startGameEpoch後は正の値を返す
        ShogiUtils::startGameEpoch();
        qint64 elapsed = ShogiUtils::nowMs();
        QVERIFY(elapsed >= 0);
    }

    void gameEpoch_monotonic()
    {
        ShogiUtils::startGameEpoch();
        qint64 t1 = ShogiUtils::nowMs();
        qint64 t2 = ShogiUtils::nowMs();
        QVERIFY(t2 >= t1);
    }
};

QTEST_MAIN(TestShogiUtils)
#include "tst_shogiutils.moc"
