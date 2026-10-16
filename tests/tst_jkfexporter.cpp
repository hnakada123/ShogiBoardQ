/// @file tst_jkfexporter.cpp
/// @brief JKF の書き出し（分岐ツリーのノードの指し手と局面から作る）と、読み込みとの往復のテスト

#include <QtTest>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryFile>

#include "kifu_file_helper.h"
#include "kifuexport.h"
#include "jkftosfenconverter.h"
#include "kifparsetypes.h"
#include "kifu_test_helper.h"
#include "kifubranchnode.h"
#include "kifubranchtree.h"
#include "sfenutils.h"
#include "test_replay.h"

namespace {

/// 指し手を足す。表示の文字列には USI をそのまま入れる（書き出しが表示の文字列を使わないことも確かめる）
KifuBranchNode* addMove(KifuBranchTree& tree, KifuBranchNode* parent, const QString& usi)
{
    const QStringList positions = TestReplay::buildSfenRecord(parent->sfen(), {usi}, false);
    const QList<ShogiMove> moves = TestReplay::buildGameMoves(parent->sfen(), {usi});
    return tree.addMove(parent, moves.value(0), usi, positions.last());
}

QByteArray exportJkf(KifuBranchTree& tree, const QString& startSfen)
{
    KifuExportMetadata ctx;
    ctx.startSfen = startSfen;
    return KifuExport::lines(tree, ctx, KifuFormat::Jkf).join(QLatin1Char('\n')).toUtf8();
}

bool readJkf(const QByteArray& json, KifParseResult& result, QString* error)
{
    QTemporaryFile file;
    if (!KifuTestHelper::writeToTempFile(file, json, QStringLiteral("jkf"))) return false;
    return KifuFile::parse<JkfToSfenConverter>(file.fileName(), result, error);
}

QString jsonText(const QJsonValue& value)
{
    return QString::fromUtf8(value.isArray() ? QJsonDocument(value.toArray()).toJson(QJsonDocument::Compact)
                                             : QJsonDocument(value.toObject()).toJson(QJsonDocument::Compact));
}

QJsonObject square(int file, int rank)
{
    return QJsonObject{{QStringLiteral("x"), file}, {QStringLiteral("y"), rank}};
}

/// 盤上の駒を動かす手の JKF の move
QJsonObject boardMove(int color, int fromFile, int fromRank, int toFile, int toRank, const QString& piece,
                      const QJsonObject& extra = {})
{
    QJsonObject move{{QStringLiteral("color"), color}, {QStringLiteral("from"), square(fromFile, fromRank)},
                     {QStringLiteral("to"), square(toFile, toRank)}, {QStringLiteral("piece"), piece}};
    for (auto it = extra.begin(); it != extra.end(); ++it) move.insert(it.key(), it.value());
    return move;
}

/// 駒を打つ手の JKF の move
QJsonObject dropMove(int color, int toFile, int toRank, const QString& piece, const QJsonObject& extra = {})
{
    QJsonObject move{{QStringLiteral("color"), color}, {QStringLiteral("to"), square(toFile, toRank)},
                     {QStringLiteral("piece"), piece}};
    for (auto it = extra.begin(); it != extra.end(); ++it) move.insert(it.key(), it.value());
    return move;
}

QJsonObject moveOf(const QJsonValue& entry)
{
    return entry.toObject().value(QStringLiteral("move")).toObject();
}

} // namespace

class TestJkfExporter : public QObject
{
    Q_OBJECT

private slots:
    void terminalExportIgnoresDisplayText_data()
    {
        QTest::addColumn<int>("terminal");
        QTest::addColumn<bool>("white");
        for (int type = int(TerminalType::Resign); type <= int(TerminalType::ForfeitWin); ++type) {
            QTest::newRow(qPrintable(QStringLiteral("black-%1").arg(type))) << type << false;
            QTest::newRow(qPrintable(QStringLiteral("white-%1").arg(type))) << type << true;
        }
    }
    void terminalExportIgnoresDisplayText()
    {
        QFETCH(int, terminal);
        QFETCH(bool, white);
        KifuBranchTree tree;
        tree.setRootSfen(SfenUtils::hirateSfen());
        auto* node = tree.addRecord(tree.root(), RecordEntry::ending(static_cast<TerminalType>(terminal),
                                                   white ? Turn::White : Turn::Black));
        QVERIFY(node);
        KifuExportMetadata ctx;
        ctx.startSfen = tree.root()->sfen();
        const auto exportAll = [&] {
            return QList<QStringList>{KifuExport::lines(tree, ctx, KifuFormat::Kif), KifuExport::lines(tree, ctx, KifuFormat::Ki2), KifuExport::lines(tree, ctx, KifuFormat::Csa),
                KifuExport::lines(tree, ctx, KifuFormat::Jkf), KifuExport::lines(tree, ctx, KifuFormat::Usi), KifuExport::lines(tree, ctx, KifuFormat::Usen)};
        };
        const auto original = exportAll();
        node->record().displayText = QStringLiteral("display text has no result or side");
        QCOMPARE(exportAll(), original);
        node->record().displayText = {};
        QCOMPARE(exportAll(), original);
        if (static_cast<TerminalType>(terminal) == TerminalType::IllegalWin) {
            QVERIFY(KifuExport::lines(tree, ctx, KifuFormat::Csa).contains(white ? QStringLiteral("%+ILLEGAL_ACTION")
                                                         : QStringLiteral("%-ILLEGAL_ACTION")));
            QVERIFY(KifuExport::lines(tree, ctx, KifuFormat::Ki2).contains(white ? QStringLiteral("まで0手で後手の反則勝ち")
                                                       : QStringLiteral("まで0手で先手の反則勝ち")));
        }
    }

