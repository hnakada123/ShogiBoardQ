/// @file tst_parsecommon.cpp
/// @brief KifuParseCommon ユーティリティのユニットテスト

#include <QTest>
#include "parsecommon.h"
#include "shogitypes.h"

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
    void formatTimeHMS_negative();

    // ---- formatTimeText ----
    void formatTimeText_basic();

    // ---- toGameInfoMap ----
    void toGameInfoMap_empty();
    void toGameInfoMap_basic();
    void toGameInfoMap_duplicateKey();

    // ---- digit parsing ----
    void asciiDigitToInt_valid();
    void asciiDigitToInt_invalid();
    void zenkakuDigitToInt_valid();
    void zenkakuDigitToInt_invalid();
    void flexDigitToIntNoDetach_ascii();
    void flexDigitToIntNoDetach_zenkaku();
    void flexDigitToIntNoDetach_invalid();
    void flexDigitsToIntNoDetach_multiDigit();
    void flexDigitsToIntNoDetach_empty();
    void flexDigitsToIntNoDetach_mixed();

    // ---- coordinate parsing ----
    void parseFileChar_valid();
    void parseFileChar_invalid();
    void parseRankChar_valid();
    void parseRankChar_invalid();
    void parseDigit_valid();
    void parseDigit_invalid();

    // ---- terminal words ----
    void terminalWords_count();
    void isTerminalWordExact_match();
    void isTerminalWordExact_noMatch();
    void isTerminalWordExact_withWhitespace();
    void isTerminalWordContains_match();
    void isTerminalWordContains_noMatch();

    // ---- mapKanjiPiece ----
    void mapKanjiPiece_pawn();
    void mapKanjiPiece_promotedPawn();
    void mapKanjiPiece_bishop();
    void mapKanjiPiece_promotedBishop();
    void mapKanjiPiece_rook();
    void mapKanjiPiece_promotedRook();
    void mapKanjiPiece_king();
    void mapKanjiPiece_unknown();

    // ---- helper functions ----
    void appendLine_empty();
    void appendLine_nonEmpty();
    void tebanMark_odd();
    void tebanMark_even();

    // ---- board/format detection ----
    void isBoardHeaderOrFrame_digitLine();
    void isBoardHeaderOrFrame_emptyLine();
    void isBoardHeaderOrFrame_normalLine();
    void isKifSkippableHeaderLine_empty();
    void isKifSkippableHeaderLine_comment();
    void isKifSkippableHeaderLine_header();
    void isKifSkippableHeaderLine_moveLine();
    void isBodHandsLine_sente();
    void isBodHandsLine_gote();
    void isBodHandsLine_normal();

    // ---- USI piece conversion ----
    void usiPieceToKanji_valid();
    void usiPieceToKanji_invalid();
    void usiTokenToKanji_normal();
    void usiTokenToKanji_promoted();
    void usiTokenToKanji_empty();
    void usiTokenToKanji_unknown();

    // ---- containsAnyTerminal ----
    void containsAnyTerminal_match();
    void containsAnyTerminal_noMatch();

    // ---- table-driven: mapKanjiPiece abnormal ----
    void mapKanjiPiece_abnormal_data();
    void mapKanjiPiece_abnormal();

    // ---- table-driven: flexDigitsToIntNoDetach abnormal ----
    void flexDigitsToIntNoDetach_abnormal_data();
    void flexDigitsToIntNoDetach_abnormal();

    // ---- table-driven: usiTokenToKanji abnormal ----
    void usiTokenToKanji_abnormal_data();
    void usiTokenToKanji_abnormal();

    // ---- table-driven: coordinate parsing boundary ----
    void parseCoordinate_boundary_data();
    void parseCoordinate_boundary();
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

// ==== digit parsing ====

void TestParseCommon::asciiDigitToInt_valid()
{
    QCOMPARE(KifuParseCommon::asciiDigitToInt(QChar('0')), 0);
    QCOMPARE(KifuParseCommon::asciiDigitToInt(QChar('5')), 5);
    QCOMPARE(KifuParseCommon::asciiDigitToInt(QChar('9')), 9);
}

void TestParseCommon::asciiDigitToInt_invalid()
{
    QCOMPARE(KifuParseCommon::asciiDigitToInt(QChar('a')), 0);
    QCOMPARE(KifuParseCommon::asciiDigitToInt(QChar('Z')), 0);
    QCOMPARE(KifuParseCommon::asciiDigitToInt(QChar(' ')), 0);
}

