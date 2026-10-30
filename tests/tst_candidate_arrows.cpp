#include <QtTest>
#include <QAction>
#include <QCheckBox>
#include <QTemporaryDir>
#include "analysissession.h"
#include "analysissettings.h"
#include "appactivitystate.h"
#include "engineslots.h"
#include "josekibooksession.h"
#include "boardarrowactions.h"
#include "candidatearrowpresenter.h"
#include "considerationpanel.h"
#include "josekisettings.h"
#include "sfenreplay.h"
#include "thinkingpanel.h"
#include "sfenutils.h"
#include "shogiboard.h"
#include "shogiclock.h"
#include "gameboard.h"
#include "shogienginethinkingmodel.h"
#include "usi.h"
#include "usienginesession.h"
#include "usiprotocolhandler.h"

class TestCandidateArrows : public QObject
{
    Q_OBJECT
    QTemporaryDir m_settings;

    /// 思考の表の1行（局面 baseSfen の読み筋 pv。lastMove はその局面に至った手）
    static ThinkingInfoRow row(const QString& baseSfen, const QString& pv, int multipv = 1,
                               const QString& lastMove = QString())
    {
        ThinkingInfoRow result;
        result.score = UsiScore::cp(30);
        result.pv = PvText::play(baseSfen, pv);
        result.lastUsiMove = lastMove;
        result.multipv = multipv;
        return result;
    }

private slots:
    void initTestCase()
    {
        QVERIFY(m_settings.isValid());
        qputenv("XDG_CONFIG_HOME", m_settings.path().toUtf8());
        qputenv("SHOGIBOARDQ_CONFIG_HOME", m_settings.path().toUtf8());
    }
    /// 対局の手番側の矢印: エンジンの思考の表の最新の最善の読み筋（multipv 1）の最初の手を赤の実線で出す。
    /// 探索を終えたら、思考の表に残った行からは描かない
    void matchArrowsFollowThinkingRows()
    {
        const QString hirate = SfenUtils::hirateSfen();
        ShogiBoard board;
        board.setSfen(hirate);
        ShogiView view;
        view.setBoard(&board);
        AppActivityState activity;
        ActivityLease game = activity.begin(AppActivityState::Activity::DuringGame, nullptr);
        EngineSlots engines;
        EngineLease engine = engines.acquire(this, 1, QStringLiteral("E1"));
        UsiProtocolHandler* protocol = engine.engine()->session()->handler();
        ShogiEngineThinkingModel thinking1;
        ShogiEngineThinkingModel thinking2;
        BoardArrowActions toggles(nullptr);
        toggles.match()->setChecked(true);
        toggles.ponder()->setChecked(false);
        CandidateArrowPresenter::Deps deps;
        deps.view = &view;
        deps.engines = &engines;
        deps.thinking = {&thinking1, &thinking2};
        deps.activity = &activity;
        deps.arrows = &toggles;
        CandidateArrowPresenter presenter(deps);

        protocol->startSearch(*UsiPosition::fromSfen(hirate), UsiGo::infinite());
        thinking1.prependRow(row(hirate, QStringLiteral("7g7f 3c3d")), 100);
        QTRY_COMPARE(view.highlighting()->arrows().size(), 1);
        QCOMPARE(view.highlighting()->arrows().first().toFile, 7);
        QCOMPARE(view.highlighting()->arrows().first().penStyle, Qt::SolidLine);
        QVERIFY(view.highlighting()->arrows().first().color.red() > view.highlighting()->arrows().first().color.blue());
        thinking1.prependRow(row(hirate, QStringLiteral("2g2f 8c8d")), 100);
        QTRY_COMPARE(view.highlighting()->arrows().first().toFile, 2);
        // 2番目以降の候補手と、最初の手を指せない読み筋は使わない
        thinking1.prependRow(row(hirate, QStringLiteral("9g9f"), 2), 100);
        thinking1.prependRow(row(hirate, QStringLiteral("resign")), 100);
        presenter.refresh();
        QCOMPARE(view.highlighting()->arrows().size(), 1);
        QCOMPARE(view.highlighting()->arrows().first().toFile, 2);

        toggles.match()->setChecked(false);
        QTRY_VERIFY(view.highlighting()->arrows().isEmpty());
        toggles.match()->setChecked(true);
        QTRY_COMPARE(view.highlighting()->arrows().size(), 1);
        // 探索の局面と違う局面を表示している
        board.setSfen(QString(hirate).replace(QStringLiteral(" b "), QStringLiteral(" w ")));
        QTRY_VERIFY(view.highlighting()->arrows().isEmpty());
        board.setSfen(hirate);
        QTRY_COMPARE(view.highlighting()->arrows().size(), 1);

        // 探索を終えた（bestmove）。思考の表は残る
        protocol->sendStop();
        protocol->onDataReceived(QStringLiteral("bestmove 2g2f"));
        QTRY_VERIFY(view.highlighting()->arrows().isEmpty());
        QCOMPARE(thinking1.rowCount(), 4);

        // 対局中でなければ描かない
        protocol->startSearch(*UsiPosition::fromSfen(hirate), UsiGo::infinite());
        thinking1.prependRow(row(hirate, QStringLiteral("5g5f")), 100);
        QTRY_COMPARE(view.highlighting()->arrows().size(), 1);
        game.reset();
        QTRY_VERIFY(view.highlighting()->arrows().isEmpty());
    }

