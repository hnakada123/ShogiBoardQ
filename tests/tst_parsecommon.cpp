/// @file tst_parsecommon.cpp
/// @brief KifuParseCommon ユーティリティのユニットテスト

#include <QTest>
#include "parsecommon.h"

class TestParseCommon : public QObject
{
    Q_OBJECT

private slots:
    // ---- isKifCommentLine ----
    void isKifCommentLine_ascii();
    void isKifCommentLine_fullwidth();
    void isKifCommentLine_empty();
    void isKifCommentLine_normalLine();

    // ---- isBookmarkLine ----
    void isBookmarkLine_valid();
    void isBookmarkLine_empty();
    void isBookmarkLine_normalLine();

    // ---- csaSpecialToJapanese ----
    void csaSpecial_toryo();
    void csaSpecial_chudan();
    void csaSpecial_sennichite();
    void csaSpecial_oute_sennichite();
    void csaSpecial_jishogi();
    void csaSpecial_timeUp();
    void csaSpecial_illegalMove();
    void csaSpecial_illegalAction();
    void csaSpecial_plusIllegalAction();
    void csaSpecial_kachi();
    void csaSpecial_hikiwake();
    void csaSpecial_maxMoves();
    void csaSpecial_tsumi();
    void csaSpecial_fuzumi();
    void csaSpecial_error();
    void csaSpecial_withPercentPrefix();
    void csaSpecial_unknown();

    // ---- formatTimeMS ----
    void formatTimeMS_zero();
    void formatTimeMS_basic();
    void formatTimeMS_minutes();
    void formatTimeMS_negative();

    // ---- formatTimeHMS ----
    void formatTimeHMS_zero();
    void formatTimeHMS_basic();
    void formatTimeHMS_hours();

    // ---- formatTimeText ----
    void formatTimeText_basic();

    // ---- toGameInfoMap ----
    void toGameInfoMap_empty();
    void toGameInfoMap_basic();
    void toGameInfoMap_duplicateKey();
};

// ==== isKifCommentLine ====

void TestParseCommon::isKifCommentLine_ascii()
{
    QVERIFY(KifuParseCommon::isKifCommentLine("*コメント"));
}

void TestParseCommon::isKifCommentLine_fullwidth()
{
    QVERIFY(KifuParseCommon::isKifCommentLine(QString::fromUtf8("＊コメント")));
}

void TestParseCommon::isKifCommentLine_empty()
{
    QVERIFY(!KifuParseCommon::isKifCommentLine(""));
}

void TestParseCommon::isKifCommentLine_normalLine()
{
    QVERIFY(!KifuParseCommon::isKifCommentLine("1 ７六歩(77)"));
}

// ==== isBookmarkLine ====

void TestParseCommon::isBookmarkLine_valid()
{
    QVERIFY(KifuParseCommon::isBookmarkLine("&しおり名"));
}

void TestParseCommon::isBookmarkLine_empty()
{
    QVERIFY(!KifuParseCommon::isBookmarkLine(""));
}

void TestParseCommon::isBookmarkLine_normalLine()
{
    QVERIFY(!KifuParseCommon::isBookmarkLine("1 ７六歩(77)"));
}

// ==== csaSpecialToJapanese ====

void TestParseCommon::csaSpecial_toryo()
{
    QCOMPARE(KifuParseCommon::csaSpecialToJapanese("TORYO"), QString::fromUtf8("投了"));
}

void TestParseCommon::csaSpecial_chudan()
{
    QCOMPARE(KifuParseCommon::csaSpecialToJapanese("CHUDAN"), QString::fromUtf8("中断"));
}

void TestParseCommon::csaSpecial_sennichite()
{
    QCOMPARE(KifuParseCommon::csaSpecialToJapanese("SENNICHITE"), QString::fromUtf8("千日手"));
}

void TestParseCommon::csaSpecial_oute_sennichite()
{
    QCOMPARE(KifuParseCommon::csaSpecialToJapanese("OUTE_SENNICHITE"), QString::fromUtf8("王手千日手"));
}

void TestParseCommon::csaSpecial_jishogi()
{
    QCOMPARE(KifuParseCommon::csaSpecialToJapanese("JISHOGI"), QString::fromUtf8("持将棋"));
}

void TestParseCommon::csaSpecial_timeUp()
{
    QCOMPARE(KifuParseCommon::csaSpecialToJapanese("TIME_UP"), QString::fromUtf8("切れ負け"));
}

void TestParseCommon::csaSpecial_illegalMove()
{
    QCOMPARE(KifuParseCommon::csaSpecialToJapanese("ILLEGAL_MOVE"), QString::fromUtf8("反則負け"));
}

void TestParseCommon::csaSpecial_illegalAction()
{
    QCOMPARE(KifuParseCommon::csaSpecialToJapanese("ILLEGAL_ACTION"), QString::fromUtf8("反則負け"));
}

void TestParseCommon::csaSpecial_plusIllegalAction()
{
    QCOMPARE(KifuParseCommon::csaSpecialToJapanese("+ILLEGAL_ACTION"), QString::fromUtf8("反則負け"));
}

void TestParseCommon::csaSpecial_kachi()
{
    QCOMPARE(KifuParseCommon::csaSpecialToJapanese("KACHI"), QString::fromUtf8("入玉勝ち"));
}