void TestParseCommon::zenkakuDigitToInt_valid()
{
    QCOMPARE(KifuParseCommon::zenkakuDigitToInt(QChar(u'０')), 0);
    QCOMPARE(KifuParseCommon::zenkakuDigitToInt(QChar(u'５')), 5);
    QCOMPARE(KifuParseCommon::zenkakuDigitToInt(QChar(u'９')), 9);
}

void TestParseCommon::zenkakuDigitToInt_invalid()
{
    QCOMPARE(KifuParseCommon::zenkakuDigitToInt(QChar('5')), 0);
    QCOMPARE(KifuParseCommon::zenkakuDigitToInt(QChar('A')), 0);
}

void TestParseCommon::flexDigitToIntNoDetach_ascii()
{
    QCOMPARE(KifuParseCommon::flexDigitToIntNoDetach(QChar('7')), 7);
}

void TestParseCommon::flexDigitToIntNoDetach_zenkaku()
{
    QCOMPARE(KifuParseCommon::flexDigitToIntNoDetach(QChar(u'７')), 7);
}

void TestParseCommon::flexDigitToIntNoDetach_invalid()
{
    QCOMPARE(KifuParseCommon::flexDigitToIntNoDetach(QChar('X')), 0);
}

void TestParseCommon::flexDigitsToIntNoDetach_multiDigit()
{
    QCOMPARE(KifuParseCommon::flexDigitsToIntNoDetach(QStringLiteral("123")), 123);
}

void TestParseCommon::flexDigitsToIntNoDetach_empty()
{
    QCOMPARE(KifuParseCommon::flexDigitsToIntNoDetach(QString()), 0);
}

void TestParseCommon::flexDigitsToIntNoDetach_mixed()
{
    // Mixed ASCII and zenkaku digits
    QCOMPARE(KifuParseCommon::flexDigitsToIntNoDetach(QStringLiteral("1２3")), 123);
}

// ==== coordinate parsing ====

void TestParseCommon::parseFileChar_valid()
{
    QCOMPARE(KifuParseCommon::parseFileChar(QChar('1')), std::optional<int>(1));
    QCOMPARE(KifuParseCommon::parseFileChar(QChar('9')), std::optional<int>(9));
    QCOMPARE(KifuParseCommon::parseFileChar(QChar('5')), std::optional<int>(5));
}

void TestParseCommon::parseFileChar_invalid()
{
    QCOMPARE(KifuParseCommon::parseFileChar(QChar('0')), std::nullopt);
    QCOMPARE(KifuParseCommon::parseFileChar(QChar('a')), std::nullopt);
    QCOMPARE(KifuParseCommon::parseFileChar(QChar(' ')), std::nullopt);
}

void TestParseCommon::parseRankChar_valid()
{
    QCOMPARE(KifuParseCommon::parseRankChar(QChar('a')), std::optional<int>(1));
    QCOMPARE(KifuParseCommon::parseRankChar(QChar('i')), std::optional<int>(9));
    QCOMPARE(KifuParseCommon::parseRankChar(QChar('e')), std::optional<int>(5));
}

void TestParseCommon::parseRankChar_invalid()
{
    QCOMPARE(KifuParseCommon::parseRankChar(QChar('j')), std::nullopt);
    QCOMPARE(KifuParseCommon::parseRankChar(QChar('1')), std::nullopt);
    QCOMPARE(KifuParseCommon::parseRankChar(QChar('A')), std::nullopt);
}

void TestParseCommon::parseDigit_valid()
{
    QCOMPARE(KifuParseCommon::parseDigit(QChar('0')), std::optional<int>(0));
    QCOMPARE(KifuParseCommon::parseDigit(QChar('9')), std::optional<int>(9));
}

void TestParseCommon::parseDigit_invalid()
{
    QCOMPARE(KifuParseCommon::parseDigit(QChar('a')), std::nullopt);
    QCOMPARE(KifuParseCommon::parseDigit(QChar(' ')), std::nullopt);
}

// ==== terminal words ====

void TestParseCommon::terminalWords_count()
{
    QCOMPARE(KifuParseCommon::terminalWords().size(), 16u);
}

void TestParseCommon::isTerminalWordExact_match()
{
    QString norm;
    QVERIFY(KifuParseCommon::isTerminalWordExact(QStringLiteral("投了"), &norm));
    QCOMPARE(norm, QStringLiteral("投了"));
}

void TestParseCommon::isTerminalWordExact_noMatch()
{
    QVERIFY(!KifuParseCommon::isTerminalWordExact(QStringLiteral("開始")));
}

void TestParseCommon::isTerminalWordExact_withWhitespace()
{
    // Trimmed before matching
    QString norm;
    QVERIFY(KifuParseCommon::isTerminalWordExact(QStringLiteral("  投了  "), &norm));
    QCOMPARE(norm, QStringLiteral("投了"));
}