    /// 先読み側の矢印: 表示中の局面に予想した手を指した局面の読みを青の破線で出す。予想が当たれば
    /// 新しい info を待たずに手番側（赤の実線）になる。先読みでない探索の行は、局面が合っても先読みとみなさない
    void ponderArrowsUsePredictedPosition()
    {
        const QString hirate = SfenUtils::hirateSfen();
        const UsiPosition predicted = *UsiPosition::parse(u"position startpos moves 7g7f");
        ShogiBoard board;
        board.setSfen(hirate);
        ShogiView view;
        view.setBoard(&board);
        AppActivityState activity;
        ActivityLease game = activity.begin(AppActivityState::Activity::DuringGame, nullptr);
        EngineSlots engines;
        EngineLease engine = engines.acquire(this, 2, QStringLiteral("E2"));
        UsiProtocolHandler* protocol = engine.engine()->session()->handler();
        ShogiEngineThinkingModel thinking1;
        ShogiEngineThinkingModel thinking2;
        BoardArrowActions toggles(nullptr);
        toggles.match()->setChecked(true);
        toggles.ponder()->setChecked(false);
        CandidateArrowPresenter::Deps deps;
        deps.view = &view;
        deps.engines = &engines;
        deps.thinking = {&thinking1, &thinking2};
        deps.activity = &activity;
        deps.arrows = &toggles;
        CandidateArrowPresenter presenter(deps);

        // 先読みでない探索は、表示中の局面に至った手が合っても先読みの矢印にしない
        protocol->startSearch(predicted, UsiGo::infinite());
        thinking2.prependRow(row(predicted.sfen(), QStringLiteral("3c3d"), 1, QStringLiteral("7g7f")), 100);
        toggles.ponder()->setChecked(true);
        presenter.refresh();
        QVERIFY(view.highlighting()->arrows().isEmpty());
        protocol->sendStop();
        protocol->onDataReceived(QStringLiteral("bestmove 3c3d"));
        toggles.ponder()->setChecked(false);

        protocol->startSearch(predicted, UsiGo::ponder({}));
        thinking2.clear();
        thinking2.prependRow(row(predicted.sfen(), QStringLiteral("3c3d 2g2f"), 1, QStringLiteral("7g7f")), 100);
        presenter.refresh();
        QVERIFY(view.highlighting()->arrows().isEmpty());
        toggles.ponder()->setChecked(true);
        QTRY_COMPARE(view.highlighting()->arrows().size(), 1);
        QCOMPARE(view.highlighting()->arrows().first().penStyle, Qt::DashLine);
        QVERIFY(view.highlighting()->arrows().first().color.blue() > view.highlighting()->arrows().first().color.red());
        QCOMPARE(view.highlighting()->arrows().first().toFile, 3);
        QVERIFY(presenter.ponderDescription().startsWith(QStringLiteral("先読み：")));
        QVERIFY(presenter.ponderDescription().contains(QStringLiteral("７六歩")));
        // 手番側の矢印を切ると先読み側も出さない
        toggles.match()->setChecked(false);
        QTRY_VERIFY(view.highlighting()->arrows().isEmpty());
        toggles.match()->setChecked(true);
        QTRY_COMPARE(view.highlighting()->arrows().size(), 1);

        // 予想が外れた
        board.setSfen(SfenReplay::positionAfter(hirate, QStringLiteral("2g2f")));
        QTRY_VERIFY(view.highlighting()->arrows().isEmpty());
        QVERIFY(presenter.ponderDescription().isEmpty());
        // 予想が当たった（ponderhit の前でも、探索の局面が表示中の局面なら手番側）
        board.setSfen(predicted.sfen());
        QTRY_COMPARE(view.highlighting()->arrows().size(), 1);
        QCOMPARE(view.highlighting()->arrows().first().penStyle, Qt::SolidLine);
        protocol->sendPonderHit();
        presenter.refresh();
        QCOMPARE(view.highlighting()->arrows().first().penStyle, Qt::SolidLine);
        protocol->sendStop();
        protocol->onDataReceived(QStringLiteral("bestmove 3c3d"));
        QTRY_VERIFY(view.highlighting()->arrows().isEmpty());
    }

