/// @file tst_kifu_format_compliance.cpp
/// @brief 公開仕様の局面・終局・コメントを読み込み、他形式への保存後も保持する。

#include <QtTest>

#include "kifuconversionservice.h"
#include "kifuloadparser.h"
#include "kifubranchtree.h"
#include "kifubranchtreebuilder.h"
#include "kifubranchnode.h"
#include "bodtextgenerator.h"
#include "parsecommon.h"
#include "sfenutils.h"

using Format = KifuFormat;

class TestKifuFormatCompliance : public QObject
{
    Q_OBJECT

    static KifuConversionService::Result convert(const QString& text, Format output,
                                                  std::optional<KifuFormat> input = std::nullopt)
    {
        KifuConversionService::Request request;
        request.text = text;
        request.inputFormat = input;
        request.outputFormat = output;
        return KifuConversionService::convert(request);
    }

    /// 本譜の最後の行の表記（終局の行があればその表記。行がなければ空）
    static QString lastText(const KifuConversionService::Result& result)
    {
        const QList<RecordEntry>& entries = result.mainline.entries;
        return entries.isEmpty() ? QString() : entries.last().displayText;
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
    }

    /// 修飾語・打・成・不成は日本将棋連盟の棋譜の表記方法に合わせ、読み込むと同じ手に戻る
    void ki2ModifierNotation_data()
    {
        QTest::addColumn<QString>("sfen");
        QTest::addColumn<QString>("usi");
        QTest::addColumn<QString>("ki2");
        const auto add = [](const char* name, const char* sfen, const char* usi, const QString& ki2) {
            QTest::newRow(name) << QString::fromLatin1(sfen) << QString::fromLatin1(usi) << ki2;
        };
        // 図「上・寄・引」：動作で区別できれば動作を書く
        add("action-A-up", "8k/2G6/G8/9/9/9/9/9/4K4 b - 1", "9c8b", QStringLiteral("▲８二金上"));
        add("action-A-side", "8k/2G6/G8/9/9/9/9/9/4K4 b - 1", "7b8b", QStringLiteral("▲８二金寄"));
        add("action-B-up", "6G1k/9/5G3/9/9/9/9/9/4K4 b - 1", "4c3b", QStringLiteral("▲３二金上"));
        add("action-B-back", "6G1k/9/5G3/9/9/9/9/9/4K4 b - 1", "3a3b", QStringLiteral("▲３二金引"));
        add("action-C-up", "8k/9/9/9/5G3/4G4/9/9/4K4 b - 1", "5f5e", QStringLiteral("▲５五金上"));
        add("action-C-side", "8k/9/9/9/5G3/4G4/9/9/4K4 b - 1", "4e5e", QStringLiteral("▲５五金寄"));
        add("action-D-up", "8k/9/9/9/9/9/2S6/9/1S2K4 b - 1", "8i8h", QStringLiteral("▲８八銀上"));
        add("action-D-back", "8k/9/9/9/9/9/2S6/9/1S2K4 b - 1", "7g8h", QStringLiteral("▲８八銀引"));
        add("action-E-up", "8k/9/9/9/9/9/7S1/9/4KS3 b - 1", "4i3h", QStringLiteral("▲３八銀上"));
        add("action-E-back", "8k/9/9/9/9/9/7S1/9/4KS3 b - 1", "2g3h", QStringLiteral("▲３八銀引"));
        // 図「右・左1」：動作が同じなら位置。金・銀が真っ直ぐ1段上がるときは「直」
        add("side-A-left", "8k/G1G6/9/9/9/9/9/9/4K4 b - 1", "9b8a", QStringLiteral("▲８一金左"));
        add("side-A-right", "8k/G1G6/9/9/9/9/9/9/4K4 b - 1", "7b8a", QStringLiteral("▲８一金右"));
        add("side-B-left", "k8/6G1G/9/9/9/9/9/9/4K4 b - 1", "3b2b", QStringLiteral("▲２二金左"));
        add("side-B-right", "k8/6G1G/9/9/9/9/9/9/4K4 b - 1", "1b2b", QStringLiteral("▲２二金右"));
        add("side-C-left", "8k/9/9/9/3S1S3/9/9/9/4K4 b - 1", "6e5f", QStringLiteral("▲５六銀左"));
        add("side-C-right", "8k/9/9/9/3S1S3/9/9/9/4K4 b - 1", "4e5f", QStringLiteral("▲５六銀右"));
        add("side-D-left", "8k/9/9/9/9/9/9/9/1GG1K4 b - 1", "8i7h", QStringLiteral("▲７八金左"));
        add("side-D-straight", "8k/9/9/9/9/9/9/9/1GG1K4 b - 1", "7i7h", QStringLiteral("▲７八金直"));
        add("side-E-straight", "8k/9/9/9/9/9/9/9/4K1SS1 b - 1", "3i3h", QStringLiteral("▲３八銀直"));
        add("side-E-right", "8k/9/9/9/9/9/9/9/4K1SS1 b - 1", "2i3h", QStringLiteral("▲３八銀右"));
        // 図「右・左2」：3枚以上。1字で区別できなければ位置・動作の2字
        add("three-A-left", "8k/9/3GGG3/9/9/9/9/9/4K4 b - 1", "6c5b", QStringLiteral("▲５二金左"));
        add("three-A-straight", "8k/9/3GGG3/9/9/9/9/9/4K4 b - 1", "5c5b", QStringLiteral("▲５二金直"));
        add("three-A-right", "8k/9/3GGG3/9/9/9/9/9/4K4 b - 1", "4c5b", QStringLiteral("▲５二金右"));
        const char* tokins = "8k/9/9/9/9/9/1+P7/+P8/+P+P+P1K4 b - 1";
        add("three-B-right", tokins, "7i8h", QStringLiteral("▲８八と右"));
        add("three-B-straight", tokins, "8i8h", QStringLiteral("▲８八と直"));
        add("three-B-left-up", tokins, "9i8h", QStringLiteral("▲８八と左上"));
        add("three-B-side", tokins, "9h8h", QStringLiteral("▲８八と寄"));
        add("three-B-back", tokins, "8g8h", QStringLiteral("▲８八と引"));
        const char* silvers = "8k/9/9/9/9/9/6S1S/9/4K1SS1 b - 1";
        add("three-C-straight", silvers, "2i2h", QStringLiteral("▲２八銀直"));
        add("three-C-right", silvers, "1g2h", QStringLiteral("▲２八銀右"));
        add("three-C-left-up", silvers, "3i2h", QStringLiteral("▲２八銀左上"));
        add("three-C-left-back", silvers, "3g2h", QStringLiteral("▲２八銀左引"));
        // 図「竜」：動作を優先し、「直」は使わず右・左
        add("dragon-A-back", "+R8/9/9/1+R7/9/9/9/9/4K3k b - 1", "9a8b", QStringLiteral("▲８二龍引"));
        add("dragon-A-up", "+R8/9/9/1+R7/9/9/9/9/4K3k b - 1", "8d8b", QStringLiteral("▲８二龍上"));
        add("dragon-B-side", "8k/4+R4/7+R1/9/9/9/9/9/4K4 b - 1", "2c4c", QStringLiteral("▲４三龍寄"));
        add("dragon-B-back", "8k/4+R4/7+R1/9/9/9/9/9/4K4 b - 1", "5b4c", QStringLiteral("▲４三龍引"));
        add("dragon-C-left", "k8/9/9/9/4+R3+R/9/9/9/4K4 b - 1", "5e3e", QStringLiteral("▲３五龍左"));
        add("dragon-C-right", "k8/9/9/9/4+R3+R/9/9/9/4K4 b - 1", "1e3e", QStringLiteral("▲３五龍右"));
        add("dragon-D-left", "8k/9/9/9/9/9/9/9/+R+R2K4 b - 1", "9i8h", QStringLiteral("▲８八龍左"));
        add("dragon-D-right", "8k/9/9/9/9/9/9/9/+R+R2K4 b - 1", "8i8h", QStringLiteral("▲８八龍右"));
        add("dragon-E-left", "k8/9/9/9/9/9/9/7+R1/4K3+R b - 1", "2h1g", QStringLiteral("▲１七龍左"));
        add("dragon-E-right", "k8/9/9/9/9/9/9/7+R1/4K3+R b - 1", "1i1g", QStringLiteral("▲１七龍右"));
        // 図「馬」
        add("horse-A-left", "+B+B6k/9/9/9/9/9/9/9/4K4 b - 1", "9a8b", QStringLiteral("▲８二馬左"));
        add("horse-A-right", "+B+B6k/9/9/9/9/9/9/9/4K4 b - 1", "8a8b", QStringLiteral("▲８二馬右"));
        add("horse-B-side", "8k/9/3+B5/9/+B8/9/9/9/4K4 b - 1", "9e8e", QStringLiteral("▲８五馬寄"));
        add("horse-B-back", "8k/9/3+B5/9/+B8/9/9/9/4K4 b - 1", "6c8e", QStringLiteral("▲８五馬引"));
        add("horse-C-back", "k7+B/9/9/6+B2/9/9/9/9/4K4 b - 1", "1a1b", QStringLiteral("▲１二馬引"));
        add("horse-C-up", "k7+B/9/9/6+B2/9/9/9/9/4K4 b - 1", "3d1b", QStringLiteral("▲１二馬上"));
        add("horse-D-left", "8k/9/9/9/9/9/9/9/+B3+B3K b - 1", "9i7g", QStringLiteral("▲７七馬左"));
        add("horse-D-right", "8k/9/9/9/9/9/9/9/+B3+B3K b - 1", "5i7g", QStringLiteral("▲７七馬右"));
        add("horse-E-left", "8k/9/9/9/9/9/5+B3/8+B/4K4 b - 1", "4g2i", QStringLiteral("▲２九馬左"));
        add("horse-E-right", "8k/9/9/9/9/9/5+B3/8+B/4K4 b - 1", "1h2i", QStringLiteral("▲２九馬右"));
        // 後手は後手から見た右・左・上・引
        add("white-left", "4k4/9/9/9/9/9/3ggg3/9/K8 w - 1", "4g5h", QStringLiteral("△５八金左"));
        add("white-straight", "4k4/9/9/9/9/9/3ggg3/9/K8 w - 1", "5g5h", QStringLiteral("△５八金直"));
        add("white-right", "4k4/9/9/9/9/9/3ggg3/9/K8 w - 1", "6g5h", QStringLiteral("△５八金右"));
        add("white-knight-left", "1k3n1n1/9/9/9/9/9/9/9/4K4 w - 1", "2a3c", QStringLiteral("△３三桂左"));
        add("white-knight-right", "1k3n1n1/9/9/9/9/9/9/9/4K4 w - 1", "4a3c", QStringLiteral("△３三桂右"));
        // 桂は直を使わず右・左
        add("knight-left", "8k/9/9/9/9/9/9/9/N1N1K4 b - 1", "9i8g", QStringLiteral("▲８七桂左"));
        add("knight-right", "8k/9/9/9/9/9/9/9/N1N1K4 b - 1", "7i8g", QStringLiteral("▲８七桂右"));
        // 成・不成は成れる手だけに書き、修飾語の後に置く
        add("promote-with-modifier", "k8/9/9/S1S6/9/9/9/9/4K4 b - 1", "9d8c+", QStringLiteral("▲８三銀左成"));
        add("not-promote-with-modifier", "k8/9/9/S1S6/9/9/9/9/4K4 b - 1", "7d8c", QStringLiteral("▲８三銀右不成"));
        add("not-promote", "8k/9/9/4S4/9/9/9/9/4K4 b - 1", "5d5c", QStringLiteral("▲５三銀不成"));
        add("no-promotion-outside-zone", "8k/9/9/9/4S4/9/9/9/4K4 b - 1", "5e5d", QStringLiteral("▲５四銀"));
        // ピンされた金は動ける駒に数えない（修飾語も「打」も要らない）
        add("pinned-omits-modifier", "4r3k/9/9/9/9/9/9/4G1G2/4K4 b G 1", "3h4g", QStringLiteral("▲４七金"));
        add("drop-needs-marker", "4r3k/9/9/9/9/9/9/4G1G2/4K4 b G 1", "G*4g", QStringLiteral("▲４七金打"));
        add("pinned-omits-drop-marker", "4r3k/9/9/9/9/9/9/4G1G2/4K4 b G 1", "G*6g", QStringLiteral("▲６七金"));
        add("pinned-along-the-pin", "4r3k/9/9/9/9/9/9/4G1G2/4K4 b G 1", "5h5g", QStringLiteral("▲５七金"));
        // 反則手（二歩・王手放置）も書き、駒の動きだけで読み戻す
        add("illegal-double-pawn", "4k4/9/9/9/9/9/4P4/9/4K4 b P 1", "P*5e", QStringLiteral("▲５五歩"));
        add("illegal-double-pawn-marker", "4k4/9/9/9/9/4P4/9/9/4K4 b P 1", "P*5e", QStringLiteral("▲５五歩打"));
        add("illegal-king-in-check", "4k4/9/9/9/4r4/9/9/9/4K3G b - 1", "1i1h", QStringLiteral("▲１八金"));
    }