void TestParseCommon::isTerminalWordContains_match()
{
    QString norm;
    QVERIFY(KifuParseCommon::isTerminalWordContains(QStringLiteral("まで82手で投了"), &norm));
    QCOMPARE(norm, QStringLiteral("投了"));
}

void TestParseCommon::isTerminalWordContains_noMatch()
{
    QVERIFY(!KifuParseCommon::isTerminalWordContains(QStringLiteral("普通のテキスト")));
}

// ==== mapKanjiPiece ====

void TestParseCommon::mapKanjiPiece_pawn()
{
    Piece base;
    bool promoted = false;
    QVERIFY(KifuParseCommon::mapKanjiPiece(QStringLiteral("歩"), base, promoted));
    QCOMPARE(base, Piece::BlackPawn);
    QVERIFY(!promoted);
}

void TestParseCommon::mapKanjiPiece_promotedPawn()
{
    Piece base;
    bool promoted = false;
    QVERIFY(KifuParseCommon::mapKanjiPiece(QStringLiteral("と"), base, promoted));
    QCOMPARE(base, Piece::BlackPawn);
    QVERIFY(promoted);
}

void TestParseCommon::mapKanjiPiece_bishop()
{
    Piece base;
    bool promoted = false;
    QVERIFY(KifuParseCommon::mapKanjiPiece(QStringLiteral("角"), base, promoted));
    QCOMPARE(base, Piece::BlackBishop);
    QVERIFY(!promoted);
}

void TestParseCommon::mapKanjiPiece_promotedBishop()
{
    Piece base;
    bool promoted = false;
    QVERIFY(KifuParseCommon::mapKanjiPiece(QStringLiteral("馬"), base, promoted));
    QCOMPARE(base, Piece::BlackBishop);
    QVERIFY(promoted);
}

void TestParseCommon::mapKanjiPiece_rook()
{
    Piece base;
    bool promoted = false;
    QVERIFY(KifuParseCommon::mapKanjiPiece(QStringLiteral("飛"), base, promoted));
    QCOMPARE(base, Piece::BlackRook);
    QVERIFY(!promoted);
}

void TestParseCommon::mapKanjiPiece_promotedRook()
{
    Piece base;
    bool promoted = false;
    QVERIFY(KifuParseCommon::mapKanjiPiece(QStringLiteral("龍"), base, promoted));
    QCOMPARE(base, Piece::BlackRook);
    QVERIFY(promoted);
}

void TestParseCommon::mapKanjiPiece_king()
{
    Piece base;
    bool promoted = false;
    QVERIFY(KifuParseCommon::mapKanjiPiece(QStringLiteral("玉"), base, promoted));
    QCOMPARE(base, Piece::BlackKing);
    QVERIFY(!promoted);
}

void TestParseCommon::mapKanjiPiece_unknown()
{
    Piece base;
    bool promoted = false;
    QVERIFY(!KifuParseCommon::mapKanjiPiece(QStringLiteral("X"), base, promoted));
}

// ==== helper functions ====

void TestParseCommon::appendLine_empty()
{
    QString buf;
    KifuParseCommon::appendLine(buf, QStringLiteral("first"));
    QCOMPARE(buf, QStringLiteral("first"));
}

void TestParseCommon::appendLine_nonEmpty()
{
    QString buf = QStringLiteral("line1");
    KifuParseCommon::appendLine(buf, QStringLiteral("line2"));
    QCOMPARE(buf, QStringLiteral("line1\nline2"));
}

void TestParseCommon::tebanMark_odd()
{
    QCOMPARE(KifuParseCommon::tebanMark(1), QStringLiteral("▲"));
    QCOMPARE(KifuParseCommon::tebanMark(3), QStringLiteral("▲"));
}

void TestParseCommon::tebanMark_even()
{
    QCOMPARE(KifuParseCommon::tebanMark(2), QStringLiteral("△"));
    QCOMPARE(KifuParseCommon::tebanMark(0), QStringLiteral("△"));
}

// ==== board/format detection ====

void TestParseCommon::isBoardHeaderOrFrame_digitLine()
{
    QVERIFY(KifuParseCommon::isBoardHeaderOrFrame(QStringLiteral("  ９ ８ ７ ６ ５ ４ ３ ２ １")));
}

void TestParseCommon::isBoardHeaderOrFrame_emptyLine()
{
    QVERIFY(!KifuParseCommon::isBoardHeaderOrFrame(QStringLiteral("")));
    QVERIFY(!KifuParseCommon::isBoardHeaderOrFrame(QStringLiteral("   ")));
}

