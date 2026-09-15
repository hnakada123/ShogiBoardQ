#include <QtTest>
#include <QSignalSpy>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QTemporaryFile>
#include <QClipboard>
#include <QTableWidget>
#include "bodtextgenerator.h"
#include "csaformatter.h"
#include "kifuclipboardservice.h"

#include "gamerecordmodel.h"
#include "kifubranchtree.h"
#include "kifubranchnode.h"
#include "kifunavigationstate.h"
#include "kifdisplayitem.h"
#include "kiftosfenconverter.h"
#include "ki2tosfenconverter.h"
#include "csatosfenconverter.h"
#include "jkftosfenconverter.h"
#include "usitosfenconverter.h"
#include "usentosfenconverter.h"
#include "sfenpositiontracer.h"
#include "livegamesession.h"
#include "kifuexportclipboard.h"
#include "kifu_test_helper.h"

static const QString kHirateSfen =
    QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");

class TestGameRecordModel : public QObject
{
    Q_OBJECT

private:
    static KifuBranchNode* addTestMove(KifuBranchTree& tree, KifuBranchNode* parent,
                                       const QString& usi, const QString& pretty,
                                       const QString& time = {})
    {
        const QStringList positions = SfenPositionTracer::buildSfenRecord(parent->sfen(), {usi}, false);
        return tree.addMove(parent, ShogiMove(), pretty, positions.last(), time);
    }

    void setupBasicModel(GameRecordModel& model, KifuBranchTree& tree,
                         KifuNavigationState& navState, QList<KifDisplayItem>& disp)
    {
        tree.setRootSfen(kHirateSfen);
        ShogiMove move;
        auto* current = tree.root();

        QStringList moves = {
            QStringLiteral("▲７六歩"), QStringLiteral("△３四歩"),
            QStringLiteral("▲２六歩"), QStringLiteral("△８四歩"),
            QStringLiteral("▲２五歩"), QStringLiteral("△８五歩"),
            QStringLiteral("▲７八金")
        };

        for (int i = 0; i < moves.size(); ++i) {
            current = tree.addMove(current, move, moves[i],
                                   QStringLiteral("sfen%1").arg(i + 1));
        }

        disp.clear();
        disp.append(KifDisplayItem(QStringLiteral("開始局面")));
        for (int i = 0; i < moves.size(); ++i) {
            disp.append(KifDisplayItem(moves[i], QString(), QString(), i + 1));
        }

        navState.setTree(&tree);
        navState.goToRoot();

        model.setBranchTree(&tree);
        model.setNavigationState(&navState);
        model.bind(&disp);
        model.initializeFromDisplayItems(disp, static_cast<int>(disp.size()));
    }

private slots:
    void allFormatsRoundTripCustomPosition_data()
    {
        QTest::addColumn<QString>("format");
        for (const auto& format : {"kif", "ki2", "csa", "jkf", "usi", "usen"})
            QTest::newRow(format) << QString::fromLatin1(format);
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
        GameRecordModel model;
        model.setBranchTree(&tree);
        GameRecordModel::ExportContext context;
        context.startSfen = initial;
        QStringList output;
        if (format == "kif") output = model.toKifLines(context);
        else if (format == "ki2") output = model.toKi2Lines(context);
        else if (format == "csa") output = model.toCsaLines(context, usi);
        else if (format == "jkf") output = model.toJkfLines(context);
        else if (format == "usi") output = model.toUsiLines(context, usi);
        else output = model.toUsenLines(context, usi);
        QTemporaryFile file;
        QVERIFY(KifuTestHelper::writeToTempFile(file, output.join(QLatin1Char('\n')).toUtf8(), format));
        KifParseResult result;
        QString error;
        bool parsed = false;
        if (format == "kif") parsed = KifToSfenConverter::parseWithVariations(file.fileName(), result, &error);
        else if (format == "ki2") parsed = Ki2ToSfenConverter::parseWithVariations(file.fileName(), result, &error);
        else if (format == "csa") parsed = CsaToSfenConverter::parse(file.fileName(), result, &error);
        else if (format == "jkf") parsed = JkfToSfenConverter::parseWithVariations(file.fileName(), result, &error);
        else if (format == "usi") parsed = UsiToSfenConverter::parseWithVariations(file.fileName(), result, &error);
        else parsed = UsenToSfenConverter::parseWithVariations(file.fileName(), result, &error);
        QVERIFY2(parsed, qPrintable(error));
        QCOMPARE(result.mainline.baseSfen, initial);
        QCOMPARE(result.mainline.usiMoves, usi);
        QCOMPARE(SfenPositionTracer::buildSfenRecord(result.mainline.baseSfen, result.mainline.usiMoves, false).last(), tip->sfen());
    }

