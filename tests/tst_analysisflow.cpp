/// @file tst_analysisflow.cpp
/// @brief 棋譜解析（KifuAnalysisController・AnalysisJob）と検討の開始（ConsiderationUiController）のテスト
///
/// 解析の開始・中止・完了・エラーの状態遷移（動作状態と実行中か）と、検討タブの設定で検討を始める流れを確かめる。

#include <QTest>
#include <QComboBox>
#include <QLabel>
#include <QRadioButton>
#include <QSpinBox>
#include <QTemporaryDir>
#include "engineprocessmanager.h"
#include <QSignalSpy>
#include <QSettings>
#include <QStringList>
#include <QVBoxLayout>
#include <QWidget>
#include <memory>

#include "analysisjob.h"
#include "analysisresultspanel.h"
#include "analysissession.h"
#include "appactivitystate.h"
#include "considerationpanel.h"
#include "considerationuicontroller.h"
#include "enginelifecyclemanager.h"
#include "enginesettingsconstants.h"
#include "errorbus.h"
#include "evaluationchartwidget.h"
#include "evaluationgraphpresenter.h"
#include "evaluationseries.h"
#include "kifuanalysiscontroller.h"
#include "kifuanalysisdialog.h"
#include "kifuanalysislistmodel.h"
#include "kifubranchnode.h"
#include "kifubranchtree.h"
#include "branchtree_test_util.h"
#include "kifunavigationcontroller.h"
#include "kifunavigationstate.h"
#include "settingscommon.h"
#include "sfenutils.h"
#include "shogiclock.h"
#include "shogienginethinkingmodel.h"
#include "test_replay.h"
#include "usienginesession.h"
#include "usi.h"
#include "usicommlogmodel.h"
#include "usimodelbinding.h"

namespace {
/// 検討・詰み探索のセッションと、それが使うエンジンの枠
struct AnalysisHarness {
    ShogiClock clock;
    AppActivityState activity;
    EngineLifecycleManager engines;
    AnalysisSession session{AnalysisSession::Deps{&engines, &activity}};
};

void writeEngineSettings(const QString& path, const QString& name)
{
    using namespace EngineSettingsConstants;
    QSettings settings(SettingsCommon::settingsFilePath(), QSettings::IniFormat);
    settings.clear();
    settings.beginWriteArray(EnginesGroupName);
    settings.setArrayIndex(0);
    settings.setValue(QString::fromLatin1(EnginePathKey), path);
    settings.setValue(QString::fromLatin1(EngineNameKey), name);
    settings.endArray();
    settings.sync();
}

QString mockEngine()
{
    return QCoreApplication::applicationDirPath() + QStringLiteral("/mock_usi_match");
}
} // namespace

/// 平手から2手の棋譜（表示中の手順）と、棋譜解析の部品
class TestHelper
{
public:
    QStringList sfenRecord;
    QStringList usiMoves{QStringLiteral("7g7f"), QStringLiteral("3c3d")};
    KifuBranchTree tree;
    KifuNavigationState nav{&tree};
    KifuNavigationController navController{&nav};
    AppActivityState activity;
    KifuAnalysisListModel analysisModel;
    AnalysisResultsPanel resultsPanel;
    EvaluationChartWidget evalChart;
    EvaluationGraphPresenter evalGraph{{&evalChart, &nav, nullptr}};
    UsiCommLogModel logModel;
    ShogiEngineThinkingModel thinkingModel;
    ShogiClock clock;
    EngineLifecycleManager engines;
    Usi* usi = nullptr;
    QSignalSpy errors{&ErrorBus::instance(), &ErrorBus::messagePosted};

    TestHelper()
    {
        writeEngineSettings(mockEngine(), QStringLiteral("AnalysisTest"));
        sfenRecord = TestReplay::buildSfenRecord(SfenUtils::hirateSfen(), usiMoves, false);
        const QList<ShogiMove> moves = TestReplay::buildGameMoves(SfenUtils::hirateSfen(), usiMoves);
        tree.setRootSfen(sfenRecord.first());
        KifuBranchNode* node = tree.root();
        for (int i = 0; i < moves.size(); ++i) node = TestTree::addMove(tree, node, moves.at(i), usiMoves.at(i), sfenRecord.at(i + 1));
        usi = new Usi();
        new UsiModelBinding(usi, {&logModel, &thinkingModel});
    }

