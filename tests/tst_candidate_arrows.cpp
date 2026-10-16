#include <QtTest>
#include <QCheckBox>
#include <QTemporaryDir>
#include "analysissettings.h"
#include "candidatearrowpresenter.h"
#include "thinkingpanel.h"
#include "sfenutils.h"
#include "shogiboard.h"
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
        connect(&protocol, &UsiProtocolHandler::positionChanged, &presenter, &ThinkingInfoFormatter::setBaseSfen);
        // 読み筋履歴のパーサにも実局面を渡す。
        protocol.startSearch(QStringLiteral("position sfen ") + SfenUtils::hirateSfen(), UsiGo::infinite());
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
        protocol.startSearch(QString(), UsiGo::infinite());
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
        connect(&protocol, &UsiProtocolHandler::positionChanged, &presenter, &ThinkingInfoFormatter::setBaseSfen);
        protocol.onDataReceived("bestmove 7g7f ponder 3c3d");
        protocol.startSearch(QStringLiteral("position sfen ") + SfenUtils::hirateSfen(), UsiGo::ponder({}));
        protocol.onDataReceived("info pv 2g2f");
        QVERIFY(protocol.searchCandidate().pondering);
        QCOMPARE(protocol.searchCandidate().predictedMove, QStringLiteral("3c3d"));
        protocol.sendPonderHit();
        QVERIFY(protocol.searchCandidate().active);
        QVERIFY(!protocol.searchCandidate().pondering);
        QCOMPARE(protocol.searchCandidate().move, QStringLiteral("2g2f"));
        protocol.onDataReceived("bestmove 2g2f ponder 8c8d");
        protocol.startSearch(QString(), UsiGo::ponder({}));
        protocol.onDataReceived("info pv 2f2e");
        protocol.sendStop();
        protocol.onDataReceived("info pv 9g9f");
        QVERIFY(!protocol.searchCandidate().active);
        protocol.onDataReceived("bestmove resign");
        protocol.startSearch(QString(), UsiGo::infinite());
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
    void considerationAndSettings()
    {
        AnalysisSettings::matchArrowsVisible.set(true);
        AnalysisSettings::ponderArrowsVisible.set(true);
        ShogiBoard board;
        board.setSfen(SfenUtils::hirateSfen());
        ShogiView view;
        view.setBoard(&board);
        CandidateArrowPresenter::Deps deps;
        deps.view = &view;
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
        ShogiEngineThinkingModel model;
        ThinkingInfoRow record;
        record.score = UsiScore::cp(30);
        record.pvKanji = QStringLiteral("▲７六歩");
        record.usiPv = QStringLiteral("7g7f 3c3d");
        record.baseSfen = SfenUtils::hirateSfen();
        record.multipv = 1;
        model.updateByMultipv(record, 3);
        controller->setConsiderationState(true, true, &model);
        QTRY_COMPARE(view.arrows().size(), 1);
        QVERIFY(!master->isEnabled());
        QVERIFY(!ponder->isEnabled());
        ThinkingInfoRow second;
        second.score = UsiScore::cp(20);
        second.pvKanji = QStringLiteral("▲２六歩");
        second.usiPv = QStringLiteral("2g2f");
        second.baseSfen = SfenUtils::hirateSfen();
        second.multipv = 2;
        model.updateByMultipv(second, 3);
        QTRY_COMPARE(view.arrows().size(), 2);
        QCOMPARE(view.arrows().at(0).priority, 1);
        QCOMPARE(view.arrows().at(1).priority, 2);
        QVERIFY(view.arrows().at(0).color.alpha() > view.arrows().at(1).color.alpha());
        model.trimToMaxRows(1);
        QTRY_COMPARE(view.arrows().size(), 1);
        controller->setConsiderationState(true, false, &model);
        QTRY_VERIFY(view.arrows().isEmpty());
        controller->setConsiderationState(true, true, &model);
        QTRY_COMPARE(view.arrows().size(), 1);
        board.setSfen(SfenUtils::hirateSfen().replace(" b ", " w "));
        QTRY_VERIFY(view.arrows().isEmpty());
        board.setSfen(SfenUtils::hirateSfen());
        QTRY_COMPARE(view.arrows().size(), 1);
        controller->setConsiderationState(false, true, &model);
        QTRY_VERIFY(view.arrows().isEmpty());
        QVERIFY(master->isEnabled());
        model.clearAllItems();
        QCoreApplication::processEvents();
        QVERIFY(view.arrows().isEmpty());
    }
};
QTEST_MAIN(TestCandidateArrows)
#include "tst_candidate_arrows.moc"