    void kifBodUsesDeclaredTurn_data()
    {
        QTest::addColumn<bool>("withBod");
        QTest::newRow("bod") << true;
        QTest::newRow("header-only") << false;
    }

    void kifBodUsesDeclaredTurn()
    {
        QFETCH(bool, withBod);
        QString initial = kHirateSfen;
        initial.replace(QStringLiteral(" b "), QStringLiteral(" w "));
        const QString text = (withBod ? BodTextGenerator::generate(initial, 0, {}) : QStringLiteral("後手番"))
            + QStringLiteral("\n手数----指手---------消費時間--\n1 ３四歩(33)\n2 投了\n");
        QTemporaryFile file;
        QVERIFY(KifuTestHelper::writeToTempFile(file, text.toUtf8(), QStringLiteral("kif")));
        QCOMPARE(KifToSfenConverter::detectInitialSfenFromFile(file.fileName()), initial);
        const auto items = KifToSfenConverter::extractMovesWithTimes(file.fileName());
        QCOMPARE(items.size(), 3);
        QCOMPARE(items[1].ply, 1);
        QVERIFY(items[1].prettyMove.startsWith(QStringLiteral("△")));
        QCOMPARE(items[2].ply, 2);
        QCOMPARE(items[2].prettyMove, QStringLiteral("▲投了"));
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
        const QString initial = KifToSfenConverter::mapHandicapToSfen(QStringLiteral("角落ち"));
        KifuBranchTree tree;
        tree.setRootSfen(initial);
        auto* tip = tree.root();
        if (moveCount >= 1) tip = addTestMove(tree, tip, QStringLiteral("3c3d"), QStringLiteral("△３四歩(33)"));
        if (moveCount >= 2) tip = addTestMove(tree, tip, QStringLiteral("7g7f"), QStringLiteral("▲７六歩(77)"));
        tree.addTerminalMove(tip, TerminalType::Resign,
                             moveCount % 2 == 0 ? QStringLiteral("△投了") : QStringLiteral("▲投了"));
        addTestMove(tree, tree.root(), QStringLiteral("8c8d"), QStringLiteral("△８四歩(83)"));
        GameRecordModel model;
        model.setBranchTree(&tree);
        GameRecordModel::ExportContext ctx;
        ctx.startSfen = initial;
        const QStringList lines = format == QStringLiteral("kif") ? model.toKifLines(ctx) : model.toKi2Lines(ctx);
        QVERIFY(lines.contains(QStringLiteral("まで%1手で%2の勝ち").arg(moveCount)
            .arg(moveCount % 2 == 0 ? QStringLiteral("先手") : QStringLiteral("後手"))));
        QTemporaryFile file;
        QVERIFY(KifuTestHelper::writeToTempFile(file, lines.join(QLatin1Char('\n')).toUtf8(), format));
        KifParseResult result;
        QString error;
        const bool ok = format == QStringLiteral("kif")
            ? KifToSfenConverter::parseWithVariations(file.fileName(), result, &error)
            : Ki2ToSfenConverter::parseWithVariations(file.fileName(), result, &error);
        QVERIFY2(ok, qPrintable(error));
        QCOMPARE(result.mainline.usiMoves.size(), moveCount);
        QCOMPARE(result.mainline.disp[1].ply, 1);
        QVERIFY(result.mainline.disp[1].prettyMove.startsWith(QStringLiteral("△")));
        QCOMPARE(result.variations.size(), 1);
        QCOMPARE(result.variations[0].line.usiMoves, QStringList({"8c8d"}));
        QCOMPARE(result.variations[0].line.disp[0].ply, 1);
        QVERIFY(result.variations[0].line.disp[0].prettyMove.startsWith(QStringLiteral("△")));
    }