    ~TestHelper() { delete usi; }

    /// エンジンが1行返したことにする
    void engineReplies(const QString& line) const
    {
        const_cast<UsiProtocolHandler*>(usi->protocol())->onDataReceived(line);
    }

    RecordLine line() const { return KifuBranchTree::toRecordLine(tree.mainLine()); }
    /// index 番の手順（KifuBranchTree::allLines()）の末尾の手の ID
    int lineEnd(int index = 0) const { return tree.allLines().at(index).leaf->nodeId(); }
    /// 本譜を解析する依頼（エンジンと範囲は呼ぶ側が決める）
    AnalysisRequest request() const
    {
        AnalysisRequest r;
        r.line = line();
        r.lineEndNodeId = lineEnd();
        return r;
    }

    std::unique_ptr<KifuAnalysisController> controller(bool borrowEngine = true)
    {
        KifuAnalysisController::Deps d;
        d.activity = &activity;
        d.analysisModel = &analysisModel;
        d.resultsPanel = &resultsPanel;
        d.evalChart = &evalChart;
        d.evalGraph = &evalGraph;
        d.engines = &engines;
        d.navState = &nav;
        d.navController = &navController;
        d.engine = borrowEngine ? usi : nullptr;
        return std::make_unique<KifuAnalysisController>(d);
    }

    /// 条件ダイアログの既定値（全局面）で解析を始める
    bool startAnalysis(KifuAnalysisController& ctrl)
    {
        KifuAnalysisDialog dialog;
        dialog.setMaxPly(static_cast<int>(usiMoves.size()));
        const std::optional<AnalysisRequest> request = ctrl.requestFromDialog(dialog);
        return request && ctrl.start(*request);
    }

    QString lastError() const { return errors.isEmpty() ? QString() : errors.last().at(1).toString(); }

private:
    Q_DISABLE_COPY(TestHelper)
};

// ======================================================================

