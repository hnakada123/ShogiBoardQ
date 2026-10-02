/// @file tst_kifu_format_compliance.cpp
/// @brief 公開仕様の局面・終局・コメントを読み込み、他形式への保存後も保持する。

#include <QtTest>

#include "kifuconversionservice.h"
#include "kifuloadparser.h"
#include "kifubranchtree.h"
#include "bodtextgenerator.h"
#include "ki2lexer.h"
#include "parsecommon.h"
#include "sfenutils.h"

using Format = KifuConversionService::Format;

class TestKifuFormatCompliance : public QObject
{
    Q_OBJECT

    static KifuConversionService::Result convert(const QString& text, Format output,
                                                  Format input = Format::Auto)
    {
        KifuConversionService::Request request;
        request.text = text;
        request.inputFormat = input;
        request.outputFormat = output;
        return KifuConversionService::convert(request);
    }

private slots:
    void ki2GoldRetreat_data()
    {
        QTest::addColumn<QString>("piece");
        QTest::addColumn<bool>("black");
        for (const auto* piece : {"G", "+P", "+L", "+N", "+S"}) {
            QTest::newRow(qPrintable(QString::fromLatin1(piece) + "-black")) << QString::fromLatin1(piece) << true;
            QTest::newRow(qPrintable(QString::fromLatin1(piece) + "-white")) << QString::fromLatin1(piece).toLower() << false;
        }
    }

    void ki2GoldRetreat()
    {
        QFETCH(QString, piece);
        QFETCH(bool, black);
        const QString sfen = QStringLiteral("4k4/9/9/9/4") + piece + QStringLiteral("4/9/9/9/4K4 ")
            + (black ? QStringLiteral("b") : QStringLiteral("w")) + QStringLiteral(" - 1");
        const QString move = black ? QStringLiteral("5e5f") : QStringLiteral("5e5d");
        const auto saved = convert(QStringLiteral("position sfen %1 moves %2").arg(sfen, move), Format::Ki2);
        QVERIFY2(saved.ok, qPrintable(saved.error));
        const auto reread = convert(saved.lines.join(QLatin1Char('\n')), Format::Usi);
        QVERIFY2(reread.ok, qPrintable(reread.error));
        QCOMPARE(reread.usiMoves, QStringList{move});
    }

    void ki2DirectionalNotation()
    {
        // 日本将棋連盟の「竜は動作を優先」の例（２三竜・５二竜→４三）。
        const QString sfen = QStringLiteral("4k4/4+R4/7+R1/9/9/9/9/9/4K4 b - 1");
        const auto saved = convert(QStringLiteral("position sfen %1 moves 2c4c").arg(sfen), Format::Ki2);
        QVERIFY2(saved.ok, qPrintable(saved.error));
        QVERIFY(saved.lines.join(QLatin1Char('\n')).contains(QStringLiteral("４三龍寄")));
        const auto resaved = convert(saved.lines.join(QLatin1Char('\n')), Format::Ki2);
        QVERIFY2(resaved.ok, qPrintable(resaved.error));
        QVERIFY(!resaved.lines.join(QLatin1Char('\n')).contains(QStringLiteral("寄寄")));
        QCOMPARE(resaved.usiMoves, QStringList{QStringLiteral("2c4c")});
        QVERIFY(!Ki2Lexer::canPieceMoveTo(Piece::BlackGold, false, 5, 5, 5, 5, true));
    }

    void ki2PieceSymbolsAreDetected()
    {
        const auto loaded = KifuLoadParser::parseText(QStringLiteral("☗７六歩 ☖３四歩\n"));
        QVERIFY2(loaded.success, qPrintable(loaded.error));
        QCOMPARE(loaded.record.mainline.usiMoves, QStringList({QStringLiteral("7g7f"), QStringLiteral("3c3d")}));
    }

    void csaHeaderRoundTrip()
    {
        const QString text = QStringLiteral("V3.0\n$TIME:900+0+5\n$TIME+:450+0+5\n$TIME-:900+0+5\n"
            "$JISHOGI:27\n$NOTE:first\\nsecond\\\\path\nPI\n+\n");
        const auto saved = convert(text, Format::Csa);
        QVERIFY2(saved.ok, qPrintable(saved.error));
        for (const auto& field : {QStringLiteral("$TIME:900+0+5"), QStringLiteral("$TIME+:450+0+5"),
                                  QStringLiteral("$TIME-:900+0+5"), QStringLiteral("$JISHOGI:27"),
                                  QStringLiteral("$NOTE:first\\nsecond\\\\path")}) {
            QVERIFY2(saved.lines.contains(field), qPrintable(field));
        }
    }

