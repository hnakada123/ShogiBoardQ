#include <QtTest>
#include <QTranslator>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QLabel>
#include <QLibraryInfo>
#include "kifupresentation.h"
#include "applicationfonts.h"
#include "applicationtranslations.h"
#include "appsettings.h"
#include "kifuloadparser.h"
#include "kifubranchtreebuilder.h"
#include "kifubranchtree.h"
#include "gamerecordmodel.h"
#include "gamerecordpresenter.h"
#include "gamerecordupdateservice.h"
#include "livegamesession.h"
#include "livegamesessionupdater.h"
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
#include "kifuanalysisdialog.h"

class TestI18nWorkflows : public QObject
{
    Q_OBJECT
    QTemporaryDir m_config;
private slots:
    void initTestCase() { qputenv("XDG_CONFIG_HOME", m_config.path().toUtf8()); }
    void cleanup()
    {
        AppSettings::setLanguage("system");
        AppSettings::setMoveNotation("auto");
        AppSettings::setNotationOrigin(false);
        KifuPresentation::configure("ja_JP", "auto", false);
        ApplicationFonts::initialize();
    }

    void startupTranslationsLiveForScope_data()
    {
        QTest::addColumn<QString>("language");
        QTest::addColumn<QString>("notation");
        QTest::addColumn<bool>("alwaysOrigin");
        QTest::addColumn<QString>("resign");
        QTest::addColumn<QString>("firstRank");
        QTest::newRow("japanese") << QString("ja_JP") << QString("auto") << false << QString("投了") << QString("一");
        QTest::newRow("english") << QString("en") << QString("auto") << false << QString("Resign") << QString("a");
        QTest::newRow("simplified") << QString("zh_CN") << QString("auto") << false << QString("认输") << QString("一");
        QTest::newRow("traditional") << QString("zh_TW") << QString("auto") << false << QString("認輸") << QString("一");
        QTest::newRow("japanese-western") << QString("ja_JP") << QString("western") << true << QString("投了") << QString("a");
        QTest::newRow("english-japanese") << QString("en") << QString("japanese") << true << QString("Resign") << QString("一");
    }

    void startupTranslationsLiveForScope()
    {
        QFETCH(QString, language);
        QFETCH(QString, notation);
        QFETCH(bool, alwaysOrigin);
        QFETCH(QString, resign);
        QFETCH(QString, firstRank);
        AppSettings::setLanguage(language);
        AppSettings::setMoveNotation(notation);
        AppSettings::setNotationOrigin(alwaysOrigin);
        QCOMPARE(KifuPresentation::status("投了"), QStringLiteral("投了"));
        {
            const ApplicationTranslations translations(QStringLiteral(TRANSLATIONS_DIR));
            QCOMPARE(translations.language(), language);
            QCoreApplication::processEvents();
            QCOMPARE(KifuPresentation::status("投了"), resign);
            QCOMPARE(KifuPresentation::rankLabel(1), firstRank);
            QCOMPARE(KifuPresentation::options().alwaysOrigin, alwaysOrigin);
        }
        QCOMPARE(KifuPresentation::status("投了"), QStringLiteral("投了"));
    }

    void startupQtTranslationsLiveForScope()
    {
        QTranslator reference;
        if (!reference.load("qtbase_ja", QLibraryInfo::path(QLibraryInfo::TranslationsPath)))
            QSKIP("Qt Japanese translations are not installed");
        const QString cancel = reference.translate("QPlatformTheme", "Cancel");
        QVERIFY(!cancel.isEmpty());
        const QString original = QCoreApplication::translate("QPlatformTheme", "Cancel");
        AppSettings::setLanguage("ja_JP");
        {
            const ApplicationTranslations translations(QStringLiteral(TRANSLATIONS_DIR));
            QCOMPARE(QCoreApplication::translate("QPlatformTheme", "Cancel"), cancel);
        }
        QCOMPARE(QCoreApplication::translate("QPlatformTheme", "Cancel"), original);
    }

    void missingStartupCatalogKeepsNotationSettings()
    {
        QTemporaryDir emptyDirectory;
        QVERIFY(emptyDirectory.isValid());
        AppSettings::setLanguage("en");
        AppSettings::setMoveNotation("western");
        AppSettings::setNotationOrigin(true);
        QTest::ignoreMessage(QtWarningMsg, "Translation file not found: \"en\"");
        const ApplicationTranslations translations(emptyDirectory.path());
        QCOMPARE(translations.language(), QStringLiteral("en"));
        QCOMPARE(KifuPresentation::status("投了"), QStringLiteral("投了"));
        QCOMPARE(KifuPresentation::rankLabel(1), QStringLiteral("a"));
        QVERIFY(KifuPresentation::options().alwaysOrigin);
    }

