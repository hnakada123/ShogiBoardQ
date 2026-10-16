/// @file kifuanalysiscontroller.cpp
/// @brief 棋譜解析（条件ダイアログ・解析の進み具合の表示・結果の行の選択）の実装

#include "kifuanalysiscontroller.h"

#include "analysisresulthandler.h"
#include "analysisresultspanel.h"
#include "appactivitystate.h"
#include "errorbus.h"
#include "evaluationchartwidget.h"
#include "gameinfokeys.h"
#include "gameinfopane.h"
#include "kifuanalysisdialog.h"
#include "kifuanalysislistmodel.h"
#include "kifunavigationcontroller.h"
#include "kifunavigationstate.h"
#include "kifurecordlistmodel.h"
#include "logcategories.h"
#include "shogiview.h"
#include "enginelifecyclemanager.h"

#include <limits>

KifuAnalysisController::KifuAnalysisController(const Deps& deps, QObject* parent)
    : QObject(parent)
    , m_deps(deps)
{
    Q_ASSERT(m_deps.activity && m_deps.analysisModel);
    if (m_deps.evalChart) {
        connect(m_deps.evalChart, &EvaluationChartWidget::analysisPlyClicked,
                this, &KifuAnalysisController::onAnalysisPlyClicked);
    }
}

KifuAnalysisController::~KifuAnalysisController()
{
    if (m_running) stop();
}

// ============================================================
// 開始・中止
// ============================================================

void KifuAnalysisController::showDialogAndStart()
{
    const RecordLine line = displayedLine();
    if (line.isEmpty()) {
        showError(tr("解析対象の棋譜データが不正です。"));
        return;
    }
    KifuAnalysisDialog dialog(m_deps.parentWidget);
    dialog.setMaxPly(line.moveCount());
    if (dialog.exec() != QDialog::Accepted) return;
    if (std::optional<AnalysisRequest> request = requestFromDialog(dialog)) start(std::move(*request));
}

std::optional<AnalysisRequest> KifuAnalysisController::requestFromDialog(const KifuAnalysisDialog& dialog) const
{
    const auto engines = dialog.engineList();
    const int index = dialog.engineNumber();
    if (index < 0 || index >= engines.size()) {
        showError(tr("エンジン選択が不正です。"));
        return std::nullopt;
    }
    AnalysisRequest request;
    request.line = displayedLine();
    request.lineIndex = m_deps.navState ? m_deps.navState->currentLineIndex() : 0;
    request.enginePath = engines.at(index).path;
    request.engineName = dialog.engineName();
    const int last = request.line.moveCount();
    request.options.startPly = dialog.initPosition() ? 0 : qBound(0, dialog.startPly(), last);
    request.options.endPly = dialog.initPosition() ? last : qBound(request.options.startPly, dialog.endPly(), last);
    request.options.movetimeMs = dialog.byoyomiSec() * 1000;
    return request;
}

RecordLine KifuAnalysisController::displayedLine() const
{
    // 表示中の手順（分岐を選んでいればその分岐。終局の行は含まない）。解析中に棋譜欄を切り替えても
    // 解析には影響しないよう、表の表記（手数・分岐の印付き）はここで値にしておく
    RecordLine line = m_deps.navState ? m_deps.navState->currentRecordLine() : RecordLine{};
    if (const KifuRecordListModel* model = m_deps.recordModel) {
        for (int ply = 1; ply <= line.moveCount() && ply < model->rowCount(); ++ply)
            line.entries[ply - 1].displayText = model->data(model->index(ply, KifuRecordListModel::MoveColumn)).toString();
    }
    return line;
}

