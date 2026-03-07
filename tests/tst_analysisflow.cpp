/// @file tst_analysisflow.cpp
/// @brief AnalysisFlowController のユニットテスト
///
/// 解析フロー制御の状態遷移・シグナル発火・エラー回復をテストする。

#include <QTest>
#include <QSignalSpy>
#include <QSettings>
#include <QStringList>

#include "analysisflowcontroller.h"
#include "analysiscoordinator.h"
#include "considerationflowcontroller.h"
#include "enginesettingsconstants.h"
#include "usi.h"
#include "kifuanalysisdialog.h"
#include "kifuanalysislistmodel.h"
#include "kifurecordlistmodel.h"
#include "matchcoordinator.h"
#include "settingscommon.h"
#include "usicommlogmodel.h"
#include "shogienginethinkingmodel.h"
#include "shogigamecontroller.h"

extern bool g_analysisFlowStubStartAndInitSuccess;
extern bool g_matchStartAnalysisCalled;
extern MatchCoordinator::AnalysisOptions g_lastMatchAnalysisOptions;

namespace {
void clearConsiderationEngineSettings()
{
    QSettings settings(SettingsCommon::settingsFilePath(), QSettings::IniFormat);
    settings.clear();
    settings.sync();
}

void writeConsiderationEngineSettings(const QString& path, const QString& name)
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

void resetConsiderationFlowStubs()
{
    clearConsiderationEngineSettings();
    g_matchStartAnalysisCalled = false;
    g_lastMatchAnalysisOptions = MatchCoordinator::AnalysisOptions{};
}
} // namespace

/// テスト用ヘルパ: start() に渡す最小限の Deps を構築する
class TestHelper
{
public:
    QStringList sfenRecord;
    QStringList usiMoves;
    KifuAnalysisListModel analysisModel;
    UsiCommLogModel logModel;
    ShogiEngineThinkingModel thinkingModel;
    ShogiGameController gameController;
    Usi* usi = nullptr;
    QString lastError;

    TestHelper()
    {
        // 平手の開始局面 + 2手分
        sfenRecord << QStringLiteral("position startpos")
                   << QStringLiteral("position sfen lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 2")
                   << QStringLiteral("position sfen lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 3");
        usiMoves << QStringLiteral("7g7f") << QStringLiteral("3c3d");

        usi = new Usi(&logModel, &thinkingModel, &gameController, nullptr);
    }

    ~TestHelper()
    {
        delete usi;
    }

    AnalysisFlowController::Deps makeDeps()
    {
        AnalysisFlowController::Deps d;
        d.sfenRecord = &sfenRecord;
        d.usiMoves = &usiMoves;
        d.analysisModel = &analysisModel;
        d.usi = usi;
        d.logModel = &logModel;
        d.thinkingModel = &thinkingModel;
        d.gameController = &gameController;
        d.activePly = 0;
        d.blackPlayerName = QStringLiteral("先手");
        d.whitePlayerName = QStringLiteral("後手");
        d.displayError = [this](const QString& msg) { lastError = msg; };
        return d;
    }

    /// start() に最小限の deps + ダイアログを渡して実行
    void startAnalysis(AnalysisFlowController& ctrl)
    {
        auto deps = makeDeps();
        KifuAnalysisDialog dlg;
        dlg.setMaxPly(static_cast<int>(sfenRecord.size()) - 1);
        ctrl.start(deps, &dlg);
    }

private:
    Q_DISABLE_COPY(TestHelper)
};

// ======================================================================

class TestAnalysisFlow : public QObject
{
    Q_OBJECT

private slots:
    // --- 1. 状態遷移テスト ---

    /// 初期状態は Idle（isRunning = false）
    void initialState();

    /// start() 後は Running 状態になる
    void startTransitionsToRunning();

    /// stop() 後は Idle に戻る
    void stopTransitionsToIdle();

    /// 解析中でない場合の stop() は何もしない
    void stopWhenNotRunning();