    /// 成・不成・打・同・取る手・修飾語と、変化（入れ子を含む）を、局面と指し手から書き、読み込むと同じ手順に戻る
    void movesRoundTripWithVariations()
    {
        // 後手：玉5一・金5二・歩4三、持駒 金。先手：銀4四・角8八・金7八と5八・玉5九
        const QString initial = QStringLiteral("4k4/4g4/5p3/5S3/9/9/9/1BG1G4/4K4 b g 1");
        KifuBranchTree tree;
        tree.setRootSfen(initial);
        const QStringList mainUsi = {"4d4c", "5b4c", "5h6h", "G*3c", "8h3c+", "4c3c", "P*3d"};
        QList<KifuBranchNode*> mainNodes{tree.root()};
        for (const QString& usi : mainUsi) mainNodes.append(addMove(tree, mainNodes.last(), usi));
        tree.addTerminalMove(mainNodes.last(), TerminalType::Resign, Turn::White);

        // 1手目の変化（銀成）と、その中の2手目の変化（玉）
        auto* silverPromotes = addMove(tree, mainNodes.at(0), QStringLiteral("4d4c+"));
        addMove(tree, silverPromotes, QStringLiteral("5b4c"));
        auto* kingEscapes = addMove(tree, silverPromotes, QStringLiteral("5a6b"));
        // 3手目の変化（左の金）
        addMove(tree, mainNodes.at(2), QStringLiteral("7h6h"));

        const QByteArray json = exportJkf(tree, initial);
        const QJsonObject root = QJsonDocument::fromJson(json).object();
        QCOMPARE(root.value(QStringLiteral("initial")).toObject().value(QStringLiteral("preset")).toString(),
                 QStringLiteral("OTHER"));
        const QJsonArray moves = root.value(QStringLiteral("moves")).toArray();
        QCOMPARE(moves.size(), 9);
        QCOMPARE(jsonText(moves.at(0)), QStringLiteral("{}"));

        const QJsonValue t(true);
        const QJsonValue f(false);
        // ▲４三銀不成：成れるのに成らない手は promote:false。取った駒は capture
        QCOMPARE(jsonText(moveOf(moves.at(1))), jsonText(boardMove(0, 4, 4, 4, 3, "GI",
            {{"capture", "FU"}, {"promote", f}})));
        // △同　金：same と、取った銀
        QCOMPARE(jsonText(moveOf(moves.at(2))), jsonText(boardMove(1, 5, 2, 4, 3, "KI",
            {{"same", t}, {"capture", "GI"}})));
        // ▲６八金右：金2枚が6八へ動けるので relative。金は成れないので promote を書かない
        QCOMPARE(jsonText(moveOf(moves.at(3))), jsonText(boardMove(0, 5, 8, 6, 8, "KI", {{"relative", "R"}})));
        // △３三金打：4三の金も3三へ動けるので relative は H
        QCOMPARE(jsonText(moveOf(moves.at(4))), jsonText(dropMove(1, 3, 3, "KI", {{"relative", "H"}})));
        // ▲同　角成
        QCOMPARE(jsonText(moveOf(moves.at(5))), jsonText(boardMove(0, 8, 8, 3, 3, "KA",
            {{"same", t}, {"capture", "KI"}, {"promote", t}})));
        // △同　金：取った駒は成駒の種類（馬）
        QCOMPARE(jsonText(moveOf(moves.at(6))), jsonText(boardMove(1, 4, 3, 3, 3, "KI",
            {{"same", t}, {"capture", "UM"}})));
        // ▲３四歩打：盤上の歩は3四へ動けないので relative を書かない
        QCOMPARE(jsonText(moveOf(moves.at(7))), jsonText(dropMove(0, 3, 4, "FU")));
        QCOMPARE(moves.at(8).toObject().value(QStringLiteral("special")).toString(), QStringLiteral("TORYO"));

        // forks は代わられる手の要素に付く
        const QJsonArray firstForks = moves.at(1).toObject().value(QStringLiteral("forks")).toArray();
        QCOMPARE(firstForks.size(), 1);
        const QJsonArray promoteLine = firstForks.at(0).toArray();
        QCOMPARE(promoteLine.size(), 2);
        QCOMPARE(jsonText(moveOf(promoteLine.at(0))), jsonText(boardMove(0, 4, 4, 4, 3, "GI",
            {{"capture", "FU"}, {"promote", t}})));
        QCOMPARE(jsonText(moveOf(promoteLine.at(1))), jsonText(boardMove(1, 5, 2, 4, 3, "KI",
            {{"same", t}, {"capture", "NG"}})));
        const QJsonArray nestedForks = promoteLine.at(1).toObject().value(QStringLiteral("forks")).toArray();
        QCOMPARE(nestedForks.size(), 1);
        QCOMPARE(jsonText(nestedForks.at(0)),
                 jsonText(QJsonArray{QJsonObject{{"move", boardMove(1, 5, 1, 6, 2, "OU")}}}));
        const QJsonArray thirdForks = moves.at(3).toObject().value(QStringLiteral("forks")).toArray();
        QCOMPARE(jsonText(thirdForks),
                 jsonText(QJsonArray{QJsonArray{QJsonObject{{"move", boardMove(0, 7, 8, 6, 8, "KI",
                                                                                {{"relative", "L"}})}}}}));
        for (int i : {2, 4, 5, 6, 7, 8}) QVERIFY(!moves.at(i).toObject().contains(QStringLiteral("forks")));

        // 読み込むと、同じ手順・変化・局面に戻り、表示は同・修飾語・打・成・不成を含む
        KifParseResult result;
        QString error;
        QVERIFY2(readJkf(json, result, &error), qPrintable(error));
        QCOMPARE(result.mainline.startSfen, initial);
        QCOMPARE(result.mainline.usiMoves(), mainUsi);
        QCOMPARE(result.mainline.sfens().last(), mainNodes.last()->sfen());
        QStringList pretty;
        for (const RecordEntry& entry : std::as_const(result.mainline.entries)) pretty << entry.displayText;
        QCOMPARE(pretty, QStringList({QStringLiteral("▲４三銀不成(44)"), QStringLiteral("△同　金(52)"),
                                            QStringLiteral("▲６八金右(58)"), QStringLiteral("△３三金打"),
                                            QStringLiteral("▲同　角成(88)"), QStringLiteral("△同　金(43)"),
                                            QStringLiteral("▲３四歩打"), QStringLiteral("△投了")}));
        QVERIFY(result.mainline.endsWithTerminal());

        QCOMPARE(result.variations.size(), 3);
        QCOMPARE(result.variations.at(0).startPly, 1);
        QCOMPARE(result.variations.at(0).line.usiMoves(), QStringList({"4d4c+", "5b4c"}));
        QCOMPARE(result.variations.at(0).line.entries.at(0).displayText, QStringLiteral("▲４三銀成(44)"));
        QCOMPARE(result.variations.at(0).line.entries.at(1).displayText, QStringLiteral("△同　金(52)"));
        QCOMPARE(result.variations.at(1).startPly, 2);
        QCOMPARE(result.variations.at(1).line.usiMoves(), QStringList({"5a6b"}));
        QCOMPARE(result.variations.at(1).line.startSfen, silverPromotes->sfen());
        QCOMPARE(result.variations.at(1).line.sfens().last(), kingEscapes->sfen());
        QCOMPARE(result.variations.at(2).startPly, 3);
        QCOMPARE(result.variations.at(2).line.usiMoves(), QStringList({"7h6h"}));
        QCOMPARE(result.variations.at(2).line.entries.at(0).displayText, QStringLiteral("▲６八金左(78)"));
    }