bool KifuAnalysisController::start(AnalysisRequest request)
{
    if (request.line.isEmpty()) {
        showError(tr("解析対象の棋譜データが不正です。"));
        return false;
    }
    const int last = request.line.moveCount();
    request.options.startPly = qBound(0, request.options.startPly, last);
    request.options.endPly = request.options.endPly < 0 ? last
        : qBound(request.options.startPly, request.options.endPly, last);
    stop();
    m_resultHandler.reset();
    m_job.reset();
    m_stoppedByUser = false;
    m_lineIndex = request.lineIndex;
    m_startPly = request.options.startPly;
    m_deps.analysisModel->clearAllItems();

    if (!m_resultsPanel) {
        m_resultsPanel = m_deps.resultsPanel ? m_deps.resultsPanel : new AnalysisResultsPanel(this);
        connect(m_resultsPanel, &AnalysisResultsPanel::stopRequested, this, &KifuAnalysisController::stop);
        connect(m_resultsPanel, &AnalysisResultsPanel::rowDoubleClicked,
                this, &KifuAnalysisController::onResultRowDoubleClicked);
        connect(m_resultsPanel, &AnalysisResultsPanel::rowSelected, this, &KifuAnalysisController::onResultRowSelected);
    }
    m_resultsPanel->showWithModel(m_deps.analysisModel);
    m_resultsPanel->beginAnalysis(request.options.endPly - request.options.startPly + 1, request.engineName);

    // 評価値グラフは解析の結果だけにする（条件ダイアログをやめたときは消さない）
    if (EvaluationChartWidget* chart = m_deps.evalChart) {
        chart->clearAll();
        chart->setEngine1Name(request.engineName);
        chart->setAnalysisLineIndex(request.lineIndex);
    }

    // 解析のエンジンは枠 E1 に置く（思考情報・通信ログは E1 の表示に出る）
    Usi* engine = m_deps.engine ? m_deps.engine : m_deps.engines->createEngine(1, std::nullopt, request.engineName);
    m_job = std::make_unique<AnalysisJob>(request, AnalysisJob::Deps{engine});
    connect(m_job.get(), &AnalysisJob::progress, this, &KifuAnalysisController::onProgress);
    connect(m_job.get(), &AnalysisJob::positionPrepared, this, &KifuAnalysisController::onPositionPrepared);
    connect(m_job.get(), &AnalysisJob::resultReady, this, &KifuAnalysisController::onResultReady);
    connect(m_job.get(), &AnalysisJob::thinkingInfoUpdated, this, &KifuAnalysisController::onThinkingInfoUpdated);
    connect(m_job.get(), &AnalysisJob::errorOccurred, this, &KifuAnalysisController::onEngineError);
    connect(m_job.get(), &AnalysisJob::finished, this, &KifuAnalysisController::onFinished);

    AnalysisResultHandler::Refs refs;
    refs.analysisModel = m_deps.analysisModel;
    refs.line = &m_job->line();
    refs.coord = m_job->coordinator();
    refs.presenter = m_resultsPanel;
    if (const GameInfoPane* gameInfo = m_deps.gameInfo) {
        const QList<KifGameInfoItem> items = gameInfo->gameInfo();
        for (const KifGameInfoItem& item : items) {
            if (item.key == GameInfoKeys::kBlackPlayer || item.key == GameInfoKeys::kShitatePlayer)
                refs.blackPlayerName = item.value;
            else if (item.key == GameInfoKeys::kWhitePlayer || item.key == GameInfoKeys::kUwatePlayer)
                refs.whitePlayerName = item.value;
        }
    }
    refs.boardFlipped = m_deps.shogiView && m_deps.shogiView->flipMode();
    m_resultHandler = std::make_unique<AnalysisResultHandler>(refs);

    m_running = true;
    m_deps.activity->setActivity(AppActivityState::Activity::DuringAnalysis);
    if (!m_job->start()) {
        if (m_job->error().isEmpty()) showError(tr("エンジン初期化に失敗しました。エンジン設定を確認してください。"));
        finishRun();
        return false;
    }
    return true;
}

void KifuAnalysisController::stop()
{
    if (!m_running) return;
    m_stoppedByUser = true;
    if (m_job) m_job->stop();
}

void KifuAnalysisController::finishRun()
{
    if (!m_running) return;
    m_running = false;
    if (m_resultsPanel) m_resultsPanel->setStopButtonEnabled(false);
    if (m_deps.activity->is(AppActivityState::Activity::DuringAnalysis))
        m_deps.activity->setActivity(AppActivityState::Activity::Idle);
}

// ============================================================
// 解析の進み具合
// ============================================================

void KifuAnalysisController::onPositionPrepared(int ply, const QString& /*sfen*/)
{
    publishCommittedEvaluation();
    // 解析している手を棋譜欄・盤に出す
    if (m_deps.navController) m_deps.navController->goToLine(m_lineIndex, ply);
}

void KifuAnalysisController::onProgress(int ply, const UsiInfo& info)
{
    m_resultHandler->updatePending(ply, info);
}

void KifuAnalysisController::onResultReady()
{
    m_resultHandler->commitPendingResult();
}

void KifuAnalysisController::onThinkingInfoUpdated(const ThinkingInfoRow& row)
{
    if (!row.pvKanji.isEmpty()) m_resultHandler->updatePendingPvKanji(row.pvKanji, row.multipv);
}

void KifuAnalysisController::publishCommittedEvaluation()
{
    if (!m_resultHandler || m_resultHandler->lastCommittedPly() < 0) return;
    // 定跡など評価値のない結果は表には残し、グラフには足さない
    const UsiScore& score = m_resultHandler->lastCommittedScore();
    if (!score.isNone() && m_deps.evalChart)
        m_deps.evalChart->appendScoreP1Buffered(m_resultHandler->lastCommittedPly(), score);
    m_resultHandler->resetLastCommitted();
}

void KifuAnalysisController::onFinished(AnalysisCoordinator::Mode mode)
{
    qCDebug(lcAnalysis).noquote() << "analysis finished, stoppedByUser=" << m_stoppedByUser;
    publishCommittedEvaluation();
    const bool completed = m_running && !m_stoppedByUser && mode == AnalysisCoordinator::RangePositions;
    finishRun();
    if (completed && m_resultsPanel) m_resultsPanel->showAnalysisComplete(m_deps.analysisModel->rowCount());
}

void KifuAnalysisController::onEngineError(const QString& message)
{
    qCWarning(lcAnalysis).noquote() << "Engine error during analysis:" << message;
    showError(tr("エンジンエラー: %1").arg(message));
    stop();
}

// ============================================================
// 結果の行・グラフの点の選択
// ============================================================

void KifuAnalysisController::onResultRowSelected(int row)
{
    if (m_running || row < 0 || row >= m_deps.analysisModel->rowCount()) return;
    if (m_deps.navController) m_deps.navController->goToLine(m_lineIndex, m_startPly + row);
}

void KifuAnalysisController::onResultRowDoubleClicked(int row)
{
    if (m_resultHandler) m_resultHandler->showPvBoardDialog(row);
}

void KifuAnalysisController::onAnalysisPlyClicked(int lineIndex, int ply)
{
    if (m_deps.navController) m_deps.navController->goToLine(lineIndex, ply);
}

void KifuAnalysisController::showError(const QString& message) const
{
    ErrorBus::instance().postMessage(ErrorBus::ErrorLevel::Warning, message);
}
