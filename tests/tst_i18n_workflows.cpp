#include <QtTest>
#include <QTranslator>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QLabel>
#include <QLibraryInfo>
#include "kifupresentation.h"
#include "handicaps.h"
#include "applicationfonts.h"
#include "applicationtranslations.h"
#include "appsettings.h"
#include "kifuloadparser.h"
#include "kifubranchtreebuilder.h"
#include "kifubranchtree.h"
#include "branchtree_test_util.h"
#include "kifudocument.h"
#include "kifuexport.h"
#include "appactivitystate.h"
#include "csagamecoordinator.h"
#include "engineslots.h"
#include "gamesession.h"
#include "matchcoordinator.h"
#include "shogiclock.h"
#include "gameboard.h"
#include "kifudisplaypresenter.h"
#include "kifunavigationstate.h"
#include "livegamesession.h"
#include "kifurecordlistmodel.h"
#include "kifubranchlistmodel.h"
#include "gameinfopane.h"
#include "game_info_table_helper.h"
#include "shogienginethinkingmodel.h"
#include "shogiinforecord.h"
#include "usilineparser.h"
#include "pvline.h"
#include "pvboarddialog.h"
#include "csagameprogress.h"
#include "kifuconversionservice.h"
#include "sfenreplay.h"
#include "sfenutils.h"
#include "kifuanalysisrowpresenter.h"
#include "kifuanalysislistmodel.h"
#include "kifuanalysisrow.h"
#include "kifuanalysisdialog.h"
#include "test_replay.h"

class TestI18nWorkflows : public QObject
{
    Q_OBJECT

    /// 先手の投了の行の表記（表示言語に合わせた終局語）
    static QString resignLabel()
    {
        return KifuPresentation::entryLabel(RecordEntry::ending(TerminalType::Resign, Turn::Black), QString());
    }
    QTemporaryDir m_config;
private slots:
    void initTestCase() { qputenv("XDG_CONFIG_HOME", m_config.path().toUtf8()); qputenv("SHOGIBOARDQ_CONFIG_HOME", m_config.path().toUtf8()); }
    void cleanup()
    {
        AppSettings::language.set("system");
        AppSettings::moveNotation.set("auto");
        AppSettings::notationOrigin.set(false);
        NotationStyle::configure("ja_JP", "auto", false);
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
        AppSettings::language.set(language);
        AppSettings::moveNotation.set(notation);
        AppSettings::notationOrigin.set(alwaysOrigin);
        QCOMPARE(resignLabel(), QStringLiteral("▲投了"));
        {
            const ApplicationTranslations translations(QStringLiteral(TRANSLATIONS_DIR));
            QCOMPARE(translations.language(), language);
            QCoreApplication::processEvents();
            QCOMPARE(resignLabel(), QStringLiteral("▲") + resign);
            QCOMPARE(KifuPresentation::rankLabel(1), firstRank);
            QCOMPARE(KifuPresentation::options().alwaysOrigin, alwaysOrigin);
        }
        QCOMPARE(resignLabel(), QStringLiteral("▲投了"));
    }