    /// 終局は分岐ツリーの終局の種類から special にする（反則勝ちは手番で符号が決まる）
    void terminalSpecials_data()
    {
        QTest::addColumn<int>("plies");
        QTest::addColumn<QString>("word");
        QTest::addColumn<QString>("special");
        const struct { int plies; const char16_t* word; const char* special; } rows[] = {
            {1, u"投了", "TORYO"}, {1, u"詰み", "TSUMI"}, {1, u"千日手", "SENNICHITE"},
            {1, u"王手千日手", "OUTE_SENNICHITE"}, {1, u"持将棋", "JISHOGI"}, {1, u"切れ負け", "TIME_UP"},
            {1, u"時間切れ", "TIME_UP"}, {1, u"反則負け", "ILLEGAL_MOVE"},
            // 手番側の反則勝ちは、相手の反則
            {0, u"反則勝ち", "-ILLEGAL_ACTION"}, {1, u"反則勝ち", "+ILLEGAL_ACTION"},
            {1, u"中断", "CHUDAN"}, {1, u"不詰", "FUZUMI"}, {1, u"入玉勝ち", "KACHI"},
            {1, u"引き分け", "HIKIWAKE"}, {1, u"最大手数到達", "MAX_MOVES"}, {1, u"エラー", "ERROR"},
            // JKF に無い終局は書かない
            {1, u"不戦敗", ""},
        };
        for (const auto& row : rows) {
            const QString word = QString::fromUtf16(row.word);
            QTest::newRow(qPrintable(QStringLiteral("%1-%2").arg(word).arg(row.plies)))
                << row.plies << word << QString::fromLatin1(row.special);
        }
    }