void TestParseCommon::csaSpecial_hikiwake()
{
    QCOMPARE(KifuParseCommon::csaSpecialToJapanese("HIKIWAKE"), QString::fromUtf8("引き分け"));
}

void TestParseCommon::csaSpecial_maxMoves()
{
    QCOMPARE(KifuParseCommon::csaSpecialToJapanese("MAX_MOVES"), QString::fromUtf8("最大手数到達"));
}

void TestParseCommon::csaSpecial_tsumi()
{
    QCOMPARE(KifuParseCommon::csaSpecialToJapanese("TSUMI"), QString::fromUtf8("詰み"));
}

void TestParseCommon::csaSpecial_fuzumi()
{
    QCOMPARE(KifuParseCommon::csaSpecialToJapanese("FUZUMI"), QString::fromUtf8("不詰"));
}

void TestParseCommon::csaSpecial_error()
{
    QCOMPARE(KifuParseCommon::csaSpecialToJapanese("ERROR"), QString::fromUtf8("エラー"));
}

void TestParseCommon::csaSpecial_withPercentPrefix()
{
    QCOMPARE(KifuParseCommon::csaSpecialToJapanese("%TORYO"), QString::fromUtf8("投了"));
    QCOMPARE(KifuParseCommon::csaSpecialToJapanese("%SENNICHITE"), QString::fromUtf8("千日手"));
    QCOMPARE(KifuParseCommon::csaSpecialToJapanese("%+ILLEGAL_ACTION"), QString::fromUtf8("反則負け"));
    QCOMPARE(KifuParseCommon::csaSpecialToJapanese("%-ILLEGAL_ACTION"), QString::fromUtf8("反則負け"));
}

void TestParseCommon::csaSpecial_unknown()
{
    // 不明なコードはそのまま返される
    QCOMPARE(KifuParseCommon::csaSpecialToJapanese("UNKNOWN"), QStringLiteral("UNKNOWN"));
    QCOMPARE(KifuParseCommon::csaSpecialToJapanese("%UNKNOWN"), QStringLiteral("%UNKNOWN"));
}

// ==== formatTimeMS ====

void TestParseCommon::formatTimeMS_zero()
{
    QCOMPARE(KifuParseCommon::formatTimeMS(0), QStringLiteral("00:00"));
}

void TestParseCommon::formatTimeMS_basic()
{
    // 30秒 = 30000ms
    QCOMPARE(KifuParseCommon::formatTimeMS(30000), QStringLiteral("00:30"));
}

void TestParseCommon::formatTimeMS_minutes()
{
    // 5分30秒 = 330000ms
    QCOMPARE(KifuParseCommon::formatTimeMS(330000), QStringLiteral("05:30"));
}

void TestParseCommon::formatTimeMS_negative()
{
    // 負値は0として扱う
    QCOMPARE(KifuParseCommon::formatTimeMS(-1000), QStringLiteral("00:00"));
}

// ==== formatTimeHMS ====

void TestParseCommon::formatTimeHMS_zero()
{
    QCOMPARE(KifuParseCommon::formatTimeHMS(0), QStringLiteral("00:00:00"));
}

void TestParseCommon::formatTimeHMS_basic()
{
    // 1分30秒 = 90000ms
    QCOMPARE(KifuParseCommon::formatTimeHMS(90000), QStringLiteral("00:01:30"));
}

void TestParseCommon::formatTimeHMS_hours()
{
    // 2時間30分15秒 = 9015000ms
    QCOMPARE(KifuParseCommon::formatTimeHMS(9015000), QStringLiteral("02:30:15"));
}

// ==== formatTimeText ====

void TestParseCommon::formatTimeText_basic()
{
    // 30秒手, 累計5分30秒
    QCOMPARE(KifuParseCommon::formatTimeText(30000, 330000),
             QStringLiteral("00:30/00:05:30"));
}

// ==== toGameInfoMap ====

void TestParseCommon::toGameInfoMap_empty()
{
    QList<KifGameInfoItem> items;
    auto m = KifuParseCommon::toGameInfoMap(items);
    QVERIFY(m.isEmpty());
}

void TestParseCommon::toGameInfoMap_basic()
{
    QList<KifGameInfoItem> items;
    items.append({QStringLiteral("先手"), QStringLiteral("田中")});
    items.append({QStringLiteral("後手"), QStringLiteral("佐藤")});
    auto m = KifuParseCommon::toGameInfoMap(items);
    QCOMPARE(m.size(), 2);
    QCOMPARE(m.value(QStringLiteral("先手")), QStringLiteral("田中"));
    QCOMPARE(m.value(QStringLiteral("後手")), QStringLiteral("佐藤"));
}

void TestParseCommon::toGameInfoMap_duplicateKey()
{
    // 重複キーは後勝ち
    QList<KifGameInfoItem> items;
    items.append({QStringLiteral("備考"), QStringLiteral("一行目")});
    items.append({QStringLiteral("備考"), QStringLiteral("二行目")});
    auto m = KifuParseCommon::toGameInfoMap(items);
    QCOMPARE(m.size(), 1);
    QCOMPARE(m.value(QStringLiteral("備考")), QStringLiteral("二行目"));
}

QTEST_MAIN(TestParseCommon)
#include "tst_parsecommon.moc"