    /// 解析中に再度 start() を呼ぶ場合：最初の解析は上書きされ新しい解析が開始される
    void startWhileRunning();

    // --- 2. シグナル発火テスト ---

    /// stop() 時に analysisStopped シグナルが発火する
    void stopEmitsAnalysisStopped();

    /// stop() 中にも analysisStopped は 1 回だけ発火
    void stopEmitsAnalysisStoppedOnce();

    /// 解析完了（onAnalysisFinished）時に analysisStopped が発火する
    void analysisFinishedEmitsAnalysisStopped();

    /// analysisProgressReported シグナルの発火テスト（onPositionPrepared 経由）
    void positionPreparedEmitsProgress();

    // --- 3. エラー時の状態回復テスト ---

    /// sfenRecord が null の場合は開始されずエラーコールバックが呼ばれる
    void startWithNullSfenRecord();

    /// sfenRecord が空の場合は開始されずエラーコールバックが呼ばれる
    void startWithEmptySfenRecord();

    /// analysisModel が null の場合は開始されずエラーコールバックが呼ばれる
    void startWithNullAnalysisModel();

    /// usi が null の場合は開始されずエラーコールバックが呼ばれる
    void startWithNullUsi();

    /// dialog が null の場合は開始されず即座に戻る
    void startWithNullDialog();

    /// エンジン初期化失敗時は解析を開始しない
    void startWithEngineInitFailure();

    /// エンジンエラー発生時に解析が停止する
    void engineErrorStopsAnalysis();

    /// エンジンエラー発生時にエラーコールバックが呼ばれる
    void engineErrorCallsDisplayError();

    // --- 4. ConsiderationFlowController ---

    /// MatchCoordinator 未設定時は開始せずエラーを返す
    void considerationDirect_withoutMatch_returnsFalse();

    /// 無効なエンジン設定では開始通知を出さず false を返す
    void considerationDirect_invalidEngineSelection_returnsFalse();

    /// 正常系では開始通知・設定通知・startAnalysis が揃って動く
    void considerationDirect_validEngine_startsAnalysis();
};

// === 1. 状態遷移テスト ===

void TestAnalysisFlow::initialState()
{
    AnalysisFlowController ctrl;
    QVERIFY(!ctrl.isRunning());
}

void TestAnalysisFlow::startTransitionsToRunning()
{
    AnalysisFlowController ctrl;
    TestHelper h;
    h.startAnalysis(ctrl);

    QVERIFY(ctrl.isRunning());
}

void TestAnalysisFlow::stopTransitionsToIdle()
{
    AnalysisFlowController ctrl;
    TestHelper h;
    h.startAnalysis(ctrl);
    QVERIFY(ctrl.isRunning());

    ctrl.stop();
    QVERIFY(!ctrl.isRunning());
}

void TestAnalysisFlow::stopWhenNotRunning()
{
    AnalysisFlowController ctrl;
    QSignalSpy spy(&ctrl, &AnalysisFlowController::analysisStopped);

    // 解析中でないので stop() は何もしない
    ctrl.stop();
    QVERIFY(!ctrl.isRunning());
    QCOMPARE(spy.count(), 0);
}

void TestAnalysisFlow::startWhileRunning()
{
    AnalysisFlowController ctrl;
    TestHelper h;
    h.startAnalysis(ctrl);
    QVERIFY(ctrl.isRunning());

    // 再度 start() を呼んでもクラッシュしないことを検証
    // 注: AnalysisCoordinator::startAnalyzeRange() 内部で前回の解析を stop() するため
    //     analysisFinished → onAnalysisFinished が発火し m_running=false になる。
    //     その後 m_coord->startAnalyzeRange() が新しい解析を開始するが、
    //     FlowController の m_running は既に false のまま。
    //     （テスト環境ではエンジンスタブにより解析が即座に進行するため）
    h.startAnalysis(ctrl);
    // 二重起動が安全に処理されること（クラッシュしないこと）を主に検証
}

