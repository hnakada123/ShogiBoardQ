#include <QtTest>
#include <QCheckBox>
#include <QTemporaryDir>
#include "analysissettings.h"
#include "candidatearrowcontroller.h"
#include "engineanalysistab.h"
#include "sfenutils.h"
#include "shogiboard.h"
#include "shogigamecontroller.h"
#include "shogienginethinkingmodel.h"
#include "thinkinginfopresenter.h"
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
        ShogiGameController game;
        QString initial = SfenUtils::hirateSfen();
        game.newGame(initial);
        ThinkingInfoPresenter presenter;
        presenter.setGameController(&game);
        // 読み筋履歴のパーサにも実局面を渡す。
        QList<QChar> pieces;
        for (const auto piece : game.board()->boardData()) pieces.append(pieceToChar(piece));
        presenter.setClonedBoardData(pieces);
        UsiProtocolHandler protocol;
        protocol.setPresenter(&presenter);
        presenter.setBaseSfen(SfenUtils::hirateSfen());
        protocol.sendGoDepth(20);
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
        protocol.sendGoDepth(30);
        QVERIFY(protocol.searchCandidate().move.isEmpty());
        protocol.onDataReceived("info pv 7g7f");
        protocol.cancelCurrentOperation();
        protocol.onDataReceived("info pv 2g2f");
        QVERIFY(!protocol.searchCandidate().active);
    }
    void ponderHitAndMiss()
    {
        ThinkingInfoPresenter presenter;
        UsiProtocolHandler protocol;
        protocol.setPresenter(&presenter);
        protocol.onDataReceived("bestmove 7g7f ponder 3c3d");
        presenter.setBaseSfen(SfenUtils::hirateSfen());
        protocol.sendGoPonder({});
        protocol.onDataReceived("info pv 2g2f");
        QVERIFY(protocol.searchCandidate().pondering);
        QCOMPARE(protocol.searchCandidate().predictedMove, QStringLiteral("3c3d"));
        protocol.sendPonderHit();
        QVERIFY(protocol.searchCandidate().active);
        QVERIFY(!protocol.searchCandidate().pondering);
        QCOMPARE(protocol.searchCandidate().move, QStringLiteral("2g2f"));
        protocol.onDataReceived("bestmove 2g2f ponder 8c8d");
        protocol.sendGoPonder({});
        protocol.onDataReceived("info pv 2f2e");
        protocol.sendStop();
        protocol.onDataReceived("info pv 9g9f");
        QVERIFY(!protocol.searchCandidate().active);
        protocol.onDataReceived("bestmove resign");
        protocol.sendGoDepth(20);
        QVERIFY(protocol.searchCandidate().move.isEmpty());
    }
    void sharedConversion()
    {
        const auto black = SfenUtils::hirateSfen();
        const auto white = QString(black).replace(" b ", " w ");
        const auto promotion = CandidateArrowController::arrowForMove("8h2b+", black, 1);
        QVERIFY(promotion);
        QCOMPARE(promotion->fromFile, 8);
        QCOMPARE(promotion->toRank, 2);
        const auto drop = CandidateArrowController::arrowForMove("P*5e", white, 2);
        QVERIFY(drop);
        QCOMPARE(drop->fromFile, 0);
        QCOMPARE(drop->dropPiece, QChar('p'));
        QCOMPARE(drop->priority, 2);
        QVERIFY(!CandidateArrowController::arrowForMove("resign", black, 1));
        QVERIFY(!CandidateArrowController::arrowForMove("7g0f", black, 1));
    }
    void considerationAndSettings()
    {
        AnalysisSettings::setMatchArrowsVisible(true);
        AnalysisSettings::setPonderArrowsVisible(true);
        ShogiBoard board;
        board.setSfen(SfenUtils::hirateSfen());
        ShogiView view;
        view.setBoard(&board);
        auto* controller = CandidateArrowController::forView(&view);
        EngineAnalysisTab tab;
        tab.buildUi();
        tab.setArrowController(controller);
        auto* master = tab.findChild<QCheckBox*>("matchArrows");
        auto* ponder = tab.findChild<QCheckBox*>("ponderArrows");
        QVERIFY(master && ponder);
        master->setChecked(false);
        QVERIFY(!ponder->isEnabled());
        QVERIFY(ponder->isChecked());
        QVERIFY(AnalysisSettings::ponderArrowsVisible());
        master->setChecked(true);
        ShogiEngineThinkingModel model;
        auto* record = new ShogiInfoRecord("10", "12", "1000", "30", "▲７六歩");
        record->setUsiPv("7g7f 3c3d");
        record->setBaseSfen(SfenUtils::hirateSfen());
        record->setMultipv(1);
        model.updateByMultipv(record, 3);
        controller->setConsiderationState(true, true, &model);
        QTRY_COMPARE(view.arrows().size(), 1);
        QVERIFY(!master->isEnabled());
        QVERIFY(!ponder->isEnabled());
        auto* second = new ShogiInfoRecord("10", "12", "1000", "20", "▲２六歩");
        second->setUsiPv("2g2f");
        second->setBaseSfen(SfenUtils::hirateSfen());
        second->setMultipv(2);
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