    void usenWhiteToMoveBranchDrop()
    {
        QString initial = kHirateSfen;
        initial.replace(QStringLiteral(" b - "), QStringLiteral(" w p "));
        KifuBranchTree tree;
        tree.setRootSfen(initial);
        addTestMove(tree, tree.root(), QStringLiteral("3c3d"), QStringLiteral("△３四歩(33)"));
        addTestMove(tree, tree.root(), QStringLiteral("P*5e"), QStringLiteral("△５五歩打"));
        GameRecordModel model;
        model.setBranchTree(&tree);
        GameRecordModel::ExportContext ctx;
        ctx.startSfen = initial;
        QTemporaryFile file;
        QVERIFY(KifuTestHelper::writeToTempFile(file, model.toUsenLines(ctx, {"3c3d"})
            .join(QLatin1Char('\n')).toUtf8(), QStringLiteral("usen")));
        KifParseResult result;
        QString error;
        QVERIFY2(UsenToSfenConverter::parseWithVariations(file.fileName(), result, &error), qPrintable(error));
        QCOMPARE(result.variations.size(), 1);
        QCOMPARE(result.variations[0].line.usiMoves, QStringList({"P*5e"}));
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
        const QString initial = QStringLiteral("4k4/9/9/9/9/5+%13/9/9/4K4 b - 1").arg(piece);
        KifuBranchTree tree;
        tree.setRootSfen(initial);
        addTestMove(tree, tree.root(), QStringLiteral("4f5e"), QStringLiteral("▲５五%1(46)").arg(kanji));
        GameRecordModel model;
        model.setBranchTree(&tree);
        GameRecordModel::ExportContext ctx;
        ctx.startSfen = initial;
        const QByteArray json = model.toJkfLines(ctx).join(QLatin1Char('\n')).toUtf8();
        const auto moves = QJsonDocument::fromJson(json).object()[QStringLiteral("moves")].toArray();
        QCOMPARE(moves[1].toObject()[QStringLiteral("move")].toObject()[QStringLiteral("piece")].toString(), code);
        QTemporaryFile file;
        QVERIFY(KifuTestHelper::writeToTempFile(file, json, QStringLiteral("jkf")));
        KifParseResult result;
        QString error;
        QVERIFY2(JkfToSfenConverter::parseWithVariations(file.fileName(), result, &error), qPrintable(error));
        QCOMPARE(result.mainline.usiMoves, QStringList({"4f5e"}));
        QCOMPARE(result.mainline.sfenList.last(), tree.mainLine().last()->sfen());
    }

    void csaTimeUsesMinutesAndSeconds()
    {
        QCOMPARE(CsaFormatter::convertToCsaTime(QStringLiteral("10:30+30")), QStringLiteral("$TIME:630+30+0"));
        QCOMPARE(CsaFormatter::convertToCsaTime(QStringLiteral("120:00+60")), QStringLiteral("$TIME:7200+60+0"));
        QCOMPARE(CsaFormatter::convertToCsaTime(QStringLiteral("600+30+5")), QStringLiteral("$TIME:600+30+5"));
    }

