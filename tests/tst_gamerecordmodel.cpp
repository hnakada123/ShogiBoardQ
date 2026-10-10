#include <QtTest>
#include <QSignalSpy>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QTemporaryFile>
#include <QClipboard>
#include "bodtextgenerator.h"
#include "csaformatter.h"
#include "kifuclipboardservice.h"
#include "kifucontentbuilder.h"

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
#include "kifuexportmetadata.h"
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

    /// 開始日時・終了日時が記録にない棋譜を CSA にしても、変換時刻を対局日時として書かない
    void csaDoesNotInventGameTimes()
    {
        const QStringList usi = {"7g7f", "3c3d"};
        const QStringList pretty = {QStringLiteral("▲７六歩(77)"), QStringLiteral("△３四歩(33)")};
        KifuBranchTree tree;
        tree.setRootSfen(kHirateSfen);
        auto* tip = tree.root();
        for (int i = 0; i < usi.size(); ++i) tip = addTestMove(tree, tip, usi[i], pretty[i]);
        GameRecordModel model;
        model.setBranchTree(&tree);
        GameRecordModel::ExportContext context;
        context.startSfen = kHirateSfen;
        // 読み込んだ棋譜（日時の記載なし）の対局情報をそのまま渡す（画面・CLI の変換と同じ）
        context.gameInfoProvided = true;
        context.gameInfoItems = {{QStringLiteral("先手"), QStringLiteral("鈴木")},
                                 {QStringLiteral("後手"), QStringLiteral("山田")}};
        const QString csa = model.toCsaLines(context, usi).join(QLatin1Char('\n'));
        QVERIFY2(!csa.contains(QStringLiteral("$START_TIME")), qPrintable(csa));
        QVERIFY2(!csa.contains(QStringLiteral("$END_TIME")), qPrintable(csa));

        // このアプリで対局した開始時刻が分かる場合は記録する
        context.gameStartDateTime = QDateTime(QDate(2026, 10, 5), QTime(9, 30, 0));
        const QString live = model.toCsaLines(context, usi).join(QLatin1Char('\n'));
        QVERIFY2(live.contains(QStringLiteral("$START_TIME:2026/10/05 09:30:00")), qPrintable(live));
        QVERIFY(!live.contains(QStringLiteral("$END_TIME")));
    }

    /// 持ち時間「無制限」は CSA の時間行を書かない（$TIME_LIMIT:無制限 にしない）
    void csaOmitsTimeForUnlimitedGame()
    {
        const QStringList usi = {"7g7f"};
        KifuBranchTree tree;
        tree.setRootSfen(kHirateSfen);
        addTestMove(tree, tree.root(), usi.first(), QStringLiteral("▲７六歩(77)"));
        GameRecordModel model;
        model.setBranchTree(&tree);
        GameRecordModel::ExportContext context;
        context.startSfen = kHirateSfen;
        context.gameInfoProvided = true;
        context.gameInfoItems = {{QStringLiteral("持ち時間"), QStringLiteral("無制限")}};
        const QString csa = model.toCsaLines(context, usi).join(QLatin1Char('\n'));
        QVERIFY2(!csa.contains(QStringLiteral("$TIME")), qPrintable(csa));
        QVERIFY2(csa.contains(QStringLiteral("+7776FU")), qPrintable(csa));

        context.gameInfoItems = {{QStringLiteral("持ち時間"), QStringLiteral("10:00+10")}};
        const QString timed = model.toCsaLines(context, usi).join(QLatin1Char('\n'));
        QVERIFY2(timed.contains(QStringLiteral("$TIME:600+10+0")), qPrintable(timed));
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
        GameRecordModel model;
        model.setBranchTree(&tree);
        GameRecordModel::ExportContext context;
        context.startSfen = initial;
        const QStringList lines = model.toKi2Lines(context);
        const QString text = lines.join(QLatin1Char('\n'));
        QVERIFY2(text.contains(QStringLiteral("▲５七金打")), qPrintable(text));  // 5八の金も5七へ動ける
        QVERIFY2(text.contains(QStringLiteral("▲１五金")), qPrintable(text));
        QVERIFY2(!text.contains(QStringLiteral("▲１五金打")), qPrintable(text)); // 盤上の金は1五へ動けない

        // 「打」を省いても、読み込むと同じ手順に戻る
        QTemporaryFile file;
        QVERIFY(KifuTestHelper::writeToTempFile(file, text.toUtf8(), QStringLiteral("ki2")));
        KifParseResult result;
        QString error;
        QVERIFY2(Ki2ToSfenConverter::parseWithVariations(file.fileName(), result, &error), qPrintable(error));
        QCOMPARE(result.mainline.usiMoves, usi);
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
        const QString text = (withBod ? BodTextGenerator::generate(initial, 0, {}, handicapNames) : turnLine)
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
        tree.addTerminalMove(tip, TerminalType::DeclarationWin, terminal);
        GameRecordModel model;
        model.setBranchTree(&tree);
        GameRecordModel::ExportContext ctx;
        ctx.startSfen = kHirateSfen;
        const QStringList lines = model.toKifLines(ctx);
        QVERIFY2(lines.contains(QStringLiteral("まで1手で%1の勝ち").arg(winner)), qPrintable(lines.join('\n')));
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
        using KifuExportMetadataBuilder::timeControlText;
        QCOMPARE(timeControlText(600000, 10000, 0), QStringLiteral("10:00+10"));
        QCOMPARE(timeControlText(300000, 0, 10000), QStringLiteral("05:00+10秒加算"));
        QCOMPARE(timeControlText(7200000, 0, 0), QStringLiteral("120:00"));
        QCOMPARE(timeControlText(0, 30000, 0), QStringLiteral("00:00+30"));
    }

    /// 先後で持ち時間が違えば両方を書き、駒落ちの見出しは下手・上手にする
    void timeControlTextPerSide()
    {
        using KifuExportMetadataBuilder::timeControlText;
        const KifuTimeControlSide black{60000, 2000, 0};
        const KifuTimeControlSide white{120000, 3000, 0};
        QCOMPARE(timeControlText(black, black, false), QStringLiteral("01:00+2"));
        QCOMPARE(timeControlText(black, white, false), QStringLiteral("先手 01:00+2 / 後手 02:00+3"));
        QCOMPARE(timeControlText(black, white, true), QStringLiteral("下手 01:00+2 / 上手 02:00+3"));
    }

    /// 駒落ちの棋譜は柿木形式と同じく対局者を下手・上手で書き、CSA では下手が先手（N+）になる
    void handicapHeaderUsesShitateUwate()
    {
        GameRecordModel model;
        GameRecordModel::ExportContext ctx;
        ctx.startSfen = QStringLiteral("lnsgkgsnl/1r7/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1");  // 角落ち
        ctx.playMode = PlayMode::HandicapEngineVsEngine;
        ctx.engine1 = QStringLiteral("下手エンジン");
        ctx.engine2 = QStringLiteral("上手エンジン");
        ctx.hasTimeControl = true;
        ctx.blackTime = {60000, 2000, 0};
        ctx.whiteTime = {120000, 3000, 0};
        const QString kif = model.toKifLines(ctx).join(QLatin1Char('\n'));
        QVERIFY2(kif.contains(QStringLiteral("下手：下手エンジン")), qPrintable(kif));
        QVERIFY(kif.contains(QStringLiteral("上手：上手エンジン")));
        QVERIFY(!kif.contains(QStringLiteral("先手：")));
        QVERIFY(!kif.contains(QStringLiteral("後手：")));
        QVERIFY(kif.contains(QStringLiteral("持ち時間：下手 01:00+2 / 上手 02:00+3")));
        const QString csa = model.toCsaLines(ctx, {}).join(QLatin1Char('\n'));
        QVERIFY2(csa.contains(QStringLiteral("N+下手エンジン")), qPrintable(csa));
        QVERIFY(csa.contains(QStringLiteral("N-上手エンジン")));
        QVERIFY(csa.contains(QStringLiteral("$TIME+:60+2+0")));
        QVERIFY(csa.contains(QStringLiteral("$TIME-:120+3+0")));
        QVERIFY(!csa.contains(QStringLiteral("$TIME:")));

        // 平手は従来どおり先手・後手
        ctx.startSfen = kHirateSfen;
        ctx.whiteTime = ctx.blackTime;
        const QString hirate = model.toKifLines(ctx).join(QLatin1Char('\n'));
        QVERIFY(hirate.contains(QStringLiteral("先手：下手エンジン")));
        QVERIFY(hirate.contains(QStringLiteral("持ち時間：01:00+2")));
        QVERIFY(model.toCsaLines(ctx, {}).contains(QStringLiteral("$TIME:60+2+0")));

        // 読み込んだ棋譜の見出しが先手・後手なら、駒落ちでも局面図をその呼び方にそろえる
        ctx.startSfen = QStringLiteral("lnsgkgsnl/1r7/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1");
        ctx.gameInfoProvided = true;
        ctx.gameInfoItems = {{QStringLiteral("先手"), QStringLiteral("A")}, {QStringLiteral("後手"), QStringLiteral("B")},
                             {QStringLiteral("手合割"), QStringLiteral("角落ち")}};
        const QString keepsHeader = model.toKifLines(ctx).join(QLatin1Char('\n'));
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
        tree.addTerminalMove(tip, TerminalType::Timeout, QStringLiteral("▲時間切れ"));
        GameRecordModel model;
        model.setBranchTree(&tree);
        GameRecordModel::ExportContext ctx;
        ctx.startSfen = initial;
        const QStringList kif = model.toKifLines(ctx);
        QVERIFY2(kif.contains(QStringLiteral("まで1手で上手の勝ち")), qPrintable(kif.join(QLatin1Char('\n'))));
        const QStringList ki2 = model.toKi2Lines(ctx);
        QVERIFY2(ki2.contains(QStringLiteral("まで1手で時間切れにより上手の勝ち")), qPrintable(ki2.join(QLatin1Char('\n'))));
        QVERIFY(!ki2.contains(QStringLiteral("▲時間切れ")));
        QVERIFY(model.toUsiLines(ctx, {QStringLiteral("3c3d")}).join(QLatin1Char('\n')).endsWith(QStringLiteral("3c3d timeout")));
        QVERIFY(model.toUsenLines(ctx, {QStringLiteral("3c3d")}).join(QLatin1Char('\n')).trimmed().endsWith(QStringLiteral(".t")));
        QVERIFY(model.toJkfLines(ctx).join(QLatin1Char('\n')).contains(QStringLiteral("\"special\":\"TIME_UP\"")));
    }

    /// 駒落ちの呼び方（下手・上手）で書いた局面図の持駒も読み込める
    void bodReadsShitateUwateHands()
    {
        const QString sfen = QStringLiteral("4k4/9/9/9/9/9/9/9/4K4 w 2Pr 1");
        const QString bod = BodTextGenerator::generate(sfen, 0, {}, true);
        QVERIFY2(bod.contains(QStringLiteral("上手の持駒：")) && bod.contains(QStringLiteral("下手の持駒：")),
                 qPrintable(bod));
        QVERIFY(!bod.contains(QStringLiteral("なし")));
        QTemporaryFile file;
        QVERIFY(KifuTestHelper::writeToTempFile(
            file, (bod + QStringLiteral("\n手数----指手---------消費時間--\n")).toUtf8(), QStringLiteral("kif")));
        QCOMPARE(KifToSfenConverter::detectInitialSfenFromFile(file.fileName()), sfen);
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
        QStringList sfens = SfenPositionTracer::buildSfenRecord(startSfen, {move}, false);
        KifuExportClipboard exporter(nullptr);
        KifuExportClipboard::Deps deps;
        deps.sfenRecord = &sfens;
        deps.startSfenStr = startSfen;
        deps.currentMoveIndex = 1;
        exporter.setDependencies(deps);
        QVERIFY(exporter.copyBodToClipboard());
        const QStringList bod = QApplication::clipboard()->text().split(QLatin1Char('\n'));
        for (const QString& line : std::as_const(lines)) {
            QVERIFY2(bod.contains(line), qPrintable(bod.join(QLatin1Char('\n'))));
        }
    }

    void clipboardKeepsTimeMetadata()
    {
        KifuBranchTree tree;
        tree.setRootSfen(kHirateSfen);
        GameRecordModel model;
        model.setBranchTree(&tree);
        GameRecordModel::ExportContext ctx;
        ctx.startSfen = kHirateSfen;
        ctx.hasTimeControl = true;
        ctx.blackTime = {630000, 30000, 5000};
        ctx.whiteTime = ctx.blackTime;
        ctx.gameStartDateTime = QDateTime(QDate(2025, 1, 2), QTime(3, 4, 5));
        ctx.gameEndDateTime = QDateTime(QDate(2025, 1, 2), QTime(6, 7, 8));
        QVERIFY(KifuClipboardService::copyKif(model, ctx));
        QCOMPARE(QApplication::clipboard()->text(), model.toKifLines(ctx).join(QLatin1Char('\n')));
        QVERIFY(KifuClipboardService::copyKi2(model, ctx));
        QCOMPARE(QApplication::clipboard()->text(), model.toKi2Lines(ctx).join(QLatin1Char('\n')));
        QVERIFY(QApplication::clipboard()->text().contains(QStringLiteral("2025/01/02 06:07:08")));
    }

    void removedGameInfoStaysEmptyOnExport()
    {
        GameRecordModel model;
        GameRecordModel::ExportContext ctx;
        ctx.gameInfoProvided = true;
        ctx.startSfen = kHirateSfen;
        const QString kif = model.toKifLines(ctx).join(QLatin1Char('\n'));
        QVERIFY(!kif.contains(QStringLiteral("開始日時：")));
        QVERIFY(!kif.contains(QStringLiteral("先手：")));
        QVERIFY(!kif.contains(QStringLiteral("後手：")));
        KifuExportContext legacy;
        legacy.gameInfoProvided = true;
        legacy.startSfen = kHirateSfen;
        const QString legacyKif = KifuContentBuilder::buildKifuDataList(legacy).join(QLatin1Char('\n'));
        QVERIFY(!legacyKif.contains(QStringLiteral("開始日時：")));
        QVERIFY(!legacyKif.contains(QStringLiteral("先手：")));
        ctx.gameInfoProvided = false;
        QVERIFY(model.toKifLines(ctx).join(QLatin1Char('\n')).contains(QStringLiteral("開始日時：")));
    }

    void removedGameInfoPreservesCustomPosition()
    {
        const QString initial = QStringLiteral("4k4/9/9/9/4p4/4+S4/9/9/4K4 w Pp 1");
        KifuBranchTree tree;
        tree.setRootSfen(initial);
        GameRecordModel model;
        model.setBranchTree(&tree);
        GameRecordModel::ExportContext ctx;
        ctx.gameInfoProvided = true;
        ctx.startSfen = initial;
        QTemporaryFile kif;
        QVERIFY(KifuTestHelper::writeToTempFile(kif, model.toKifLines(ctx).join(QLatin1Char('\n')).toUtf8(), QStringLiteral("kif")));
        QCOMPARE(KifToSfenConverter::detectInitialSfenFromFile(kif.fileName()), initial);
        QTemporaryFile ki2;
        QVERIFY(KifuTestHelper::writeToTempFile(ki2, model.toKi2Lines(ctx).join(QLatin1Char('\n')).toUtf8(), QStringLiteral("ki2")));
        QCOMPARE(Ki2ToSfenConverter::detectInitialSfenFromFile(ki2.fileName()), initial);
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

    // KI2 の変化：分岐前の手順を正しい手番で並べ直し、同じ地点へ動ける駒の区別（右・左）を付ける
    void ki2VariationDisambiguatesWithCorrectSide()
    {
        KifuBranchTree tree;
        tree.setRootSfen(kHirateSfen);
        auto* m1 = addTestMove(tree, tree.root(), QStringLiteral("7g7f"), QStringLiteral("▲７六歩(77)"));
        addTestMove(tree, m1, QStringLiteral("3c3d"), QStringLiteral("△３四歩(33)"));
        // ２手目の変化：後手の金は 41・61 のどちらも 52 へ動ける
        addTestMove(tree, m1, QStringLiteral("4a5b"), QStringLiteral("△５二金(41)"));
        GameRecordModel model;
        model.setBranchTree(&tree);
        GameRecordModel::ExportContext ctx;
        ctx.gameInfoProvided = true;
        ctx.startSfen = kHirateSfen;

        QTemporaryFile ki2;
        QVERIFY(KifuTestHelper::writeToTempFile(ki2, model.toKi2Lines(ctx).join(QLatin1Char('\n')).toUtf8(),
                                                QStringLiteral("ki2u")));
        KifParseResult result;
        QString error;
        QVERIFY2(Ki2ToSfenConverter::parseWithVariations(ki2.fileName(), result, &error), qPrintable(error));
        QCOMPARE(result.variations.size(), 1);
        QCOMPARE(result.variations.at(0).line.usiMoves, QStringList({QStringLiteral("4a5b")}));
    }

    // KI2 の結果行から終局の種類（詰み・切れ負けなど）を読み戻せる
    void ki2TerminalKindsRoundTrip()
    {
        KifuBranchTree tree;
        tree.setRootSfen(kHirateSfen);
        auto* m1 = addTestMove(tree, tree.root(), QStringLiteral("7g7f"), QStringLiteral("▲７六歩(77)"));
        auto* m2 = addTestMove(tree, m1, QStringLiteral("3c3d"), QStringLiteral("△３四歩(33)"));
        tree.addTerminalMove(m2, TerminalType::Checkmate, QStringLiteral("▲詰み"));
        auto* v2 = addTestMove(tree, m1, QStringLiteral("8c8d"), QStringLiteral("△８四歩(83)"));
        tree.addTerminalMove(v2, TerminalType::Timeout, QStringLiteral("▲切れ負け"));
        GameRecordModel model;
        model.setBranchTree(&tree);
        GameRecordModel::ExportContext ctx;
        ctx.gameInfoProvided = true;
        ctx.startSfen = kHirateSfen;
        const QString text = model.toKi2Lines(ctx).join(QLatin1Char('\n'));
        QVERIFY2(text.contains(QStringLiteral("まで2手で詰み")), qPrintable(text));
        QVERIFY2(text.contains(QStringLiteral("時間切れ")), qPrintable(text));

        QTemporaryFile ki2;
        QVERIFY(KifuTestHelper::writeToTempFile(ki2, text.toUtf8(), QStringLiteral("ki2u")));
        KifParseResult result;
        QString error;
        QVERIFY2(Ki2ToSfenConverter::parseWithVariations(ki2.fileName(), result, &error), qPrintable(error));
        QVERIFY(result.mainline.disp.last().prettyMove.contains(QStringLiteral("詰み")));
        QCOMPARE(result.variations.size(), 1);
        QVERIFY(result.variations.at(0).line.disp.last().prettyMove.contains(QStringLiteral("切れ負け")));
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

QTEST_MAIN(TestGameRecordModel)
#include "tst_gamerecordmodel.moc"
