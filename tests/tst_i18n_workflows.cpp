#include <QtTest>
#include <QTranslator>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QLabel>
#include "kifupresentation.h"
#include "kifuloadparser.h"
#include "kifubranchtreebuilder.h"
#include "kifubranchtree.h"
#include "gamerecordmodel.h"
#include "gamerecordpresenter.h"
#include "kifurecordlistmodel.h"
#include "kifubranchlistmodel.h"
#include "gameinfopanecontroller.h"
#include "shogienginethinkingmodel.h"
#include "shogiinforecord.h"
#include "pvboardcontroller.h"
#include "pvboarddialog.h"
#include "csagamewiring.h"
#include "kifuconversionservice.h"
#include "sfenpositiontracer.h"
#include "analysisresulthandler.h"
#include "kifuanalysislistmodel.h"

class TestI18nWorkflows : public QObject
{
    Q_OBJECT
    QTemporaryDir m_config;
private slots:
    void initTestCase() { qputenv("XDG_CONFIG_HOME", m_config.path().toUtf8()); }
    void cleanup() { KifuPresentation::configure("ja_JP", "auto", false); }

    void whiteStartAnalysisUsesPositionAndMoveIdentity()
    {
        SfenPositionTracer board;
        QString initial = board.toSfenString();
        initial.replace(" b ", " w ");
        QVERIFY(board.setFromSfen(initial));
        QStringList history{initial};
        QVERIFY(board.applyUsiMove("3c3d"));
        history.append(board.toSfenString());
        QStringList moves{QStringLiteral("3c3d")};
        KifuAnalysisListModel model;
        AnalysisResultHandler handler;
        AnalysisResultHandler::Refs refs;
        refs.analysisModel = &model;
        refs.sfenHistory = &history;
        refs.usiMoves = &moves;
        handler.setRefs(refs);
        KifuPresentation::configure("en", "auto", false);
        handler.updatePending(0, 100, 0, "3c3d 7g7f");
        handler.commitPendingResult();
        QCOMPARE(model.item(0)->evaluationValue(), QStringLiteral("-100"));
        handler.updatePending(1, 200, 0, "7g7f");
        handler.commitPendingResult();
        QCOMPARE(model.item(1)->evaluationValue(), QStringLiteral("200"));
        QCOMPARE(model.index(1, 1).data().toString(), QStringLiteral("△P-3d"));
        QCOMPARE(model.index(1, 2).data().toString(), QStringLiteral("◯"));
        // Equal labels must not create a match between different positions.
        model.item(1)->candidateSfen = board.toSfenString();
        QVERIFY(model.index(1, 2).data().toString().isEmpty());
    }

    void importDisplayAndExport_data()
    {
        QTest::addColumn<QString>("language");
        QTest::addColumn<QString>("notation");
        for (const auto* language : {"ja_JP", "en", "zh_CN", "zh_TW"})
            for (const auto* notation : {"auto", "japanese", "western"})
                QTest::newRow(qPrintable(QStringLiteral("%1-%2").arg(language, notation)))
                    << QString::fromLatin1(language) << QString::fromLatin1(notation);
    }

