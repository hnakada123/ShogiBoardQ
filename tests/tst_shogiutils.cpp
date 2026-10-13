#include <QtTest>

#include "shoginotation.h"
#include "shogiutils.h"
#include "shogimove.h"

class TestShogiUtils : public QObject
{
    Q_OBJECT

private slots:
    // === ShogiNotation::rankKanji ===

    void rankKanji_validRange()
    {
        QCOMPARE(ShogiNotation::rankKanji(1), QStringLiteral("一"));
        QCOMPARE(ShogiNotation::rankKanji(2), QStringLiteral("二"));
        QCOMPARE(ShogiNotation::rankKanji(3), QStringLiteral("三"));
        QCOMPARE(ShogiNotation::rankKanji(4), QStringLiteral("四"));
        QCOMPARE(ShogiNotation::rankKanji(5), QStringLiteral("五"));
        QCOMPARE(ShogiNotation::rankKanji(6), QStringLiteral("六"));
        QCOMPARE(ShogiNotation::rankKanji(7), QStringLiteral("七"));
        QCOMPARE(ShogiNotation::rankKanji(8), QStringLiteral("八"));
        QCOMPARE(ShogiNotation::rankKanji(9), QStringLiteral("九"));
    }

    void rankKanji_outOfRange()
    {
        QCOMPARE(ShogiNotation::rankKanji(0), QString());
        QCOMPARE(ShogiNotation::rankKanji(10), QString());
    }

    void rankKanji_sentinel()
    {
        // -1 はセンチネル値
        QCOMPARE(ShogiNotation::rankKanji(-1), QString());
    }

    // === ShogiNotation::fileZenkaku ===

    void fileZenkaku_validRange()
    {
        QCOMPARE(ShogiNotation::fileZenkaku(1), QStringLiteral("１"));
        QCOMPARE(ShogiNotation::fileZenkaku(5), QStringLiteral("５"));
        QCOMPARE(ShogiNotation::fileZenkaku(9), QStringLiteral("９"));
    }

    void fileZenkaku_outOfRange()
    {
        QCOMPARE(ShogiNotation::fileZenkaku(0), QString());
        QCOMPARE(ShogiNotation::fileZenkaku(10), QString());
    }

    void fileZenkaku_sentinel()
    {
        QCOMPARE(ShogiNotation::fileZenkaku(-1), QString());
    }

    // === ShogiNotation::fileFromZenkaku ===

    void fileFromZenkaku_valid()
    {
        QCOMPARE(ShogiNotation::fileFromZenkaku(QChar(0xFF11)), 1); // '１'
        QCOMPARE(ShogiNotation::fileFromZenkaku(QChar(0xFF15)), 5); // '５'
        QCOMPARE(ShogiNotation::fileFromZenkaku(QChar(0xFF19)), 9); // '９'
    }

    void fileFromZenkaku_invalid()
    {
        QCOMPARE(ShogiNotation::fileFromZenkaku(QChar('1')), 0);
        QCOMPARE(ShogiNotation::fileFromZenkaku(QChar('A')), 0);
        QCOMPARE(ShogiNotation::fileFromZenkaku(QChar(0xFF10)), 0); // '０' is out of range
    }

    // === ShogiNotation::rankFromKanji ===

    void rankFromKanji_valid()
    {
        QCOMPARE(ShogiNotation::rankFromKanji(QChar(u'一')), 1);
        QCOMPARE(ShogiNotation::rankFromKanji(QChar(u'五')), 5);
        QCOMPARE(ShogiNotation::rankFromKanji(QChar(u'九')), 9);
    }

    void rankFromKanji_invalid()
    {
        QCOMPARE(ShogiNotation::rankFromKanji(QChar('1')), 0);
        QCOMPARE(ShogiNotation::rankFromKanji(QChar(u'十')), 0);
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

    // === ShogiMove::toUsiMove ===

    void toUsiMove_invalidCoordinates()
    {
        // 範囲外の移動先座標
        ShogiMove m(QPoint(0, 0), QPoint(-1, 0), Piece::BlackPawn, Piece::None, false);
        QCOMPARE(m.toUsiMove().toString(), QString());
    }
};

QTEST_MAIN(TestShogiUtils)
#include "tst_shogiutils.moc"
