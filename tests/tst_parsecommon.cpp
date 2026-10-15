/// @file tst_parsecommon.cpp
/// @brief KifuParseCommon ユーティリティのユニットテスト

#include <QTest>
#include <QStringView>
#include "usimove.h"
#include "parsecommon.h"
#include "terminaltype.h"
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

    // ---- foulTerminalMove ----

    // ---- formatTimeMS ----
    void csaSpecialToTerminal_data();
    void csaSpecialToTerminal();
    void csaSpecial_illegalActionDependsOnSide();
    void terminalWords_readAliases();
    void terminalWords_findInText();
    void terminalWords_labelRoundTrip();
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
    void parseDigit_valid();
    void parseDigit_invalid();

    // ---- terminal words ----

    // ---- mapKanjiPiece ----
    void mapKanjiPiece_pawn();
    void mapKanjiPiece_promotedPawn();
    void mapKanjiPiece_bishop();
    void mapKanjiPiece_promotedBishop();
    void mapKanjiPiece_rook();
    void mapKanjiPiece_promotedRook();
    void mapKanjiPiece_king();
    void mapKanjiPiece_unknown();
    void mapKanjiPiece_minorPieces_data();
    void mapKanjiPiece_minorPieces();

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

    // ---- containsAnyTerminal ----

    // ---- table-driven: mapKanjiPiece abnormal ----
    void mapKanjiPiece_abnormal_data();
    void mapKanjiPiece_abnormal();

    // ---- table-driven: flexDigitsToIntNoDetach abnormal ----
    void flexDigitsToIntNoDetach_abnormal_data();
    void flexDigitsToIntNoDetach_abnormal();


    // ---- table-driven: coordinate parsing boundary ----
    void parseCoordinate_boundary_data();
    void parseCoordinate_boundary();
};

// ==== isKifCommentLine ====

void TestParseCommon::isKifCommentLine_ascii()
{
    QVERIFY(KifuParseCommon::isKifCommentLine(QStringLiteral("*コメント")));
}

void TestParseCommon::isKifCommentLine_fullwidth()
{
    QVERIFY(KifuParseCommon::isKifCommentLine(QStringLiteral("＊コメント")));
}

void TestParseCommon::isKifCommentLine_empty()
{
    QVERIFY(!KifuParseCommon::isKifCommentLine(QStringView()));
}

void TestParseCommon::isKifCommentLine_normalLine()
{
    QVERIFY(!KifuParseCommon::isKifCommentLine(QStringLiteral("1 ７六歩(77)")));
}

// ==== isBookmarkLine ====

void TestParseCommon::isBookmarkLine_valid()
{
    QVERIFY(KifuParseCommon::isBookmarkLine(QStringLiteral("&しおり名")));
}

void TestParseCommon::isBookmarkLine_empty()
{
    QVERIFY(!KifuParseCommon::isBookmarkLine(QStringView()));
}

void TestParseCommon::isBookmarkLine_normalLine()
{
    QVERIFY(!KifuParseCommon::isBookmarkLine(QStringLiteral("1 ７六歩(77)")));
}

// ==== csaSpecialToTerminal ====




















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








// ==== mapKanjiPiece ====

void TestParseCommon::mapKanjiPiece_pawn()
{
    const auto result = KifuParseCommon::mapKanjiPiece(QStringLiteral("歩"));
    QVERIFY(result.has_value());
    QCOMPARE(result->base, Piece::BlackPawn);
    QVERIFY(!result->promoted);
}

void TestParseCommon::mapKanjiPiece_promotedPawn()
{
    const auto result = KifuParseCommon::mapKanjiPiece(QStringLiteral("と"));
    QVERIFY(result.has_value());
    QCOMPARE(result->base, Piece::BlackPawn);
    QVERIFY(result->promoted);
}

void TestParseCommon::mapKanjiPiece_bishop()
{
    const auto result = KifuParseCommon::mapKanjiPiece(QStringLiteral("角"));
    QVERIFY(result.has_value());
    QCOMPARE(result->base, Piece::BlackBishop);
    QVERIFY(!result->promoted);
}

void TestParseCommon::mapKanjiPiece_promotedBishop()
{
    const auto result = KifuParseCommon::mapKanjiPiece(QStringLiteral("馬"));
    QVERIFY(result.has_value());
    QCOMPARE(result->base, Piece::BlackBishop);
    QVERIFY(result->promoted);
}

void TestParseCommon::mapKanjiPiece_rook()
{
    const auto result = KifuParseCommon::mapKanjiPiece(QStringLiteral("飛"));
    QVERIFY(result.has_value());
    QCOMPARE(result->base, Piece::BlackRook);
    QVERIFY(!result->promoted);
}