    /// 4つの切り替えは変えるたびに保存し、次に作ったときに戻す（検討の矢印も保存する）
    void togglesPersist()
    {
        {
            QAction menu;
            BoardArrowActions toggles(&menu);
            QCOMPARE(toggles.joseki(), &menu);
            QVERIFY(menu.isCheckable());
            toggles.match()->setChecked(true);
            toggles.ponder()->setChecked(true);
            toggles.consideration()->setChecked(false);
            menu.setChecked(true);
        }
        QVERIFY(AnalysisSettings::matchArrowsVisible());
        QVERIFY(AnalysisSettings::ponderArrowsVisible());
        QVERIFY(!AnalysisSettings::considerationArrowsVisible());
        QVERIFY(JosekiSettings::boardArrowsVisible());
        {
            QAction menu;
            BoardArrowActions toggles(&menu);
            QVERIFY(toggles.match()->isChecked());
            QVERIFY(toggles.ponder()->isChecked());
            QVERIFY(!toggles.consideration()->isChecked());
            QVERIFY(menu.isChecked());
            toggles.match()->setChecked(false);
            toggles.ponder()->setChecked(false);
            toggles.consideration()->setChecked(true);
            menu.setChecked(false);
        }
        QVERIFY(AnalysisSettings::considerationArrowsVisible());
        QVERIFY(!JosekiSettings::boardArrowsVisible());
    }