    void terminalSpecials()
    {
        QFETCH(int, plies);
        QFETCH(QString, word);
        QFETCH(QString, special);
        KifuBranchTree tree;
        tree.setRootSfen(SfenUtils::hirateSfen());
        KifuBranchNode* tip = tree.root();
        if (plies > 0) tip = addMove(tree, tip, QStringLiteral("7g7f"));
        tree.addTerminalMove(tip, TerminalTypes::fromWord(word), plies % 2 == 0 ? Turn::Black : Turn::White);

        const QByteArray json = exportJkf(tree, SfenUtils::hirateSfen());
        const QJsonArray moves = QJsonDocument::fromJson(json).object().value(QStringLiteral("moves")).toArray();
        KifParseResult result;
        QString error;
        QVERIFY2(readJkf(json, result, &error), qPrintable(error));
        if (special.isEmpty()) {
            QCOMPARE(moves.size(), plies + 1);
            QVERIFY(!result.mainline.endsWithTerminal());
            return;
        }
        QCOMPARE(moves.size(), plies + 2);
        QCOMPARE(moves.last().toObject().value(QStringLiteral("special")).toString(), special);
        // 読み込むと同じ種類の終局になる（反則勝ち・負けは手番の印も同じ）
        const RecordEntry reread = result.mainline.entries.last();
        QCOMPARE(reread.terminal, TerminalTypes::fromWord(word));
        QCOMPARE(reread.mover, plies % 2 == 0 ? Turn::Black : Turn::White);
    }

    /// 手合割の名前（preset）は、盤・手番・持ち駒が手合割の開始局面と同じときだけ書く
    void initialPreset_data()
    {
        QTest::addColumn<QString>("sfen");
        QTest::addColumn<QString>("preset");
        const QString hirate = SfenUtils::hirateSfen();
        QTest::newRow("hirate") << hirate << QStringLiteral("HIRATE");
        QTest::newRow("six-pieces") << QStringLiteral("2sgkgs2/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1")
                                    << QStringLiteral("6");
        // 平手の配置でも後手から指す局面は平手ではない
        QTest::newRow("hirate-white-to-move") << QString(hirate).replace(QStringLiteral(" b "), QStringLiteral(" w "))
                                              << QStringLiteral("OTHER");
        // 香落ちの配置で下手から指す局面
        QTest::newRow("lance-black-to-move")
            << QStringLiteral("lnsgkgsn1/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1")
            << QStringLiteral("OTHER");
        QTest::newRow("custom") << QStringLiteral("4k4/9/9/9/4p4/4+S4/9/9/4K4 w 2Pr 1") << QStringLiteral("OTHER");
    }

    void initialPreset()
    {
        QFETCH(QString, sfen);
        QFETCH(QString, preset);
        KifuBranchTree tree;
        tree.setRootSfen(sfen);
        const QByteArray json = exportJkf(tree, sfen);
        const QJsonObject initial = QJsonDocument::fromJson(json).object().value(QStringLiteral("initial")).toObject();
        QCOMPARE(initial.value(QStringLiteral("preset")).toString(), preset);
        QCOMPARE(initial.contains(QStringLiteral("data")), preset == QStringLiteral("OTHER"));

        KifParseResult result;
        QString error;
        QVERIFY2(readJkf(json, result, &error), qPrintable(error));
        QCOMPARE(SfenUtils::normalizeSfenKey(result.mainline.startSfen), SfenUtils::normalizeSfenKey(sfen));
    }
};

QTEST_GUILESS_MAIN(TestJkfExporter)
#include "tst_jkfexporter.moc"