    void clipboardKeepsTimeMetadata()
    {
        KifuBranchTree tree;
        tree.setRootSfen(kHirateSfen);
        GameRecordModel model;
        model.setBranchTree(&tree);
        QTableWidget emptyInfo;
        GameRecordModel::ExportContext ctx;
        ctx.gameInfoTable = &emptyInfo;
        ctx.startSfen = kHirateSfen;
        ctx.hasTimeControl = true;
        ctx.initialTimeMs = 630000;
        ctx.byoyomiMs = 30000;
        ctx.fischerIncrementMs = 5000;
        ctx.gameStartDateTime = QDateTime(QDate(2025, 1, 2), QTime(3, 4, 5));
        ctx.gameEndDateTime = QDateTime(QDate(2025, 1, 2), QTime(6, 7, 8));
        QVERIFY(KifuClipboardService::copyKif(model, ctx));
        QCOMPARE(QApplication::clipboard()->text(), model.toKifLines(ctx).join(QLatin1Char('\n')));
        QVERIFY(KifuClipboardService::copyKi2(model, ctx));
        QCOMPARE(QApplication::clipboard()->text(), model.toKi2Lines(ctx).join(QLatin1Char('\n')));
        QVERIFY(QApplication::clipboard()->text().contains(QStringLiteral("2025/01/02 06:07:08")));
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
        QStringList sessionSfens = SfenPositionTracer::buildSfenRecord(anchor->sfen(), sessionUsi, false);

        LiveGameSession session;
        session.setTree(&tree);
        session.startFromNode(anchor);
        session.addMove(ShogiMove(), newPretty, sessionSfens.last(), QStringLiteral("00:05/00:00:05"));
        QVERIFY(session.commit());

        GameRecordModel model;
        model.setBranchTree(&tree);
        KifuExportClipboard exporter(nullptr);
        KifuExportClipboard::Deps deps;
        deps.gameRecord = &model;
        deps.usiMoves = &sessionUsi;
        deps.sfenRecord = &sessionSfens;
        deps.startSfenStr = anchor->sfen();
        exporter.setDependencies(deps);
        if (format == QStringLiteral("csa")) QVERIFY(exporter.copyCsaToClipboard());
        else if (format == QStringLiteral("usi")) QVERIFY(exporter.copyUsiToClipboard());
        else QVERIFY(exporter.copyUsenToClipboard());

        QTemporaryFile tmp;
        const QByteArray text = QApplication::clipboard()->text().toUtf8();
        QVERIFY(KifuTestHelper::writeToTempFile(tmp, text, format));
        KifParseResult result;
        QString error;
        bool ok;
        if (format == QStringLiteral("csa")) ok = CsaToSfenConverter::parse(tmp.fileName(), result, &error);
        else if (format == QStringLiteral("usi")) ok = UsiToSfenConverter::parseWithVariations(tmp.fileName(), result, &error);
        else ok = UsenToSfenConverter::parseWithVariations(tmp.fileName(), result, &error);
        QVERIFY2(ok, qPrintable(error));
        QStringList expected = {QStringLiteral("7g7f"), QStringLiteral("3c3d")};
        if (!branch) expected.append(newUsi);
        QCOMPARE(result.mainline.baseSfen, kHirateSfen);
        QCOMPARE(result.mainline.usiMoves, expected);
        QCOMPARE(model.collectMainlineUsiForExport(), expected);

        // 現在手までのUSIコピーでは、本譜の先頭から指定手数に制限する。
        deps.currentMoveIndex = 1;
        exporter.setDependencies(deps);
        QVERIFY(exporter.copyUsiCurrentToClipboard());
        QCOMPARE(QApplication::clipboard()->text(), QStringLiteral("position startpos moves 7g7f"));
    }