    void csaMillisecondTimesRoundTrip()
    {
        const auto saved = convert(QStringLiteral("V3.0\nPI\n+\n+7776FU,T1.125\n-3334FU,T62.500\n%TORYO,T0.250\n"), Format::Csa);
        QVERIFY2(saved.ok, qPrintable(saved.error));
        QVERIFY(saved.lines.contains(QStringLiteral("T1.125")));
        QVERIFY(saved.lines.contains(QStringLiteral("T62.500")));
        QVERIFY(saved.lines.contains(QStringLiteral("T0.250")));
        const auto reread = convert(saved.lines.join(QLatin1Char('\n')), Format::Csa);
        QVERIFY2(reread.ok, qPrintable(reread.error));
        QCOMPARE(reread.moves.last().timeText, QStringLiteral("00:00.250/00:00:01.375"));
        const auto kif = convert(saved.lines.join(QLatin1Char('\n')), Format::Kif);
        QVERIFY(kif.lines.join(QLatin1Char('\n')).contains(QStringLiteral("1:02/00:01:02")));
    }

    void csaV3TerminalRoundTrip_data()
    {
        QTest::addColumn<QString>("code");
        for (const auto* code : {"MAX_MOVES", "HIKIWAKE", "ERROR"})
            QTest::newRow(code) << QString::fromLatin1(code);
    }

    void csaV3TerminalRoundTrip()
    {
        QFETCH(QString, code);
        const auto result = convert(QStringLiteral("V3.0\nPI\n+\n+7776FU\n%") + code, Format::Csa);
        QVERIFY2(result.ok, qPrintable(result.error));
        QVERIFY(result.lines.contains(QLatin1Char('%') + code));
        const auto loaded = KifuLoadParser::parseText(result.lines.join(QLatin1Char('\n')));
        QVERIFY2(loaded.success, qPrintable(loaded.error));
        QCOMPARE(loaded.sfens.size(), 3);
        QCOMPARE(loaded.sfens.at(1), loaded.sfens.at(2));
        KifuBranchTree tree;
        tree.setRootSfen(loaded.sfens.at(1));
        auto* terminal = tree.addMove(tree.root(), {}, loaded.record.mainline.disp.last().prettyMove, {});
        QVERIFY(terminal);
        QVERIFY(terminal->isTerminal());
        QCOMPARE(terminal->sfen(), tree.root()->sfen());
    }

    void noMateIsNotCheckmate()
    {
        QString terminal;
        QVERIFY(KifuParseCommon::isTerminalWordContains(QStringLiteral("▲不詰"), &terminal));
        QCOMPARE(terminal, QStringLiteral("不詰"));
    }

    void csaIllegalActionWinner_data()
    {
        QTest::addColumn<QString>("code");
        QTest::addColumn<QString>("label");
        QTest::newRow("side-to-move-loses") << QStringLiteral("-ILLEGAL_ACTION") << QStringLiteral("△反則負け");
        QTest::newRow("other-side-loses") << QStringLiteral("+ILLEGAL_ACTION") << QStringLiteral("△反則勝ち");
    }

    void csaIllegalActionWinner()
    {
        QFETCH(QString, code);
        QFETCH(QString, label);
        const QString text = QStringLiteral("V2.2\nPI\n+\n+7776FU\n%") + code;
        for (const Format fmt : {Format::Csa, Format::Jkf, Format::Usi}) {
            const auto saved = convert(text, fmt);
            QVERIFY2(saved.ok, qPrintable(saved.error));
            QCOMPARE(saved.moves.last().prettyMove, label);
            const auto reread = convert(saved.lines.join(QLatin1Char('\n')), Format::Kif);
            QVERIFY2(reread.ok, qPrintable(reread.error));
            QCOMPARE(reread.moves.last().prettyMove, label);
        }
    }

    void whiteToMoveTerminalRoundTrip()
    {
        const QString sfen = QStringLiteral("4k4/9/9/9/9/9/9/9/4K4 w - 1");
        const QString text = BodTextGenerator::generate(sfen, 0, {})
            + QStringLiteral("\n手数----指手---------消費時間--\n1 ４一玉(51)\n2 投了\n");
        for (const Format fmt : {Format::Usi, Format::Usen}) {
            const auto saved = convert(text, fmt);
            QVERIFY2(saved.ok, qPrintable(saved.error));
            const auto reread = convert(saved.lines.join(QLatin1Char('\n')), Format::Kif);
            QVERIFY2(reread.ok, qPrintable(reread.error));
            QCOMPARE(reread.moves.last().prettyMove, QStringLiteral("▲投了"));
        }
    }

