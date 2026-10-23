#include <QtTest>
#include <QAction>
#include <QCheckBox>
#include <QTemporaryDir>
#include "analysissession.h"
#include "analysissettings.h"
#include "appactivitystate.h"
#include "enginelifecyclemanager.h"
#include "josekibooksession.h"
#include "candidatearrowpresenter.h"
#include "considerationsettings.h"
#include "thinkingpanel.h"
#include "sfenutils.h"
#include "shogiboard.h"
#include "shogiclock.h"
#include "shogigamecontroller.h"
#include "shogienginethinkingmodel.h"
#include "thinkinginfoformatter.h"
#include "usiprotocolhandler.h"

class TestCandidateArrows : public QObject
{
    Q_OBJECT
    QTemporaryDir m_settings;
private slots:
    void initTestCase()
    {
        QVERIFY(m_settings.isValid());
        qputenv("XDG_CONFIG_HOME", m_settings.path().toUtf8());
        qputenv("SHOGIBOARDQ_CONFIG_HOME", m_settings.path().toUtf8());
    }
    void latestCandidateAndSearchLifetime()
    {
        ThinkingInfoFormatter presenter;
        UsiProtocolHandler protocol;
        connect(&protocol, &UsiProtocolHandler::infoReceived, &presenter, &ThinkingInfoFormatter::onInfoReceived);
        connect(&protocol, &UsiProtocolHandler::positionChanged, &presenter, &ThinkingInfoFormatter::setSearchPosition);
        // 読み筋履歴のパーサにも実局面を渡す。
        protocol.startSearch(*UsiPosition::fromSfen(SfenUtils::hirateSfen()), UsiGo::infinite());
        protocol.onDataReceived("info depth 10 score cp 300 pv 7g7f 3c3d");
        QCOMPARE(protocol.searchCandidate().move, QStringLiteral("7g7f"));
        protocol.onDataReceived("info depth 20 score cp 100 pv 2g2f 8c8d");
        QCOMPARE(protocol.searchCandidate().move, QStringLiteral("2g2f"));
        for (const QString& line : {QStringLiteral("info multipv 2 pv 9g9f"),
                                   QStringLiteral("info nodes 1000"),
                                   QStringLiteral("info string pv 9g9f"),
                                   QStringLiteral("info pv resign")}) {
            protocol.onDataReceived(line);
            QCOMPARE(protocol.searchCandidate().move, QStringLiteral("2g2f"));
        }
        protocol.sendStop();
        protocol.onDataReceived("info pv 9g9f");
        QVERIFY(!protocol.searchCandidate().active);
        protocol.onDataReceived("bestmove 2g2f ponder 8c8d");
        QVERIFY(protocol.searchCandidate().move.isEmpty());
        // 遅延表示バッファを処理しても盤面用の候補は復活しない。
        presenter.flushInfoBuffer();
        QVERIFY(!protocol.searchCandidate().active);
        protocol.startSearch(*UsiPosition::fromSfen(SfenUtils::hirateSfen()), UsiGo::infinite());
        QVERIFY(protocol.searchCandidate().move.isEmpty());
        protocol.onDataReceived("info pv 7g7f");
        protocol.cancelCurrentOperation();
        protocol.onDataReceived("info pv 2g2f");
        QVERIFY(!protocol.searchCandidate().active);
    }
    void ponderHitAndMiss()
    {
        ThinkingInfoFormatter presenter;
        UsiProtocolHandler protocol;
        connect(&protocol, &UsiProtocolHandler::infoReceived, &presenter, &ThinkingInfoFormatter::onInfoReceived);
        connect(&protocol, &UsiProtocolHandler::positionChanged, &presenter, &ThinkingInfoFormatter::setSearchPosition);
        protocol.onDataReceived("bestmove 7g7f ponder 3c3d");
        protocol.startSearch(*UsiPosition::fromSfen(SfenUtils::hirateSfen()), UsiGo::ponder({}));
        protocol.onDataReceived("info pv 2g2f");
        QVERIFY(protocol.searchCandidate().pondering);
        QCOMPARE(protocol.searchCandidate().predictedMove, QStringLiteral("3c3d"));
        protocol.sendPonderHit();
        QVERIFY(protocol.searchCandidate().active);
        QVERIFY(!protocol.searchCandidate().pondering);
        QCOMPARE(protocol.searchCandidate().move, QStringLiteral("2g2f"));
        protocol.onDataReceived("bestmove 2g2f ponder 8c8d");
        protocol.startSearch(*UsiPosition::fromSfen(SfenUtils::hirateSfen()), UsiGo::ponder({}));
        protocol.onDataReceived("info pv 2f2e");
        protocol.sendStop();
        protocol.onDataReceived("info pv 9g9f");
        QVERIFY(!protocol.searchCandidate().active);
        protocol.onDataReceived("bestmove resign");
        protocol.startSearch(*UsiPosition::fromSfen(SfenUtils::hirateSfen()), UsiGo::infinite());
        QVERIFY(protocol.searchCandidate().move.isEmpty());
    }
    void sharedConversion()
    {
        const auto black = SfenUtils::hirateSfen();
        const auto white = QString(black).replace(" b ", " w ");
        const auto promotion = CandidateArrowPresenter::arrowForMove("8h2b+", black, 1);
        QVERIFY(promotion);
        QCOMPARE(promotion->fromFile, 8);
        QCOMPARE(promotion->toRank, 2);
        const auto drop = CandidateArrowPresenter::arrowForMove("P*5e", white, 2);
        QVERIFY(drop);
        QCOMPARE(drop->fromFile, 0);
        QCOMPARE(drop->dropPiece, Piece::WhitePawn);
        QCOMPARE(drop->priority, 2);
        QVERIFY(!CandidateArrowPresenter::arrowForMove("resign", black, 1));
        QVERIFY(!CandidateArrowPresenter::arrowForMove("7g0f", black, 1));
    }
    /// 定跡手の矢印: 表の順に番号を付け、出現頻度の高い手ほど濃い緑にする。定跡が変われば描き直し、
    /// 「盤に定跡手を表示」がオフのとき・定跡ウィンドウで表示を止めたときは出さない
    void josekiArrows()
    {
        const QString hirate = SfenUtils::hirateSfen();
        ShogiBoard board;
        board.setSfen(hirate);
        ShogiView view;
        view.setBoard(&board);
        JosekiBookSession book;
        book.addMove(hirate, {QStringLiteral("7g7f"), QStringLiteral("none"), 0, 0, 30, {}});
        book.addMove(hirate, {QStringLiteral("2g2f"), QStringLiteral("none"), 0, 0, 10, {}});
        QAction arrowsAction;
        arrowsAction.setCheckable(true);
        QAction displayAction;
        displayAction.setCheckable(true);
        displayAction.setChecked(true);
        CandidateArrowPresenter::Deps deps;
        deps.view = &view;
        deps.josekiBook = &book;
        deps.josekiArrows = &arrowsAction;
        deps.josekiDisplay = &displayAction;
        CandidateArrowPresenter presenter(deps);
        presenter.refresh();
        QVERIFY(view.arrows().isEmpty());

        arrowsAction.setChecked(true);
        QTRY_COMPARE(view.arrows().size(), 2);
        QCOMPARE(view.arrows().at(0).priority, 1);
        QCOMPARE(view.arrows().at(1).priority, 2);
        QCOMPARE(view.arrows().at(0).toFile, 7);
        QCOMPARE(view.arrows().at(0).color.green(), 150);
        QVERIFY(view.arrows().at(0).color.alpha() > view.arrows().at(1).color.alpha());

        book.addMove(hirate, {QStringLiteral("5g5f"), QStringLiteral("none"), 0, 0, 0, {}});
        QTRY_COMPARE(view.arrows().size(), 3);
        displayAction.setChecked(false);
        QTRY_VERIFY(view.arrows().isEmpty());
        displayAction.setChecked(true);
        QTRY_COMPARE(view.arrows().size(), 3);
        board.setSfen(QStringLiteral("4k4/9/9/9/9/9/9/9/4K4 b - 1"));
        presenter.refresh();
        QVERIFY(view.arrows().isEmpty());
        board.setSfen(hirate);
        arrowsAction.setChecked(false);
        QTRY_VERIFY(view.arrows().isEmpty());
    }
    void considerationAndSettings()
    {
        AnalysisSettings::matchArrowsVisible.set(true);
        AnalysisSettings::ponderArrowsVisible.set(true);
        ShogiBoard board;
        board.setSfen(SfenUtils::hirateSfen());
        ShogiView view;
        view.setBoard(&board);
        ShogiClock clock;
        AppActivityState activity;
        EngineLifecycleManager engines;
        AnalysisSession session(AnalysisSession::Deps{&engines, &activity});
        ShogiEngineThinkingModel model;
        ConsiderationSettings considerationSettings;   // 検討タブの設定（showArrows）
        CandidateArrowPresenter::Deps deps;
        deps.view = &view;
        deps.analysis = &session;
        deps.consideration = &model;
        deps.considerationSettings = &considerationSettings;
        CandidateArrowPresenter presenter(deps);
        auto* controller = &presenter;
        ThinkingPanel tab;
        connect(controller, &CandidateArrowPresenter::displayStateChanged,
                &tab, &ThinkingPanel::setArrowDisplayState);
        connect(&tab, &ThinkingPanel::arrowSettingsChanged,
                controller, &CandidateArrowPresenter::scheduleRefresh);
        auto* master = tab.findChild<QCheckBox*>("matchArrows");
        auto* ponder = tab.findChild<QCheckBox*>("ponderArrows");
        QVERIFY(master && ponder);
        master->setChecked(false);
        QVERIFY(!ponder->isEnabled());
        QVERIFY(ponder->isChecked());
        QVERIFY(AnalysisSettings::ponderArrowsVisible());
        master->setChecked(true);
        ThinkingInfoRow record;
        record.score = UsiScore::cp(30);
        record.pv = PvText::play(SfenUtils::hirateSfen(), QStringLiteral("7g7f 3c3d"));
        record.multipv = 1;
        model.updateByMultipv(record, 3);
        // 検討中かは検討のセッションが決める（矢印は検討の表から作る）
        AnalysisOptions options;
        options.mode = AnalysisOptions::Mode::Consideration;
        options.enginePath = QCoreApplication::applicationDirPath() + QStringLiteral("/mock_usi_match");
        options.engineName = QStringLiteral("ArrowTest");
        options.position = *UsiPosition::parse(QStringLiteral("position startpos"));
        QVERIFY(session.start(options));
        QTRY_COMPARE(view.arrows().size(), 1);
        QVERIFY(!master->isEnabled());
        QVERIFY(!ponder->isEnabled());
        ThinkingInfoRow second;
        second.score = UsiScore::cp(20);
        second.pv = PvText::play(SfenUtils::hirateSfen(), QStringLiteral("2g2f"));
        second.multipv = 2;
        model.updateByMultipv(second, 3);
        QTRY_COMPARE(view.arrows().size(), 2);
        QCOMPARE(view.arrows().at(0).priority, 1);
        QCOMPARE(view.arrows().at(1).priority, 2);
        QVERIFY(view.arrows().at(0).color.alpha() > view.arrows().at(1).color.alpha());
        model.trimToMaxRows(1);
        QTRY_COMPARE(view.arrows().size(), 1);
        // 検討タブの「矢印」の設定を切ると消え、戻すと出る（設定の変更で描き直す）
        considerationSettings.showArrows = false;
        controller->scheduleRefresh();
        QTRY_VERIFY(view.arrows().isEmpty());
        considerationSettings.showArrows = true;
        controller->scheduleRefresh();
        QTRY_COMPARE(view.arrows().size(), 1);
        board.setSfen(SfenUtils::hirateSfen().replace(" b ", " w "));
        QTRY_VERIFY(view.arrows().isEmpty());
        board.setSfen(SfenUtils::hirateSfen());
        QTRY_COMPARE(view.arrows().size(), 1);
        session.stop();
        QTRY_VERIFY(view.arrows().isEmpty());
        QVERIFY(master->isEnabled());
        model.clear();
        QCoreApplication::processEvents();
        QVERIFY(view.arrows().isEmpty());
    }
};
QTEST_MAIN(TestCandidateArrows)
#include "tst_candidate_arrows.moc"
