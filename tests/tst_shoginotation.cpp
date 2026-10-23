#include <QtTest>

#include "kifmovetext.h"
#include "shoginotation.h"

#include <limits>
#include "shogimove.h"

class TestShogiNotation : public QObject
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

    // === ShogiNotation::fileZenkakuChar / rankKanjiChar ===

    void zenkakuAndKanjiChars()
    {
        QCOMPARE(ShogiNotation::fileZenkakuChar(7), QChar(u'７'));
        QCOMPARE(ShogiNotation::rankKanjiChar(6), QChar(u'六'));
        QVERIFY(ShogiNotation::fileZenkakuChar(0).isNull());
        QVERIFY(ShogiNotation::rankKanjiChar(10).isNull());
    }

    // === KifMoveText ===

    void kifMoveText_format()
    {
        const auto move = [](const char* usi) { return *UsiMove::parse(QString::fromLatin1(usi)); };
        QCOMPARE(KifMoveText::format(Turn::Black, Piece::BlackPawn, move("7g7f")), QStringLiteral("▲７六歩(77)"));
        QCOMPARE(KifMoveText::format(Turn::Black, Piece::BlackBishop, move("8h2b+")), QStringLiteral("▲２二角成(88)"));
        QCOMPARE(KifMoveText::format(Turn::White, Piece::WhiteSilver, move("3a2b"), BoardSquare{2, 2}),
                 QStringLiteral("△同　銀(31)"));
        QCOMPARE(KifMoveText::format(Turn::Black, Piece::BlackBishop, move("B*5e")), QStringLiteral("▲５五角打"));
        QCOMPARE(KifMoveText::format(Turn::White, Piece::WhitePromotedLance, move("1a1b")),
                 QStringLiteral("△１二成香(11)"));
    }

    // 読み筋のように続けて足すときは「同」を詰めて書ける
    void kifMoveText_appendCompactSameSquare()
    {
        QString pv;
        KifMoveText::append(pv, Turn::Black, Piece::BlackBishop, *UsiMove::parse(u"8h2b+"), BoardSquare{},
                            KifMoveText::SameSquare::Compact);
        KifMoveText::append(pv, Turn::White, Piece::WhiteSilver, *UsiMove::parse(u"3a2b"), BoardSquare{2, 2},
                            KifMoveText::SameSquare::Compact);
        QCOMPARE(pv, QStringLiteral("▲２二角成(88)△同銀(31)"));
    }

    void usiMove_invalidIsEmptyText()
    {
        QCOMPARE(UsiMove::fromString(u"0a1a").toString(), QString());
        QCOMPARE(ShogiMove().move.toString(), QString());
    }

    void numberFromDigits_saturates()
    {
        // 桁の多い数（「変化：99999999999手」など）は int の最大値で止める（セキュリティ監査 S13）
        QCOMPARE(ShogiNotation::numberFromDigits(u"１２３"), 123);
        QCOMPARE(ShogiNotation::numberFromDigits(u"2147483647"), std::numeric_limits<int>::max());
        QCOMPARE(ShogiNotation::numberFromDigits(u"99999999999"), std::numeric_limits<int>::max());
        QCOMPARE(ShogiNotation::numberFromDigits(u"2147483648"), std::numeric_limits<int>::max());
    }
};

QTEST_MAIN(TestShogiNotation)
#include "tst_shoginotation.moc"