    /// 思考ドック・検討タブのチェックは切り替えの QAction に結ぶだけ（どちらから変えても揃う）
    void panelsBindToggles()
    {
        BoardArrowActions toggles(nullptr);
        toggles.consideration()->setChecked(false);
        toggles.match()->setChecked(true);
        ConsiderationPanel consideration(toggles.consideration());
        ThinkingPanel thinking({toggles.match(), toggles.ponder()});
        auto* considerationBox = consideration.findChild<QCheckBox*>(QStringLiteral("considerationArrows"));
        auto* matchBox = thinking.findChild<QCheckBox*>(QStringLiteral("matchArrows"));
        QVERIFY(considerationBox && matchBox);
        QVERIFY(!considerationBox->isChecked());
        QVERIFY(matchBox->isChecked());
        // 矢印の切り替えは検討の条件ではない（検討をやり直させない）
        QSignalSpy settingsChanged(&consideration, &ConsiderationPanel::settingsChanged);
        considerationBox->setChecked(true);
        QVERIFY(toggles.consideration()->isChecked());
        toggles.consideration()->setChecked(false);
        QVERIFY(!considerationBox->isChecked());
        QCOMPARE(settingsChanged.count(), 0);
        toggles.match()->setChecked(false);
        QVERIFY(!matchBox->isChecked());
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
        BoardArrowActions toggles(nullptr);
        QAction& arrowsAction = *toggles.joseki();
        arrowsAction.setChecked(false);
        QAction displayAction;
        displayAction.setCheckable(true);
        displayAction.setChecked(true);
        CandidateArrowPresenter::Deps deps;
        deps.view = &view;
        deps.josekiBook = &book;
        deps.arrows = &toggles;
        deps.josekiDisplay = &displayAction;
        CandidateArrowPresenter presenter(deps);
        presenter.refresh();
        QVERIFY(view.highlighting()->arrows().isEmpty());

        arrowsAction.setChecked(true);
        QTRY_COMPARE(view.highlighting()->arrows().size(), 2);
        QCOMPARE(view.highlighting()->arrows().at(0).priority, 1);
        QCOMPARE(view.highlighting()->arrows().at(1).priority, 2);
        QCOMPARE(view.highlighting()->arrows().at(0).toFile, 7);
        QCOMPARE(view.highlighting()->arrows().at(0).color.green(), 150);
        QVERIFY(view.highlighting()->arrows().at(0).color.alpha() > view.highlighting()->arrows().at(1).color.alpha());

        book.addMove(hirate, {QStringLiteral("5g5f"), QStringLiteral("none"), 0, 0, 0, {}});
        QTRY_COMPARE(view.highlighting()->arrows().size(), 3);
        displayAction.setChecked(false);
        QTRY_VERIFY(view.highlighting()->arrows().isEmpty());
        displayAction.setChecked(true);
        QTRY_COMPARE(view.highlighting()->arrows().size(), 3);
        board.setSfen(QStringLiteral("4k4/9/9/9/9/9/9/9/4K4 b - 1"));
        presenter.refresh();
        QVERIFY(view.highlighting()->arrows().isEmpty());
        board.setSfen(hirate);
        arrowsAction.setChecked(false);
        QTRY_VERIFY(view.highlighting()->arrows().isEmpty());
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
        EngineSlots engines;
        AnalysisSession session(AnalysisSession::Deps{&engines, &activity});
        ShogiEngineThinkingModel model;
        BoardArrowActions toggles(nullptr);   // 対局の矢印は設定から戻す
        toggles.consideration()->setChecked(true);
        CandidateArrowPresenter::Deps deps;
        deps.view = &view;
        deps.analysis = &session;
        deps.consideration = &model;
        deps.arrows = &toggles;
        CandidateArrowPresenter presenter(deps);
        auto* controller = &presenter;
        ThinkingPanel tab({toggles.match(), toggles.ponder()});
        connect(controller, &CandidateArrowPresenter::displayStateChanged,
                &tab, &ThinkingPanel::setArrowDisplayState);
        auto* master = tab.findChild<QCheckBox*>("matchArrows");
        auto* ponder = tab.findChild<QCheckBox*>("ponderArrows");
        QVERIFY(master && ponder);
        QVERIFY(master->isChecked() && ponder->isChecked());
        master->setChecked(false);
        QVERIFY(!toggles.match()->isChecked());
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
        QTRY_COMPARE(view.highlighting()->arrows().size(), 1);
        QVERIFY(!master->isEnabled());
        QVERIFY(!ponder->isEnabled());
        ThinkingInfoRow second;
        second.score = UsiScore::cp(20);
        second.pv = PvText::play(SfenUtils::hirateSfen(), QStringLiteral("2g2f"));
        second.multipv = 2;
        model.updateByMultipv(second, 3);
        QTRY_COMPARE(view.highlighting()->arrows().size(), 2);
        QCOMPARE(view.highlighting()->arrows().at(0).priority, 1);
        QCOMPARE(view.highlighting()->arrows().at(1).priority, 2);
        QVERIFY(view.highlighting()->arrows().at(0).color.alpha() > view.highlighting()->arrows().at(1).color.alpha());
        model.trimToMaxRows(1);
        QTRY_COMPARE(view.highlighting()->arrows().size(), 1);
        // 検討の矢印の切り替えを切ると消え、戻すと出る（切り替えの変更で描き直す）
        toggles.consideration()->setChecked(false);
        QTRY_VERIFY(view.highlighting()->arrows().isEmpty());
        toggles.consideration()->setChecked(true);
        QTRY_COMPARE(view.highlighting()->arrows().size(), 1);
        board.setSfen(SfenUtils::hirateSfen().replace(" b ", " w "));
        QTRY_VERIFY(view.highlighting()->arrows().isEmpty());
        board.setSfen(SfenUtils::hirateSfen());
        QTRY_COMPARE(view.highlighting()->arrows().size(), 1);
        session.stop();
        QTRY_VERIFY(view.highlighting()->arrows().isEmpty());
        QVERIFY(master->isEnabled());
        model.clear();
        QCoreApplication::processEvents();
        QVERIFY(view.highlighting()->arrows().isEmpty());
    }
};
QTEST_MAIN(TestCandidateArrows)
#include "tst_candidate_arrows.moc"