    void kifuAnalysisRangeFollowsWordOrder_data()
    {
        QTest::addColumn<QString>("language");
        QTest::addColumn<QString>("prefix");
        QTest::addColumn<QString>("middle");
        QTest::addColumn<QString>("suffix");
        QTest::newRow("ja") << "ja_JP" << "" << "手目から" << "手目まで";
        QTest::newRow("en") << "en" << "from move" << "to" << "";
        QTest::newRow("zh_CN") << "zh_CN" << "从第" << "手到第" << "手";
    }

    void kifuAnalysisRangeFollowsWordOrder()
    {
        // 範囲指定の数値欄の前・間・後ろの文字列を、言語ごとの語順で並べる
        QFETCH(QString, language);
        QFETCH(QString, prefix);
        QFETCH(QString, middle);
        QFETCH(QString, suffix);
        QTranslator translator;
        QVERIFY(translator.load(QStringLiteral(TRANSLATIONS_DIR "/ShogiBoardQ_") + language + ".qm"));
        qApp->installTranslator(&translator);
        KifuAnalysisDialog dialog;
        const auto text = [&dialog](const char* name) {
            auto* label = dialog.findChild<QLabel*>(QString::fromLatin1(name));
            return label && !label->isHidden() ? label->text() : QString();
        };
        QCOMPARE(text("labelRangePrefix"), prefix);
        QCOMPARE(text("labelFrom"), middle);
        QCOMPARE(text("labelTo"), suffix);
        qApp->removeTranslator(&translator);
    }

    void liveTimeoutIsTerminal_data()
    {
        QTest::addColumn<QString>("language");
        QTest::addColumn<int>("plies");
        for (const auto* language : {"ja_JP", "en", "zh_CN", "zh_TW"})
            for (int plies : {1, 2})
                QTest::newRow(qPrintable(QStringLiteral("%1-after-%2").arg(language).arg(plies)))
                    << QString::fromLatin1(language) << plies;
    }