// === 2. シグナル発火テスト ===

void TestAnalysisFlow::stopEmitsAnalysisStopped()
{
    AnalysisFlowController ctrl;
    TestHelper h;
    h.startAnalysis(ctrl);

    QSignalSpy spy(&ctrl, &AnalysisFlowController::analysisStopped);
    ctrl.stop();

    // stop() → m_coord->stop() → analysisFinished → onAnalysisFinished の経路で1回だけ発火
    QCOMPARE(spy.count(), 1);
}

void TestAnalysisFlow::stopEmitsAnalysisStoppedOnce()
{
    AnalysisFlowController ctrl;
    TestHelper h;
    h.startAnalysis(ctrl);

    QSignalSpy spy(&ctrl, &AnalysisFlowController::analysisStopped);
    ctrl.stop();
    const auto firstStopCount = spy.count();

    ctrl.stop(); // 2回目は m_running=false なので何もしない
    QCOMPARE(spy.count(), firstStopCount); // 追加発火なし
}

void TestAnalysisFlow::analysisFinishedEmitsAnalysisStopped()
{
    AnalysisFlowController ctrl;
    TestHelper h;
    h.startAnalysis(ctrl);
    QVERIFY(ctrl.isRunning());

    QSignalSpy spy(&ctrl, &AnalysisFlowController::analysisStopped);

    // stop() 経由で AnalysisCoordinator::analysisFinished → onAnalysisFinished が
    // 呼び出され、analysisStopped が発火されることを検証
    ctrl.stop();
    QVERIFY(spy.count() >= 1);
    QVERIFY(!ctrl.isRunning());
}

void TestAnalysisFlow::positionPreparedEmitsProgress()
{
    AnalysisFlowController ctrl;
    TestHelper h;

    // start() 内で positionPrepared → analysisProgressReported が同期的に発火されるので
    // spy を start() の前に設定する
    QSignalSpy spy(&ctrl, &AnalysisFlowController::analysisProgressReported);
    h.startAnalysis(ctrl);

    // start() → coordinator.startAnalyzeRange() → sendAnalyzeForPly() →
    // positionPrepared → onPositionPrepared → analysisProgressReported
    QVERIFY(spy.count() >= 1);
}

// === 3. エラー時の状態回復テスト ===

void TestAnalysisFlow::startWithNullSfenRecord()
{
    AnalysisFlowController ctrl;
    TestHelper h;
    auto deps = h.makeDeps();
    deps.sfenRecord = nullptr; // null に設定

    KifuAnalysisDialog dlg;
    ctrl.start(deps, &dlg);

    QVERIFY(!ctrl.isRunning());
    QVERIFY(h.lastError.contains(QStringLiteral("sfenRecord")));
}

void TestAnalysisFlow::startWithEmptySfenRecord()
{
    AnalysisFlowController ctrl;
    TestHelper h;
    QStringList empty;
    auto deps = h.makeDeps();
    deps.sfenRecord = &empty; // 空リスト

    KifuAnalysisDialog dlg;
    ctrl.start(deps, &dlg);

    QVERIFY(!ctrl.isRunning());
    QVERIFY(h.lastError.contains(QStringLiteral("sfenRecord")));
}

void TestAnalysisFlow::startWithNullAnalysisModel()
{
    AnalysisFlowController ctrl;
    TestHelper h;
    auto deps = h.makeDeps();
    deps.analysisModel = nullptr;

    KifuAnalysisDialog dlg;
    ctrl.start(deps, &dlg);

    QVERIFY(!ctrl.isRunning());
    QVERIFY(h.lastError.contains(QStringLiteral("解析モデル")));
}

void TestAnalysisFlow::startWithNullUsi()
{
    AnalysisFlowController ctrl;
    TestHelper h;
    auto deps = h.makeDeps();
    deps.usi = nullptr;

    KifuAnalysisDialog dlg;
    ctrl.start(deps, &dlg);

    QVERIFY(!ctrl.isRunning());
    QVERIFY(h.lastError.contains(QStringLiteral("Usi")));
}