void TestParseCommon::mapKanjiPiece_promotedRook()
{
    const auto result = KifuParseCommon::mapKanjiPiece(QStringLiteral("龍"));
    QVERIFY(result.has_value());
    QCOMPARE(result->base, Piece::BlackRook);
    QVERIFY(result->promoted);
}

void TestParseCommon::mapKanjiPiece_king()
{
    const auto result = KifuParseCommon::mapKanjiPiece(QStringLiteral("玉"));
    QVERIFY(result.has_value());
    QCOMPARE(result->base, Piece::BlackKing);
    QVERIFY(!result->promoted);
}

void TestParseCommon::mapKanjiPiece_unknown()
{
    QVERIFY(!KifuParseCommon::mapKanjiPiece(QStringLiteral("X")).has_value());
}

void TestParseCommon::mapKanjiPiece_minorPieces_data()
{
    QTest::addColumn<QString>("text");
    QTest::addColumn<Piece>("base");
    QTest::addColumn<bool>("promoted");
    const QStringList names = {QStringLiteral("銀"), QStringLiteral("桂"), QStringLiteral("香")};
    const QStringList aliases = {QStringLiteral("全"), QStringLiteral("圭"), QStringLiteral("杏")};
    const Piece pieces[] = {Piece::BlackSilver, Piece::BlackKnight, Piece::BlackLance};
    for (int i = 0; i < 3; ++i) {
        const QString& name = names[i];
        QTest::newRow(qPrintable(name)) << name << pieces[i] << false;
        QTest::newRow(qPrintable(name + QStringLiteral("成")))
            << name + QStringLiteral("成") << pieces[i] << false;
        QTest::newRow(qPrintable(name + QStringLiteral("不成")))
            << name + QStringLiteral("不成") << pieces[i] << false;
        QTest::newRow(qPrintable(QStringLiteral("成") + name))
            << QStringLiteral("成") + name << pieces[i] << true;
        QTest::newRow(qPrintable(aliases[i])) << aliases[i] << pieces[i] << true;
    }
}

void TestParseCommon::mapKanjiPiece_minorPieces()
{
    QFETCH(QString, text);
    QFETCH(Piece, base);
    QFETCH(bool, promoted);
    const auto result = KifuParseCommon::mapKanjiPiece(text);
    QVERIFY(result.has_value());
    QCOMPARE(result->base, base);
    QCOMPARE(result->promoted, promoted);
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
    // 駒落ちの棋譜は先手側を下手、後手側を上手と書く
    QVERIFY(KifuParseCommon::isBodHandsLine(QStringLiteral("上手の持駒：なし")));
    QVERIFY(KifuParseCommon::isBodHandsLine(QStringLiteral("下手の持ち駒：歩")));
    QVERIFY(KifuParseCommon::isKifSkippableHeaderLine(QStringLiteral("上手番")));
    QVERIFY(!KifuParseCommon::isBodHandsLine(QStringLiteral("上手：田中")));
}

void TestParseCommon::isBodHandsLine_normal()
{
    QVERIFY(!KifuParseCommon::isBodHandsLine(QStringLiteral("先手：田中")));
    QVERIFY(!KifuParseCommon::isBodHandsLine(QStringLiteral("普通の行")));
}

// ==== USI piece conversion ====




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

    QCOMPARE(KifuParseCommon::mapKanjiPiece(input).has_value(), expectedResult);
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

    // USI のマスは筋・段の両方が読めるときだけ読める
    const auto square = UsiNotation::parseSquare(QString(fileChar) + rankChar);
    QCOMPARE(square.has_value(), fileValid && rankValid);
}