    void ki2ModifierNotation()
    {
        QFETCH(QString, sfen);
        QFETCH(QString, usi);
        QFETCH(QString, ki2);
        const auto saved = convert(QStringLiteral("position sfen %1 moves %2").arg(sfen, usi), Format::Ki2);
        QVERIFY2(saved.ok, qPrintable(saved.error));
        QVERIFY2(saved.lines.contains(ki2), qPrintable(saved.lines.join(QLatin1Char('\n'))));
        const auto reread = convert(saved.lines.join(QLatin1Char('\n')), Format::Usi, Format::Ki2);
        QVERIFY2(reread.ok, qPrintable(reread.error));
        QCOMPARE(reread.usiMoves, QStringList{usi});
    }

    /// 「同」は直前の手と同じマスへの手。変化の最初の手でも分岐する前の手を見る
    void ki2SameSquareRoundTrip()
    {
        const QString kif = QStringLiteral(
            "手合割：平手\n"
            "手数----指手---------消費時間--\n"
            "   1 ７六歩(77)\n   2 ３四歩(33)\n   3 ２二角成(88)+\n   4 同　銀(31)\n"
            "\n変化：3手\n   3 ２二角不成(88)\n   4 同　飛(82)\n");
        const auto saved = convert(kif, Format::Ki2, Format::Kif);
        QVERIFY2(saved.ok, qPrintable(saved.error));
        const QString text = saved.lines.join(QLatin1Char('\n'));
        QVERIFY2(text.contains(QStringLiteral("▲２二角成    △同　銀")), qPrintable(text));
        QVERIFY2(text.contains(QStringLiteral("▲２二角不成    △同　飛")), qPrintable(text));
        const auto reread = convert(text, Format::Kif, Format::Ki2);
        QVERIFY2(reread.ok, qPrintable(reread.error));
        QCOMPARE(reread.usiMoves, QStringList({"7g7f", "3c3d", "8h2b+", "3a2b"}));
        const QString rereadKif = reread.lines.join(QLatin1Char('\n'));
        QVERIFY2(rereadKif.contains(QStringLiteral("同　飛(82)")), qPrintable(rereadKif));
    }