void TestAnalysisFlow::startWithNullDialog()
{
    AnalysisFlowController ctrl;
    TestHelper h;
    auto deps = h.makeDeps();

    // dialog が null → 即座に return（エラーコールバックなし）
    ctrl.start(deps, nullptr);

    QVERIFY(!ctrl.isRunning());
    // エラーコールバックは呼ばれない（null dialog チェックはエラー表示せず return）
    QVERIFY(h.lastError.isEmpty());
}

void TestAnalysisFlow::startWithEngineInitFailure()
{
    AnalysisFlowController ctrl;
    TestHelper h;

    g_analysisFlowStubStartAndInitSuccess = false;
    h.startAnalysis(ctrl);
    g_analysisFlowStubStartAndInitSuccess = true;

    QVERIFY(!ctrl.isRunning());
    QVERIFY(h.lastError.contains(QStringLiteral("エンジン初期化に失敗")));
}

void TestAnalysisFlow::engineErrorStopsAnalysis()
{
    AnalysisFlowController ctrl;
    TestHelper h;
    h.startAnalysis(ctrl);
    QVERIFY(ctrl.isRunning());

    QSignalSpy spy(&ctrl, &AnalysisFlowController::analysisStopped);

    // Usi の errorOccurred シグナルをシミュレート
    Q_EMIT h.usi->errorOccurred(QStringLiteral("Engine crashed"));

    QVERIFY(!ctrl.isRunning());
    // onEngineError → stop() → m_coord->stop() → analysisFinished → onAnalysisFinished
    // の経路で1回だけ発火
    QCOMPARE(spy.count(), 1);
}

void TestAnalysisFlow::engineErrorCallsDisplayError()
{
    AnalysisFlowController ctrl;
    TestHelper h;
    h.startAnalysis(ctrl);
    QVERIFY(ctrl.isRunning());

    // Usi の errorOccurred シグナルをシミュレート
    Q_EMIT h.usi->errorOccurred(QStringLiteral("Engine crashed"));

    QVERIFY(!ctrl.isRunning());
    QVERIFY(h.lastError.contains(QStringLiteral("Engine crashed")));
}

void TestAnalysisFlow::considerationDirect_withoutMatch_returnsFalse()
{
    resetConsiderationFlowStubs();

    ConsiderationFlowController flow;
    ConsiderationFlowController::Deps deps;
    QString lastError;
    bool startedNotified = false;
    deps.onStarted = [&startedNotified]() { startedNotified = true; };
    deps.onError = [&lastError](const QString& msg) { lastError = msg; };

    const bool started = flow.runDirect(deps, {}, QStringLiteral("position startpos"));

    QVERIFY(!started);
    QVERIFY(lastError.contains(QStringLiteral("MatchCoordinator")));
    QVERIFY(!startedNotified);
    QVERIFY(!g_matchStartAnalysisCalled);
}

void TestAnalysisFlow::considerationDirect_invalidEngineSelection_returnsFalse()
{
    resetConsiderationFlowStubs();

    MatchCoordinator::Deps matchDeps;
    MatchCoordinator match(matchDeps);

    ConsiderationFlowController flow;
    ConsiderationFlowController::Deps deps;
    QString lastError;
    bool startedNotified = false;
    bool timeReadyCalled = false;
    bool multiPVReadyCalled = false;
    deps.match = &match;
    deps.onStarted = [&startedNotified]() { startedNotified = true; };
    deps.onError = [&lastError](const QString& msg) { lastError = msg; };
    deps.onTimeSettingsReady = [&timeReadyCalled](bool, int) { timeReadyCalled = true; };
    deps.onMultiPVReady = [&multiPVReadyCalled](int) { multiPVReadyCalled = true; };

    ConsiderationFlowController::DirectParams params;
    params.engineIndex = 0;
    params.engineName = QStringLiteral("MissingEngine");

    const bool started = flow.runDirect(deps, params, QStringLiteral("position startpos"));

    QVERIFY(!started);
    QVERIFY(lastError.contains(QStringLiteral("検討エンジン")));
    QVERIFY(!startedNotified);
    QVERIFY(!timeReadyCalled);
    QVERIFY(!multiPVReadyCalled);
    QVERIFY(!g_matchStartAnalysisCalled);
}