    void csaInitialPosition_data()
    {
        QTest::addColumn<QString>("text");
        QTest::addColumn<QString>("sfen");
        QTest::addColumn<QStringList>("moves");
        const QString handicap = QStringLiteral("lnsgkgsnl/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1");
        QTest::newRow("PI-handicap") << QStringLiteral("V2.2\nPI82HI22KA\n-\n-3334FU\n")
                                   << handicap << QStringList{QStringLiteral("3c3d")};
        QTest::newRow("multi-statement") << QStringLiteral("V2.2\nPI82HI22KA,-,-3334FU,T2\n")
                                       << handicap << QStringList{QStringLiteral("3c3d")};
        QTest::newRow("piece-position") << QStringLiteral("V3.0\nP-51OU\nP+59OU00HI\n+\n+0055HI\n")
                                      << QStringLiteral("4k4/9/9/9/9/9/9/9/4K4 b R 1")
                                      << QStringList{QStringLiteral("R*5e")};
        QTest::newRow("no-board") << QStringLiteral("V3.0\n+\n")
                                 << QStringLiteral("9/9/9/9/9/9/9/9/9 b - 1") << QStringList{};
        QTest::newRow("all-in-hand") << QStringLiteral("V3.0\nP+00AL\n+\n")
                                    << QStringLiteral("9/9/9/9/9/9/9/9/9 b 2R2B4G4S4N4L18P 1") << QStringList{};
    }

    void csaInitialPosition()
    {
        QFETCH(QString, text);
        QFETCH(QString, sfen);
        QFETCH(QStringList, moves);
        const auto loaded = KifuLoadParser::parseText(text);
        QVERIFY2(loaded.success, qPrintable(loaded.error));
        QCOMPARE(loaded.initialSfen, sfen);
        QCOMPARE(loaded.record.mainline.baseSfen, sfen);
        QCOMPARE(loaded.record.mainline.usiMoves, moves);
        QCOMPARE(loaded.sfens.first(), sfen);
    }

    void csaPromotionSurvivesKifConversion()
    {
        const auto kif = convert(QStringLiteral("V2.2\nPI\n+\n+7776FU\n-3334FU\n+8822UM\n"), Format::Kif);
        QVERIFY2(kif.ok, qPrintable(kif.error));
        QVERIFY(kif.lines.join(QLatin1Char('\n')).contains(QStringLiteral("２二角成(88)")));
        const auto reread = convert(kif.lines.join(QLatin1Char('\n')), Format::Usi);
        QVERIFY2(reread.ok, qPrintable(reread.error));
        QCOMPARE(reread.usiMoves, QStringList({QStringLiteral("7g7f"), QStringLiteral("3c3d"), QStringLiteral("8h2b+")}));
    }

    void csaMultipleGamesAreNotConcatenated()
    {
        const auto result = convert(QStringLiteral("V2.2\nPI\n+\n+7776FU\n%TORYO\n/\nPI\n+\n+2726FU\n%TORYO\n"), Format::Usi);
        QVERIFY2(result.ok, qPrintable(result.error));
        QCOMPARE(result.usiMoves, QStringList{QStringLiteral("7g7f")});
        QVERIFY(!result.warnings.isEmpty());
    }

    void usenPublishedExample_data()
    {
        QTest::addColumn<QString>("text");
        QTest::addColumn<int>("count");
        QTest::newRow("published") << QStringLiteral("~0.7ku36e8uc.") << 3;
        QTest::newRow("empty") << QStringLiteral("~0..") << 0;
        QTest::newRow("legacy-without-result-field") << QStringLiteral("~0.7ku36e8uc") << 3;
    }

    void usenPublishedExample()
    {
        QFETCH(QString, text);
        QFETCH(int, count);
        const auto result = convert(text, Format::Usi);
        QVERIFY2(result.ok, qPrintable(result.error));
        QCOMPARE(result.inputFormat, Format::Usen);
        QCOMPARE(result.usiMoves.size(), count);
        QCOMPARE(result.moves.size(), count + 1);
    }

    void usenDoesNotInventResignation()
    {
        const auto result = convert(QStringLiteral("position startpos moves 7g7f 3c3d"), Format::Usen);
        QVERIFY2(result.ok, qPrintable(result.error));
        QVERIFY(!result.lines.first().endsWith(QStringLiteral(".r")));
        const auto reread = convert(result.lines.first(), Format::Usi);
        QVERIFY2(reread.ok, qPrintable(reread.error));
        QCOMPARE(reread.moves.size(), 3);
        QCOMPARE(reread.usiMoves, result.usiMoves);
    }