    void liveTimeoutIsTerminal()
    {
        QFETCH(QString, language);
        QFETCH(int, plies);
        QTranslator translator;
        QVERIFY(translator.load(QStringLiteral(TRANSLATIONS_DIR "/ShogiBoardQ_") + language + ".qm"));
        qApp->installTranslator(&translator);
        KifuPresentation::configure(language, "auto", false);
        SfenPositionTracer board;
        const QString initial = board.toSfenString();
        const QStringList usiMoves = QStringList{"7g7f", "3c3d"}.mid(0, plies);
        auto moves = SfenPositionTracer::buildGameMoves(initial, usiMoves);
        auto history = SfenPositionTracer::buildSfenRecord(initial, usiMoves, false);
        KifuBranchTree tree;
        tree.setRootSfen(initial);
        auto* node = tree.root();
        for (int i = 0; i < plies; ++i) {
            const QString japanese = KifuPresentation::move(history.at(i), usiMoves.at(i),
                {KifuPresentation::Notation::Japanese, false});
            node = tree.addMove(node, moves.at(i), japanese, history.at(i + 1));
        }
        LiveGameSession session;
        session.setTree(&tree);
        session.startFromNode(node);
        LiveGameSessionUpdater updater;
        LiveGameSessionUpdater::Deps liveDeps;
        liveDeps.liveSession = &session;
        liveDeps.sfenRecord = &history;
        updater.updateDeps(liveDeps);
        KifuRecordListModel rows;
        GameRecordPresenter presenter({&rows, nullptr});
        presenter.presentGameRecord(tree.displayItemsForLine(0));
        GameRecordUpdateService service;
        GameRecordUpdateService::Deps deps;
        deps.gameMoves = &moves;
        deps.sfenRecord = &history;
        deps.liveGameSession = &session;
        deps.ensureRecordPresenter = [&presenter] { return &presenter; };
        deps.ensureLiveGameSessionUpdater = [&updater] { return &updater; };
        service.updateDeps(deps);
        const QString terminal = (plies == 2 ? QStringLiteral("▲") : QStringLiteral("△"))
            + QStringLiteral("時間切れ");
        service.appendKifuLine(terminal, "00:01/00:00:01");
        const auto index = rows.index(plies + 1, 0);
        QVERIFY(rows.item(plies + 1)->usiMove.isEmpty());
        QVERIFY(index.data().toString().endsWith(KifuPresentation::status(terminal)));
        if (language == "en") QVERIFY(index.data().toString().endsWith("Time loss"));
        auto* end = session.liveNode();
        QCOMPARE(end->terminalType(), TerminalType::Timeout);
        QCOMPARE(end->move().movingPiece, Piece::None);
        QCOMPARE(end->sfen(), node->sfen());
        QVERIFY(!session.canAddMove());
        QVERIFY(session.moves().last().terminal);
        QVERIFY(session.moves().last().usiMove.isEmpty());
        GameRecordModel record;
        record.setBranchTree(&tree);
        GameRecordModel::ExportContext context;
        context.startSfen = initial;
        const QString saved = record.toKifLines(context).join('\n');
        QVERIFY(saved.contains(QStringLiteral("時間切れ")));
        QVERIFY(!saved.contains("Time loss"));
        const auto reloaded = KifuLoadParser::parseText(saved);
        QVERIFY2(reloaded.success, qPrintable(reloaded.error));
        QCOMPARE(reloaded.record.mainline.usiMoves, usiMoves);
        KifuBranchTree reopened;
        KifuBranchTreeBuilder::buildFromKifParseResult(&reopened, reloaded.record, reloaded.initialSfen);
        QCOMPARE(reopened.mainLine().last()->terminalType(), TerminalType::Timeout);
    }

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
        ApplicationFonts::initialize({}, language);

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
        // UIの書体と棋譜の書体を分け、ビュー個別の文字サイズを維持する。
        QFont viewFont = QApplication::font();
        viewFont.setPointSize(19);
        viewFont.setBold(true);
        const QFont expectedFont = western ? viewFont : ApplicationFonts::japaneseFont(viewFont);
        for (const auto& index : {rows.index(1, 0), branches.index(0, 0), thinking.index(0, 5)}) {
            const QVariant role = index.data(Qt::FontRole);
            const QFont painted = role.isValid() ? qvariant_cast<QFont>(role).resolve(viewFont) : viewFont;
            QCOMPARE(painted.family(), expectedFont.family());
            QCOMPARE(painted.pointSize(), 19);
            QVERIFY(painted.bold());
        }
        QVERIFY(!rows.index(1, 3).data(Qt::FontRole).isValid()); // 注釈はUIの書体。
        QVERIFY(!rows.index(4, 0).data(Qt::FontRole).isValid()); // 翻訳された終局もUIの書体。
        PvBoardController board(imported.initialSfen, {"7g7f", "3c3d"});
        board.setKanjiPv(info->pv());
        QCOMPARE(board.displayPv(), pv);
        PvBoardDialog pvDialog(imported.initialSfen, {"7g7f", "3c3d"});
        pvDialog.setKanjiPv(info->pv());
        bool foundPv = false;
        for (const auto* label : pvDialog.findChildren<QLabel*>()) {
            if (label->text() == pv) {
                foundPv = true;
                QCOMPARE(label->font().family(), expectedFont.family());
            }
        }
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
            // 連続王手の千日手は手番側の行。先手（自分）の王手で成立して後手番なので、後手の反則勝ち
            {Cause::IllegalMove, TerminalType::IllegalLoss}, {Cause::OuteSennichite, TerminalType::IllegalWin},
            {Cause::Sennichite, TerminalType::Repetition}, {Cause::Jishogi, TerminalType::DeclarationWin},
            {Cause::MaxMoves, TerminalType::MaxMoves}, {Cause::Chudan, TerminalType::Interrupt}};
        for (const auto& ending : endings) {
            QVERIFY(QMetaObject::invokeMethod(&csaWiring, "onGameEnded", Qt::DirectConnection,
                Q_ARG(CsaClient::GameResult, CsaClient::GameResult::Lose),
                Q_ARG(CsaClient::GameEndCause, ending.first), Q_ARG(int, 0)));
            QCOMPARE(endLines.size(), 1);
            QCOMPARE(detectTerminalType(endLines.takeFirst().at(0).toString()), ending.second);
        }
        // 入玉宣言は宣言した勝者の印を付け（保存時に勝者を判定するため）、点数による引き分けは持将棋
        QVERIFY(QMetaObject::invokeMethod(&csaWiring, "onGameEnded", Qt::DirectConnection,
            Q_ARG(CsaClient::GameResult, CsaClient::GameResult::Lose),
            Q_ARG(CsaClient::GameEndCause, Cause::Jishogi), Q_ARG(int, 0)));
        QCOMPARE(endLines.takeFirst().at(0).toString(),
                 (csa.isBlackSide() ? QStringLiteral("△") : QStringLiteral("▲")) + QStringLiteral("入玉勝ち"));
        QVERIFY(QMetaObject::invokeMethod(&csaWiring, "onGameEnded", Qt::DirectConnection,
            Q_ARG(CsaClient::GameResult, CsaClient::GameResult::Draw),
            Q_ARG(CsaClient::GameEndCause, Cause::Jishogi), Q_ARG(int, 0)));
        QCOMPARE(endLines.takeFirst().at(0).toString(), QStringLiteral("持将棋"));

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