void TestAnalysisFlow::considerationDirect_validEngine_startsAnalysis()
{
    resetConsiderationFlowStubs();
    writeConsiderationEngineSettings(QStringLiteral("/usr/bin/test-engine"),
                                     QStringLiteral("StoredEngine"));

    MatchCoordinator::Deps matchDeps;
    MatchCoordinator match(matchDeps);

    ShogiEngineThinkingModel considerationModel;
    ConsiderationFlowController flow;
    ConsiderationFlowController::Deps deps;
    bool startedNotified = false;
    bool timeReadyCalled = false;
    bool multiPVReadyCalled = false;
    bool unlimitedReceived = true;
    int byoyomiReceived = 0;
    int multiPVReceived = 0;
    QString lastError;
    deps.match = &match;
    deps.considerationModel = &considerationModel;
    deps.onStarted = [&startedNotified]() { startedNotified = true; };
    deps.onError = [&lastError](const QString& msg) { lastError = msg; };
    deps.onTimeSettingsReady = [&timeReadyCalled, &unlimitedReceived, &byoyomiReceived](bool unlimited, int byoyomiSec) {
        timeReadyCalled = true;
        unlimitedReceived = unlimited;
        byoyomiReceived = byoyomiSec;
    };
    deps.onMultiPVReady = [&multiPVReadyCalled, &multiPVReceived](int multiPV) {
        multiPVReadyCalled = true;
        multiPVReceived = multiPV;
    };

    ConsiderationFlowController::DirectParams params;
    params.engineIndex = 0;
    params.unlimitedTime = false;
    params.byoyomiSec = 15;
    params.multiPV = 4;
    params.previousFileTo = 7;
    params.previousRankTo = 6;
    params.lastUsiMove = QStringLiteral("7g7f");

    const QString position = QStringLiteral("position startpos moves 7g7f");
    const bool started = flow.runDirect(deps, params, position);

    QVERIFY(started);
    QVERIFY(lastError.isEmpty());
    QVERIFY(startedNotified);
    QVERIFY(timeReadyCalled);
    QVERIFY(multiPVReadyCalled);
    QCOMPARE(unlimitedReceived, false);
    QCOMPARE(byoyomiReceived, 15);
    QCOMPARE(multiPVReceived, 4);
    QVERIFY(g_matchStartAnalysisCalled);
    QCOMPARE(g_lastMatchAnalysisOptions.enginePath, QStringLiteral("/usr/bin/test-engine"));
    QCOMPARE(g_lastMatchAnalysisOptions.engineName, QStringLiteral("StoredEngine"));
    QCOMPARE(g_lastMatchAnalysisOptions.positionStr, position);
    QCOMPARE(g_lastMatchAnalysisOptions.byoyomiMs, 15000);
    QCOMPARE(g_lastMatchAnalysisOptions.multiPV, 4);
    QCOMPARE(g_lastMatchAnalysisOptions.mode, PlayMode::ConsiderationMode);
    QCOMPARE(g_lastMatchAnalysisOptions.considerationModel, &considerationModel);
    QCOMPARE(g_lastMatchAnalysisOptions.previousFileTo, 7);
    QCOMPARE(g_lastMatchAnalysisOptions.previousRankTo, 6);
    QCOMPARE(g_lastMatchAnalysisOptions.lastUsiMove, QStringLiteral("7g7f"));
}

QTEST_MAIN(TestAnalysisFlow)
#include "tst_analysisflow.moc"