    void ki2PieceSymbolsAreDetected()
    {
        const auto loaded = KifuLoadParser::parseText(QStringLiteral("☗７六歩 ☖３四歩\n"));
        QVERIFY2(loaded.success, qPrintable(loaded.error));
        QCOMPARE(loaded.record.mainline.usiMoves(), QStringList({QStringLiteral("7g7f"), QStringLiteral("3c3d")}));
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
        QVERIFY(reread.mainline.endsWithTerminal());
        QCOMPARE(reread.mainline.entries.last().time.toText(), QStringLiteral("00:00.250/00:00:01.375"));
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
        // 棋譜の正本（分岐ツリー）では、終局行は直前の局面を持つ終局ノードになる
        KifuBranchTree tree;
        KifuBranchTreeBuilder::buildFromKifParseResult(&tree, loaded.record, loaded.initialSfen);
        const QList<KifuBranchNode*> mainLine = tree.mainLine();
        QCOMPARE(mainLine.size(), 3);
        QVERIFY(mainLine.last()->isTerminal());
        QCOMPARE(mainLine.last()->sfen(), mainLine.at(1)->sfen());
    }

    void noMateIsNotCheckmate()
    {
        QString terminal;
        QCOMPARE(TerminalTypes::findWord(u"▲不詰", &terminal), TerminalType::NoCheckmate);
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
            QCOMPARE(lastText(saved), label);
            const auto reread = convert(saved.lines.join(QLatin1Char('\n')), Format::Kif);
            QVERIFY2(reread.ok, qPrintable(reread.error));
            QCOMPARE(lastText(reread), label);
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
            QCOMPARE(lastText(reread), QStringLiteral("▲投了"));
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
        QCOMPARE(loaded.record.mainline.startSfen, sfen);
        QCOMPARE(loaded.record.mainline.usiMoves(), moves);
        KifuBranchTree tree;
        KifuBranchTreeBuilder::buildFromKifParseResult(&tree, loaded.record, loaded.initialSfen);
        QCOMPARE(tree.root()->sfen(), sfen);
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
        // 終局の行は作らない（結果の欄が空でも、欄がなくても）
        QCOMPARE(result.mainline.moveCount(), count);
        QVERIFY(!result.mainline.endsWithTerminal());
    }