    void startupQtTranslationsLiveForScope()
    {
        QTranslator reference;
        if (!reference.load("qtbase_ja", QLibraryInfo::path(QLibraryInfo::TranslationsPath)))
            QSKIP("Qt Japanese translations are not installed");
        const QString cancel = reference.translate("QPlatformTheme", "Cancel");
        QVERIFY(!cancel.isEmpty());
        const QString original = QCoreApplication::translate("QPlatformTheme", "Cancel");
        AppSettings::language.set("ja_JP");
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
        AppSettings::language.set("en");
        AppSettings::moveNotation.set("western");
        AppSettings::notationOrigin.set(true);
        QTest::ignoreMessage(QtWarningMsg, "Translation file not found: \"en\"");
        const ApplicationTranslations translations(emptyDirectory.path());
        QCOMPARE(translations.language(), QStringLiteral("en"));
        QCOMPARE(resignLabel(), QStringLiteral("▲投了"));
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
        NotationStyle::configure(language, "auto", false);
        const QString initial = SfenUtils::hirateSfen();
        const QStringList usiMoves = QStringList{"7g7f", "3c3d"}.mid(0, plies);
        auto moves = TestReplay::buildGameMoves(initial, usiMoves);
        auto history = TestReplay::buildSfenRecord(initial, usiMoves, false);
        KifuBranchTree tree;
        tree.setRootSfen(initial);
        auto* node = tree.root();
        for (int i = 0; i < plies; ++i) {
            const QString japanese = KifuPresentation::move(history.at(i), usiMoves.at(i),
                {KifuPresentation::Notation::Japanese, false});
            node = TestTree::addMove(tree, node, moves.at(i), japanese, history.at(i + 1));
        }
        KifuNavigationState navigation(&tree);
        LiveGameSession session(&tree, &navigation);
        KifuRecordListModel rows(&tree);
        KifuBranchListModel branches(&tree);
        KifuDisplayPresenter::Deps displayDeps;
        displayDeps.navState = &navigation;
        displayDeps.liveSession = &session;
        displayDeps.recordModel = &rows;
        displayDeps.branchModel = &branches;
        KifuDisplayPresenter display(displayDeps);
        navigation.setCurrentNode(node);
        session.startFromNode(node);
        // 棋譜の受け手（本体と同じ GameSession。追記に使う部品と、作るときに要る対局の司令塔だけ渡す）
        AppActivityState activity;
        KifuDocument document(&tree);
        GameBoard gc;
        ShogiClock clock;
        EngineSlots engines;
        MatchCoordinator match({&gc, &clock, &activity, &engines, &document});
        CsaGameCoordinator csa;
        GameSession::Deps deps;
        deps.activity = &activity;
        deps.match = &match;
        deps.csaGame = &csa;
        deps.navState = &navigation;
        deps.liveSession = &session;
        deps.document = &document;
        GameSession service(deps);
        const RecordEntry timeout = RecordEntry::ending(TerminalType::Timeout,
            plies == 2 ? Turn::Black : Turn::White, RecordTime::fromMilliseconds(1000, 1000));
        service.appendRecord(timeout);
        const auto index = rows.index(plies + 1, 0);
        QVERIFY(rows.nodeAt(plies + 1)->isTerminal());
        QVERIFY(index.data().toString().endsWith(KifuPresentation::entryLabel(timeout, QString())));
        if (language == "en") QVERIFY(index.data().toString().endsWith("Time loss"));
        auto* end = session.liveNode();
        QCOMPARE(end->terminalType(), TerminalType::Timeout);
        QCOMPARE(end->move().movingPiece, Piece::None);
        QCOMPARE(end->sfen(), node->sfen());
        QVERIFY(!session.canAddMove());
        QVERIFY(end->isTerminal());
        QVERIFY(!end->isActualMove());
        KifuExportMetadata context;
        context.startSfen = initial;
        const QString saved = KifuExport::lines(tree, context, KifuFormat::Kif).join('\n');
        QVERIFY(saved.contains(QStringLiteral("時間切れ")));
        QVERIFY(!saved.contains("Time loss"));
        const auto reloaded = KifuLoadParser::parseText(saved);
        QVERIFY2(reloaded.success, qPrintable(reloaded.error));
        QCOMPARE(reloaded.record.mainline.usiMoves(), usiMoves);
        KifuBranchTree reopened;
        KifuBranchTreeBuilder::buildFromKifParseResult(&reopened, reloaded.record);
        QCOMPARE(reopened.mainLine().last()->terminalType(), TerminalType::Timeout);
    }