void TestParseCommon::isBoardHeaderOrFrame_normalLine()
{
    QVERIFY(!KifuParseCommon::isBoardHeaderOrFrame(QStringLiteral("先手：田中")));
}

void TestParseCommon::isKifSkippableHeaderLine_empty()
{
    QVERIFY(KifuParseCommon::isKifSkippableHeaderLine(QString()));
}

void TestParseCommon::isKifSkippableHeaderLine_comment()
{
    QVERIFY(KifuParseCommon::isKifSkippableHeaderLine(QStringLiteral("#コメント")));
}

void TestParseCommon::isKifSkippableHeaderLine_header()
{
    QVERIFY(KifuParseCommon::isKifSkippableHeaderLine(QStringLiteral("開始日時：2025/01/01")));
    QVERIFY(KifuParseCommon::isKifSkippableHeaderLine(QStringLiteral("棋戦：王将戦")));
    QVERIFY(KifuParseCommon::isKifSkippableHeaderLine(QStringLiteral("手合割：平手")));
}

void TestParseCommon::isKifSkippableHeaderLine_moveLine()
{
    // Move lines should not be skippable
    QVERIFY(!KifuParseCommon::isKifSkippableHeaderLine(QStringLiteral("   1 ７六歩(77)")));
}

void TestParseCommon::isBodHandsLine_sente()
{
    QVERIFY(KifuParseCommon::isBodHandsLine(QStringLiteral("先手の持駒：歩二")));
    QVERIFY(KifuParseCommon::isBodHandsLine(QStringLiteral("先手の持ち駒：なし")));
}

void TestParseCommon::isBodHandsLine_gote()
{
    QVERIFY(KifuParseCommon::isBodHandsLine(QStringLiteral("後手の持駒：角")));
    QVERIFY(KifuParseCommon::isBodHandsLine(QStringLiteral("後手の持ち駒：なし")));
}

void TestParseCommon::isBodHandsLine_normal()
{
    QVERIFY(!KifuParseCommon::isBodHandsLine(QStringLiteral("先手：田中")));
    QVERIFY(!KifuParseCommon::isBodHandsLine(QStringLiteral("普通の行")));
}

// ==== USI piece conversion ====

void TestParseCommon::usiPieceToKanji_valid()
{
    QCOMPARE(KifuParseCommon::usiPieceToKanji(QChar('P')), QStringLiteral("歩"));
    QCOMPARE(KifuParseCommon::usiPieceToKanji(QChar('L')), QStringLiteral("香"));
    QCOMPARE(KifuParseCommon::usiPieceToKanji(QChar('N')), QStringLiteral("桂"));
    QCOMPARE(KifuParseCommon::usiPieceToKanji(QChar('S')), QStringLiteral("銀"));
    QCOMPARE(KifuParseCommon::usiPieceToKanji(QChar('G')), QStringLiteral("金"));
    QCOMPARE(KifuParseCommon::usiPieceToKanji(QChar('B')), QStringLiteral("角"));
    QCOMPARE(KifuParseCommon::usiPieceToKanji(QChar('R')), QStringLiteral("飛"));
    QCOMPARE(KifuParseCommon::usiPieceToKanji(QChar('K')), QStringLiteral("玉"));
}

void TestParseCommon::usiPieceToKanji_invalid()
{
    QCOMPARE(KifuParseCommon::usiPieceToKanji(QChar('X')), QStringLiteral("?"));
    QCOMPARE(KifuParseCommon::usiPieceToKanji(QChar('Z')), QStringLiteral("?"));
}

void TestParseCommon::usiTokenToKanji_normal()
{
    QCOMPARE(KifuParseCommon::usiTokenToKanji(QStringLiteral("P")), QStringLiteral("歩"));
    QCOMPARE(KifuParseCommon::usiTokenToKanji(QStringLiteral("B")), QStringLiteral("角"));
}

void TestParseCommon::usiTokenToKanji_promoted()
{
    QCOMPARE(KifuParseCommon::usiTokenToKanji(QStringLiteral("+P")), QStringLiteral("と"));
    QCOMPARE(KifuParseCommon::usiTokenToKanji(QStringLiteral("+B")), QStringLiteral("馬"));
    QCOMPARE(KifuParseCommon::usiTokenToKanji(QStringLiteral("+R")), QStringLiteral("龍"));
}

void TestParseCommon::usiTokenToKanji_empty()
{
    QCOMPARE(KifuParseCommon::usiTokenToKanji(QString()), QStringLiteral("?"));
}