    void exportUsiSource_ignoresTerminalAndSessionFallback()
    {
        KifuBranchTree tree;
        tree.setRootSfen(kHirateSfen);
        auto* first = addTestMove(tree, tree.root(), QStringLiteral("7g7f"), QStringLiteral("▲７六歩(77)"));
        tree.addMove(first, ShogiMove(), QStringLiteral("△投了"), first->sfen());
        GameRecordModel model;
        model.setBranchTree(&tree);
        QCOMPARE(model.collectMainlineUsiForExport(), QStringList({QStringLiteral("7g7f")}));

        // 開始局面だけの本譜に、別セッションの指し手を混ぜない。
        tree.setRootSfen(kHirateSfen);
        QStringList staleUsi = {QStringLiteral("2g2f")};
        KifuExportClipboard exporter(nullptr);
        KifuExportClipboard::Deps deps;
        deps.gameRecord = &model;
        deps.usiMoves = &staleUsi;
        deps.startSfenStr = kHirateSfen;
        exporter.setDependencies(deps);
        QVERIFY(exporter.copyUsiToClipboard());
        QCOMPARE(QApplication::clipboard()->text(), QStringLiteral("position startpos"));

        // ツリーがなければ従来どおり対局用データを出力する。
        model.setBranchTree(nullptr);
        QVERIFY(exporter.copyUsiToClipboard());
        QCOMPARE(QApplication::clipboard()->text(), QStringLiteral("position startpos moves 2g2f"));
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
        tree.root()->setDisplayText(openingText);
        tree.root()->setComment(QStringLiteral("opening comment"));
        auto* first = addTestMove(tree, tree.root(), QStringLiteral("7g7f"),
                                 QStringLiteral("▲７六歩(77)"), QStringLiteral("00:12/00:00:12"));
        first->setComment(QStringLiteral("first comment"));
        auto* last = addTestMove(tree, first, QStringLiteral("3c3d"),
                                QStringLiteral("△３四歩(33)"), QStringLiteral("00:34/00:00:34"));
        last->setComment(QStringLiteral("last comment"));

        GameRecordModel model;
        model.setBranchTree(&tree);
        GameRecordModel::ExportContext ctx;
        ctx.startSfen = kHirateSfen;
        const QStringList expectedMoves = {QStringLiteral("7g7f"), QStringLiteral("3c3d")};
        QStringList output;
        if (format == QStringLiteral("kif")) output = model.toKifLines(ctx);
        else if (format == QStringLiteral("ki2")) output = model.toKi2Lines(ctx);
        else if (format == QStringLiteral("csa")) output = model.toCsaLines(ctx, expectedMoves);
        else output = model.toJkfLines(ctx);

        QTemporaryFile tmp;
        QVERIFY(KifuTestHelper::writeToTempFile(tmp, output.join(QLatin1Char('\n')).toUtf8(), format));
        KifParseResult result;
        QString error;
        bool ok;
        if (format == QStringLiteral("kif")) ok = KifToSfenConverter::parseWithVariations(tmp.fileName(), result, &error);
        else if (format == QStringLiteral("ki2")) ok = Ki2ToSfenConverter::parseWithVariations(tmp.fileName(), result, &error);
        else if (format == QStringLiteral("csa")) ok = CsaToSfenConverter::parse(tmp.fileName(), result, &error);
        else ok = JkfToSfenConverter::parseWithVariations(tmp.fileName(), result, &error);
        QVERIFY2(ok, qPrintable(error));
        QCOMPARE(result.mainline.usiMoves, expectedMoves);
        QVERIFY(result.mainline.disp.size() >= 3);
        QCOMPARE(result.mainline.disp[0].comment.trimmed(), QStringLiteral("opening comment"));
        QCOMPARE(result.mainline.disp[1].comment.trimmed(), QStringLiteral("first comment"));
        QCOMPARE(result.mainline.disp[2].comment.trimmed(), QStringLiteral("last comment"));
        if (format != QStringLiteral("ki2")) {
            QVERIFY(result.mainline.disp[1].timeText.contains(QStringLiteral("00:12")));
            QVERIFY(result.mainline.disp[2].timeText.contains(QStringLiteral("00:34")));
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

        GameRecordModel model;
        model.setBranchTree(&tree);
        GameRecordModel::ExportContext ctx;
        ctx.startSfen = kHirateSfen;
        const QByteArray json = model.toJkfLines(ctx).join(QLatin1Char('\n')).toUtf8();
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
        QVERIFY2(JkfToSfenConverter::parseWithVariations(tmp.fileName(), result, &error), qPrintable(error));
        QCOMPARE(result.mainline.usiMoves, QStringList({QStringLiteral("7g7f"), QStringLiteral("3c3d"), QStringLiteral("2g2f")}));
        QCOMPARE(result.variations.size(), 3);
        QCOMPARE(result.variations[0].startPly, 1);
        QCOMPARE(result.variations[0].line.usiMoves, QStringList({QStringLiteral("2g2f")}));
        QCOMPARE(result.variations[1].startPly, 2);
        QCOMPARE(result.variations[1].line.usiMoves, QStringList({QStringLiteral("8c8d"), QStringLiteral("1g1f")}));
        QCOMPARE(result.variations[2].startPly, 3);
        QCOMPARE(result.variations[2].line.usiMoves, QStringList({QStringLiteral("1g1f")}));

        // 分岐内の分岐も、代替される子の手に forks を付ける。
        addTestMove(tree, alternateSecond, QStringLiteral("9g9f"), QStringLiteral("▲９六歩(97)"));
        const auto nestedMoves = QJsonDocument::fromJson(model.toJkfLines(ctx).join(QLatin1Char('\n')).toUtf8())
            .object()[QStringLiteral("moves")].toArray();
        const auto secondFork = nestedMoves[2].toObject()[QStringLiteral("forks")].toArray()[0].toArray();
        QCOMPARE(secondFork.size(), 2);
        QVERIFY(!secondFork[0].toObject().contains(QStringLiteral("forks")));
        QCOMPARE(secondFork[1].toObject()[QStringLiteral("forks")].toArray().size(), 1);
        QTemporaryFile nestedFile;
        QVERIFY(KifuTestHelper::writeToTempFile(nestedFile, model.toJkfLines(ctx).join(QLatin1Char('\n')).toUtf8(), QStringLiteral("jkf")));
        QVERIFY2(JkfToSfenConverter::parseWithVariations(nestedFile.fileName(), result, &error), qPrintable(error));
        QCOMPARE(result.variations.size(), 4);
        QCOMPARE(result.variations[2].startPly, 3);
        QCOMPARE(result.variations[2].line.usiMoves, QStringList({"9g9f"}));
        QCOMPARE(result.variations[2].line.baseSfen, alternateSecond->sfen());
        QCOMPARE(result.variations[2].line.sfenList.size(), 2);
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
        GameRecordModel model;
        model.setBranchTree(&tree);
        GameRecordModel::ExportContext ctx;
        ctx.startSfen = kHirateSfen;
        QTemporaryFile tmp;
        QVERIFY(KifuTestHelper::writeToTempFile(tmp, model.toJkfLines(ctx).join(QLatin1Char('\n')).toUtf8(),
                                               QStringLiteral("jkf")));
        KifParseResult result;
        QString error;
        QVERIFY2(JkfToSfenConverter::parseWithVariations(tmp.fileName(), result, &error), qPrintable(error));
        QCOMPARE(result.variations.size(), 1);
        QCOMPARE(result.variations[0].startPly, 4);
        QCOMPARE(result.variations[0].line.usiMoves, QStringList({QStringLiteral("3a2b")}));
    }

    // === Comment Management ===

    void setComment_roundTrip()
    {
        GameRecordModel model;
        KifuBranchTree tree;
        KifuNavigationState navState;
        QList<KifDisplayItem> disp;
        setupBasicModel(model, tree, navState, disp);

        model.setComment(1, QStringLiteral("初手のコメント"));
        QCOMPARE(model.comment(1), QStringLiteral("初手のコメント"));
    }

    void commentChanged_signal()
    {
        GameRecordModel model;
        KifuBranchTree tree;
        KifuNavigationState navState;
        QList<KifDisplayItem> disp;
        setupBasicModel(model, tree, navState, disp);

        QSignalSpy spy(&model, &GameRecordModel::commentChanged);
        QVERIFY(spy.isValid());

        model.setComment(2, QStringLiteral("テストコメント"));
        QCOMPARE(spy.count(), 1);
    }

    // === Bookmark Management ===

    void setBookmark_roundTrip()
    {
        GameRecordModel model;
        KifuBranchTree tree;
        KifuNavigationState navState;
        QList<KifDisplayItem> disp;
        setupBasicModel(model, tree, navState, disp);

        model.setBookmark(3, QStringLiteral("重要局面"));
        QCOMPARE(model.bookmark(3), QStringLiteral("重要局面"));
    }

    // === Initialize / Clear ===

    void clear_resetsAll()
    {
        GameRecordModel model;
        KifuBranchTree tree;
        KifuNavigationState navState;
        QList<KifDisplayItem> disp;
        setupBasicModel(model, tree, navState, disp);

        model.setComment(1, QStringLiteral("test"));
        model.clear();

        QCOMPARE(model.commentCount(), 0);
        QVERIFY(!model.isDirty());
    }

    // === Dirty Flag ===

    void dirty_flag()
    {
        GameRecordModel model;
        KifuBranchTree tree;
        KifuNavigationState navState;
        QList<KifDisplayItem> disp;
        setupBasicModel(model, tree, navState, disp);

        QVERIFY(!model.isDirty());
        model.setComment(1, QStringLiteral("comment"));
        QVERIFY(model.isDirty());
        model.clearDirty();
        QVERIFY(!model.isDirty());
    }

    // === Export: KIF ===

    void toKifLines()
    {
        GameRecordModel model;
        KifuBranchTree tree;
        KifuNavigationState navState;
        QList<KifDisplayItem> disp;
        setupBasicModel(model, tree, navState, disp);

        GameRecordModel::ExportContext ctx;
        ctx.startSfen = kHirateSfen;
        auto lines = model.toKifLines(ctx);
        QVERIFY(!lines.isEmpty());

        // Should contain "手合割：平手"
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
        GameRecordModel model;
        KifuBranchTree tree;
        KifuNavigationState navState;
        QList<KifDisplayItem> disp;
        setupBasicModel(model, tree, navState, disp);

        GameRecordModel::ExportContext ctx;
        ctx.startSfen = kHirateSfen;
        auto lines = model.toKi2Lines(ctx);
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
        GameRecordModel model;
        KifuBranchTree tree;
        KifuNavigationState navState;
        QList<KifDisplayItem> disp;
        setupBasicModel(model, tree, navState, disp);

        GameRecordModel::ExportContext ctx;
        ctx.startSfen = kHirateSfen;
        auto lines = model.toJkfLines(ctx);
        QVERIFY(!lines.isEmpty());

        // Should be valid JSON
        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(lines.join(QString()).toUtf8(), &parseError);
        QVERIFY2(parseError.error == QJsonParseError::NoError,
                 qPrintable(parseError.errorString()));
        QVERIFY(doc.isObject());
    }

    // === Export: USEN ===

    void toUsenLines()
    {
        GameRecordModel model;
        KifuBranchTree tree;
        KifuNavigationState navState;
        QList<KifDisplayItem> disp;
        setupBasicModel(model, tree, navState, disp);

        GameRecordModel::ExportContext ctx;
        ctx.startSfen = kHirateSfen;
        QStringList usiMoves = {
            QStringLiteral("7g7f"), QStringLiteral("3c3d"),
            QStringLiteral("2g2f"), QStringLiteral("8c8d"),
            QStringLiteral("2f2e"), QStringLiteral("8d8e"),
            QStringLiteral("6i7h")
        };
        auto lines = model.toUsenLines(ctx, usiMoves);
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
        auto* n1 = tree.addMove(tree.root(), move, QStringLiteral("▲７六歩(77)"), QStringLiteral("s1"),
                                QStringLiteral("( 0:01/00:00:01)"));
        auto* n2 = tree.addMove(n1, move, QStringLiteral("△３四歩(33)"), QStringLiteral("s2"),
                                QStringLiteral("( 0:02/00:00:02)"));
        auto* n3 = tree.addMove(n2, move, QStringLiteral("▲２六歩(27)"), QStringLiteral("s3"),
                                QStringLiteral("( 0:02/00:00:03)"));
        auto* n4 = tree.addMove(n3, move, QStringLiteral("△８四歩(83)"), QStringLiteral("s4"),
                                QStringLiteral("( 0:01/00:00:03)"));
        auto* n5 = tree.addMove(n4, move, QStringLiteral("▲２五歩(26)"), QStringLiteral("s5"),
                                QStringLiteral("( 0:01/00:00:04)"));
        tree.addTerminalMove(n5, TerminalType::Resign, QStringLiteral("△投了"),
                             QStringLiteral("( 0:03/00:00:06)"));

        // 変化：5手（本譜の▲２五歩の兄弟）
        auto* v5 = tree.addMove(n4, move, QStringLiteral("▲１六歩(17)"), QStringLiteral("v5"),
                                QStringLiteral("( 0:05/00:00:08)"));
        auto* v5b = tree.addMove(v5, move, QStringLiteral("△８五歩(84)"), QStringLiteral("v5b"),
                                 QStringLiteral("( 0:04/00:00:07)"));
        tree.addTerminalMove(v5b, TerminalType::Resign, QStringLiteral("▲投了"),
                             QStringLiteral("( 0:02/00:00:10)"));

        // 変化：3手（本譜の▲２六歩の兄弟1: ▲１六歩）
        auto* v3a = tree.addMove(n2, move, QStringLiteral("▲１六歩(17)"), QStringLiteral("v3a"),
                                 QStringLiteral("( 0:00/00:00:01)"));
        auto* v3a4 = tree.addMove(v3a, move, QStringLiteral("△９四歩(93)"), QStringLiteral("v3a4"),
                                  QStringLiteral("( 0:00/00:00:02)"));
        auto* v3a5 = tree.addMove(v3a4, move, QStringLiteral("▲４六歩(47)"), QStringLiteral("v3a5"),
                                  QStringLiteral("( 0:00/00:00:01)"));
        tree.addMove(v3a5, move, QStringLiteral("△７四歩(73)"), QStringLiteral("v3a6"),
                     QStringLiteral("( 0:00/00:00:02)"));

        // 変化：5手（▲１六歩ラインの▲４六歩の兄弟: ▲５六歩）
        auto* v3a5b = tree.addMove(v3a4, move, QStringLiteral("▲５六歩(57)"), QStringLiteral("v3a5b"),
                                   QStringLiteral("( 0:00/00:00:01)"));
        auto* v3a5b6 = tree.addMove(v3a5b, move, QStringLiteral("△５二金(61)"), QStringLiteral("v3a5b6"),
                                    QStringLiteral("( 0:00/00:00:02)"));
        tree.addMove(v3a5b6, move, QStringLiteral("▲５五歩(56)"), QStringLiteral("v3a5b7"),
                     QStringLiteral("( 0:00/00:00:01)"));

        // 変化：3手（本譜の▲２六歩の兄弟2: ▲７七桂）
        auto* v3b = tree.addMove(n2, move, QStringLiteral("▲７七桂(89)"), QStringLiteral("v3b"),
                                 QStringLiteral("( 0:00/00:00:01)"));
        tree.addMove(v3b, move, QStringLiteral("△９二香(91)"), QStringLiteral("v3b4"),
                     QStringLiteral("( 0:00/00:00:02)"));

        // モデルセットアップ
        GameRecordModel model;
        KifuNavigationState navState;
        navState.setTree(&tree);
        navState.goToRoot();
        model.setBranchTree(&tree);
        model.setNavigationState(&navState);

        QList<KifDisplayItem> disp;
        disp.append(KifDisplayItem(QStringLiteral("開始局面")));
        auto mainNodes = tree.allLines().at(0).nodes;
        for (int i = 1; i < mainNodes.size(); ++i) {
            auto* node = mainNodes.at(i);
            disp.append(KifDisplayItem(node->displayText(), node->timeText(), QString(), node->ply()));
        }
        model.bind(&disp);
        model.initializeFromDisplayItems(disp, static_cast<int>(disp.size()));

        GameRecordModel::ExportContext ctx;
        ctx.startSfen = kHirateSfen;
        auto lines = model.toKifLines(ctx);

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
};

QTEST_MAIN(TestGameRecordModel)
#include "tst_gamerecordmodel.moc"
