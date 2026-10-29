#include <QtTest>
#include "shogiposition.h"
#include <QSignalSpy>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QClipboard>
#include "kifu_file_helper.h"
#include "bodtextgenerator.h"
#include "csaformatter.h"

#include "gameinfokeys.h"
#include "kifudocument.h"
#include "timecontroltext.h"
#include "kifuexport.h"
#include "kifubranchtree.h"
#include "branchtree_test_util.h"
#include "kifubranchnode.h"
#include "kifunavigationstate.h"
#include "handicaps.h"
#include "kiftextparser.h"
#include "csatosfenconverter.h"
#include "jkftosfenconverter.h"
#include "usitosfenconverter.h"
#include "usentosfenconverter.h"
#include "livegamesession.h"
#include "kifufilecontroller.h"
#include "kifuloader.h"
#include "shogiboard.h"
#include "shogigamecontroller.h"
#include "matchtimerecord.h"
#include "kifunavigationstate.h"
#include "kifuexportmetadata.h"
#include "kifu_test_helper.h"
#include "kifuloadparser.h"
#include "kifubranchtreebuilder.h"
#include "test_replay.h"

static const QString kHirateSfen =
    QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");

/// 棋譜の文書とファイル・コピーの部品（ダイアログを出さない操作だけを使う）
struct FileHarness {
    FileHarness(KifuBranchTree* tree, const KifuNavigationState* nav)
        : document(tree)
        , loader(&document)
    {
        KifuFileController::Deps deps;
        deps.document = &document;
        deps.loader = &loader;
        deps.gameController = &gc;
        deps.navState = nav;
        controller = std::make_unique<KifuFileController>(deps);
    }

    KifuDocument document;
    KifuLoader loader;
    ShogiGameController gc;
    std::unique_ptr<KifuFileController> controller;
};

class TestKifuDocument : public QObject
{
    Q_OBJECT

private:
    static KifuBranchNode* addTestMove(KifuBranchTree& tree, KifuBranchNode* parent,
                                       const QString& usi, const QString& pretty,
                                       const QString& time = {})
    {
        const QStringList positions = TestReplay::buildSfenRecord(parent->sfen(), {usi}, false);
        const QList<ShogiMove> moves = TestReplay::buildGameMoves(parent->sfen(), {usi});
        return TestTree::addMove(tree, parent, moves.value(0), pretty, positions.last(), time);
    }