void TestParseCommon::usiTokenToKanji_unknown()
{
    QCOMPARE(KifuParseCommon::usiTokenToKanji(QStringLiteral("X")), QStringLiteral("?"));
}

// ==== containsAnyTerminal ====

void TestParseCommon::containsAnyTerminal_match()
{
    QString matched;
    QVERIFY(KifuParseCommon::containsAnyTerminal(QStringLiteral("まで82手で投了"), &matched));
    QCOMPARE(matched, QStringLiteral("投了"));
}

void TestParseCommon::containsAnyTerminal_noMatch()
{
    QVERIFY(!KifuParseCommon::containsAnyTerminal(QStringLiteral("普通のテキスト")));
}

// ==== formatTimeHMS negative ====

void TestParseCommon::formatTimeHMS_negative()
{
    // Negative values are clamped to 0
    QCOMPARE(KifuParseCommon::formatTimeHMS(-5000), QStringLiteral("00:00:00"));
}

// ==== table-driven: mapKanjiPiece abnormal ====

void TestParseCommon::mapKanjiPiece_abnormal_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<bool>("expectedResult");

    QTest::newRow("empty_string") << QString() << false;
    QTest::newRow("ascii_letter_P") << QStringLiteral("P") << false;
    QTest::newRow("number_digit") << QStringLiteral("1") << false;
    QTest::newRow("multi_char_invalid") << QStringLiteral("XX") << false;
}

void TestParseCommon::mapKanjiPiece_abnormal()
{
    QFETCH(QString, input);
    QFETCH(bool, expectedResult);

    Piece base;
    bool promoted = false;
    QCOMPARE(KifuParseCommon::mapKanjiPiece(input, base, promoted), expectedResult);
}

// ==== table-driven: flexDigitsToIntNoDetach abnormal ====

void TestParseCommon::flexDigitsToIntNoDetach_abnormal_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<int>("expectedValue");

    QTest::newRow("all_spaces") << QStringLiteral("   ") << 0;
    QTest::newRow("negative_sign") << QStringLiteral("-123") << 123;
    QTest::newRow("special_chars") << QStringLiteral("!@#") << 0;
    QTest::newRow("leading_zeros") << QStringLiteral("007") << 7;
}

void TestParseCommon::flexDigitsToIntNoDetach_abnormal()
{
    QFETCH(QString, input);
    QFETCH(int, expectedValue);

    QCOMPARE(KifuParseCommon::flexDigitsToIntNoDetach(input), expectedValue);
}

// ==== table-driven: usiTokenToKanji abnormal ====

void TestParseCommon::usiTokenToKanji_abnormal_data()
{
    QTest::addColumn<QString>("token");
    QTest::addColumn<QString>("expectedKanji");

    QTest::newRow("plus_only") << QStringLiteral("+") << QStringLiteral("?");
    QTest::newRow("lowercase_p") << QStringLiteral("p") << QStringLiteral("歩");
    QTest::newRow("plus_invalid") << QStringLiteral("+X") << QStringLiteral("?");
    QTest::newRow("number") << QStringLiteral("1") << QStringLiteral("?");
}

void TestParseCommon::usiTokenToKanji_abnormal()
{
    QFETCH(QString, token);
    QFETCH(QString, expectedKanji);

    QCOMPARE(KifuParseCommon::usiTokenToKanji(token), expectedKanji);
}

// ==== table-driven: coordinate parsing boundary ====

void TestParseCommon::parseCoordinate_boundary_data()
{
    QTest::addColumn<QChar>("fileChar");
    QTest::addColumn<bool>("fileValid");
    QTest::addColumn<QChar>("rankChar");
    QTest::addColumn<bool>("rankValid");

    QTest::newRow("file_at_boundary_low")
        << QChar('0') << false << QChar('a') << true;
    QTest::newRow("file_at_boundary_high")
        << QChar(':') << false << QChar('i') << true;
    QTest::newRow("rank_at_boundary_low")
        << QChar('1') << true << QChar('`') << false;
    QTest::newRow("rank_at_boundary_high")
        << QChar('9') << true << QChar('j') << false;
}

void TestParseCommon::parseCoordinate_boundary()
{
    QFETCH(QChar, fileChar);
    QFETCH(bool, fileValid);
    QFETCH(QChar, rankChar);
    QFETCH(bool, rankValid);

    auto fileResult = KifuParseCommon::parseFileChar(fileChar);
    QCOMPARE(fileResult.has_value(), fileValid);

    auto rankResult = KifuParseCommon::parseRankChar(rankChar);
    QCOMPARE(rankResult.has_value(), rankValid);
}

QTEST_MAIN(TestParseCommon)
#include "tst_parsecommon.moc"