    void whiteStartAnalysisUsesPositionAndMoveIdentity()
    {
        QString initial = SfenUtils::hirateSfen();
        initial.replace(" b ", " w ");
        QStringList history{initial, SfenReplay::positionAfter(initial, "3c3d")};
        QVERIFY(!history.last().isEmpty());
        QStringList moves{QStringLiteral("3c3d")};
        KifuAnalysisListModel model;
        KifuAnalysisRowPresenter::Refs refs;
        refs.analysisModel = &model;
        RecordLine line = TestReplay::recordLine(history, TestReplay::buildGameMoves(initial, moves));
        refs.line = &line;
        KifuAnalysisRowPresenter handler(refs);
        NotationStyle::configure("en", "auto", false);
        PlyAnalysis first;
        first.ply = 0;
        first.score = UsiScore::cp(100);
        first.pv = PvText::play(history.at(0), QStringList{QStringLiteral("3c3d"), QStringLiteral("7g7f")});
        handler.append(first);
        QCOMPARE(model.index(0, 3).data().toString(), QStringLiteral("-100"));
        PlyAnalysis second;
        second.ply = 1;
        second.score = UsiScore::cp(200);
        second.pv = PvText::play(history.at(1), QStringList{QStringLiteral("7g7f")});
        handler.append(second);
        QCOMPARE(model.index(1, 3).data().toString(), QStringLiteral("200"));
        QCOMPARE(model.index(1, 1).data().toString(), QStringLiteral("△P-3d"));
        QCOMPARE(model.index(1, 2).data().toString(), QStringLiteral("◯"));
        // Equal labels must not create a match between different positions.
        KifuAnalysisRow otherPosition = *model.row(1);
        otherPosition.candidateSfen = history.last();
        model.appendRow(otherPosition);
        QVERIFY(model.index(2, 2).data().toString().isEmpty());
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
        KifuBranchTreeBuilder::buildFromKifParseResult(&tree, imported.record);
        KifuExportMetadata context;
        context.startSfen = imported.startSfen();
        context.gameInfoItems = imported.gameInfo;
        const QStringList expected = KifuExport::lines(tree, context, KifuFormat::Kif);

        QTranslator translator;
        QVERIFY(translator.load(QStringLiteral(TRANSLATIONS_DIR "/ShogiBoardQ_") + language + ".qm"));
        qApp->installTranslator(&translator);
        NotationStyle::configure(language, notation, false);
        const bool western = KifuPresentation::options().notation == KifuPresentation::Notation::Western;
        ApplicationFonts::initialize({}, language);

        KifuRecordListModel rows(&tree);
        rows.setPath(tree.mainLine());
        QVERIFY(rows.data(rows.index(1, 0), Qt::DisplayRole).toString().contains(
            western ? QStringLiteral("▲P-7f") : QStringLiteral("７六歩")));
        QVERIFY(rows.data(rows.index(3, 0), Qt::DisplayRole).toString().contains(
            western ? QStringLiteral("Bx2b+") : QStringLiteral("角成")));
        QVERIFY(rows.nodeAt(1)->displayText().contains(QStringLiteral("７六歩")));
        QCOMPARE(rows.data(rows.index(1, KifuRecordListModel::CommentColumn)).toString(),
                 QStringLiteral("日本語の注釈 <tag>"));

        KifuBranchListModel branches(&tree);
        branches.setCandidates({tree.mainLine().at(3)}, -1, false);
        if (western) QVERIFY(branches.data(branches.index(0, 0), Qt::DisplayRole).toString().endsWith('+'));

        ShogiEngineThinkingModel thinking;
        ThinkingInfoRow row;
        row.timeMs = 10;
        row.depth = 5;
        row.nodes = 100;
        row.score = UsiScore::cp(25);
        const QString japanesePv = QStringLiteral("▲７六歩(77)△３四歩(33)");
        row.pv = PvText::play(imported.startSfen(), QStringLiteral("7g7f 3c3d"));
        thinking.prependRow(row, 10);
        const ShogiInfoRecord* info = thinking.recordAt(0);
        const QString pv = thinking.data(thinking.index(0, ShogiEngineThinkingModel::PvColumn), Qt::DisplayRole).toString();
        QCOMPARE(pv, western ? QStringLiteral("▲P-7f △P-3d") : japanesePv);
        // 表示用の読み筋は覚えておくが、表記の設定を変えたら作り直す
        NotationStyle::configure(language, western ? QStringLiteral("japanese") : QStringLiteral("western"), true);
        QCOMPARE(info->displayPv(), western ? japanesePv : QStringLiteral("▲P7g-7f △P3c-3d"));
        NotationStyle::configure(language, notation, false);
        QCOMPARE(info->displayPv(), pv);
        if (western) {
            QCOMPARE(info->displayPv(true), QStringLiteral("▲P7g-7f △P3c-3d"));
            ThinkingInfoRow shorter = row;
            shorter.pv = PvText::play(imported.startSfen(), QStringLiteral("7g7f"));
            QCOMPARE(ShogiInfoRecord(shorter).displayPv(), QStringLiteral("▲P-7f"));
        }
        // 棋譜解析の結果の表も同じ（表記の設定を変えたら作り直す）
        KifuAnalysisListModel results;
        KifuAnalysisRow result;
        result.ply = 1;
        result.move = QStringLiteral("▲７六歩(77)");
        result.evaluation = UsiScore::cp(10);
        result.evaluationDifference = 0;
        result.pv = info->pv();  // 思考の表と同じ読み筋の値
        result.beforeSfen = imported.startSfen();
        result.lastUsiMove = QStringLiteral("7g7f");
        result.sfen = imported.startSfen();
        results.appendRow(result);
        const auto cell = [&results](int resultRow, int column) { return results.index(resultRow, column).data().toString(); };
        // 手数は表が付ける（棋譜欄と同じ書式）
        QCOMPARE(cell(0, KifuAnalysisListModel::MoveColumn), western ? QStringLiteral("   1 ▲P-7f") : QStringLiteral("   1 ▲７六歩(77)"));
        QCOMPARE(cell(0, KifuAnalysisListModel::PvColumn), pv);
        // 行ごとに、その行の値から表記を作る
        result.lastUsiMove = QStringLiteral("2g2f");
        results.appendRow(result);
        QCOMPARE(cell(1, KifuAnalysisListModel::MoveColumn), western ? QStringLiteral("   1 ▲P-2f") : QStringLiteral("   1 ▲７六歩(77)"));
        result.beforeSfen.clear();
        result.branch = true;
        results.appendRow(result);
        QCOMPARE(cell(2, KifuAnalysisListModel::MoveColumn),
                 western ? QStringLiteral("   1 2g2f [+]") : QStringLiteral("   1 ▲７六歩(77)+"));
        result.pv = PvText::play(imported.startSfen(), QStringLiteral("7g7f"));
        results.appendRow(result);
        QCOMPARE(cell(3, KifuAnalysisListModel::PvColumn), western ? QStringLiteral("▲P-7f") : QStringLiteral("▲７六歩(77)"));
        // UIの書体と棋譜の書体を分け、ビュー個別の文字サイズを維持する。
        QFont viewFont = QApplication::font();
        viewFont.setPointSize(19);
        viewFont.setBold(true);
        const QFont expectedFont = western ? viewFont : ApplicationFonts::japaneseFont(viewFont);
        for (const auto& index : {rows.index(1, 0), branches.index(0, 0), thinking.index(0, ShogiEngineThinkingModel::PvColumn)}) {
            const QVariant role = index.data(Qt::FontRole);
            const QFont painted = role.isValid() ? qvariant_cast<QFont>(role).resolve(viewFont) : viewFont;
            QCOMPARE(painted.family(), expectedFont.family());
            QCOMPARE(painted.pointSize(), 19);
            QVERIFY(painted.bold());
        }
        QVERIFY(!rows.index(1, 3).data(Qt::FontRole).isValid()); // 注釈はUIの書体。
        QVERIFY(!rows.index(4, 0).data(Qt::FontRole).isValid()); // 翻訳された終局もUIの書体。
        const PvLine board(info->pv());
        QCOMPARE(board.displayPv(), pv);
        PvBoardDialog pvDialog(info->pv());
        bool foundPv = false;
        for (const auto* label : pvDialog.findChildren<QLabel*>()) {
            if (label->text() == pv) {
                foundPv = true;
                QCOMPARE(label->font().family(), expectedFont.family());
            }
        }
        QVERIFY(foundPv);
        QCOMPARE(board.moveCount(), 2);
        if (western) QCOMPARE(board.moveText(1), QStringLiteral("▲P-7f"));

        // Network game results must stay canonical even with a translated UI.
        CsaGameProgress csa;  // 自分は先手。終局した局面は後手番
        using Cause = CsaClient::GameEndCause;
        const QList<QPair<Cause, TerminalType>> endings = {
            {Cause::Resign, TerminalType::Resign}, {Cause::TimeUp, TerminalType::Timeout},
            // 連続王手の千日手は手番側の行。先手（自分）の王手で成立して後手番なので、後手の反則勝ち
            {Cause::IllegalMove, TerminalType::IllegalLoss}, {Cause::OuteSennichite, TerminalType::IllegalWin},
            {Cause::Sennichite, TerminalType::Repetition}, {Cause::Jishogi, TerminalType::DeclarationWin},
            {Cause::MaxMoves, TerminalType::MaxMoves}, {Cause::Chudan, TerminalType::Interrupt}};
        for (const auto& ending : endings) {
            QCOMPARE(csa.endRecord(CsaClient::GameResult::Lose, ending.first, false, 0).terminal, ending.second);
        }
        // 入玉宣言は宣言した勝者の印を付け（保存時に勝者を判定するため）、点数による引き分けは手番側の持将棋
        QCOMPARE(csa.endRecord(CsaClient::GameResult::Lose, Cause::Jishogi, false, 0).displayText,
                 QStringLiteral("△入玉勝ち"));
        QCOMPARE(csa.endRecord(CsaClient::GameResult::Draw, Cause::Jishogi, false, 0).displayText,
                 QStringLiteral("△持将棋"));

        GameInfoPane metadata;
        metadata.setGameInfo(imported.gameInfo);
        GameInfoTable table{metadata.tableView()};
        QCOMPARE(table.text(0, 0), QStringLiteral("先手"));
        QCOMPARE(table.text(0, 1), QStringLiteral("佐藤 一郎"));
        QCOMPARE(table.accessibleText(0, 0), KifuPresentation::infoKey("先手"));
        QCOMPARE(metadata.gameInfo().size(), imported.gameInfo.size());
        context.gameInfoItems = metadata.gameInfo();
        QCOMPARE(KifuExport::lines(tree, context, KifuFormat::Kif), expected);
        table.setText(0, 1, QStringLiteral("編集した名前"));
        metadata.undo();
        QCOMPARE(metadata.gameInfo().first().value, QStringLiteral("佐藤 一郎"));
        metadata.redo();
        QCOMPARE(metadata.gameInfo().first().value, QStringLiteral("編集した名前"));
        metadata.undo();
        QVERIFY(expected.contains(QStringLiteral("独自タグ：最初")));
        QVERIFY(expected.contains(QStringLiteral("独自タグ：次の値")));
        const auto reread = KifuLoadParser::parseText(expected.join('\n'));
        QVERIFY2(reread.success, qPrintable(reread.error));
        QCOMPARE(reread.record.mainline.usiMoves(), imported.record.mainline.usiMoves());

        // All existing save formats must be independent of presentation settings.
        using Format = KifuFormat;
        for (auto format : {Format::Kif, Format::Ki2, Format::Csa, Format::Jkf, Format::Usi, Format::Usen}) {
            KifuConversionService::Request request;
            request.text = kif;
            request.outputFormat = format;
            const auto localized = KifuConversionService::convert(request);
            QVERIFY2(localized.ok, qPrintable(localized.failure.message));
            NotationStyle::configure("ja_JP", "japanese", false);
            const auto japanese = KifuConversionService::convert(request);
            QCOMPARE(localized.lines, japanese.lines);
            NotationStyle::configure(language, notation, false);
        }
        qApp->removeTranslator(&translator);
    }

    void handicapNamesAreTranslated_data()
    {
        QTest::addColumn<QString>("language");
        for (const auto* language : {"en", "zh_CN", "zh_TW"}) QTest::newRow(language) << QString::fromLatin1(language);
    }

    /// 対局情報の手合割は、手合割の表（Handicaps::kAll）のどの名前も表示言語に訳す（B4）
    void handicapNamesAreTranslated()
    {
        QFETCH(QString, language);
        QTranslator translator;
        QVERIFY(translator.load(QStringLiteral(TRANSLATIONS_DIR "/ShogiBoardQ_") + language + ".qm"));
        qApp->installTranslator(&translator);
        for (const Handicaps::Handicap& handicap : Handicaps::kAll) {
            const QString label = Handicaps::labelOf(handicap);
            QVERIFY2(!translator.translate("KifuPresentation", handicap.label).isEmpty(), qPrintable(label));
            if (language == QLatin1String("en"))
                QVERIFY2(KifuPresentation::infoValue(QStringLiteral("手合割"), label) != label, qPrintable(label));
        }
        qApp->removeTranslator(&translator);
    }
};
QTEST_MAIN(TestI18nWorkflows)
#include "tst_i18n_workflows.moc"