    /// 7手の本譜（指し手は表記だけ）を作り、モデルに渡す
    static void setupBasicTree(KifuBranchTree& tree)
    {
        tree.setRootSfen(kHirateSfen);
        ShogiMove move;
        auto* current = tree.root();
        const QStringList moves = {
            QStringLiteral("▲７六歩"), QStringLiteral("△３四歩"),
            QStringLiteral("▲２六歩"), QStringLiteral("△８四歩"),
            QStringLiteral("▲２五歩"), QStringLiteral("△８五歩"),
            QStringLiteral("▲７八金")
        };
        for (qsizetype i = 0; i < moves.size(); ++i) {
            current = TestTree::addMove(tree, current, move, moves.at(i), QStringLiteral("sfen%1").arg(i + 1));
        }
    }

private slots:
    /// 後手から指す棋譜（駒落ち・局面指定）を読んでも、分岐ツリーの各手が USI の指し手になる
    void whiteFirstRecordTreeHasUsiMoves_data()
    {
        QTest::addColumn<QString>("text");
        QTest::addColumn<QString>("moves");
        QTest::newRow("csa-handicap") << QString("V2.2\nPI82HI22KA,-,-3334FU,T2\n") << QString("3c3d");
        QTest::newRow("jkf-white-first") << QString(R"({"initial":{"preset":"6"},"moves":[{},
            {"move":{"color":1,"piece":"OU","from":{"x":5,"y":1},"to":{"x":4,"y":2},"promote":null}}]})")
            << QString("5a4b");
    }
    void whiteFirstRecordTreeHasUsiMoves()
    {
        QFETCH(QString, text);
        QFETCH(QString, moves);
        const KifuLoadResult loaded = KifuLoadParser::parseText(text);
        QVERIFY2(loaded.success, qPrintable(loaded.error));
        KifuBranchTree tree;
        KifuBranchTreeBuilder::buildFromKifParseResult(&tree, loaded.record);
        QCOMPARE(tree.mainRecordLine().usiMoves().join(QLatin1Char(' ')), moves);
    }

    void allFormatsRoundTripCustomPosition_data()
    {
        QTest::addColumn<QString>("format");
        for (const auto& format : {"kif", "ki2", "csa", "jkf", "usi", "usen"})
            QTest::newRow(format) << QString::fromLatin1(format);
    }

    /// 開始日時・終了日時が記録にない棋譜を CSA にしても、変換時刻を対局日時として書かない
    void csaDoesNotInventGameTimes()
    {
        const QStringList usi = {"7g7f", "3c3d"};
        const QStringList pretty = {QStringLiteral("▲７六歩(77)"), QStringLiteral("△３四歩(33)")};
        KifuBranchTree tree;
        tree.setRootSfen(kHirateSfen);
        auto* tip = tree.root();
        for (int i = 0; i < usi.size(); ++i) tip = addTestMove(tree, tip, usi[i], pretty[i]);
        KifuExportMetadata context;
        context.startSfen = kHirateSfen;
        context.gameInfoItems = {{QStringLiteral("先手"), QStringLiteral("鈴木")},
                                 {QStringLiteral("後手"), QStringLiteral("山田")}};
        const QString csa = KifuExport::lines(tree, context, KifuFormat::Csa).join(QLatin1Char('\n'));
        QVERIFY2(!csa.contains(QStringLiteral("$START_TIME")), qPrintable(csa));
        QVERIFY2(!csa.contains(QStringLiteral("$END_TIME")), qPrintable(csa));

        // 対局情報の開始日時は CSA でも書く
        context.gameInfoItems.append({QStringLiteral("開始日時"), QStringLiteral("2026/10/05 09:30:00")});
        const QString live = KifuExport::lines(tree, context, KifuFormat::Csa).join(QLatin1Char('\n'));
        QVERIFY2(live.contains(QStringLiteral("$START_TIME:2026/10/05 09:30:00")), qPrintable(live));
        QVERIFY(!live.contains(QStringLiteral("$END_TIME")));
    }

    /// CSA の開始日時・持ち時間も KIF と同じく対局情報から書く。対局の後に直した持ち時間を書き、
    /// 途中から続けた対局（対局情報は元の棋譜のまま）では続けた対局の日時・持ち時間を足さない（監査 F3）
    void csaWritesTimesFromGameInfo()
    {
        KifuBranchTree tree;
        tree.setRootSfen(kHirateSfen);
        KifuDocument document(&tree);
        MatchPlayers players;
        players.seat(Turn::Black).humanName = QStringLiteral("太郎");
        players.seat(Turn::White).humanName = QStringLiteral("花子");
        document.beginMatch(players, TimeControl{{60000, 2000, 0}, {60000, 2000, 0}}, false);
        QVERIFY(KifuExport::lines(document, KifuFormat::Csa).contains(QStringLiteral("$TIME:60+2+0")));

        QList<KifGameInfoItem> edited = document.header();
        for (KifGameInfoItem& item : edited)
            if (item.key == GameInfoKeys::kTimeControl) item.value = QStringLiteral("10:00+10");
        document.editHeader(edited);
        QVERIFY(KifuExport::lines(document, KifuFormat::Kif).contains(QStringLiteral("持ち時間：10:00+10")));
        const QString csa = KifuExport::lines(document, KifuFormat::Csa).join(QLatin1Char('\n'));
        QVERIFY2(csa.contains(QStringLiteral("$TIME:600+10+0")), qPrintable(csa));
        QVERIFY(!csa.contains(QStringLiteral("$TIME:60+2+0")));

        // 読み込んだ棋譜（開始日時・持ち時間なし）の途中から続けた対局
        document.startNew(kHirateSfen);
        document.editHeader({{GameInfoKeys::kBlackPlayer, QStringLiteral("A")}, {GameInfoKeys::kWhitePlayer, QStringLiteral("B")}});
        document.beginMatch(players, TimeControl{{60000, 2000, 0}, {60000, 2000, 0}}, true);
        const QString continued = KifuExport::lines(document, KifuFormat::Csa).join(QLatin1Char('\n'));
        QVERIFY2(!continued.contains(QStringLiteral("$START_TIME")), qPrintable(continued));
        QVERIFY(!continued.contains(QStringLiteral("$TIME")));
    }

    /// 対局情報・コメント・しおりの改行（CR だけも）は、書き出した棋譜で別の行にならない（セキュリティ監査 S9）
    void lineBreaksInValuesDoNotAddLines()
    {
        KifuBranchTree tree;
        tree.setRootSfen(kHirateSfen);
        KifuBranchNode* move = addTestMove(tree, tree.root(), QStringLiteral("7g7f"), QStringLiteral("▲７六歩(77)"));
        move->record().comment = QStringLiteral("一行目\r手合割：香落ち\n三行目");
        move->record().bookmark = QStringLiteral("しおり\r+2726FU");
        KifuExportMetadata context;
        context.startSfen = kHirateSfen;
        context.gameInfoItems = {{QStringLiteral("先手"), QStringLiteral("Alice\r手合割：香落ち")},
                                 {QStringLiteral("後手"), QStringLiteral("Bob\nPI82HI22KA")},
                                 {QStringLiteral("棋戦\r手合割"), QStringLiteral("x")}};
        for (const KifuFormat format : {KifuFormat::Kif, KifuFormat::Ki2, KifuFormat::Csa}) {
            const QString text = KifuExport::lines(tree, context, format).join(QLatin1Char('\n'));
            QVERIFY2(!text.contains(QLatin1Char('\r')), qPrintable(text));
            const QStringList lines = text.split(QLatin1Char('\n'));
            for (const QString& line : lines) {
                QVERIFY2(!line.startsWith(QStringLiteral("手合割")), qPrintable(text));
                QVERIFY2(!line.startsWith(QStringLiteral("PI82")), qPrintable(text));
                QVERIFY2(!line.startsWith(QStringLiteral("+2726FU")), qPrintable(text));
            }
            // 読み直しても開始局面は平手のまま
            const KifuParseOutcome outcome = format == KifuFormat::Kif ? KifuFile::parseText<KifuFormat::Kif>(text, {})
                : format == KifuFormat::Ki2 ? KifuFile::parseText<KifuFormat::Ki2>(text, {})
                : KifuFile::parseText<KifuFormat::Csa>(text, {});
            QVERIFY2(outcome.success, qPrintable(outcome.warning));
            QCOMPARE(outcome.record.mainline.startSfen, kHirateSfen);
            QCOMPARE(outcome.record.mainline.usiMoves(), QStringList({QStringLiteral("7g7f")}));
        }
    }

    /// 持ち時間「無制限」は CSA の時間行を書かない（$TIME_LIMIT:無制限 にしない）
    void csaOmitsTimeForUnlimitedGame()
    {
        const QStringList usi = {"7g7f"};
        KifuBranchTree tree;
        tree.setRootSfen(kHirateSfen);
        addTestMove(tree, tree.root(), usi.first(), QStringLiteral("▲７六歩(77)"));
        KifuExportMetadata context;
        context.startSfen = kHirateSfen;
        context.gameInfoItems = {{QStringLiteral("持ち時間"), QStringLiteral("無制限")}};
        const QString csa = KifuExport::lines(tree, context, KifuFormat::Csa).join(QLatin1Char('\n'));
        QVERIFY2(!csa.contains(QStringLiteral("$TIME")), qPrintable(csa));
        QVERIFY2(csa.contains(QStringLiteral("+7776FU")), qPrintable(csa));

        context.gameInfoItems = {{QStringLiteral("持ち時間"), QStringLiteral("10:00+10")}};
        const QString timed = KifuExport::lines(tree, context, KifuFormat::Csa).join(QLatin1Char('\n'));
        QVERIFY2(timed.contains(QStringLiteral("$TIME:600+10+0")), qPrintable(timed));
    }

    /// KI2 の書き出しで、局面の上で指せない手（読み込んだ棋譜の反則手の後など）は、指し手の値から
    /// 移動元を書かない表記にする（棋譜の表記の文字列を削って作らない）
    void ki2UnplayableMoveUsesMoveValue()
    {
        KifuBranchTree tree;
        tree.setRootSfen(kHirateSfen);
        KifuBranchNode* first = addTestMove(tree, tree.root(), QStringLiteral("7g7f"), QStringLiteral("▲７六歩(77)"));
        // 5五に後手の駒はない。表記の文字列は使わないことを確かめるため、指し手と違う文字列にしておく
        const ShogiMove ghost(*UsiMove::parse(u"5e5f"), Piece::WhitePawn);
        TestTree::addMove(tree, first, ghost, QStringLiteral("（読めない手）"), first->sfen());
        KifuExportMetadata context;
        context.startSfen = kHirateSfen;
        const QString text = KifuExport::lines(tree, context, KifuFormat::Ki2).join(QLatin1Char('\n'));
        QVERIFY2(text.contains(QStringLiteral("△５六歩")), qPrintable(text));
        QVERIFY2(!text.contains(QStringLiteral("(55)")) && !text.contains(QStringLiteral("読めない")), qPrintable(text));
    }

    /// KI2 では、盤上の同じ駒もその地点へ動ける場合だけ「打」を付ける（日本将棋連盟の棋譜表記）
    void ki2DropMarkerOnlyWhenAmbiguous()
    {
        const QString initial = QStringLiteral("4k4/9/9/9/9/9/9/4G4/4K4 b 2G 1");
        const QStringList usi = {"G*5g", "5a4a", "G*1e"};
        const QStringList pretty = {QStringLiteral("▲５七金打"), QStringLiteral("△４一玉(51)"),
                                    QStringLiteral("▲１五金打")};
        KifuBranchTree tree;
        tree.setRootSfen(initial);
        auto* tip = tree.root();
        for (int i = 0; i < usi.size(); ++i) tip = addTestMove(tree, tip, usi[i], pretty[i]);
        KifuExportMetadata context;
        context.startSfen = initial;
        const QStringList lines = KifuExport::lines(tree, context, KifuFormat::Ki2);
        const QString text = lines.join(QLatin1Char('\n'));
        QVERIFY2(text.contains(QStringLiteral("▲５七金打")), qPrintable(text));  // 5八の金も5七へ動ける
        QVERIFY2(text.contains(QStringLiteral("▲１五金")), qPrintable(text));
        QVERIFY2(!text.contains(QStringLiteral("▲１五金打")), qPrintable(text)); // 盤上の金は1五へ動けない

        // 「打」を省いても、読み込むと同じ手順に戻る
        QTemporaryFile file;
        QVERIFY(KifuTestHelper::writeToTempFile(file, text.toUtf8(), QStringLiteral("ki2")));
        KifParseResult result;
        QString error;
        QVERIFY2(KifuFile::parse<KifuFormat::Ki2>(file.fileName(), result, &error), qPrintable(error));
        QCOMPARE(result.mainline.usiMoves(), usi);
    }

    void allFormatsRoundTripCustomPosition()
    {
        QFETCH(QString, format);
        const QString initial = QStringLiteral("4k4/9/9/9/4p4/4+S4/9/9/4K4 w Pp 1");
        const QStringList usi = {"P*3d", "5f5e", "5a4a", "P*7f"};
        const QStringList pretty = {QStringLiteral("△３四歩打"), QStringLiteral("▲５五成銀(56)"),
                                    QStringLiteral("△４一玉(51)"), QStringLiteral("▲７六歩打")};
        KifuBranchTree tree;
        tree.setRootSfen(initial);
        auto* tip = tree.root();
        for (int i = 0; i < usi.size(); ++i) tip = addTestMove(tree, tip, usi[i], pretty[i]);
        KifuExportMetadata context;
        context.startSfen = initial;
        QStringList output;
        if (format == "kif") output = KifuExport::lines(tree, context, KifuFormat::Kif);
        else if (format == "ki2") output = KifuExport::lines(tree, context, KifuFormat::Ki2);
        else if (format == "csa") output = KifuExport::lines(tree, context, KifuFormat::Csa);
        else if (format == "jkf") output = KifuExport::lines(tree, context, KifuFormat::Jkf);
        else if (format == "usi") output = KifuExport::lines(tree, context, KifuFormat::Usi);
        else output = KifuExport::lines(tree, context, KifuFormat::Usen);
        QTemporaryFile file;
        QVERIFY(KifuTestHelper::writeToTempFile(file, output.join(QLatin1Char('\n')).toUtf8(), format));
        KifParseResult result;
        QString error;
        bool parsed = false;
        if (format == "kif") parsed = KifuFile::parse<KifuFormat::Kif>(file.fileName(), result, &error);
        else if (format == "ki2") parsed = KifuFile::parse<KifuFormat::Ki2>(file.fileName(), result, &error);
        else if (format == "csa") parsed = KifuFile::parse<KifuFormat::Csa>(file.fileName(), result, &error);
        else if (format == "jkf") parsed = KifuFile::parse<KifuFormat::Jkf>(file.fileName(), result, &error);
        else if (format == "usi") parsed = KifuFile::parse<KifuFormat::Usi>(file.fileName(), result, &error);
        else parsed = KifuFile::parse<KifuFormat::Usen>(file.fileName(), result, &error);
        QVERIFY2(parsed, qPrintable(error));
        QCOMPARE(result.mainline.startSfen, initial);
        QCOMPARE(result.mainline.usiMoves(), usi);
        QCOMPARE(TestReplay::buildSfenRecord(result.mainline.startSfen, result.mainline.usiMoves(), false).last(), tip->sfen());
    }

    void kifBodUsesDeclaredTurn_data()
    {
        QTest::addColumn<bool>("withBod");
        QTest::addColumn<bool>("handicapNames");
        QTest::newRow("bod") << true << false;
        QTest::newRow("header-only") << false << false;
        // 駒落ちの呼び方（上手の持駒・上手番）で書いた局面図と手番
        QTest::newRow("bod-uwate") << true << true;
        QTest::newRow("header-only-uwate") << false << true;
    }

    void kifBodUsesDeclaredTurn()
    {
        QFETCH(bool, withBod);
        QFETCH(bool, handicapNames);
        QString initial = kHirateSfen;
        initial.replace(QStringLiteral(" b "), QStringLiteral(" w "));
        const QString turnLine = handicapNames ? QStringLiteral("上手番") : QStringLiteral("後手番");
        const QString text = (withBod ? BodTextGenerator::generate(*Position::fromSfen(initial), 0, {}, handicapNames) : turnLine)
            + QStringLiteral("\n手数----指手---------消費時間--\n1 ３四歩(33)\n2 投了\n");
        QTemporaryFile file;
        QVERIFY(KifuTestHelper::writeToTempFile(file, text.toUtf8(), QStringLiteral("kif")));
        QCOMPARE(KifuFile::initialSfen<KifuFormat::Kif>(file.fileName()), initial);
        const RecordLine line = KifuFile::mainline<KifuFormat::Kif>(file.fileName());
        QCOMPARE(line.entries.size(), 2);
        QVERIFY(line.entries[0].displayText.startsWith(QStringLiteral("△")));
        QCOMPARE(line.entries[1].displayText, QStringLiteral("▲投了"));
    }

    void declarationWinnerIsSideToMove_data()
    {
        QTest::addColumn<QString>("terminal");
        QTest::addColumn<QString>("winner");
        // 入玉宣言は宣言する側の手番で行う。▲７六歩の次は後手番なので、記号がなければ後手の勝ち
        QTest::newRow("unmarked") << QStringLiteral("入玉勝ち") << QStringLiteral("後手");
        QTest::newRow("white-mark") << QStringLiteral("△入玉勝ち") << QStringLiteral("後手");
        QTest::newRow("black-mark") << QStringLiteral("▲入玉勝ち") << QStringLiteral("先手");
    }

    void declarationWinnerIsSideToMove()
    {
        QFETCH(QString, terminal);
        QFETCH(QString, winner);
        KifuBranchTree tree;
        tree.setRootSfen(kHirateSfen);
        auto* tip = addTestMove(tree, tree.root(), QStringLiteral("7g7f"), QStringLiteral("▲７六歩(77)"));
        // 記号のない行は手番側（▲７六歩の次なので後手）
        TestTree::addTerminalMove(tree, tip, TerminalType::DeclarationWin,
                             terminal.startsWith(QStringLiteral("▲")) ? Turn::Black : Turn::White);
        KifuExportMetadata ctx;
        ctx.startSfen = kHirateSfen;
        const QStringList lines = KifuExport::lines(tree, ctx, KifuFormat::Kif);
        // KIF も Kifu for Windows と同じく、入玉宣言は終局の種類を明記する
        QVERIFY2(lines.contains(QStringLiteral("まで1手で%1の入玉勝ち").arg(winner)), qPrintable(lines.join('\n')));
    }

    void handicapTurnsAndWinner_data()
    {
        QTest::addColumn<QString>("format");
        QTest::addColumn<int>("moveCount");
        for (const QString& format : {QStringLiteral("kif"), QStringLiteral("ki2")}) {
            for (int count = 0; count <= 2; ++count)
                QTest::newRow(qPrintable(format + QString::number(count))) << format << count;
        }
    }

    void handicapTurnsAndWinner()
    {
        QFETCH(QString, format);
        QFETCH(int, moveCount);
        const QString initial = QString::fromLatin1(Handicaps::byLabel(u"角落ち")->sfen);
        KifuBranchTree tree;
        tree.setRootSfen(initial);
        auto* tip = tree.root();
        if (moveCount >= 1) tip = addTestMove(tree, tip, QStringLiteral("3c3d"), QStringLiteral("△３四歩(33)"));
        if (moveCount >= 2) tip = addTestMove(tree, tip, QStringLiteral("7g7f"), QStringLiteral("▲７六歩(77)"));
        TestTree::addTerminalMove(tree, tip, TerminalType::Resign, moveCount % 2 == 0 ? Turn::White : Turn::Black);
        addTestMove(tree, tree.root(), QStringLiteral("8c8d"), QStringLiteral("△８四歩(83)"));
        KifuExportMetadata ctx;
        ctx.startSfen = initial;
        const QStringList lines = format == QStringLiteral("kif") ? KifuExport::lines(tree, ctx, KifuFormat::Kif) : KifuExport::lines(tree, ctx, KifuFormat::Ki2);
        // 駒落ちは柿木形式と同じく、局面図の持駒・手番と終局行も下手・上手で書く（下で読み込み直す）
        QVERIFY(lines.contains(QStringLiteral("まで%1手で%2の勝ち").arg(moveCount)
            .arg(moveCount % 2 == 0 ? QStringLiteral("下手") : QStringLiteral("上手"))));
        QVERIFY(lines.contains(QStringLiteral("上手の持駒：なし")));
        QVERIFY(lines.contains(QStringLiteral("下手の持駒：なし")));
        QVERIFY(lines.contains(QStringLiteral("上手番")));
        QVERIFY(!lines.join(QLatin1Char('\n')).contains(QStringLiteral("後手の持駒")));
        QTemporaryFile file;
        QVERIFY(KifuTestHelper::writeToTempFile(file, lines.join(QLatin1Char('\n')).toUtf8(), format));
        KifParseResult result;
        QString error;
        const bool ok = format == QStringLiteral("kif")
            ? KifuFile::parse<KifuFormat::Kif>(file.fileName(), result, &error)
            : KifuFile::parse<KifuFormat::Ki2>(file.fileName(), result, &error);
        QVERIFY2(ok, qPrintable(error));
        QCOMPARE(result.mainline.usiMoves().size(), moveCount);
        QCOMPARE(result.mainline.startPly, 1);
        QVERIFY(result.mainline.entries[0].displayText.startsWith(QStringLiteral("△")));
        QCOMPARE(result.variations.size(), 1);
        QCOMPARE(result.variations[0].line.usiMoves(), QStringList({"8c8d"}));
        QCOMPARE(result.variations[0].line.startPly, 1);
        QVERIFY(result.variations[0].line.entries[0].displayText.startsWith(QStringLiteral("△")));
    }

    void usenWhiteToMoveBranchDrop()
    {
        QString initial = kHirateSfen;
        initial.replace(QStringLiteral(" b - "), QStringLiteral(" w p "));
        KifuBranchTree tree;
        tree.setRootSfen(initial);
        addTestMove(tree, tree.root(), QStringLiteral("3c3d"), QStringLiteral("△３四歩(33)"));
        addTestMove(tree, tree.root(), QStringLiteral("P*5e"), QStringLiteral("△５五歩打"));
        KifuExportMetadata ctx;
        ctx.startSfen = initial;
        QTemporaryFile file;
        QVERIFY(KifuTestHelper::writeToTempFile(file, KifuExport::lines(tree, ctx, KifuFormat::Usen)
            .join(QLatin1Char('\n')).toUtf8(), QStringLiteral("usen")));
        KifParseResult result;
        QString error;
        QVERIFY2(KifuFile::parse<KifuFormat::Usen>(file.fileName(), result, &error), qPrintable(error));
        QCOMPARE(result.variations.size(), 1);
        QCOMPARE(result.variations[0].line.usiMoves(), QStringList({"P*5e"}));
    }

    void jkfPromotedMinorPieces_data()
    {
        QTest::addColumn<QString>("piece");
        QTest::addColumn<QString>("kanji");
        QTest::addColumn<QString>("code");
        QTest::newRow("lance") << QStringLiteral("L") << QStringLiteral("成香") << QStringLiteral("NY");
        QTest::newRow("knight") << QStringLiteral("N") << QStringLiteral("成桂") << QStringLiteral("NK");
        QTest::newRow("silver") << QStringLiteral("S") << QStringLiteral("成銀") << QStringLiteral("NG");
    }

    void jkfPromotedMinorPieces()
    {
        QFETCH(QString, piece);
        QFETCH(QString, kanji);
        QFETCH(QString, code);
        // "%13" は13番目の置き換えになるので、駒の後の空きは別に書く
        const QString initial = QStringLiteral("4k4/9/9/9/9/5+%1%2/9/9/4K4 b - 1").arg(piece, QStringLiteral("3"));
        KifuBranchTree tree;
        tree.setRootSfen(initial);
        addTestMove(tree, tree.root(), QStringLiteral("4f5e"), QStringLiteral("▲５五%1(46)").arg(kanji));
        KifuExportMetadata ctx;
        ctx.startSfen = initial;
        const QByteArray json = KifuExport::lines(tree, ctx, KifuFormat::Jkf).join(QLatin1Char('\n')).toUtf8();
        const auto moves = QJsonDocument::fromJson(json).object()[QStringLiteral("moves")].toArray();
        QCOMPARE(moves[1].toObject()[QStringLiteral("move")].toObject()[QStringLiteral("piece")].toString(), code);
        QTemporaryFile file;
        QVERIFY(KifuTestHelper::writeToTempFile(file, json, QStringLiteral("jkf")));
        KifParseResult result;
        QString error;
        QVERIFY2(KifuFile::parse<KifuFormat::Jkf>(file.fileName(), result, &error), qPrintable(error));
        QCOMPARE(result.mainline.usiMoves(), QStringList({"4f5e"}));
        QCOMPARE(result.mainline.sfens().last(), tree.mainLine().last()->sfen());
    }

    void csaTimeUsesMinutesAndSeconds()
    {
        QCOMPARE(CsaFormatter::convertToCsaTime(QStringLiteral("10:30+30")), QStringLiteral("$TIME:630+30+0"));
        QCOMPARE(CsaFormatter::convertToCsaTime(QStringLiteral("120:00+60")), QStringLiteral("$TIME:7200+60+0"));
        QCOMPARE(CsaFormatter::convertToCsaTime(QStringLiteral("600+30+5")), QStringLiteral("$TIME:600+30+5"));
        // フィッシャー加算は秒読みと区別して第3項に入れる
        QCOMPARE(CsaFormatter::convertToCsaTime(QStringLiteral("05:00+10秒加算")), QStringLiteral("$TIME:300+0+10"));
        // 切れ負けの「分:秒」を CSA V2 の時:分（$TIME_LIMIT）と取り違えない
        QCOMPARE(CsaFormatter::convertToCsaTime(QStringLiteral("10:00")), QStringLiteral("$TIME:600+0+0"));
        QCOMPARE(CsaFormatter::convertToCsaTime(QStringLiteral("120:30")), QStringLiteral("$TIME:7230+0+0"));
    }

    /// 先後で違う持ち時間は CSA V3.0 の $TIME+ と $TIME- に分ける
    void csaTimeLinesSplitPerSide()
    {
        using CsaFormatter::convertToCsaTimeLines;
        QCOMPARE(convertToCsaTimeLines(QStringLiteral("10:00+30")), QStringList({"$TIME:600+30+0"}));
        QCOMPARE(convertToCsaTimeLines(QStringLiteral("先手 01:00+2 / 後手 02:00+3")),
                 QStringList({"$TIME+:60+2+0", "$TIME-:120+3+0"}));
        QCOMPARE(convertToCsaTimeLines(QStringLiteral("下手 05:00+10秒加算 / 上手 10:00+10秒加算")),
                 QStringList({"$TIME+:300+0+10", "$TIME-:600+0+10"}));
        QCOMPARE(convertToCsaTimeLines(QStringLiteral("先手 03:00 / 後手 05:00")),
                 QStringList({"$TIME+:180+0+0", "$TIME-:300+0+0"}));
    }

    /// 対局情報の持ち時間は秒読みと加算を書き分ける
    void timeControlTextDistinguishesIncrement()
    {
        using TimeControlText::format;
        QCOMPARE(format(TimeControlSide{600000, 10000, 0}), QStringLiteral("10:00+10"));
        QCOMPARE(format(TimeControlSide{300000, 0, 10000}), QStringLiteral("05:00+10秒加算"));
        QCOMPARE(format(TimeControlSide{7200000, 0, 0}), QStringLiteral("120:00"));
        QCOMPARE(format(TimeControlSide{0, 30000, 0}), QStringLiteral("00:00+30"));
    }

    /// 先後で持ち時間が違えば両方を書き、駒落ちの見出しは下手・上手にする
    /// 持ち時間の値は書いたとおりに読み戻せる（先後・下手上手・秒読み・加算）
    void timeControlTextRoundTrips()
    {
        const TimeControl tc{{60000, 2000, 0}, {120000, 0, 3000}};
        for (const bool handicap : {false, true}) {
            const auto parsed = TimeControlText::parse(TimeControlText::format(tc, handicap));
            QVERIFY(parsed);
            QCOMPARE(parsed->timeControl, tc);
            QVERIFY(parsed->perSide);
            QCOMPARE(parsed->handicapNames, handicap);
        }
        const auto same = TimeControlText::parse(QStringLiteral("10:00+30"));
        QVERIFY(same && !same->perSide);
        QCOMPARE(same->timeControl.white.byoyomiMs, 30000);
        QVERIFY(!TimeControlText::parse(QStringLiteral("10分")));
        QVERIFY(!TimeControlText::parse(QStringLiteral("10:75")));
    }

    void timeControlTextPerSide()
    {
        using TimeControlText::format;
        const TimeControlSide black{60000, 2000, 0};
        const TimeControlSide white{120000, 3000, 0};
        QCOMPARE(format(TimeControl{black, black}, false), QStringLiteral("01:00+2"));
        QCOMPARE(format(TimeControl{black, white}, false), QStringLiteral("先手 01:00+2 / 後手 02:00+3"));
        QCOMPARE(format(TimeControl{black, white}, true), QStringLiteral("下手 01:00+2 / 上手 02:00+3"));
    }

    /// 駒落ちの棋譜は柿木形式と同じく対局者を下手・上手で書き、CSA では下手が先手（N+）になる。
    /// 対局情報は対局を始めたときに文書が作る（書き出しは作らない）
    void handicapHeaderUsesShitateUwate()
    {
        KifuBranchTree tree;
        tree.setRootSfen(QStringLiteral("lnsgkgsnl/1r7/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"));  // 角落ち
        KifuDocument document(&tree);
        MatchPlayers players;
        players.seat(Turn::Black).engine = true;
        players.seat(Turn::White).engine = true;
        players.seat(Turn::Black).engineName = QStringLiteral("下手エンジン");
        players.seat(Turn::White).engineName = QStringLiteral("上手エンジン");
        document.beginMatch(players, TimeControl{{60000, 2000, 0}, {120000, 3000, 0}}, false);
        const QString kif = KifuExport::lines(document, KifuFormat::Kif).join(QLatin1Char('\n'));
        QVERIFY2(kif.contains(QStringLiteral("下手：下手エンジン")), qPrintable(kif));
        QVERIFY(kif.contains(QStringLiteral("上手：上手エンジン")));
        QVERIFY(!kif.contains(QStringLiteral("先手：")));
        QVERIFY(!kif.contains(QStringLiteral("後手：")));
        QVERIFY(kif.contains(QStringLiteral("持ち時間：下手 01:00+2 / 上手 02:00+3")));
        const QString csa = KifuExport::lines(document, KifuFormat::Csa).join(QLatin1Char('\n'));
        QVERIFY2(csa.contains(QStringLiteral("N+下手エンジン")), qPrintable(csa));
        QVERIFY(csa.contains(QStringLiteral("N-上手エンジン")));
        QVERIFY(csa.contains(QStringLiteral("$TIME+:60+2+0")));
        QVERIFY(csa.contains(QStringLiteral("$TIME-:120+3+0")));
        QVERIFY(!csa.contains(QStringLiteral("$TIME:")));

        // 平手は従来どおり先手・後手
        document.startNew(kHirateSfen);
        document.beginMatch(players, TimeControl{{60000, 2000, 0}, {60000, 2000, 0}}, false);
        const QString hirate = KifuExport::lines(document, KifuFormat::Kif).join(QLatin1Char('\n'));
        QVERIFY2(hirate.contains(QStringLiteral("先手：下手エンジン")), qPrintable(hirate));
        QVERIFY(hirate.contains(QStringLiteral("持ち時間：01:00+2")));
        QVERIFY(KifuExport::lines(document, KifuFormat::Csa).contains(QStringLiteral("$TIME:60+2+0")));

        // 読み込んだ棋譜の見出しが先手・後手なら、駒落ちでも局面図をその呼び方にそろえる
        KifuExportMetadata ctx;
        ctx.startSfen = QStringLiteral("lnsgkgsnl/1r7/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1");
        ctx.gameInfoItems = {{QStringLiteral("先手"), QStringLiteral("A")}, {QStringLiteral("後手"), QStringLiteral("B")},
                             {QStringLiteral("手合割"), QStringLiteral("角落ち")}};
        KifuBranchTree empty;
        const QString keepsHeader = KifuExport::lines(empty, ctx, KifuFormat::Kif).join(QLatin1Char('\n'));
        QVERIFY2(keepsHeader.contains(QStringLiteral("後手の持駒：なし")), qPrintable(keepsHeader));
        QVERIFY(keepsHeader.contains(QStringLiteral("後手番")));
        QVERIFY(!keepsHeader.contains(QStringLiteral("上手の持駒")));
    }

    /// 対局で時間切れになったときの終局語「時間切れ」を、どの形式でも終局として書く
    void timeoutTerminalExportsInEveryFormat()
    {
        const QString initial =
            QStringLiteral("lnsgkgsnl/1r7/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1");  // 角落ち
        KifuBranchTree tree;
        tree.setRootSfen(initial);
        auto* tip = addTestMove(tree, tree.root(), QStringLiteral("3c3d"), QStringLiteral("△３四歩(33)"));
        TestTree::addTerminalMove(tree, tip, TerminalType::Timeout, Turn::Black);
        KifuExportMetadata ctx;
        ctx.startSfen = initial;
        const QStringList kif = KifuExport::lines(tree, ctx, KifuFormat::Kif);
        QVERIFY2(kif.contains(QStringLiteral("まで1手で時間切れにより上手の勝ち")), qPrintable(kif.join(QLatin1Char('\n'))));
        const QStringList ki2 = KifuExport::lines(tree, ctx, KifuFormat::Ki2);
        QVERIFY2(ki2.contains(QStringLiteral("まで1手で時間切れにより上手の勝ち")), qPrintable(ki2.join(QLatin1Char('\n'))));
        QVERIFY(!ki2.contains(QStringLiteral("▲時間切れ")));
        QVERIFY(KifuExport::lines(tree, ctx, KifuFormat::Usi).join(QLatin1Char('\n')).endsWith(QStringLiteral("3c3d timeout")));
        QVERIFY(KifuExport::lines(tree, ctx, KifuFormat::Usen).join(QLatin1Char('\n')).trimmed().endsWith(QStringLiteral(".t")));
        QVERIFY(KifuExport::lines(tree, ctx, KifuFormat::Jkf).join(QLatin1Char('\n')).contains(QStringLiteral("\"special\":\"TIME_UP\"")));
    }

    /// 駒落ちの呼び方（下手・上手）で書いた局面図の持駒も読み込める
    void bodReadsShitateUwateHands()
    {
        const QString sfen = QStringLiteral("4k4/9/9/9/9/9/9/9/4K4 w 2Pr 1");
        const QString bod = BodTextGenerator::generate(*Position::fromSfen(sfen), 0, {}, true);
        QVERIFY2(bod.contains(QStringLiteral("上手の持駒：")) && bod.contains(QStringLiteral("下手の持駒：")),
                 qPrintable(bod));
        QVERIFY(!bod.contains(QStringLiteral("なし")));
        QTemporaryFile file;
        QVERIFY(KifuTestHelper::writeToTempFile(
            file, (bod + QStringLiteral("\n手数----指手---------消費時間--\n")).toUtf8(), QStringLiteral("kif")));
        QCOMPARE(KifuFile::initialSfen<KifuFormat::Kif>(file.fileName()), sfen);
    }

    /// 「局面図をコピー」も、駒落ちは KIF の局面図と同じく下手・上手で書く（途中の局面も同じ）
    void bodClipboardFollowsHandicapNames_data()
    {
        QTest::addColumn<QString>("startSfen");
        QTest::addColumn<QString>("move");
        QTest::addColumn<QStringList>("lines");
        QTest::newRow("even") << kHirateSfen << QStringLiteral("7g7f")
            << QStringList{QStringLiteral("後手の持駒：なし"), QStringLiteral("先手の持駒：なし"),
                           QStringLiteral("後手番")};
        QTest::newRow("handicap")
            << QStringLiteral("lnsgkgsnl/1r7/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1")  // 角落ち
            << QStringLiteral("3c3d")
            << QStringList{QStringLiteral("上手の持駒：なし"), QStringLiteral("下手の持駒：なし"),
                           QStringLiteral("下手番")};
    }

    void bodClipboardFollowsHandicapNames()
    {
        QFETCH(QString, startSfen);
        QFETCH(QString, move);
        QFETCH(QStringList, lines);
        // 局面は棋譜の正本（分岐ツリー）の表示中の手順から取る。
        // 対局情報のない局面の棋譜なので、呼び方は開始局面の手合割で決まる
        KifuBranchTree tree;
        KifuNavigationState nav(&tree);
        FileHarness files(&tree, &nav);
        files.document.replaceWithPosition(startSfen);
        addTestMove(tree, tree.root(), move, QStringLiteral("1手目"));
        nav.setCurrentNode(TestTree::findByPlyOnMainLine(tree, 1));
        files.gc.board()->setSfen(nav.currentSfen());
        QVERIFY(files.controller->copyBodToClipboard());
        const QStringList bod = QApplication::clipboard()->text().split(QLatin1Char('\n'));
        for (const QString& line : std::as_const(lines)) {
            QVERIFY2(bod.contains(line), qPrintable(bod.join(QLatin1Char('\n'))));
        }
    }

    /// クリップボードへのコピーも、持ち時間と開始・終了の日時を棋譜に書く
    void clipboardKeepsTimeMetadata()
    {
        KifuBranchTree tree;
        tree.setRootSfen(kHirateSfen);
        const TimeControl timeControl{{630000, 30000, 5000}, {630000, 30000, 5000}};
        KifuNavigationState nav(&tree);
        FileHarness files(&tree, &nav);
        files.document.beginMatch(MatchPlayers{}, timeControl, false);
        files.document.recordMatchEnd();
        const MatchTimeRecord& timeRecord = files.document.timeRecord();
        const QString end = timeRecord.endDateTime().toString(QStringLiteral("yyyy/MM/dd HH:mm:ss"));

        QVERIFY(files.controller->copyKifToClipboard());
        QCOMPARE(QApplication::clipboard()->text(),
                 KifuExport::lines(files.document, KifuFormat::Kif).join(QLatin1Char('\n')));
        QVERIFY(QApplication::clipboard()->text().contains(QStringLiteral("持ち時間：10:30")));
        QVERIFY(QApplication::clipboard()->text().contains(end));
        QVERIFY(files.controller->copyKi2ToClipboard());
        QVERIFY(QApplication::clipboard()->text().contains(end));
        QVERIFY(files.controller->copyCsaToClipboard());
        const QString csa = QApplication::clipboard()->text();
        QVERIFY2(csa.contains(QStringLiteral("$START_TIME:")) && csa.contains(QStringLiteral("$END_TIME:")), qPrintable(csa));
    }

    /// 保存ファイル名の既定値は対局情報の対局者名から作る（読み込んだ棋譜も「日時_先手_vs_後手」。監査 U1）。
    /// 新規の棋譜は対局者名が空なので日時だけ
    void defaultFileNameUsesGameInfoPlayers()
    {
        KifuBranchTree tree;
        tree.setRootSfen(kHirateSfen);
        KifuNavigationState nav(&tree);
        FileHarness files(&tree, &nav);
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        files.controller->autoSaveKifu(dir.path(), QStringLiteral("kifu"));
        QStringList saved = QDir(dir.path()).entryList(QDir::Files);
        QCOMPARE(saved.size(), 1);
        QVERIFY2(saved.first().endsWith(QStringLiteral("_Session.kifu")), qPrintable(saved.first()));

        files.document.editHeader({{GameInfoKeys::kBlackPlayer, QStringLiteral("Alice")},
                                   {GameInfoKeys::kWhitePlayer, QStringLiteral("Bob")}});
        QVERIFY(files.document.players().name(Turn::Black).isEmpty());   // 対局の記録には対局者が無い
        files.controller->autoSaveKifu(dir.path(), QStringLiteral("csa"));
        saved = QDir(dir.path()).entryList({QStringLiteral("*.csa")}, QDir::Files);
        QCOMPARE(saved.size(), 1);
        QVERIFY2(saved.first().endsWith(QStringLiteral("_Alice_vs_Bob.csa")), qPrintable(saved.first()));
    }

    void removedGameInfoStaysEmptyOnExport()
    {
        KifuBranchTree tree;
        KifuExportMetadata ctx;
        ctx.startSfen = kHirateSfen;
        // 対局情報が空なら、日時や対局者を作り出さない
        const QString kif = KifuExport::lines(tree, ctx, KifuFormat::Kif).join(QLatin1Char('\n'));
        QVERIFY(!kif.contains(QStringLiteral("開始日時：")));
        QVERIFY(!kif.contains(QStringLiteral("先手：")));
        QVERIFY(!kif.contains(QStringLiteral("後手：")));
    }

    void removedGameInfoPreservesCustomPosition()
    {
        const QString initial = QStringLiteral("4k4/9/9/9/4p4/4+S4/9/9/4K4 w Pp 1");
        KifuBranchTree tree;
        tree.setRootSfen(initial);
        KifuExportMetadata ctx;
        ctx.startSfen = initial;
        QTemporaryFile kif;
        QVERIFY(KifuTestHelper::writeToTempFile(kif, KifuExport::lines(tree, ctx, KifuFormat::Kif).join(QLatin1Char('\n')).toUtf8(), QStringLiteral("kif")));
        QCOMPARE(KifuFile::initialSfen<KifuFormat::Kif>(kif.fileName()), initial);
        QTemporaryFile ki2;
        QVERIFY(KifuTestHelper::writeToTempFile(ki2, KifuExport::lines(tree, ctx, KifuFormat::Ki2).join(QLatin1Char('\n')).toUtf8(), QStringLiteral("ki2")));
        QCOMPARE(KifuFile::initialSfen<KifuFormat::Ki2>(ki2.fileName()), initial);
    }

    void clipboardExportAfterResume_data()
    {
        QTest::addColumn<QString>("format");
        QTest::addColumn<bool>("branch");
        for (const QString& format : {QStringLiteral("csa"), QStringLiteral("usi"), QStringLiteral("usen")}) {
            QTest::newRow(qPrintable(format + QStringLiteral("-tip"))) << format << false;
            QTest::newRow(qPrintable(format + QStringLiteral("-branch"))) << format << true;
        }
    }

    void clipboardExportAfterResume()
    {
        QFETCH(QString, format);
        QFETCH(bool, branch);
        KifuBranchTree tree;
        tree.setRootSfen(kHirateSfen);
        auto* first = addTestMove(tree, tree.root(), QStringLiteral("7g7f"), QStringLiteral("▲７六歩(77)"));
        auto* second = addTestMove(tree, first, QStringLiteral("3c3d"), QStringLiteral("△３四歩(33)"));
        auto* anchor = branch ? first : second;
        const QString newUsi = branch ? QStringLiteral("8c8d") : QStringLiteral("2g2f");
        const QString newPretty = branch ? QStringLiteral("△８四歩(83)") : QStringLiteral("▲２六歩(27)");
        QStringList sessionUsi = {newUsi};
        QStringList sessionSfens = TestReplay::buildSfenRecord(anchor->sfen(), sessionUsi, false);

        KifuNavigationState nav(&tree);
        LiveGameSession session(&tree, &nav);
        session.startFromNode(anchor);
        session.appendRecord(RecordEntry::played(TestReplay::buildGameMoves(anchor->sfen(), sessionUsi).value(0),
                                                 newPretty, sessionSfens.last(),
                                                 RecordTime::fromText(QStringLiteral("00:05/00:00:05"))));
        QVERIFY(session.commit());

        FileHarness files(&tree, &nav);
        if (format == QStringLiteral("csa")) QVERIFY(files.controller->copyCsaToClipboard());
        else if (format == QStringLiteral("usi")) QVERIFY(files.controller->copyUsiToClipboard());
        else QVERIFY(files.controller->copyUsenToClipboard());

        QTemporaryFile tmp;
        const QByteArray text = QApplication::clipboard()->text().toUtf8();
        QVERIFY(KifuTestHelper::writeToTempFile(tmp, text, format));
        KifParseResult result;
        QString error;
        bool ok;
        if (format == QStringLiteral("csa")) ok = KifuFile::parse<KifuFormat::Csa>(tmp.fileName(), result, &error);
        else if (format == QStringLiteral("usi")) ok = KifuFile::parse<KifuFormat::Usi>(tmp.fileName(), result, &error);
        else ok = KifuFile::parse<KifuFormat::Usen>(tmp.fileName(), result, &error);
        QVERIFY2(ok, qPrintable(error));
        QStringList expected = {QStringLiteral("7g7f"), QStringLiteral("3c3d")};
        if (!branch) expected.append(newUsi);
        QCOMPARE(result.mainline.startSfen, kHirateSfen);
        QCOMPARE(result.mainline.usiMoves(), expected);
        QCOMPARE(tree.mainRecordLine().usiMoves(), expected);

        // 現在手までのUSIコピーでは、本譜の先頭から指定手数に制限する。
        nav.setCurrentNode(TestTree::findByPlyOnMainLine(tree, 1));
        QVERIFY(files.controller->copyUsiCurrentToClipboard());
        QCOMPARE(QApplication::clipboard()->text(), QStringLiteral("position startpos moves 7g7f"));
    }

    void exportUsiSource_ignoresTerminalAndSessionFallback()
    {
        KifuBranchTree tree;
        tree.setRootSfen(kHirateSfen);
        auto* first = addTestMove(tree, tree.root(), QStringLiteral("7g7f"), QStringLiteral("▲７六歩(77)"));
        TestTree::addTerminalMove(tree, first, TerminalType::Resign, Turn::White);
        QCOMPARE(tree.mainRecordLine().usiMoves(), QStringList({QStringLiteral("7g7f")}));

        // 開始局面だけの本譜を書き出す（対局用の一覧など、ほかの指し手の出どころは無い）。
        tree.setRootSfen(kHirateSfen);
        KifuNavigationState nav(&tree);
        FileHarness files(&tree, &nav);
        QVERIFY(files.controller->copyUsiToClipboard());
        QCOMPARE(QApplication::clipboard()->text(), QStringLiteral("position startpos"));
    }

    void exportOpeningRow_data()
    {
        QTest::addColumn<QString>("format");
        QTest::addColumn<QString>("openingText");
        for (const QString& format : {QStringLiteral("kif"), QStringLiteral("ki2"),
                                      QStringLiteral("csa"), QStringLiteral("jkf")}) {
            QTest::newRow(qPrintable(format + QStringLiteral("-ja")))
                << format << QStringLiteral("開始局面");
            QTest::newRow(qPrintable(format + QStringLiteral("-en")))
                << format << QStringLiteral("Starting Position");
            QTest::newRow(qPrintable(format + QStringLiteral("-empty"))) << format << QString();
        }
    }

    void exportOpeningRow()
    {
        QFETCH(QString, format);
        QFETCH(QString, openingText);
        KifuBranchTree tree;
        tree.setRootSfen(kHirateSfen);
        tree.root()->record().displayText = openingText;
        tree.root()->record().comment = QStringLiteral("opening comment");
        auto* first = addTestMove(tree, tree.root(), QStringLiteral("7g7f"),
                                 QStringLiteral("▲７六歩(77)"), QStringLiteral("00:12/00:00:12"));
        first->record().comment = QStringLiteral("first comment");
        auto* last = addTestMove(tree, first, QStringLiteral("3c3d"),
                                QStringLiteral("△３四歩(33)"), QStringLiteral("00:34/00:00:34"));
        last->record().comment = QStringLiteral("last comment");

        KifuExportMetadata ctx;
        ctx.startSfen = kHirateSfen;
        const QStringList expectedMoves = {QStringLiteral("7g7f"), QStringLiteral("3c3d")};
        QStringList output;
        if (format == QStringLiteral("kif")) output = KifuExport::lines(tree, ctx, KifuFormat::Kif);
        else if (format == QStringLiteral("ki2")) output = KifuExport::lines(tree, ctx, KifuFormat::Ki2);
        else if (format == QStringLiteral("csa")) output = KifuExport::lines(tree, ctx, KifuFormat::Csa);
        else output = KifuExport::lines(tree, ctx, KifuFormat::Jkf);

        QTemporaryFile tmp;
        QVERIFY(KifuTestHelper::writeToTempFile(tmp, output.join(QLatin1Char('\n')).toUtf8(), format));
        KifParseResult result;
        QString error;
        bool ok;
        if (format == QStringLiteral("kif")) ok = KifuFile::parse<KifuFormat::Kif>(tmp.fileName(), result, &error);
        else if (format == QStringLiteral("ki2")) ok = KifuFile::parse<KifuFormat::Ki2>(tmp.fileName(), result, &error);
        else if (format == QStringLiteral("csa")) ok = KifuFile::parse<KifuFormat::Csa>(tmp.fileName(), result, &error);
        else ok = KifuFile::parse<KifuFormat::Jkf>(tmp.fileName(), result, &error);
        QVERIFY2(ok, qPrintable(error));
        QCOMPARE(result.mainline.usiMoves(), expectedMoves);
        QVERIFY(result.mainline.entries.size() >= 2);
        QCOMPARE(result.mainline.openingComment.trimmed(), QStringLiteral("opening comment"));
        QCOMPARE(result.mainline.entries[0].comment.trimmed(), QStringLiteral("first comment"));
        QCOMPARE(result.mainline.entries[1].comment.trimmed(), QStringLiteral("last comment"));
        if (format != QStringLiteral("ki2")) {
            QVERIFY(result.mainline.entries[0].time.toText().contains(QStringLiteral("00:12")));
            QVERIFY(result.mainline.entries[1].time.toText().contains(QStringLiteral("00:34")));
        }
    }

    void jkfForks_replaceMoveAtSamePly()
    {
        KifuBranchTree tree;
        tree.setRootSfen(kHirateSfen);
        auto* first = addTestMove(tree, tree.root(), QStringLiteral("7g7f"), QStringLiteral("▲７六歩(77)"));
        auto* second = addTestMove(tree, first, QStringLiteral("3c3d"), QStringLiteral("△３四歩(33)"));
        addTestMove(tree, second, QStringLiteral("2g2f"), QStringLiteral("▲２六歩(27)"));
        addTestMove(tree, tree.root(), QStringLiteral("2g2f"), QStringLiteral("▲２六歩(27)"));
        auto* alternateSecond = addTestMove(tree, first, QStringLiteral("8c8d"), QStringLiteral("△８四歩(83)"));
        addTestMove(tree, alternateSecond, QStringLiteral("1g1f"), QStringLiteral("▲１六歩(17)"));
        addTestMove(tree, second, QStringLiteral("1g1f"), QStringLiteral("▲１六歩(17)"));

        KifuExportMetadata ctx;
        ctx.startSfen = kHirateSfen;
        const QByteArray json = KifuExport::lines(tree, ctx, KifuFormat::Jkf).join(QLatin1Char('\n')).toUtf8();
        const auto moves = QJsonDocument::fromJson(json).object()[QStringLiteral("moves")].toArray();
        QCOMPARE(moves.size(), 4);
        QVERIFY(!moves[0].toObject().contains(QStringLiteral("forks")));
        for (int ply = 1; ply <= 3; ++ply) {
            QCOMPARE(moves[ply].toObject()[QStringLiteral("forks")].toArray().size(), 1);
        }
        QTemporaryFile tmp;
        QVERIFY(KifuTestHelper::writeToTempFile(tmp, json, QStringLiteral("jkf")));
        KifParseResult result;
        QString error;
        QVERIFY2(KifuFile::parse<KifuFormat::Jkf>(tmp.fileName(), result, &error), qPrintable(error));
        QCOMPARE(result.mainline.usiMoves(), QStringList({QStringLiteral("7g7f"), QStringLiteral("3c3d"), QStringLiteral("2g2f")}));
        QCOMPARE(result.variations.size(), 3);
        QCOMPARE(result.variations[0].startPly, 1);
        QCOMPARE(result.variations[0].line.usiMoves(), QStringList({QStringLiteral("2g2f")}));
        QCOMPARE(result.variations[1].startPly, 2);
        QCOMPARE(result.variations[1].line.usiMoves(), QStringList({QStringLiteral("8c8d"), QStringLiteral("1g1f")}));
        QCOMPARE(result.variations[2].startPly, 3);
        QCOMPARE(result.variations[2].line.usiMoves(), QStringList({QStringLiteral("1g1f")}));

        // 分岐内の分岐も、代替される子の手に forks を付ける。
        addTestMove(tree, alternateSecond, QStringLiteral("9g9f"), QStringLiteral("▲９六歩(97)"));
        const auto nestedMoves = QJsonDocument::fromJson(KifuExport::lines(tree, ctx, KifuFormat::Jkf).join(QLatin1Char('\n')).toUtf8())
            .object()[QStringLiteral("moves")].toArray();
        const auto secondFork = nestedMoves[2].toObject()[QStringLiteral("forks")].toArray()[0].toArray();
        QCOMPARE(secondFork.size(), 2);
        QVERIFY(!secondFork[0].toObject().contains(QStringLiteral("forks")));
        QCOMPARE(secondFork[1].toObject()[QStringLiteral("forks")].toArray().size(), 1);
        QTemporaryFile nestedFile;
        QVERIFY(KifuTestHelper::writeToTempFile(nestedFile, KifuExport::lines(tree, ctx, KifuFormat::Jkf).join(QLatin1Char('\n')).toUtf8(), QStringLiteral("jkf")));
        QVERIFY2(KifuFile::parse<KifuFormat::Jkf>(nestedFile.fileName(), result, &error), qPrintable(error));
        QCOMPARE(result.variations.size(), 4);
        QCOMPARE(result.variations[2].startPly, 3);
        QCOMPARE(result.variations[2].line.usiMoves(), QStringList({"9g9f"}));
        QCOMPARE(result.variations[2].line.startSfen, alternateSecond->sfen());
        QCOMPARE(result.variations[2].line.sfens().size(), 2);
    }

    void jkfForks_sameDestinationUsesParentMove()
    {
        KifuBranchTree tree;
        tree.setRootSfen(kHirateSfen);
        auto* first = addTestMove(tree, tree.root(), QStringLiteral("7g7f"), QStringLiteral("▲７六歩(77)"));
        auto* second = addTestMove(tree, first, QStringLiteral("3c3d"), QStringLiteral("△３四歩(33)"));
        auto* capture = addTestMove(tree, second, QStringLiteral("8h2b+"), QStringLiteral("▲２二角成(88)"));
        addTestMove(tree, capture, QStringLiteral("8b2b"), QStringLiteral("△同　飛(82)"));
        addTestMove(tree, capture, QStringLiteral("3a2b"), QStringLiteral("△同　銀(31)"));
        KifuExportMetadata ctx;
        ctx.startSfen = kHirateSfen;
        QTemporaryFile tmp;
        QVERIFY(KifuTestHelper::writeToTempFile(tmp, KifuExport::lines(tree, ctx, KifuFormat::Jkf).join(QLatin1Char('\n')).toUtf8(),
                                               QStringLiteral("jkf")));
        KifParseResult result;
        QString error;
        QVERIFY2(KifuFile::parse<KifuFormat::Jkf>(tmp.fileName(), result, &error), qPrintable(error));
        QCOMPARE(result.variations.size(), 1);
        QCOMPARE(result.variations[0].startPly, 4);
        QCOMPARE(result.variations[0].line.usiMoves(), QStringList({QStringLiteral("3a2b")}));
    }

    // === 文書（コメント・しおり・未保存・置き換え） ===

    void setComment_roundTrip()
    {
        KifuBranchTree tree;
        setupBasicTree(tree);
        KifuDocument document(&tree);

        document.setComment(tree.mainLine().at(1), QStringLiteral("初手のコメント"));
        QCOMPARE(tree.mainLine().at(1)->comment(), QStringLiteral("初手のコメント"));
    }

    void commentChanged_signal()
    {
        KifuBranchTree tree;
        setupBasicTree(tree);
        KifuDocument document(&tree);

        QSignalSpy spy(&document, &KifuDocument::commentChanged);
        QVERIFY(spy.isValid());

        document.setComment(tree.mainLine().at(2), QStringLiteral("テストコメント"));
        QCOMPARE(spy.count(), 1);
        // 同じコメントは書き換えない
        document.setComment(tree.mainLine().at(2), QStringLiteral("テストコメント"));
        QCOMPARE(spy.count(), 1);
    }

    void setBookmark_roundTrip()
    {
        KifuBranchTree tree;
        setupBasicTree(tree);
        KifuDocument document(&tree);

        QSignalSpy spy(&document, &KifuDocument::bookmarkChanged);
        document.setBookmark(tree.mainLine().at(3), QStringLiteral("重要局面"));
        QCOMPARE(tree.mainLine().at(3)->bookmark(), QStringLiteral("重要局面"));
        QCOMPARE(spy.count(), 1);
    }

    /// 未保存かどうかは、ツリー・コメント・対局情報が保存した時点と違うかで決まる
    void modifiedComparesWithSavedState()
    {
        KifuBranchTree tree;
        setupBasicTree(tree);
        KifuDocument document(&tree);
        QVERIFY(!document.isModified());

        document.setComment(tree.mainLine().at(1), QStringLiteral("comment"));
        QVERIFY(document.isModified());
        document.markSaved(QStringLiteral("/tmp/a.kif"));
        QVERIFY(!document.isModified());
        QCOMPARE(document.filePath(), QStringLiteral("/tmp/a.kif"));

        // 手を足すと未保存
        addTestMove(tree, tree.root(), QStringLiteral("2g2f"), QStringLiteral("▲２六歩(27)"));
        QVERIFY(document.isModified());
        document.markSaved(QStringLiteral("/tmp/a.kif"));

        // 対局情報を編集して元に戻せば、未保存ではない
        QList<KifGameInfoItem> header = document.header();
        const QList<KifGameInfoItem> original = header;
        header.append({QStringLiteral("棋戦"), QStringLiteral("テスト棋戦")});
        document.editHeader(header);
        QVERIFY(document.isModified());
        document.editHeader(original);
        QVERIFY(!document.isModified());

        // 保存形式が拡張子で決まらないファイルは上書き保存先にしない
        document.markSaved(QStringLiteral("/tmp/a.sfen"));
        QVERIFY(document.filePath().isEmpty());
    }

    /// 指し手のない棋譜の対局情報だけを書いても（対局の前に棋戦・場所を書いた）、保存を促さない
    void headerOnlyEditOfEmptyRecordIsNotModified()
    {
        KifuBranchTree tree;
        tree.setRootSfen(kHirateSfen);
        KifuDocument document(&tree);
        QList<KifGameInfoItem> header = document.header();
        header.append({QStringLiteral("棋戦"), QStringLiteral("練習対局")});
        document.editHeader(header);
        QVERIFY(!document.isModified());

        // 新しい対局の棋譜にしても、書いた対局情報は残る
        document.startNew(kHirateSfen, true);
        QCOMPARE(document.header(), header);

        // 指し手を指した後は未保存
        addTestMove(tree, tree.root(), QStringLiteral("7g7f"), QStringLiteral("▲７六歩(77)"));
        QVERIFY(document.isModified());
    }

    /// 対局情報を残して新しい棋譜にするときは、対局情報欄で入力中のセルも残す
    void startNewKeepsPendingHeaderEdits()
    {
        KifuBranchTree tree;
        tree.setRootSfen(kHirateSfen);
        KifuDocument document(&tree);
        QList<KifGameInfoItem> pending = document.header();
        pending.append({QStringLiteral("場所"), QStringLiteral("将棋会館")});
        KifuDocument::Hooks hooks;
        hooks.commitPendingEdits = [&document, pending]() { document.editHeader(pending); };
        document.addPendingEditor(hooks);
        document.startNew(kHirateSfen, true);
        QCOMPARE(document.header(), pending);
    }

    /// 入力中の編集は未保存に数え、書き出す・保存するときに確定させる
    void pendingEditsAreCountedAndCommittedOnExport()
    {
        KifuBranchTree tree;
        setupBasicTree(tree);
        KifuDocument document(&tree);
        document.markSaved(QStringLiteral("/tmp/a.kif"));
        QList<KifGameInfoItem> pending = document.header();
        pending.append({QStringLiteral("場所"), QStringLiteral("将棋会館")});
        bool hasPending = true;
        KifuDocument::Hooks hooks;
        hooks.commitPendingEdits = [&document, &hasPending, pending]() {
            if (!hasPending) return;
            hasPending = false;
            document.editHeader(pending);
        };
        hooks.hasPendingEdits = [&hasPending]() { return hasPending; };
        document.addPendingEditor(hooks);

        QVERIFY(document.isModified());
        QVERIFY(document.header() != pending);  // 読むだけでは確定しない
        const QStringList kif = KifuExport::lines(document, KifuFormat::Kif);
        QVERIFY2(kif.contains(QStringLiteral("場所：将棋会館")), qPrintable(kif.join(QLatin1Char('\n'))));
        QCOMPARE(document.header(), pending);
        QVERIFY(document.isModified());
        document.markSaved(QStringLiteral("/tmp/a.kif"));
        QVERIFY(!document.isModified());
    }

    /// 貼り付けた棋譜はファイルにないので未保存で、上書き保存先も持たない
    void unsavedContentHasNoFile()
    {
        KifuBranchTree tree;
        setupBasicTree(tree);
        KifuDocument document(&tree);
        document.markSaved(QStringLiteral("/tmp/a.kif"));
        document.markUnsavedContent();
        QVERIFY(document.isModified());
        QVERIFY(document.filePath().isEmpty());
        document.markSaved(QStringLiteral("/tmp/b.kifu"));
        QVERIFY(!document.isModified());
    }

    /// 読み込み・新規は文書を置き換えるので、前の対局者・持ち時間・上書き保存先が残らない
    void replacementForgetsPreviousMatch()
    {
        KifuBranchTree tree;
        setupBasicTree(tree);
        KifuDocument document(&tree);
        MatchPlayers players;
        players.seat(Turn::Black).engine = true;
        players.seat(Turn::Black).engineName = QStringLiteral("Engine A");
        players.seat(Turn::White).humanName = QStringLiteral("人間");
        document.beginMatch(players, TimeControl{{60000, 0, 0}, {60000, 0, 0}}, false);
        document.recordMatchEnd();
        document.markSaved(QStringLiteral("/tmp/a.kif"));
        QCOMPARE(document.headerPlayerName(Turn::Black), QStringLiteral("Engine A"));
        QVERIFY(document.timeRecord().startDateTime().isValid());

        QSignalSpy reset(&document, &KifuDocument::headerReset);
        QSignalSpy playersChanged(&document, &KifuDocument::playersChanged);
        KifParseResult record;
        record.mainline.startSfen = kHirateSfen;
        const QList<KifGameInfoItem> header = {{QStringLiteral("先手"), QStringLiteral("読み込んだ先手")}};
        QVERIFY(document.replaceWithRecord(record, header));
        QCOMPARE(reset.count(), 1);
        QCOMPARE(playersChanged.count(), 1);
        QVERIFY(document.players().seat(Turn::Black).engineName.isEmpty());
        QVERIFY(!document.timeRecord().startDateTime().isValid());
        QVERIFY(document.filePath().isEmpty());
        QVERIFY(!document.isModified());
        QCOMPARE(document.header(), header);
        QCOMPARE(document.headerPlayerName(Turn::Black), QStringLiteral("読み込んだ先手"));

        // 新規は既定の対局情報。keepHeader なら書きかけの対局情報を残す
        document.startNew(kHirateSfen, true);
        QCOMPARE(document.header(), header);
        document.startNew(kHirateSfen);
        QCOMPARE(document.header(), KifuDocument::defaultHeader());
        QCOMPARE(tree.mainLine().size(), 1);
    }

    /// 対局を始めると、対局情報を対局者・手合割・持ち時間で作り直し、利用者が入力した項目は残す
    void beginMatchRebuildsHeaderKeepingUserItems()
    {
        KifuBranchTree tree;
        tree.setRootSfen(kHirateSfen);
        KifuDocument document(&tree);
        QList<KifGameInfoItem> header = document.header();
        for (auto& item : header) {
            if (item.key == QStringLiteral("棋戦")) item.value = QStringLiteral("練習対局");
        }
        document.editHeader(header);

        MatchPlayers players;
        players.seat(Turn::Black).humanName = QStringLiteral("先手さん");
        players.seat(Turn::White).humanName = QStringLiteral("後手さん");
        document.beginMatch(players, TimeControl{}, false);
        const QList<KifGameInfoItem> items = document.header();
        auto valueOf = [&items](const QString& key) {
            for (const auto& item : items) {
                if (item.key == key) return item.value;
            }
            return QString();
        };
        QCOMPARE(valueOf(QStringLiteral("先手")), QStringLiteral("先手さん"));
        QCOMPARE(valueOf(QStringLiteral("後手")), QStringLiteral("後手さん"));
        QCOMPARE(valueOf(QStringLiteral("持ち時間")), QStringLiteral("無制限"));
        QCOMPARE(valueOf(QStringLiteral("棋戦")), QStringLiteral("練習対局"));
        QVERIFY(!valueOf(QStringLiteral("開始日時")).isEmpty());

        // 終了日時は自動で書くので、未保存の理由にしない
        document.markSaved(QStringLiteral("/tmp/a.kif"));
        document.recordMatchEnd();
        QVERIFY(!document.isModified());
        QVERIFY(!document.header().isEmpty());

        // 棋譜の途中から続ける対局は、対局情報を書き換えない
        const QList<KifGameInfoItem> before = document.header();
        document.beginMatch(players, TimeControl{}, true);
        document.recordMatchEnd();
        QCOMPARE(document.header(), before);
    }

    // === Export: KIF ===

    void toKifLines()
    {
        KifuBranchTree tree;
        setupBasicTree(tree);

        KifuExportMetadata ctx;
        ctx.startSfen = kHirateSfen;
        ctx.gameInfoItems = {{QStringLiteral("手合割"), QStringLiteral("平手")}};
        auto lines = KifuExport::lines(tree, ctx, KifuFormat::Kif);
        QVERIFY(!lines.isEmpty());

        // 対局情報の「手合割：平手」をそのまま書く
        bool foundHandicap = false;
        for (const auto& line : std::as_const(lines)) {
            if (line.contains(QStringLiteral("手合割"))) {
                foundHandicap = true;
                break;
            }
        }
        QVERIFY(foundHandicap);
    }

    // === Export: KI2 ===

    void toKi2Lines()
    {
        KifuBranchTree tree;
        setupBasicTree(tree);

        KifuExportMetadata ctx;
        ctx.startSfen = kHirateSfen;
        auto lines = KifuExport::lines(tree, ctx, KifuFormat::Ki2);
        QVERIFY(!lines.isEmpty());

        // Should contain ▲ or △ markers
        bool foundMarker = false;
        for (const auto& line : std::as_const(lines)) {
            if (line.contains(QStringLiteral("▲")) || line.contains(QStringLiteral("△"))) {
                foundMarker = true;
                break;
            }
        }
        QVERIFY(foundMarker);
    }

    // === Export: JKF ===

    void toJkfLines()
    {
        // JKF は分岐ツリーのノードの指し手と局面から書くので、実際の指し手で木を作る
        KifuBranchTree tree;
        tree.setRootSfen(kHirateSfen);
        const QStringList usi = {"7g7f", "3c3d", "2g2f", "8c8d", "2f2e", "8d8e", "6i7h"};
        auto* tip = tree.root();
        for (const QString& move : usi) tip = addTestMove(tree, tip, move, move);

        KifuExportMetadata ctx;
        ctx.startSfen = kHirateSfen;
        auto lines = KifuExport::lines(tree, ctx, KifuFormat::Jkf);
        QVERIFY(!lines.isEmpty());

        // Should be valid JSON
        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(lines.join(QString()).toUtf8(), &parseError);
        QVERIFY2(parseError.error == QJsonParseError::NoError,
                 qPrintable(parseError.errorString()));
        QVERIFY(doc.isObject());
        const QJsonArray moves = doc.object()[QStringLiteral("moves")].toArray();
        QCOMPARE(moves.size(), usi.size() + 1);
        for (qsizetype i = 1; i < moves.size(); ++i) {
            QVERIFY(moves[i].toObject().contains(QStringLiteral("move")));
        }
    }

    // === Export: USEN ===

    void toUsenLines()
    {
        KifuBranchTree tree;
        setupBasicTree(tree);

        KifuExportMetadata ctx;
        ctx.startSfen = kHirateSfen;
        QStringList usiMoves = {
            QStringLiteral("7g7f"), QStringLiteral("3c3d"),
            QStringLiteral("2g2f"), QStringLiteral("8c8d"),
            QStringLiteral("2f2e"), QStringLiteral("8d8e"),
            QStringLiteral("6i7h")
        };
        auto lines = KifuExport::lines(tree, ctx, KifuFormat::Usen);
        QVERIFY(!lines.isEmpty());
        // USEN output typically starts with "~0."
        QVERIFY(lines.first().startsWith(QStringLiteral("~")));
    }

    // === Branch '+' marker (Kifu for Windows compatible) ===

    void toKifLines_branchPlusMarker()
    {
        // 分岐ツリー構造（ユーザー提示の棋譜を再現）:
        //
        //   root → ▲７六歩 → △３四歩 → ▲２六歩 → △８四歩 → ▲２五歩 → △投了
        //                              ├→ ▲１六歩 → △９四歩 → ▲４六歩 → △７四歩
        //                              │                      └→ ▲５六歩 → △５二金 → ▲５五歩
        //                              └→ ▲７七桂 → △９二香
        //                                                     └→ ▲１六歩 → △８五歩 → ▲投了
        //
        // Kifu for Windows準拠の '+' マーク期待値:
        //   本譜: ply3(▲２六歩)='+', ply5(▲２五歩)='+'
        //   変化5手(▲１六歩): 先頭='+なし' (最後の兄弟)
        //   変化3手(▲１六歩): ply3='+' (▲７七桂が後続), ply5(▲４六歩)='+' (▲５六歩が後続)
        //   変化5手(▲５六歩): 先頭='+なし' (最後の兄弟)
        //   変化3手(▲７七桂): 先頭='+なし' (最後の兄弟)

        KifuBranchTree tree;
        tree.setRootSfen(kHirateSfen);
        ShogiMove move;

        // 本譜
        auto* n1 = TestTree::addMove(tree, tree.root(), move, QStringLiteral("▲７六歩(77)"), QStringLiteral("s1"),
                                QStringLiteral("( 0:01/00:00:01)"));
        auto* n2 = TestTree::addMove(tree, n1, move, QStringLiteral("△３四歩(33)"), QStringLiteral("s2"),
                                QStringLiteral("( 0:02/00:00:02)"));
        auto* n3 = TestTree::addMove(tree, n2, move, QStringLiteral("▲２六歩(27)"), QStringLiteral("s3"),
                                QStringLiteral("( 0:02/00:00:03)"));
        auto* n4 = TestTree::addMove(tree, n3, move, QStringLiteral("△８四歩(83)"), QStringLiteral("s4"),
                                QStringLiteral("( 0:01/00:00:03)"));
        auto* n5 = TestTree::addMove(tree, n4, move, QStringLiteral("▲２五歩(26)"), QStringLiteral("s5"),
                                QStringLiteral("( 0:01/00:00:04)"));
        TestTree::addTerminalMove(tree, n5, TerminalType::Resign, Turn::White,
                             QStringLiteral("( 0:03/00:00:06)"));

        // 変化：5手（本譜の▲２五歩の兄弟）
        auto* v5 = TestTree::addMove(tree, n4, move, QStringLiteral("▲１六歩(17)"), QStringLiteral("v5"),
                                QStringLiteral("( 0:05/00:00:08)"));
        auto* v5b = TestTree::addMove(tree, v5, move, QStringLiteral("△８五歩(84)"), QStringLiteral("v5b"),
                                 QStringLiteral("( 0:04/00:00:07)"));
        TestTree::addTerminalMove(tree, v5b, TerminalType::Resign, Turn::Black,
                             QStringLiteral("( 0:02/00:00:10)"));

        // 変化：3手（本譜の▲２六歩の兄弟1: ▲１六歩）
        auto* v3a = TestTree::addMove(tree, n2, move, QStringLiteral("▲１六歩(17)"), QStringLiteral("v3a"),
                                 QStringLiteral("( 0:00/00:00:01)"));
        auto* v3a4 = TestTree::addMove(tree, v3a, move, QStringLiteral("△９四歩(93)"), QStringLiteral("v3a4"),
                                  QStringLiteral("( 0:00/00:00:02)"));
        auto* v3a5 = TestTree::addMove(tree, v3a4, move, QStringLiteral("▲４六歩(47)"), QStringLiteral("v3a5"),
                                  QStringLiteral("( 0:00/00:00:01)"));
        TestTree::addMove(tree, v3a5, move, QStringLiteral("△７四歩(73)"), QStringLiteral("v3a6"),
                     QStringLiteral("( 0:00/00:00:02)"));

        // 変化：5手（▲１六歩ラインの▲４六歩の兄弟: ▲５六歩）
        auto* v3a5b = TestTree::addMove(tree, v3a4, move, QStringLiteral("▲５六歩(57)"), QStringLiteral("v3a5b"),
                                   QStringLiteral("( 0:00/00:00:01)"));
        auto* v3a5b6 = TestTree::addMove(tree, v3a5b, move, QStringLiteral("△５二金(61)"), QStringLiteral("v3a5b6"),
                                    QStringLiteral("( 0:00/00:00:02)"));
        TestTree::addMove(tree, v3a5b6, move, QStringLiteral("▲５五歩(56)"), QStringLiteral("v3a5b7"),
                     QStringLiteral("( 0:00/00:00:01)"));

        // 変化：3手（本譜の▲２六歩の兄弟2: ▲７七桂）
        auto* v3b = TestTree::addMove(tree, n2, move, QStringLiteral("▲７七桂(89)"), QStringLiteral("v3b"),
                                 QStringLiteral("( 0:00/00:00:01)"));
        TestTree::addMove(tree, v3b, move, QStringLiteral("△９二香(91)"), QStringLiteral("v3b4"),
                     QStringLiteral("( 0:00/00:00:02)"));

        // モデルセットアップ

        KifuExportMetadata ctx;
        ctx.startSfen = kHirateSfen;
        auto lines = KifuExport::lines(tree, ctx, KifuFormat::Kif);

        // '+' マーク付き行と '+' マークなし行を検証するヘルパー
        auto findLine = [&lines](const QString& keyword) -> QString {
            for (const auto& line : std::as_const(lines)) {
                if (line.contains(keyword)) return line;
            }
            return {};
        };

        // 本譜: ply3(▲２六歩) は '+' あり（▲１六歩,▲７七桂が後続）
        QString mainPly3 = findLine(QStringLiteral("２六歩(27)"));
        QVERIFY2(mainPly3.endsWith(QStringLiteral("+")),
                 qPrintable(QStringLiteral("Main ply3 should have '+': ") + mainPly3));

        // 本譜: ply5(▲２五歩) は '+' あり（▲１六歩が後続）
        QString mainPly5 = findLine(QStringLiteral("２五歩(26)"));
        QVERIFY2(mainPly5.endsWith(QStringLiteral("+")),
                 qPrintable(QStringLiteral("Main ply5 should have '+': ") + mainPly5));

        // 変化5手の▲１六歩(17): '+' なし（最後の兄弟）
        // ▲１六歩(17) は2箇所あるので、「変化：5手」の直後の行を探す
        bool foundVar5 = false;
        for (int i = 0; i < lines.size() - 1; ++i) {
            if (lines.at(i) == QStringLiteral("変化：5手")) {
                const QString& nextLine = lines.at(i + 1);
                if (nextLine.contains(QStringLiteral("１六歩(17)"))) {
                    QVERIFY2(!nextLine.endsWith(QStringLiteral("+")),
                             qPrintable(QStringLiteral("Var5 16fu should NOT have '+': ") + nextLine));
                    foundVar5 = true;
                    break;
                }
            }
        }
        QVERIFY2(foundVar5, "Should find variation at ply 5 with 16fu");

        // 変化3手の▲１六歩(17): '+' あり（▲７七桂が後続兄弟）
        bool foundVar3a = false;
        for (int i = 0; i < lines.size() - 1; ++i) {
            if (lines.at(i) == QStringLiteral("変化：3手")) {
                const QString& nextLine = lines.at(i + 1);
                if (nextLine.contains(QStringLiteral("１六歩(17)"))) {
                    QVERIFY2(nextLine.endsWith(QStringLiteral("+")),
                             qPrintable(QStringLiteral("Var3a 16fu should have '+': ") + nextLine));
                    foundVar3a = true;
                    break;
                }
            }
        }
        QVERIFY2(foundVar3a, "Should find variation at ply 3 with 16fu");

        // 変化3手の▲７七桂(89): '+' なし（最後の兄弟）
        bool foundVar3b = false;
        for (int i = 0; i < lines.size() - 1; ++i) {
            if (lines.at(i) == QStringLiteral("変化：3手")) {
                const QString& nextLine = lines.at(i + 1);
                if (nextLine.contains(QStringLiteral("７七桂(89)"))) {
                    QVERIFY2(!nextLine.endsWith(QStringLiteral("+")),
                             qPrintable(QStringLiteral("Var3b 77kei should NOT have '+': ") + nextLine));
                    foundVar3b = true;
                    break;
                }
            }
        }
        QVERIFY2(foundVar3b, "Should find variation at ply 3 with 77kei");

        // 変化5手の▲５六歩(57): '+' なし（最後の兄弟）
        bool foundVar5b = false;
        for (int i = 0; i < lines.size() - 1; ++i) {
            if (lines.at(i) == QStringLiteral("変化：5手")) {
                const QString& nextLine = lines.at(i + 1);
                if (nextLine.contains(QStringLiteral("５六歩(57)"))) {
                    QVERIFY2(!nextLine.endsWith(QStringLiteral("+")),
                             qPrintable(QStringLiteral("Var5b 56fu should NOT have '+': ") + nextLine));
                    foundVar5b = true;
                    break;
                }
            }
        }
        QVERIFY2(foundVar5b, "Should find variation at ply 5 with 56fu");
    }

    // KI2 の変化：分岐前の手順を正しい手番で並べ直し、同じ地点へ動ける駒の区別（右・左）を付ける
    void ki2VariationDisambiguatesWithCorrectSide()
    {
        KifuBranchTree tree;
        tree.setRootSfen(kHirateSfen);
        auto* m1 = addTestMove(tree, tree.root(), QStringLiteral("7g7f"), QStringLiteral("▲７六歩(77)"));
        addTestMove(tree, m1, QStringLiteral("3c3d"), QStringLiteral("△３四歩(33)"));
        // ２手目の変化：後手の金は 41・61 のどちらも 52 へ動ける
        addTestMove(tree, m1, QStringLiteral("4a5b"), QStringLiteral("△５二金(41)"));
        KifuExportMetadata ctx;
        ctx.startSfen = kHirateSfen;

        QTemporaryFile ki2;
        QVERIFY(KifuTestHelper::writeToTempFile(ki2, KifuExport::lines(tree, ctx, KifuFormat::Ki2).join(QLatin1Char('\n')).toUtf8(),
                                                QStringLiteral("ki2u")));
        KifParseResult result;
        QString error;
        QVERIFY2(KifuFile::parse<KifuFormat::Ki2>(ki2.fileName(), result, &error), qPrintable(error));
        QCOMPARE(result.variations.size(), 1);
        QCOMPARE(result.variations.at(0).line.usiMoves(), QStringList({QStringLiteral("4a5b")}));
    }

    // KI2 の結果行から終局の種類（詰み・切れ負けなど）を読み戻せる
    void ki2TerminalKindsRoundTrip()
    {
        KifuBranchTree tree;
        tree.setRootSfen(kHirateSfen);
        auto* m1 = addTestMove(tree, tree.root(), QStringLiteral("7g7f"), QStringLiteral("▲７六歩(77)"));
        auto* m2 = addTestMove(tree, m1, QStringLiteral("3c3d"), QStringLiteral("△３四歩(33)"));
        TestTree::addTerminalMove(tree, m2, TerminalType::Checkmate, Turn::Black);
        auto* v2 = addTestMove(tree, m1, QStringLiteral("8c8d"), QStringLiteral("△８四歩(83)"));
        TestTree::addTerminalMove(tree, v2, TerminalType::Timeout, Turn::Black);
        KifuExportMetadata ctx;
        ctx.startSfen = kHirateSfen;
        const QString text = KifuExport::lines(tree, ctx, KifuFormat::Ki2).join(QLatin1Char('\n'));
        QVERIFY2(text.contains(QStringLiteral("まで2手で詰み")), qPrintable(text));
        QVERIFY2(text.contains(QStringLiteral("時間切れ")), qPrintable(text));

        QTemporaryFile ki2;
        QVERIFY(KifuTestHelper::writeToTempFile(ki2, text.toUtf8(), QStringLiteral("ki2u")));
        KifParseResult result;
        QString error;
        QVERIFY2(KifuFile::parse<KifuFormat::Ki2>(ki2.fileName(), result, &error), qPrintable(error));
        QVERIFY(result.mainline.entries.last().displayText.contains(QStringLiteral("詰み")));
        QCOMPARE(result.variations.size(), 1);
        QCOMPARE(result.variations.at(0).line.entries.last().terminal, TerminalType::Timeout);
    }

    // 手合割：駒落ちの初期配置は名前で、それ以外の局面は「その他」
    void handicapLabelRecognizesPresets()
    {
        QCOMPARE(KifuExportMetadataBuilder::handicapLabel(kHirateSfen), QStringLiteral("平手"));
        QCOMPARE(KifuExportMetadataBuilder::handicapLabel(
                     QStringLiteral("lnsgkgsnl/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1")),
                 QStringLiteral("二枚落ち"));
        // 持駒がある・手番が違うなら駒落ちの初期局面ではない
        QCOMPARE(KifuExportMetadataBuilder::handicapLabel(
                     QStringLiteral("lnsgkgsnl/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w P 1")),
                 QStringLiteral("その他"));
        QCOMPARE(KifuExportMetadataBuilder::handicapLabel(
                     QStringLiteral("lnsgkgsnl/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1")),
                 QStringLiteral("その他"));
    }
};

QTEST_MAIN(TestKifuDocument)
#include "tst_kifudocument.moc"
