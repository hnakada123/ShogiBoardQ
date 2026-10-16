/// @file tst_analysisflow.cpp
/// @brief 棋譜解析（KifuAnalysisController・AnalysisJob）と検討の開始（ConsiderationUiController）のテスト
///
/// 解析の開始・中止・完了・エラーの状態遷移（動作状態と実行中か）と、検討タブの設定で検討を始める流れを確かめる。

#include <QTest>
#include <QTemporaryDir>
#include "engineprocessmanager.h"
#include <QSignalSpy>
#include <QSettings>
#include <QStringList>
#include <QVBoxLayout>
#include <QWidget>
#include <memory>

#include "analysisjob.h"
#include "analysissession.h"
#include "appactivitystate.h"
#include "considerationtabmanager.h"
#include "considerationuicontroller.h"
#include "enginelifecyclemanager.h"
#include "enginesettingsconstants.h"
#include "errorbus.h"
#include "kifuanalysiscontroller.h"
#include "kifuanalysisdialog.h"
#include "kifuanalysislistmodel.h"
#include "kifubranchnode.h"
#include "kifubranchtree.h"
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
    EngineLifecycleManager engines{&clock};
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
    UsiCommLogModel logModel;
    ShogiEngineThinkingModel thinkingModel;
    ShogiClock clock;
    EngineLifecycleManager engines{&clock};
    Usi* usi = nullptr;
    QSignalSpy errors{&ErrorBus::instance(), &ErrorBus::messagePosted};

    TestHelper()
    {
        writeEngineSettings(mockEngine(), QStringLiteral("AnalysisTest"));
        sfenRecord = TestReplay::buildSfenRecord(SfenUtils::hirateSfen(), usiMoves, false);
        const QList<ShogiMove> moves = TestReplay::buildGameMoves(SfenUtils::hirateSfen(), usiMoves);
        tree.setRootSfen(sfenRecord.first());
        KifuBranchNode* node = tree.root();
        for (int i = 0; i < moves.size(); ++i) node = tree.addMove(node, moves.at(i), usiMoves.at(i), sfenRecord.at(i + 1));
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

    std::unique_ptr<KifuAnalysisController> controller(bool borrowEngine = true)
    {
        KifuAnalysisController::Deps d;
        d.activity = &activity;
        d.analysisModel = &analysisModel;
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
        AnalysisRequest request;
        request.line = h.line();
        request.enginePath = QCoreApplication::applicationDirPath() + QStringLiteral("/mock_usi_match");
        request.engineName = QStringLiteral("HeadlessAnalysis");
        request.options.endPly = 2;
        AnalysisJob job(request, {h.usi});
        QSignalSpy positions(&job, &AnalysisJob::plyStarted);
        QSignalSpy results(&job, &AnalysisJob::plyAnalyzed);
        QSignalSpy finished(&job, &AnalysisJob::finished);
        request.line = RecordLine{};
        QVERIFY(job.start());
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 5000);
        QCOMPARE(positions.size(), 3);
        QCOMPARE(results.size(), 3);
        QVERIFY(finished.first().first().toBool());   // 範囲の全局面を解析した
        QCOMPARE(results.last().first().value<PlyAnalysis>().ply, 2);
        QCOMPARE(job.line().sfens(), h.sfenRecord);
        QVERIFY(!job.isRunning());
        job.stop();
        emit h.usi->engineInitialized();
        h.engineReplies(QStringLiteral("bestmove 7g7f"));
        QCOMPARE(finished.size(), 1);
        QCOMPARE(results.size(), 3);
    }

    void stoppedJobIgnoresInitializationAndResults()
    {
        TestHelper h;
        AnalysisRequest request;
        request.line = h.line();
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

    void considerationRestartCoalescesAndStopCancelsQueuedWork()
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
        for (const auto& args : commands) sent.append(args.first().toString());
        QVERIFY(!sent.contains(QStringLiteral("position startpos moves 7g7f")));
        QCOMPARE(sent.count(QStringLiteral("position startpos moves 2g2f")), 1);
        QCOMPARE(sent.count(QStringLiteral("go infinite")), 1);
        QVERIFY(sent.contains(QStringLiteral("setoption name MultiPV value 3")));
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
        KifuBranchNode* first = tree.addMove(tree.root(), TestReplay::buildGameMoves(SfenUtils::hirateSfen(),
                                             {QStringLiteral("7g7f")}).first(), QStringLiteral("▲７六歩(77)"), after);
        KifuNavigationState nav(&tree);
        nav.setCurrentNode(first);

        QWidget panel;
        new QVBoxLayout(&panel);
        ShogiEngineThinkingModel considerationModel;
        ConsiderationTabManager tab;
        tab.setConsiderationThinkingModel(&considerationModel);
        tab.buildConsiderationUi(&panel);
        ConsiderationSettings settings = tab.settings();
        settings.unlimitedTime = false;
        settings.byoyomiSec = 15;
        settings.multiPV = 4;
        tab.setSettings(settings);

        ConsiderationUiController::Deps deps;
        deps.considerationTabManager = &tab;
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
        QWidget panel;
        new QVBoxLayout(&panel);
        ConsiderationTabManager tab;
        tab.buildConsiderationUi(&panel);
        writeEngineSettings(mockEngine(), QStringLiteral("RenamedEngine"));   // タブの一覧を作った後に登録が変わった

        ConsiderationUiController::Deps deps;
        deps.considerationTabManager = &tab;
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