    void jkfHandicapAndNullPromotion()
    {
        const QString text = QStringLiteral(R"({"header":{},"initial":{"preset":"6"},"moves":[{},
            {"move":{"color":1,"piece":"OU","from":{"x":5,"y":1},"to":{"x":4,"y":2},"promote":null}},
            {"move":{"color":0,"piece":"FU","from":{"x":7,"y":7},"to":{"x":7,"y":6}}}]})");
        const auto result = convert(text, Format::Kif);
        QVERIFY2(result.ok, qPrintable(result.error));
        QCOMPARE(result.moves.at(1).prettyMove, QStringLiteral("△４二玉(51)"));
        QCOMPARE(result.moves.at(2).prettyMove, QStringLiteral("▲７六歩(77)"));
        const auto reread = convert(result.lines.join(QLatin1Char('\n')), Format::Usi);
        QVERIFY2(reread.ok, qPrintable(reread.error));
        QCOMPARE(reread.initialSfen, result.initialSfen);
        QCOMPARE(reread.usiMoves, result.usiMoves);
    }

    void malformedJkfIsRejected_data()
    {
        QTest::addColumn<QString>("json");
        QTest::newRow("moves-type") << QStringLiteral(R"({"moves":{}})");
        QTest::newRow("move-entry") << QStringLiteral(R"({"moves":[{},42]})");
        QTest::newRow("initial-board") << QStringLiteral(R"({"initial":{"preset":"OTHER","data":{"board":[],"hands":[{},{}],"color":0}},"moves":[{}]})");
        QTest::newRow("move-coordinate") << QStringLiteral(R"({"moves":[{},{"move":{"to":{"x":0,"y":6},"piece":"FU","color":0}}]})");
        QTest::newRow("fork-type") << QStringLiteral(R"({"moves":[{},{"forks":[{}]}]})");
    }

    void malformedJkfIsRejected()
    {
        QFETCH(QString, json);
        const auto result = KifuLoadParser::parseText(json);
        QVERIFY(!result.success);
        QVERIFY(!result.error.isEmpty());
    }

    void commentsRoundTrip_data()
    {
        QTest::addColumn<int>("format");
        for (const Format fmt : {Format::Kif, Format::Ki2, Format::Csa, Format::Jkf}) {
            QTest::newRow(qPrintable(KifuConversionService::formatName(fmt))) << static_cast<int>(fmt);
        }
    }

    void commentsRoundTrip()
    {
        const QString source = QStringLiteral("手合割：平手\n*開始局面\n*\n**先頭の記号\n"
            "手数----指手---------消費時間--\n1 ７六歩(77)\n*first, second\n*\n*次の段落\n"
            "2 投了\n*終局コメント\n");
        QFETCH(int, format);
        const auto original = convert(source, Format::Kif);
        const auto saved = convert(source, static_cast<Format>(format));
        QVERIFY2(saved.ok, qPrintable(saved.error));
        const auto reread = convert(saved.lines.join(QLatin1Char('\n')), Format::Kif);
        QVERIFY2(reread.ok, qPrintable(reread.error));
        QCOMPARE(reread.moves.size(), original.moves.size());
        for (qsizetype i = 0; i < original.moves.size(); ++i) {
            QCOMPARE(reread.moves.at(i).comment, original.moves.at(i).comment);
        }
    }

    void terminalRoundTrip_data()
    {
        QTest::addColumn<QString>("terminal");
        for (const auto& terminal : {QStringLiteral("投了"), QStringLiteral("切れ負け"),
                                    QStringLiteral("詰み"), QStringLiteral("反則負け"),
                                    QStringLiteral("持将棋")}) {
            QTest::newRow(qPrintable(terminal)) << terminal;
        }
    }

    void terminalRoundTrip()
    {
        QFETCH(QString, terminal);
        const auto saved = convert(QStringLiteral("手合割：平手\n手数----指手---------消費時間--\n"
                                                  "1 ７六歩(77)\n2 %1\n").arg(terminal), Format::Usi);
        QVERIFY2(saved.ok, qPrintable(saved.error));
        const auto reread = convert(saved.lines.join(QLatin1Char('\n')), Format::Kif);
        QVERIFY2(reread.ok, qPrintable(reread.error));
        QCOMPARE(reread.moves.size(), 3);
        QVERIFY2(reread.moves.last().prettyMove.contains(terminal), qPrintable(reread.moves.last().prettyMove));
        QVERIFY(reread.lines.join(QLatin1Char('\n')).contains(terminal));
    }
};

QTEST_GUILESS_MAIN(TestKifuFormatCompliance)
#include "tst_kifu_format_compliance.moc"