void TestParseCommon::csaSpecialToTerminal_data()
{
    QTest::addColumn<QString>("code");
    QTest::addColumn<int>("type");
    QTest::newRow("toryo") << QStringLiteral("TORYO") << int(TerminalType::Resign);
    QTest::newRow("chudan") << QStringLiteral("CHUDAN") << int(TerminalType::Interrupt);
    QTest::newRow("sennichite") << QStringLiteral("SENNICHITE") << int(TerminalType::Repetition);
    QTest::newRow("oute") << QStringLiteral("OUTE_SENNICHITE") << int(TerminalType::PerpetualCheck);
    QTest::newRow("jishogi") << QStringLiteral("JISHOGI") << int(TerminalType::Impasse);
    QTest::newRow("timeup") << QStringLiteral("TIME_UP") << int(TerminalType::Timeout);
    QTest::newRow("illegal-move") << QStringLiteral("ILLEGAL_MOVE") << int(TerminalType::IllegalLoss);
    QTest::newRow("illegal-action") << QStringLiteral("ILLEGAL_ACTION") << int(TerminalType::IllegalLoss);
    QTest::newRow("kachi") << QStringLiteral("KACHI") << int(TerminalType::DeclarationWin);
    QTest::newRow("hikiwake") << QStringLiteral("HIKIWAKE") << int(TerminalType::Draw);
    QTest::newRow("max-moves") << QStringLiteral("MAX_MOVES") << int(TerminalType::MaxMoves);
    QTest::newRow("tsumi") << QStringLiteral("TSUMI") << int(TerminalType::Checkmate);
    QTest::newRow("fuzumi") << QStringLiteral("FUZUMI") << int(TerminalType::NoCheckmate);
    QTest::newRow("error") << QStringLiteral("ERROR") << int(TerminalType::Error);
    QTest::newRow("percent") << QStringLiteral("%TORYO") << int(TerminalType::Resign);
    QTest::newRow("unknown") << QStringLiteral("UNKNOWN") << int(TerminalType::None);
    QTest::newRow("percent-unknown") << QStringLiteral("%UNKNOWN") << int(TerminalType::None);
}

void TestParseCommon::csaSpecialToTerminal()
{
    QFETCH(QString, code);
    QFETCH(int, type);
    QCOMPARE(int(KifuParseCommon::csaSpecialToTerminal(code, Turn::Black)), type);
}

void TestParseCommon::csaSpecial_illegalActionDependsOnSide()
{
    // 不正を行った側が手番側なら反則負け、相手なら手番側の反則勝ち
    QCOMPARE(KifuParseCommon::csaSpecialToTerminal(QStringLiteral("%+ILLEGAL_ACTION"), Turn::Black),
             TerminalType::IllegalLoss);
    QCOMPARE(KifuParseCommon::csaSpecialToTerminal(QStringLiteral("%+ILLEGAL_ACTION"), Turn::White),
             TerminalType::IllegalWin);
    QCOMPARE(KifuParseCommon::csaSpecialToTerminal(QStringLiteral("%-ILLEGAL_ACTION"), Turn::Black),
             TerminalType::IllegalWin);
    QCOMPARE(KifuParseCommon::csaSpecialToTerminal(QStringLiteral("%-ILLEGAL_ACTION"), Turn::White),
             TerminalType::IllegalLoss);
}

void TestParseCommon::terminalWords_readAliases()
{
    QCOMPARE(TerminalTypes::fromWord(u"投了"), TerminalType::Resign);
    QCOMPARE(TerminalTypes::fromWord(u"  投了  "), TerminalType::Resign);
    QCOMPARE(TerminalTypes::fromWord(u"切れ負け"), TerminalType::Timeout);
    QCOMPARE(TerminalTypes::fromWord(u"時間切れ"), TerminalType::Timeout);
    QCOMPARE(TerminalTypes::fromWord(u"詰"), TerminalType::Checkmate);
    QCOMPARE(TerminalTypes::fromWord(u"宣言勝ち"), TerminalType::DeclarationWin);
    QCOMPARE(TerminalTypes::fromWord(u"入玉宣言勝ち"), TerminalType::DeclarationWin);
    QCOMPARE(TerminalTypes::fromWord(u"最大手数"), TerminalType::MaxMoves);
    QCOMPARE(TerminalTypes::fromWord(u"開始"), TerminalType::None);
    QCOMPARE(TerminalTypes::fromWord(u"まで82手で投了"), TerminalType::None);
}

void TestParseCommon::terminalWords_findInText()
{
    QString matched;
    QCOMPARE(TerminalTypes::findWord(u"まで82手で投了", &matched), TerminalType::Resign);
    QCOMPARE(matched, QStringLiteral("投了"));
    // 長い語を先に調べる
    QCOMPARE(TerminalTypes::findWord(u"▲不詰"), TerminalType::NoCheckmate);
    QCOMPARE(TerminalTypes::findWord(u"△王手千日手"), TerminalType::PerpetualCheck);
    QCOMPARE(TerminalTypes::findWord(u"普通のテキスト"), TerminalType::None);
}

void TestParseCommon::terminalWords_labelRoundTrip()
{
    for (int i = int(TerminalType::Resign); i <= int(TerminalType::ForfeitWin); ++i) {
        const auto type = static_cast<TerminalType>(i);
        const QString label = TerminalTypes::label(type);
        QVERIFY2(!label.isEmpty(), qPrintable(QString::number(i)));
        QCOMPARE(TerminalTypes::fromWord(label), type);
    }
    QVERIFY(TerminalTypes::label(TerminalType::None).isEmpty());
}

QTEST_MAIN(TestParseCommon)
#include "tst_parsecommon.moc"