    void usenDoesNotInventResignation()
    {
        const auto result = convert(QStringLiteral("position startpos moves 7g7f 3c3d"), Format::Usen);
        QVERIFY2(result.ok, qPrintable(result.error));
        QVERIFY(!result.lines.first().endsWith(QStringLiteral(".r")));
        const auto reread = convert(result.lines.first(), Format::Usi);
        QVERIFY2(reread.ok, qPrintable(reread.error));
        QCOMPARE(reread.mainline.entries.size(), 2);
        QVERIFY(!reread.mainline.endsWithTerminal());
        QCOMPARE(reread.usiMoves, result.usiMoves);
    }

    void jkfHandicapAndNullPromotion()
    {
        const QString text = QStringLiteral(R"({"header":{},"initial":{"preset":"6"},"moves":[{},
            {"move":{"color":1,"piece":"OU","from":{"x":5,"y":1},"to":{"x":4,"y":2},"promote":null}},
            {"move":{"color":0,"piece":"FU","from":{"x":7,"y":7},"to":{"x":7,"y":6}}}]})");
        const auto result = convert(text, Format::Kif);
        QVERIFY2(result.ok, qPrintable(result.error));
        QCOMPARE(result.mainline.entries.size(), 2);
        QCOMPARE(result.mainline.entries.at(0).displayText, QStringLiteral("△４二玉(51)"));
        QCOMPARE(result.mainline.entries.at(1).displayText, QStringLiteral("▲７六歩(77)"));
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
            QTest::newRow(qPrintable(KifuFormats::name(fmt))) << static_cast<int>(fmt);
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
        // 開始局面のコメントと、各行（終局の行を含む）のコメント
        QCOMPARE(reread.mainline.openingComment, original.mainline.openingComment);
        QCOMPARE(reread.mainline.entries.size(), original.mainline.entries.size());
        for (qsizetype i = 0; i < original.mainline.entries.size(); ++i) {
            QCOMPARE(reread.mainline.entries.at(i).comment, original.mainline.entries.at(i).comment);
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
        QCOMPARE(reread.mainline.entries.size(), 2);
        // 終局の種類が保たれる（棋譜欄の表記は終局語の表の語になる）
        QVERIFY(reread.mainline.endsWithTerminal());
        QCOMPARE(reread.mainline.terminal()->terminal, TerminalTypes::fromWord(terminal));
        QCOMPARE(TerminalTypes::findWord(lastText(reread)), TerminalTypes::fromWord(terminal));
        QVERIFY(reread.lines.join(QLatin1Char('\n')).contains(terminal));
    }

    // 貼り付けたテキストは一時ファイルを作らず、各形式のテキスト版の入口で読む
    void parseTextEveryFormat_data()
    {
        QTest::addColumn<QString>("text");
        QTest::addColumn<QStringList>("moves");
        const QStringList two{QStringLiteral("7g7f"), QStringLiteral("3c3d")};
        QTest::newRow("kif") << QStringLiteral("手数----指手---------消費時間--\n1 ７六歩(77)\n2 ３四歩(33)\n") << two;
        QTest::newRow("ki2") << QStringLiteral("▲７六歩 △３四歩\n") << two;
        QTest::newRow("csa") << QStringLiteral("V2.2\nPI\n+\n+7776FU\n-3334FU\n") << two;
        QTest::newRow("jkf") << QStringLiteral(R"({"header":{},"moves":[{},)"
            R"({"move":{"from":{"x":7,"y":7},"to":{"x":7,"y":6},"piece":"FU","color":0}},)"
            R"({"move":{"from":{"x":3,"y":3},"to":{"x":3,"y":4},"piece":"FU","color":1}}]})") << two;
        QTest::newRow("usi") << QStringLiteral("position startpos moves 7g7f 3c3d") << two;
        QTest::newRow("usen") << QStringLiteral("~0.7ku2jm.") << two;
    }

    void parseTextEveryFormat()
    {
        QFETCH(QString, text);
        QFETCH(QStringList, moves);
        const auto loaded = KifuLoadParser::parseText(text);
        QVERIFY2(loaded.success, qPrintable(loaded.error + loaded.warning));
        QVERIFY(loaded.filePath.isEmpty());
        QCOMPARE(loaded.record.mainline.usiMoves(), moves);
    }

    // テキストの読み込みに失敗したときのメッセージに一時ファイル名を出さない
    void parseTextFailureHasNoFileName()
    {
        const auto loaded = KifuLoadParser::parseText(QStringLiteral(R"({"moves":42})"));
        QVERIFY(!loaded.success);
        QVERIFY2(!loaded.error.contains(QStringLiteral("shogi_paste")), qPrintable(loaded.error));
        QVERIFY2(!loaded.error.contains(QStringLiteral(".jkf")), qPrintable(loaded.error));
    }

    // ファイルは1回読んでテキスト版に渡す（同じ内容なら同じ結果）
    void parseFileMatchesParseContent()
    {
        const QString text = QStringLiteral("手合割：平手\n先手：A\n手数----指手---------消費時間--\n"
            "1 ７六歩(77)\n2 ３四歩(33)\n3 投了\n\n変化：2手\n2 ８四歩(83)\n");
        QTemporaryFile file(QDir::tempPath() + QStringLiteral("/parse_XXXXXX.kif"));
        QVERIFY(file.open());
        file.write(text.toUtf8());
        file.close();
        const auto fromFile = KifuLoadParser::parseFile(file.fileName());
        const auto fromText = KifuLoadParser::parseContent(text, KifuFormat::Kif);
        QVERIFY2(fromFile.success, qPrintable(fromFile.error));
        QVERIFY2(fromText.success, qPrintable(fromText.error));
        QCOMPARE(fromFile.filePath, file.fileName());
        QCOMPARE(fromText.record.mainline.usiMoves(), fromFile.record.mainline.usiMoves());
        QCOMPARE(fromText.record.variations.size(), 1);
        QCOMPARE(fromText.gameInfo.size(), fromFile.gameInfo.size());
        QCOMPARE(fromText.initialSfen, fromFile.initialSfen);
    }
};

QTEST_GUILESS_MAIN(TestKifuFormatCompliance)
#include "tst_kifu_format_compliance.moc"