    void importDisplayAndExport()
    {
        QFETCH(QString, language);
        QFETCH(QString, notation);
        const QString kif = QStringLiteral(
            "先手：佐藤 一郎\n後手：张三\n手合割：平手\n棋戦：交流大会\n"
            "独自タグ：最初\n独自タグ：次の値\n"
            "手数----指手---------消費時間--\n"
            "1 ７六歩(77) ( 0:01/00:00:01)\n*日本語の注釈 <tag>\n"
            "2 ３四歩(33) ( 0:01/00:00:01)\n"
            "3 ２二角成(88) ( 0:01/00:00:02)\n"
            "4 投了 ( 0:00/00:00:01)\n");
        const auto imported = KifuLoadParser::parseText(kif);
        QVERIFY2(imported.success, qPrintable(imported.error));
        KifuBranchTree tree;
        KifuBranchTreeBuilder::buildFromKifParseResult(&tree, imported.record, imported.initialSfen);
        GameRecordModel record;
        record.setBranchTree(&tree);
        GameRecordModel::ExportContext context;
        context.startSfen = imported.initialSfen;
        context.gameInfoItems = imported.gameInfo;
        context.gameInfoProvided = true;
        const QStringList expected = record.toKifLines(context);

        QTranslator translator;
        QVERIFY(translator.load(QStringLiteral(TRANSLATIONS_DIR "/ShogiBoardQ_") + language + ".qm"));
        qApp->installTranslator(&translator);
        KifuPresentation::configure(language, notation, false);
        const bool western = KifuPresentation::options().notation == KifuPresentation::Notation::Western;

        KifuRecordListModel rows;
        GameRecordPresenter presenter({&rows, nullptr});
        presenter.presentGameRecord(tree.displayItemsForLine(0));
        QVERIFY(rows.data(rows.index(1, 0), Qt::DisplayRole).toString().contains(
            western ? QStringLiteral("▲P-7f") : QStringLiteral("７六歩")));
        QVERIFY(rows.data(rows.index(3, 0), Qt::DisplayRole).toString().contains(
            western ? QStringLiteral("Bx2b+") : QStringLiteral("角成")));
        QVERIFY(rows.item(1)->currentMove().contains(QStringLiteral("７六歩")));
        QCOMPARE(rows.item(1)->comment(), QStringLiteral("日本語の注釈 <tag>"));

        KifuBranchListModel branches;
        branches.setBranchCandidatesFromKif({tree.displayItemsForLine(0).at(3)});
        if (western) QVERIFY(branches.data(branches.index(0, 0), Qt::DisplayRole).toString().endsWith('+'));

        ShogiEngineThinkingModel thinking;
        auto* info = new ShogiInfoRecord("10", "5", "100", "25", QStringLiteral("▲７六歩(77)△３四歩(33)"));
        info->setUsiPv("7g7f 3c3d");
        info->setBaseSfen(imported.initialSfen);
        thinking.appendItem(info);
        const QString pv = thinking.data(thinking.index(0, 5), Qt::DisplayRole).toString();
        QCOMPARE(pv, western ? QStringLiteral("▲P-7f △P-3d") : info->pv());
        PvBoardController board(imported.initialSfen, {"7g7f", "3c3d"});
        board.setKanjiPv(info->pv());
        QCOMPARE(board.displayPv(), pv);
        PvBoardDialog pvDialog(imported.initialSfen, {"7g7f", "3c3d"});
        pvDialog.setKanjiPv(info->pv());
        bool foundPv = false;
        for (const auto* label : pvDialog.findChildren<QLabel*>())
            foundPv = foundPv || label->text() == pv;
        QVERIFY(foundPv);
        QVERIFY(board.goForward());
        if (western) QCOMPARE(board.currentMoveText(), QStringLiteral("▲P-7f"));

        // Network game results must stay canonical even with a translated UI.
        CsaGameCoordinator csa;
        CsaGameWiring::Dependencies csaDeps;
        csaDeps.coordinator = &csa;
        CsaGameWiring csaWiring(csaDeps);
        QSignalSpy endLines(&csaWiring, &CsaGameWiring::appendKifuLineRequested);
        using Cause = CsaClient::GameEndCause;
        const QList<QPair<Cause, TerminalType>> endings = {
            {Cause::Resign, TerminalType::Resign}, {Cause::TimeUp, TerminalType::Timeout},
            {Cause::IllegalMove, TerminalType::IllegalLoss}, {Cause::OuteSennichite, TerminalType::IllegalLoss},
            {Cause::Sennichite, TerminalType::Repetition}, {Cause::Jishogi, TerminalType::DeclarationWin},
            {Cause::MaxMoves, TerminalType::MaxMoves}, {Cause::Chudan, TerminalType::Interrupt}};
        for (const auto& ending : endings) {
            QVERIFY(QMetaObject::invokeMethod(&csaWiring, "onGameEnded", Qt::DirectConnection,
                Q_ARG(CsaClient::GameResult, CsaClient::GameResult::Lose),
                Q_ARG(CsaClient::GameEndCause, ending.first), Q_ARG(int, 0)));
            QCOMPARE(endLines.size(), 1);
            QCOMPARE(detectTerminalType(endLines.takeFirst().at(0).toString()), ending.second);
        }

        GameInfoPaneController metadata;
        QScopedPointer<QWidget> container(metadata.containerWidget());
        metadata.setGameInfo(imported.gameInfo);
        auto* table = metadata.tableWidget();
        QCOMPARE(table->item(0, 0)->text(), QStringLiteral("先手"));
        QCOMPARE(table->item(0, 1)->text(), QStringLiteral("佐藤 一郎"));
        QCOMPARE(table->item(0, 0)->data(Qt::AccessibleTextRole).toString(), KifuPresentation::infoKey("先手"));
        QCOMPARE(metadata.gameInfo().size(), imported.gameInfo.size());
        context.gameInfoItems = metadata.gameInfo();
        QCOMPARE(record.toKifLines(context), expected);
        table->item(0, 1)->setText(QStringLiteral("編集した名前"));
        metadata.undo();
        QCOMPARE(metadata.gameInfo().first().value, QStringLiteral("佐藤 一郎"));
        metadata.redo();
        QCOMPARE(metadata.gameInfo().first().value, QStringLiteral("編集した名前"));
        metadata.undo();
        QVERIFY(expected.contains(QStringLiteral("独自タグ：最初")));
        QVERIFY(expected.contains(QStringLiteral("独自タグ：次の値")));
        const auto reread = KifuLoadParser::parseText(expected.join('\n'));
        QVERIFY2(reread.success, qPrintable(reread.error));
        QCOMPARE(reread.record.mainline.usiMoves, imported.record.mainline.usiMoves);

        // All existing save formats must be independent of presentation settings.
        using Format = KifuConversionService::Format;
        for (auto format : {Format::Kif, Format::Ki2, Format::Csa, Format::Jkf, Format::Usi, Format::Usen}) {
            KifuConversionService::Request request;
            request.text = kif;
            request.outputFormat = format;
            const auto localized = KifuConversionService::convert(request);
            QVERIFY2(localized.ok, qPrintable(localized.error));
            KifuPresentation::configure("ja_JP", "japanese", false);
            const auto japanese = KifuConversionService::convert(request);
            QCOMPARE(localized.lines, japanese.lines);
            KifuPresentation::configure(language, notation, false);
        }
        qApp->removeTranslator(&translator);
    }
};
QTEST_MAIN(TestI18nWorkflows)
#include "tst_i18n_workflows.moc"
