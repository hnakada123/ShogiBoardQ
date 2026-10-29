/// @file kifuanalysiscontroller.cpp
/// @brief 棋譜解析（条件ダイアログ・解析の進み具合の表示・結果の行の選択）の実装

#include "kifuanalysiscontroller.h"

#include "analysisresultspanel.h"
#include "appactivitystate.h"
#include "errorbus.h"
#include "evaluationchartwidget.h"
#include "evaluationgraphpresenter.h"
#include "kifuanalysisdialog.h"
#include "kifuanalysislistmodel.h"
#include "kifubranchnode.h"
#include "kifubranchtree.h"
#include "kifunavigationcontroller.h"
#include "kifunavigationstate.h"
#include "kifuanalysisrow.h"
#include "kifuanalysisrowpresenter.h"
#include "logcategories.h"
#include "pvclickcontroller.h"
#include "engineslots.h"

#include <QDockWidget>
#include <limits>

KifuAnalysisController::KifuAnalysisController(const Deps& deps, QObject* parent)
    : QObject(parent)
    , m_deps(deps)
    , m_resultsPanel(deps.resultsPanel)
{
    Q_ASSERT(m_deps.activity && m_deps.analysisModel && m_resultsPanel);
    m_resultsPanel->setModel(m_deps.analysisModel);
    connect(m_resultsPanel, &AnalysisResultsPanel::stopRequested, this, &KifuAnalysisController::stop);
    connect(m_resultsPanel, &AnalysisResultsPanel::rowDoubleClicked,
            this, &KifuAnalysisController::onResultRowDoubleClicked);
    connect(m_resultsPanel, &AnalysisResultsPanel::rowSelected, this, &KifuAnalysisController::onResultRowSelected);
    if (m_deps.engines)
        connect(m_deps.engines, &EngineSlots::engineRevoked, this, &KifuAnalysisController::onEngineRevoked);
    connect(m_deps.activity, &AppActivityState::revoked, this, &KifuAnalysisController::onActivityRevoked);
    if (m_deps.evalChart) {
        connect(m_deps.evalChart, &EvaluationChartWidget::analysisPlyClicked,
                this, &KifuAnalysisController::onAnalysisPlyClicked);
    }
    if (m_deps.navState) {
        connect(m_deps.navState, &KifuNavigationState::changed,
                this, &KifuAnalysisController::syncResultSelection);
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
    const std::optional<KifuAnalysisDialog::Engine> engine = dialog.engine();
    if (!engine) {
        showError(tr("エンジン選択が不正です。"));
        return std::nullopt;
    }
    AnalysisRequest request;
    request.line = displayedLine();
    if (m_deps.navState) {
        const QList<KifuBranchNode*> path = m_deps.navState->selectedPath();
        if (!path.isEmpty()) request.lineEndNodeId = path.constLast()->nodeId();
    }
    request.branchPlies = displayedBranchPlies();
    request.enginePath = engine->path;
    request.engineName = engine->name;
    request.options = dialog.options();   // 範囲は解析する手順の手数（setMaxPly）に収めてある
    return request;
}

RecordLine KifuAnalysisController::displayedLine() const
{
    return m_deps.navState ? m_deps.navState->currentRecordLine() : RecordLine{};
}

QSet<int> KifuAnalysisController::displayedBranchPlies() const
{
    QSet<int> plies;
    if (!m_deps.navState) return plies;
    const QList<KifuBranchNode*> path = m_deps.navState->selectedPath();
    for (const KifuBranchNode* node : path)
        if (node->parent() && node->parent()->hasBranch()) plies.insert(node->ply());
    return plies;
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
    m_rows.reset();
    m_pendingPlot.reset();
    m_job.reset();
    m_stoppedByUser = false;
    m_lineEndNodeId = request.lineEndNodeId;
    m_startPly = request.options.startPly;
    m_deps.analysisModel->clear();

    if (QDockWidget* dock = m_deps.resultsDock) {
        dock->setVisible(true);
        dock->raise();
    }
    m_resultsPanel->beginAnalysis(request.options.endPly - request.options.startPly + 1, request.engineName);

    // 評価値グラフは解析の結果だけにする（条件ダイアログをやめたときは消さない）
    if (EvaluationGraphPresenter* graph = m_deps.evalGraph) graph->beginAnalysis(request.engineName, request.lineEndNodeId);

    // 解析のエンジンは枠 E1 に置く（思考情報・通信ログは E1 の表示に出る）
    if (!m_deps.engine) m_lease = m_deps.engines->acquire(this, 1, request.engineName);
    Usi* engine = m_deps.engine ? m_deps.engine : m_lease.engine();
    m_job = std::make_unique<AnalysisJob>(request, AnalysisJob::Deps{engine});
    connect(m_job.get(), &AnalysisJob::plyStarted, this, &KifuAnalysisController::onPlyStarted);
    connect(m_job.get(), &AnalysisJob::plyAnalyzed, this, &KifuAnalysisController::onPlyAnalyzed);
    connect(m_job.get(), &AnalysisJob::errorOccurred, this, &KifuAnalysisController::onEngineError);
    connect(m_job.get(), &AnalysisJob::finished, this, &KifuAnalysisController::onFinished);

    KifuAnalysisRowPresenter::Refs refs;
    refs.analysisModel = m_deps.analysisModel;
    refs.line = &m_job->line();
    refs.branchPlies = request.branchPlies;
    m_rows = std::make_unique<KifuAnalysisRowPresenter>(refs);

    m_running = true;
    m_activityLease = m_deps.activity->begin(AppActivityState::Activity::DuringAnalysis, this);
    // エンジンの起動の失敗は、戻った後に onEngineError で届く（ここで二重に出さない）
    if (!m_job->start()) {
        showError(tr("エンジン初期化に失敗しました。エンジン設定を確認してください。"));
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

void KifuAnalysisController::onEngineRevoked(QObject* owner)
{
    // ほかの処理（検討・対局など）がエンジンの枠を取った。解析を止める
    if (owner == this) stop();
}

void KifuAnalysisController::onActivityRevoked(QObject* owner)
{
    if (owner == this) stop();
}

void KifuAnalysisController::finishRun(bool completed)
{
    if (!m_running) return;
    m_running = false;
    if (m_resultsPanel) m_resultsPanel->finishAnalysis(completed);
    // 自分の動作状態のときだけ待機中に戻す（ほかの部品が始めた動作は変えない）
    m_activityLease.reset();
    syncResultSelection();
}

// ============================================================
// 解析の進み具合
// ============================================================

void KifuAnalysisController::onPlyStarted(int ply)
{
    publishCommittedEvaluation();
    // 解析している手を棋譜欄・盤に出す
    if (m_deps.navController) m_deps.navController->goToPlyOnLine(m_lineEndNodeId, ply);
}

void KifuAnalysisController::onPlyAnalyzed(const PlyAnalysis& result)
{
    const UsiScore blackScore = m_rows->append(result);
    // グラフには次の局面に移るときに足す（定跡など評価値のない結果は表には残し、グラフには足さない）
    if (!blackScore.isNone()) m_pendingPlot = PendingPlot{result.ply, blackScore};
}

void KifuAnalysisController::publishCommittedEvaluation()
{
    if (!m_pendingPlot) return;
    if (m_deps.evalGraph) m_deps.evalGraph->plotAnalysisScore(m_pendingPlot->ply, m_pendingPlot->score);
    m_pendingPlot.reset();
}

void KifuAnalysisController::onFinished(bool completed)
{
    qCDebug(lcAnalysis).noquote() << "analysis finished, completed=" << completed << "stoppedByUser=" << m_stoppedByUser;
    publishCommittedEvaluation();
    finishRun(!m_stoppedByUser && completed);
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

void KifuAnalysisController::syncResultSelection()
{
    if (m_running || !m_resultsPanel || !m_deps.navState) return;
    // 表示中の手が解析した手順の上にあれば、その手の結果の行を選ぶ
    const KifuNavigationState* nav = m_deps.navState;
    const KifuBranchNode* current = nav->currentNode();
    const bool onLine = current && nav->tree()->nodeOnLine(m_lineEndNodeId, current->ply()) == current;
    m_resultsPanel->selectRow(onLine ? current->ply() - m_startPly : -1);
}

void KifuAnalysisController::onResultRowSelected(int row)
{
    if (m_running || row < 0 || row >= m_deps.analysisModel->rowCount()) return;
    if (m_deps.navController) m_deps.navController->goToPlyOnLine(m_lineEndNodeId, m_startPly + row);
}

void KifuAnalysisController::onResultRowDoubleClicked(int row)
{
    // 読み筋の無い行（定跡）は開かない。行は解析した局面と、そこに至った手を持つ
    const KifuAnalysisRow* item = m_deps.analysisModel->row(row);
    if (!m_deps.pvBoards || !item || item->book || !item->pv.isDisplayable() || item->pv.baseSfen().isEmpty()) return;
    m_deps.pvBoards->showPvBoard(item->pv, item->lastUsiMove);
}

void KifuAnalysisController::onAnalysisPlyClicked(int lineEndNodeId, int ply)
{
    if (m_deps.navController) m_deps.navController->goToPlyOnLine(lineEndNodeId, ply);
}

void KifuAnalysisController::showError(const QString& message) const
{
    ErrorBus::instance().postMessage(ErrorBus::ErrorLevel::Warning, message);
}
