/// @file tst_parsecommon.cpp
/// @brief KifuParseCommon ユーティリティのユニットテスト

#include <QTest>
#include <QStringView>
#include "usimove.h"
#include "parsecommon.h"
#include "shoginotation.h"
#include "terminalcodes.h"
#include "terminaltype.h"
#include "shogitypes.h"

class TestParseCommon : public QObject
{
    Q_OBJECT

private slots:
    // 数字・駒の名前の読み取り（ShogiNotation）
    void notationDigits_data();
    void notationDigits();
    void numberFromDigits_data();
    void numberFromDigits();
    void kanjiNumeralValue();
    void findPieceName_data();
    void findPieceName();
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

    void csaSpecialToTerminal_data();
    void csaSpecialToTerminal();
    void csaSpecial_illegalActionDependsOnSide();
    void terminalWords_readAliases();
    void terminalWords_findInText();
    void terminalWords_labelRoundTrip();

    // ---- digit parsing ----

    // ---- coordinate parsing ----

    // ---- terminal words ----

    // ---- mapKanjiPiece ----

    // ---- helper functions ----
    void appendLine_empty();
    void appendLine_nonEmpty();

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

    // ---- table-driven: flexDigitsToIntNoDetach abnormal ----

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

// ==== digit parsing ====

// ==== coordinate parsing ====

// ==== 数字・駒の名前（ShogiNotation） ====

void TestParseCommon::notationDigits_data()
{
    QTest::addColumn<QChar>("ch");
    QTest::addColumn<int>("value");
    QTest::newRow("ascii-0") << QChar('0') << 0;
    QTest::newRow("ascii-9") << QChar('9') << 9;
    QTest::newRow("zenkaku-0") << QChar(u'０') << 0;
    QTest::newRow("zenkaku-7") << QChar(u'７') << 7;
    QTest::newRow("letter") << QChar('a') << -1;
    QTest::newRow("space") << QChar(' ') << -1;
    QTest::newRow("kanji") << QChar(u'五') << -1;
}

void TestParseCommon::notationDigits()
{
    QFETCH(QChar, ch);
    QFETCH(int, value);
    QCOMPARE(ShogiNotation::digitValue(ch), value);
}

void TestParseCommon::numberFromDigits_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<int>("value");
    QTest::newRow("multi-digit") << QStringLiteral("123") << 123;
    QTest::newRow("empty") << QString() << 0;
    QTest::newRow("mixed-width") << QStringLiteral("1２3") << 123;
    QTest::newRow("all-spaces") << QStringLiteral("   ") << 0;
    QTest::newRow("negative-sign") << QStringLiteral("-123") << 123;
    QTest::newRow("special-chars") << QStringLiteral("!@#") << 0;
    QTest::newRow("leading-zeros") << QStringLiteral("007") << 7;
}

void TestParseCommon::numberFromDigits()
{
    QFETCH(QString, input);
    QFETCH(int, value);
    QCOMPARE(ShogiNotation::numberFromDigits(input), value);
}

void TestParseCommon::kanjiNumeralValue()
{
    QCOMPARE(ShogiNotation::kanjiNumeralValue(QChar(u'〇')), 0);
    QCOMPARE(ShogiNotation::kanjiNumeralValue(QChar(u'零')), 0);
    QCOMPARE(ShogiNotation::kanjiNumeralValue(QChar(u'一')), 1);
    QCOMPARE(ShogiNotation::kanjiNumeralValue(QChar(u'九')), 9);
    QCOMPARE(ShogiNotation::kanjiNumeralValue(QChar(u'十')), -1);
    QCOMPARE(ShogiNotation::kanjiNumeralValue(QChar('1')), -1);
}

void TestParseCommon::findPieceName_data()
{
    QTest::addColumn<QString>("text");
    QTest::addColumn<Piece>("piece");
    QTest::newRow("pawn") << QStringLiteral("歩") << Piece::BlackPawn;
    QTest::newRow("tokin") << QStringLiteral("と") << Piece::BlackPromotedPawn;
    QTest::newRow("horse") << QStringLiteral("馬") << Piece::BlackHorse;
    QTest::newRow("dragon") << QStringLiteral("龍") << Piece::BlackDragon;
    QTest::newRow("dragon-alias") << QStringLiteral("竜") << Piece::BlackDragon;
    QTest::newRow("king") << QStringLiteral("玉") << Piece::BlackKing;
    QTest::newRow("king-alias") << QStringLiteral("王") << Piece::BlackKing;
    QTest::newRow("promoted-silver") << QStringLiteral("成銀") << Piece::BlackPromotedSilver;
    QTest::newRow("promoted-silver-alias") << QStringLiteral("全") << Piece::BlackPromotedSilver;
    QTest::newRow("promoted-knight-alias") << QStringLiteral("圭") << Piece::BlackPromotedKnight;
    QTest::newRow("promoted-lance-alias") << QStringLiteral("杏") << Piece::BlackPromotedLance;
    // 指し手の行から読む（「成」「不成」「打」は駒ではない）
    QTest::newRow("move-line") << QStringLiteral("７六歩(77)") << Piece::BlackPawn;
    QTest::newRow("move-promotes") << QStringLiteral("２二角成(88)") << Piece::BlackBishop;
    QTest::newRow("move-declines") << QStringLiteral("４三銀不成(54)") << Piece::BlackSilver;
    QTest::newRow("move-same-promoted") << QStringLiteral("同　成桂(33)") << Piece::BlackPromotedKnight;
    QTest::newRow("move-drop") << QStringLiteral("５五香打") << Piece::BlackLance;
    QTest::newRow("empty") << QString() << Piece::None;
    QTest::newRow("ascii") << QStringLiteral("P") << Piece::None;
    QTest::newRow("digit") << QStringLiteral("1") << Piece::None;
    QTest::newRow("promote-only") << QStringLiteral("成") << Piece::None;
}

void TestParseCommon::findPieceName()
{
    QFETCH(QString, text);
    QFETCH(Piece, piece);
    QCOMPARE(ShogiNotation::findPieceName(text), piece);
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

// ==== table-driven: mapKanjiPiece abnormal ====

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
    QCOMPARE(int(TerminalCodes::fromCsaSpecial(code, Turn::Black)), type);
}

void TestParseCommon::csaSpecial_illegalActionDependsOnSide()
{
    // 不正を行った側が手番側なら反則負け、相手なら手番側の反則勝ち
    QCOMPARE(TerminalCodes::fromCsaSpecial(QStringLiteral("%+ILLEGAL_ACTION"), Turn::Black),
             TerminalType::IllegalLoss);
    QCOMPARE(TerminalCodes::fromCsaSpecial(QStringLiteral("%+ILLEGAL_ACTION"), Turn::White),
             TerminalType::IllegalWin);
    QCOMPARE(TerminalCodes::fromCsaSpecial(QStringLiteral("%-ILLEGAL_ACTION"), Turn::Black),
             TerminalType::IllegalWin);
    QCOMPARE(TerminalCodes::fromCsaSpecial(QStringLiteral("%-ILLEGAL_ACTION"), Turn::White),
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