class TestAnalysisFlow : public QObject
{
    Q_OBJECT
    QTemporaryDir m_config;

private slots:
    void jobRunsWithoutUiAndOwnsSnapshot()
    {
        TestHelper h;
        AnalysisRequest request = h.request();
        request.enginePath = QCoreApplication::applicationDirPath() + QStringLiteral("/mock_usi_match");
        request.engineName = QStringLiteral("HeadlessAnalysis");
        request.options.endPly = 2;
        AnalysisJob job(request, {h.usi});
        QSignalSpy positions(&job, &AnalysisJob::plyStarted);
        QSignalSpy results(&job, &AnalysisJob::plyAnalyzed);
        QSignalSpy finished(&job, &AnalysisJob::finished);
        auto* process = h.usi->findChild<EngineProcessManager*>();
        QVERIFY(process);
        QSignalSpy commands(process, &EngineProcessManager::commandSent);
        request.line = RecordLine{};
        QVERIFY(job.start());
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 5000);
        QCOMPARE(positions.size(), 3);
        QCOMPARE(results.size(), 3);
        QVERIFY(finished.first().first().toBool());   // 範囲の全局面を解析した
        QCOMPARE(results.last().first().value<PlyAnalysis>().ply, 2);
        // 局面は開始局面からの手順で送る（千日手の判断材料。CLI の analyze-kifu と同じ）
        QStringList sent;
        for (const auto& args : std::as_const(commands)) {
            if (args.first().toString().startsWith(QStringLiteral("position "))) sent << args.first().toString();
        }
        QCOMPARE(sent, (QStringList{QStringLiteral("position startpos"), QStringLiteral("position startpos moves 7g7f"),
                                    QStringLiteral("position startpos moves 7g7f 3c3d")}));
        QCOMPARE(job.line().sfens(), h.sfenRecord);
        QVERIFY(!job.isRunning());
        job.stop();
        emit h.usi->engineInitialized();
        h.engineReplies(QStringLiteral("bestmove 7g7f"));
        QCOMPARE(finished.size(), 1);
        QCOMPARE(results.size(), 3);
    }

    /// 手順をたどれず局面も読めない手数（指せない手と壊れた SFEN）はエンジンに送らずに飛ばし、
    /// 前の局面の結果をその手数の結果にしない（U5）。ほかの手数は局面だけで送る
    void jobSkipsUnreadablePositions()
    {
        TestHelper h;
        AnalysisRequest request = h.request();
        request.line.entries[0].move.move = *UsiMove::parse(QStringLiteral("5e5d"));
        request.line.entries[0].sfen = QStringLiteral("broken");
        request.enginePath = QCoreApplication::applicationDirPath() + QStringLiteral("/mock_usi_match");
        request.engineName = QStringLiteral("SkipAnalysis");
        request.options.endPly = 2;
        AnalysisJob job(request, {h.usi});
        QSignalSpy results(&job, &AnalysisJob::plyAnalyzed);
        QSignalSpy finished(&job, &AnalysisJob::finished);
        QVERIFY(job.start());
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 5000);
        QList<int> plies;
        for (const auto& args : std::as_const(results)) plies << args.first().value<PlyAnalysis>().ply;
        QCOMPARE(plies, (QList<int>{0, 2}));
        QVERIFY(finished.first().first().toBool());
    }

    void stoppedJobIgnoresInitializationAndResults()
    {
        TestHelper h;
        AnalysisRequest request = h.request();
        request.enginePath = QCoreApplication::applicationDirPath() + QStringLiteral("/mock_usi_match");
        AnalysisJob job(request, {h.usi});
        QSignalSpy positions(&job, &AnalysisJob::plyStarted);
        QSignalSpy finished(&job, &AnalysisJob::finished);
        QVERIFY(job.start());
        job.stop();
        emit h.usi->engineInitialized();
        h.engineReplies(QStringLiteral("bestmove 7g7f"));
        QCOMPARE(positions.size(), 0);
        QCOMPARE(finished.size(), 1);
        QVERIFY(!finished.first().first().toBool());
        QVERIFY(!job.isRunning());
    }

    /// 詰み探索に答えないエンジンは、制限時間＋余裕で stop し、その後も答えなければ打ち切る（U3）
    void unresponsiveMateSearchIsAbandoned()
    {
        qputenv("SBQ_MATCH_IGNORE_MATE", "1");
        AnalysisHarness h;
        AnalysisOptions options;
        options.mode = AnalysisOptions::Mode::Tsume;
        options.enginePath = mockEngine();
        options.engineName = QStringLiteral("SilentMate");
        options.position = *UsiPosition::parse(QStringLiteral("position sfen 7k1/9/9/9/9/9/9/9/K8 b RS 1"));
        options.byoyomiMs = 100;
        QSignalSpy messages(&h.session, &AnalysisSession::messageRequested);
        QSignalSpy ended(&h.session, &AnalysisSession::tsumeSearchModeEnded);
        QVERIFY(h.session.start(options));
        const int limitMs = 100 + UsiSearchRequest::kMateStopMarginMs + UsiSearchRequest::kStopReplyMs;
        QTRY_VERIFY_WITH_TIMEOUT(messages.count() == 1, limitMs + 4000);
        qunsetenv("SBQ_MATCH_IGNORE_MATE");
        QVERIFY(messages.last().at(1).toString().contains(QStringLiteral("応答")));
        QCOMPARE(ended.count(), 1);
        QCOMPARE(h.activity.activity(), AppActivityState::Activity::Idle);
    }

    /// 検討のやり直しは、探索していなければすぐ始め、探索中なら stop で止めてから最後の依頼だけを始める
    void considerationRestartKeepsLatestAndStopCancelsQueuedWork()
    {
        AnalysisHarness h;
        AnalysisSession& session = h.session;
        AnalysisOptions options;
        options.mode = AnalysisOptions::Mode::Consideration;
        options.enginePath = QCoreApplication::applicationDirPath() + QStringLiteral("/mock_usi_match");
        options.engineName = QStringLiteral("RestartTest");
        options.position = *UsiPosition::parse(QStringLiteral("position startpos"));
        QSignalSpy waiting(&session, &AnalysisSession::considerationWaitingStarted);
        QSignalSpy ended(&session, &AnalysisSession::considerationModeEnded);
        QVERIFY(session.start(options));
        QVERIFY(session.engine());
        auto* process = session.engine()->findChild<EngineProcessManager*>();
        QVERIFY(process);
        QSignalSpy commands(process, &EngineProcessManager::commandSent);
        QTRY_VERIFY_WITH_TIMEOUT(waiting.count() == 1, 5000);
        commands.clear();
        QVERIFY(session.updatePosition(*UsiPosition::parse(QStringLiteral("position startpos moves 7g7f"))));
        QVERIFY(session.updatePosition(*UsiPosition::parse(QStringLiteral("position startpos moves 2g2f"))));
        session.updateMultiPV(3);
        QTRY_VERIFY_WITH_TIMEOUT(waiting.count() == 2, 5000);
        QStringList sent;
        for (const auto& args : std::as_const(commands)) sent.append(args.first().toString());
        // 待っている間の最初の変更はすぐ探索し、次の変更で止める（その結果は捨てる）
        QCOMPARE(sent.count(QStringLiteral("position startpos moves 7g7f")), 1);
        const qsizetype stop = sent.indexOf(QStringLiteral("stop"));
        QVERIFY(stop > sent.indexOf(QStringLiteral("position startpos moves 7g7f")));
        // 止めている間の依頼は最後の1つ（局面 2g2f・候補手 3）だけを、止めた後で送る
        QCOMPARE(sent.count(QStringLiteral("position startpos moves 2g2f")), 1);
        QCOMPARE(sent.count(QStringLiteral("go infinite")), 2);
        QCOMPARE(sent.count(QStringLiteral("setoption name MultiPV value 3")), 1);
        QVERIFY(sent.indexOf(QStringLiteral("setoption name MultiPV value 3")) > stop);
        QVERIFY(sent.indexOf(QStringLiteral("position startpos moves 2g2f")) > stop);
        QVERIFY(session.updatePosition(*UsiPosition::parse(QStringLiteral("position startpos moves 7g7f 3c3d"))));
        session.stop();
        QCoreApplication::processEvents();
        QCOMPARE(ended.count(), 1);
        QCOMPARE(h.activity.activity(), AppActivityState::Activity::Idle);
        QVERIFY(!session.engine());
        session.stop();
        QCOMPARE(ended.count(), 1);
    }

    void initTestCase()
    {
        QVERIFY(m_config.isValid());
        qputenv("XDG_CONFIG_HOME", m_config.path().toUtf8());
        qputenv("SHOGIBOARDQ_CONFIG_HOME", m_config.path().toUtf8());
    }

    // === 棋譜解析の状態 ===

    void initialState()
    {
        TestHelper h;
        auto ctrl = h.controller();
        QVERIFY(!ctrl->isRunning());
        ctrl->stop();   // 解析していなければ何もしない
        QCOMPARE(h.activity.activity(), AppActivityState::Activity::Idle);
    }

    /// 解析中は動作状態を「棋譜解析中」にし、中止で待機に戻す
    void startAndStopFollowActivity()
    {
        TestHelper h;
        auto ctrl = h.controller();
        QVERIFY(h.startAnalysis(*ctrl));
        QVERIFY(ctrl->isRunning());
        QCOMPARE(h.activity.activity(), AppActivityState::Activity::DuringAnalysis);
        ctrl->stop();
        QVERIFY(!ctrl->isRunning());
        QCOMPARE(h.activity.activity(), AppActivityState::Activity::Idle);
        ctrl->stop();
        QCOMPARE(h.activity.activity(), AppActivityState::Activity::Idle);
    }

    /// 解析中に始め直すと、前の解析を止めて新しい解析を始める
    void startWhileRunningRestarts()
    {
        TestHelper h;
        auto ctrl = h.controller();
        QVERIFY(h.startAnalysis(*ctrl));
        QVERIFY(h.startAnalysis(*ctrl));
        QVERIFY(ctrl->isRunning());
        QCOMPARE(h.activity.activity(), AppActivityState::Activity::DuringAnalysis);
        ctrl->stop();
        QVERIFY(!ctrl->isRunning());
    }

    /// 最後まで解析すると結果の表が埋まり、解析している手へ棋譜が移り、待機に戻る
    void analysisRunsToCompletion()
    {
        TestHelper h;
        auto ctrl = h.controller();
        QVERIFY(h.startAnalysis(*ctrl));
        QTRY_VERIFY_WITH_TIMEOUT(!ctrl->isRunning(), 7000);
        QCOMPARE(h.analysisModel.rowCount(), 3);
        QCOMPARE(h.nav.currentPly(), 2);
        QCOMPARE(h.activity.activity(), AppActivityState::Activity::Idle);
        QVERIFY(h.lastError().isEmpty());
    }

    void resultsFollowRecordAndGraphSelection_data()
    {
        QTest::addColumn<int>("startPly");
        QTest::newRow("full-range") << 0;
        QTest::newRow("partial-range") << 1;
    }

    /// 棋譜欄・グラフからの移動で解析結果の選択も動き、選択の通知が循環しない。
    void resultsFollowRecordAndGraphSelection()
    {
        QFETCH(int, startPly);
        TestHelper h;
        auto ctrl = h.controller();
        AnalysisRequest request = h.request();
        request.enginePath = mockEngine();
        request.options.startPly = startPly;
        QVERIFY(ctrl->start(request));
        QTRY_VERIFY_WITH_TIMEOUT(!ctrl->isRunning(), 7000);
        auto* view = h.resultsPanel.view();
        QCOMPARE(view->currentIndex().row(), 2 - startPly);
        QSignalSpy selected(&h.resultsPanel, &AnalysisResultsPanel::rowSelected);
        QSignalSpy changed(&h.nav, &KifuNavigationState::changed);

        h.navController.selectRecordRow(startPly);
        QCOMPARE(view->currentIndex().row(), 0);
        QCOMPARE(view->selectionModel()->selectedRows().size(), 1);
        QCOMPARE(view->selectionModel()->selectedRows().first().row(), 0);
        QCOMPARE(h.evalChart.currentPly(), startPly);
        QCOMPARE(selected.count(), 0);
        QCOMPARE(changed.count(), 1);

        emit h.evalChart.analysisPlyClicked(h.lineEnd(), 2);
        QCOMPARE(view->currentIndex().row(), 2 - startPly);
        QCOMPARE(view->selectionModel()->selectedRows().first().row(), 2 - startPly);
        QCOMPARE(h.nav.currentPly(), 2);
        QCOMPARE(h.evalChart.currentPly(), 2);
        QCOMPARE(selected.count(), 0);
        QCOMPARE(changed.count(), 2);

        // 解析結果から棋譜・グラフへの移動も維持する。
        view->setCurrentIndex(h.analysisModel.index(0, 0));
        QCOMPARE(h.nav.currentPly(), startPly);
        QCOMPARE(h.evalChart.currentPly(), startPly);
        QCOMPARE(selected.count(), 1);
        QCOMPARE(changed.count(), 3);
    }

    void resultsClearSelectionOutsideAnalyzedRange()
    {
        TestHelper h;
        auto ctrl = h.controller();
        AnalysisRequest request = h.request();
        request.enginePath = mockEngine();
        request.options.startPly = 1;
        request.options.endPly = 1;
        QVERIFY(ctrl->start(request));
        QTRY_VERIFY_WITH_TIMEOUT(!ctrl->isRunning(), 7000);
        auto* view = h.resultsPanel.view();
        QSignalSpy selected(&h.resultsPanel, &AnalysisResultsPanel::rowSelected);
        for (const int ply : {0, 2}) {
            h.navController.selectRecordRow(ply);
            QVERIFY(!view->currentIndex().isValid());
            QVERIFY(view->selectionModel()->selectedRows().isEmpty());
            h.navController.selectRecordRow(1);
            QCOMPARE(view->currentIndex().row(), 0);
            QCOMPARE(view->selectionModel()->selectedRows().size(), 1);
        }
        QCOMPARE(selected.count(), 0);
    }

    void resultsSelectionFollowsAnalyzedBranch()
    {
        TestHelper h;
        auto* first = TestTree::findByPlyOnMainLine(h.tree, 1);
        const QStringList branchMoves{QStringLiteral("8c8d")};
        const auto sfens = TestReplay::buildSfenRecord(first->sfen(), branchMoves, false);
        const auto moves = TestReplay::buildGameMoves(first->sfen(), branchMoves);
        TestTree::addMove(h.tree, first, moves.first(), branchMoves.first(), sfens.last());
        auto ctrl = h.controller();
        AnalysisRequest request;
        request.line = KifuBranchTree::toRecordLine(h.tree.allLines().at(1).nodes());
        request.lineEndNodeId = h.lineEnd(1);
        request.options.startPly = 1;
        request.enginePath = mockEngine();
        QVERIFY(ctrl->start(request));
        QTRY_VERIFY_WITH_TIMEOUT(!ctrl->isRunning(), 7000);
        auto* view = h.resultsPanel.view();
        QCOMPARE(view->currentIndex().row(), 1);
        QSignalSpy selected(&h.resultsPanel, &AnalysisResultsPanel::rowSelected);

        const int mainEnd = h.lineEnd(0);
        const int branchEnd = h.lineEnd(1);
        h.navController.goToPlyOnLine(mainEnd, 2);
        QVERIFY(!view->currentIndex().isValid());
        QVERIFY(view->selectionModel()->selectedRows().isEmpty());
        h.navController.goToPlyOnLine(branchEnd, 1);
        QCOMPARE(view->currentIndex().row(), 0);
        QCOMPARE(h.nav.currentLineIndex(), 1);
        h.navController.goToPlyOnLine(mainEnd, 2);
        emit h.evalChart.analysisPlyClicked(branchEnd, 2);
        QCOMPARE(view->currentIndex().row(), 1);
        QCOMPARE(h.nav.currentLineIndex(), 1);
        QCOMPARE(h.evalChart.currentPly(), 2);
        QCOMPARE(selected.count(), 0);
    }

    /// 解析の後に分岐を本譜にしても、結果の行・グラフの点は解析した手順へ移る（手順の番号は変わる）
    void resultsKeepAnalyzedLineAfterBranchEditing()
    {
        TestHelper h;
        auto* first = TestTree::findByPlyOnMainLine(h.tree, 1);
        const QStringList branchMoves{QStringLiteral("8c8d")};
        const auto sfens = TestReplay::buildSfenRecord(first->sfen(), branchMoves, false);
        const auto moves = TestReplay::buildGameMoves(first->sfen(), branchMoves);
        KifuBranchNode* branch = TestTree::addMove(h.tree, first, moves.first(), branchMoves.first(), sfens.last());
        h.navController.goToPlyOnLine(branch->nodeId(), 2);
        auto ctrl = h.controller();
        QVERIFY(h.startAnalysis(*ctrl));
        QTRY_VERIFY_WITH_TIMEOUT(!ctrl->isRunning(), 7000);
        QCOMPARE(h.evalGraph.series()->analysisLineEnd(), branch->nodeId());
        QCOMPARE(h.evalGraph.series()->analysisLineName(), QStringLiteral("分岐 1"));

        // 分岐を本譜にすると、解析した手順は0番になる。グラフの見出しも付け直す
        QVERIFY(h.tree.moveChild(branch, 0));
        QCOMPARE(h.evalGraph.series()->analysisLineName(), QStringLiteral("本譜"));
        h.navController.goToPlyOnLine(h.lineEnd(1), 2);   // 元の本譜（今は1番）へ
        h.resultsPanel.view()->setCurrentIndex(h.analysisModel.index(2, 0));
        QCOMPARE(h.nav.currentNode(), branch);
        h.navController.goToPlyOnLine(h.lineEnd(1), 2);
        emit h.evalChart.analysisPlyClicked(h.evalGraph.series()->analysisLineEnd(), 1);
        QCOMPARE(h.nav.currentNode(), first);
        QCOMPARE(h.nav.currentLineIndex(), 0);

        // 解析した手順を消すと移らず、見出しも外す
        QVERIFY(h.tree.removeSubtree(branch));
        QVERIFY(h.evalGraph.series()->analysisLineName().isEmpty());
        const KifuBranchNode* before = h.nav.currentNode();
        emit h.evalChart.analysisPlyClicked(h.evalGraph.series()->analysisLineEnd(), 2);
        QCOMPARE(h.nav.currentNode(), before);
    }

    // === エラー ===

    void startWithEmptyLineReportsError()
    {
        TestHelper h;
        auto ctrl = h.controller();
        QVERIFY(!ctrl->start(AnalysisRequest{}));
        QVERIFY(!ctrl->isRunning());
        QVERIFY(h.lastError().contains(QStringLiteral("棋譜データ")));
        QCOMPARE(h.activity.activity(), AppActivityState::Activity::Idle);
    }

    /// エンジンを渡さなければ、解析ごとにエンジンを作って使う
    void startWithoutEngineOwnsOne()
    {
        TestHelper h;
        auto ctrl = h.controller(/*borrowEngine=*/false);
        QVERIFY(h.startAnalysis(*ctrl));
        QVERIFY(ctrl->isRunning());
        QVERIFY(h.lastError().isEmpty());
        ctrl->stop();
        QVERIFY(!ctrl->isRunning());
    }

    /// 結果が出る前に止めても「解析中止」の状態文を残し、結果を消したときだけ初めの案内に戻す
    void stopBeforeFirstResultKeepsStoppedStatus()
    {
        TestHelper h;
        auto ctrl = h.controller();
        QVERIFY(h.startAnalysis(*ctrl));
        auto* status = h.resultsPanel.findChild<QLabel*>(QStringLiteral("analysisStatusLabel"));
        QVERIFY(status);
        QVERIFY(status->text().contains(QStringLiteral("解析中")));
        ctrl->stop();
        QTRY_VERIFY(!ctrl->isRunning());
        QVERIFY2(status->text().contains(QStringLiteral("解析中止")), qPrintable(status->text()));
        h.analysisModel.clear();
        QVERIFY2(!status->text().contains(QStringLiteral("解析中止")), qPrintable(status->text()));
    }

    void startWithEngineInitFailure()
    {
        TestHelper h;
        auto ctrl = h.controller();
        writeEngineSettings(QStringLiteral("/missing/engine"), QStringLiteral("Missing"));
        h.startAnalysis(*ctrl);
        QTRY_VERIFY_WITH_TIMEOUT(!ctrl->isRunning(), 5000);
        QVERIFY(!h.lastError().isEmpty());
        QCOMPARE(h.activity.activity(), AppActivityState::Activity::Idle);
    }

    void engineErrorStopsAnalysis()
    {
        TestHelper h;
        auto ctrl = h.controller();
        QVERIFY(h.startAnalysis(*ctrl));
        Q_EMIT h.usi->errorOccurred(QStringLiteral("Engine crashed"));
        QVERIFY(!ctrl->isRunning());
        QVERIFY(h.lastError().contains(QStringLiteral("Engine crashed")));
        QCOMPARE(h.activity.activity(), AppActivityState::Activity::Idle);
    }

    // === 検討の開始（検討タブの設定を1回読んで、表示中の局面で始める） ===

    void considerationStartsWithTabSettings()
    {
        writeEngineSettings(mockEngine(), QStringLiteral("StoredEngine"));
        AnalysisHarness h;
        KifuBranchTree tree;
        tree.setRootSfen(SfenUtils::hirateSfen());
        const QString after = TestReplay::buildSfenRecord(SfenUtils::hirateSfen(), {QStringLiteral("7g7f")}, false).at(1);
        KifuBranchNode* first = TestTree::addMove(tree, tree.root(), TestReplay::buildGameMoves(SfenUtils::hirateSfen(),
                                             {QStringLiteral("7g7f")}).first(), QStringLiteral("▲７六歩(77)"), after);
        KifuNavigationState nav(&tree);
        nav.setCurrentNode(first);

        ShogiEngineThinkingModel considerationModel;
        ConsiderationPanel tab;
        tab.setConsiderationThinkingModel(&considerationModel);
        // 利用者と同じく画面の欄で条件を変える
        tab.findChild<QRadioButton*>(QStringLiteral("considerationTimed"))->setChecked(true);
        tab.findChild<QSpinBox*>(QStringLiteral("considerationSeconds"))->setValue(15);
        tab.findChild<QComboBox*>(QStringLiteral("considerationMultiPV"))->setCurrentIndex(3);
        const ConsiderationSettings settings = tab.settings();
        QVERIFY(!settings.unlimitedTime);
        QCOMPARE(settings.byoyomiSec, 15);
        QCOMPARE(settings.multiPV, 4);

        ConsiderationUiController::Deps deps;
        deps.considerationPanel = &tab;
        deps.session = &h.session;
        deps.navState = &nav;
        ConsiderationUiController controller(deps);
        controller.startConsideration();

        QCOMPARE(h.activity.activity(), AppActivityState::Activity::DuringConsideration);
        QVERIFY(h.session.isConsidering());
        QVERIFY(h.session.engine());
        // 設定を書き戻さない（タブの値はそのまま）
        QCOMPARE(tab.settings(), settings);
        auto* process = h.session.engine()->findChild<EngineProcessManager*>();
        QVERIFY(process);
        QSignalSpy commands(process, &EngineProcessManager::commandSent);
        QTRY_VERIFY_WITH_TIMEOUT(!h.session.engine()->isInitializing(), 5000);
        QStringList sent;
        for (const auto& args : commands) sent.append(args.first().toString());
        QVERIFY2(sent.contains(QStringLiteral("position startpos moves 7g7f")), qPrintable(sent.join('\n')));
        QVERIFY(sent.contains(QStringLiteral("setoption name MultiPV value 4")));
        h.session.stop();
        QCOMPARE(h.activity.activity(), AppActivityState::Activity::Idle);
    }

    /// 登録の無いエンジンが選ばれていれば、検討を始めずに知らせる
    void considerationWithUnknownEngineReportsError()
    {
        writeEngineSettings(mockEngine(), QStringLiteral("StoredEngine"));
        AnalysisHarness h;
        KifuBranchTree tree;
        tree.setRootSfen(SfenUtils::hirateSfen());
        KifuNavigationState nav(&tree);
        ConsiderationPanel tab;
        writeEngineSettings(mockEngine(), QStringLiteral("RenamedEngine"));   // タブの一覧を作った後に登録が変わった

        ConsiderationUiController::Deps deps;
        deps.considerationPanel = &tab;
        deps.session = &h.session;
        deps.navState = &nav;
        ConsiderationUiController controller(deps);
        QSignalSpy errors(&ErrorBus::instance(), &ErrorBus::messagePosted);
        controller.startConsideration();
        QCOMPARE(errors.count(), 1);
        QVERIFY(errors.last().at(1).toString().contains(QStringLiteral("検討エンジン")));
        QVERIFY(!h.session.isConsidering());
        QCOMPARE(h.activity.activity(), AppActivityState::Activity::Idle);
    }
};

QTEST_MAIN(TestAnalysisFlow)
#include "tst_analysisflow.moc"
