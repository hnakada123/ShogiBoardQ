/// @file analysisflowcontroller.cpp
/// @brief 棋譜解析フローコントローラクラスの実装

#include "analysisflowcontroller.h"

#include "analysiscoordinator.h"
#include "analysisresulthandler.h"
#include "analysisresultspresenter.h"
#include "kifuanalysislistmodel.h"

#include <QString>
#include <QObject>
#include <QtGlobal>
#include <QPointer>
#include <limits>

#include "logcategories.h"
#include "kifuanalysisdialog.h"

AnalysisFlowController::AnalysisFlowController(QObject* parent)
    : QObject(parent)
    , m_resultHandler(std::make_unique<AnalysisResultHandler>())
{
}

AnalysisFlowController::~AnalysisFlowController()
{
    // 解析中の場合はリソースリーク防止のため停止処理を行う
    if (m_running) {
        stop();
    }
}

void AnalysisFlowController::emitAnalysisStoppedOnce()
{
    if (m_analysisStoppedEmitted) {
        return;
    }
    m_analysisStoppedEmitted = true;
    Q_EMIT analysisStopped();
}

bool AnalysisFlowController::start(const Deps& d, AnalysisRequest request)
{
    if (request.line.isEmpty()) {
        if (d.displayError) d.displayError(tr("解析対象の棋譜データが不正です。"));
        return false;
    }
    if (!d.analysisModel) {
        if (d.displayError) d.displayError(tr("内部エラー: 解析モデルが未準備です。"));
        return false;
    }
    const int last = request.line.moveCount();
    request.options.startPly = qBound(0, request.options.startPly, last);
    request.options.endPly = request.options.endPly < 0 ? last
        : qBound(request.options.startPly, request.options.endPly, last);
    stop();
    m_resultHandler->setRefs({});
    m_job.reset();
    m_analysisStoppedEmitted = false;
    m_stoppedByUser = false;
    m_analysisLineIndex = request.lineIndex;
    m_analysisStartPly = request.options.startPly;
    m_analysisModel = d.analysisModel;
    m_err = d.displayError;
    m_analysisModel->clearAllItems();
    m_resultHandler->reset();

    if (!m_presenter) {
        m_presenter = d.presenter ? d.presenter : new AnalysisResultsPresenter(this);
        connect(m_presenter, &AnalysisResultsPresenter::stopRequested, this, &AnalysisFlowController::stop);
        connect(m_presenter, &AnalysisResultsPresenter::rowDoubleClicked, this, &AnalysisFlowController::onResultRowDoubleClicked);
        connect(m_presenter, &AnalysisResultsPresenter::rowSelected, this, &AnalysisFlowController::onResultRowSelected);
    }
    m_presenter->showWithModel(m_analysisModel);
    m_presenter->beginAnalysis(request.options.endPly - request.options.startPly + 1, request.engineName);

    AnalysisJob::Dependencies deps{d.usi, d.logModel, d.thinkingModel};
    m_job = std::make_unique<AnalysisJob>(request, deps);
    connect(m_job.get(), &AnalysisJob::progress, this, &AnalysisFlowController::onAnalysisProgress);
    connect(m_job.get(), &AnalysisJob::positionPrepared, this, &AnalysisFlowController::onPositionPrepared);
    connect(m_job.get(), &AnalysisJob::resultReady, this, &AnalysisFlowController::onBestMoveReceived);
    connect(m_job.get(), &AnalysisJob::thinkingInfoUpdated, this, &AnalysisFlowController::onThinkingInfoUpdated);
    connect(m_job.get(), &AnalysisJob::errorOccurred, this, &AnalysisFlowController::onEngineError);
    connect(m_job.get(), &AnalysisJob::finished, this, &AnalysisFlowController::onAnalysisFinished);

    AnalysisResultHandler::Refs refs;
    refs.analysisModel = m_analysisModel;
    refs.line = &m_job->line();
    refs.coord = m_job->coordinator();
    refs.presenter = m_presenter;
    refs.blackPlayerName = d.blackPlayerName;
    refs.whitePlayerName = d.whitePlayerName;
    refs.boardFlipped = d.boardFlipped;
    m_resultHandler->setRefs(refs);

    emit analysisEngineNameChanged(request.engineName);
    emit analysisStarted(request.lineIndex);
    m_running = true;
    if (!m_job->start()) {
        m_running = false;
        m_presenter->setStopButtonEnabled(false);
        if (m_err && m_job->error().isEmpty()) m_err(tr("エンジン初期化に失敗しました。エンジン設定を確認してください。"));
        emitAnalysisStoppedOnce();
        return false;
    }
    return true;
}

void AnalysisFlowController::stop()
{
    if (!m_running) return;
    m_stoppedByUser = true;
    if (m_job) m_job->stop();
}

void AnalysisFlowController::onBestMoveReceived()
{
    m_resultHandler->commitPendingResult();
}

void AnalysisFlowController::onThinkingInfoUpdated(const ThinkingInfoRow& row)
{
    if (!row.pvKanji.isEmpty()) {
        m_resultHandler->updatePendingPvKanji(row.pvKanji);
    }
}

void AnalysisFlowController::onAnalysisProgress(int ply, const UsiInfo& info)
{
    // scoreCp 未設定は INT_MIN、mate=0 は手数の情報なし（"mate +" / "mate -" は mateText で渡す）
    const int scoreCp = info.score.isCp() ? info.score.value : std::numeric_limits<int>::min();
    const int mate = info.score.isMate() ? info.score.value : 0;
    m_resultHandler->updatePending(ply, scoreCp, mate, info.pv.join(QLatin1Char(' ')),
                                   info.score.mateText());
}


void AnalysisFlowController::onAnalysisFinished(AnalysisCoordinator::Mode mode)
{
    qCDebug(lcAnalysis).noquote() << "analysis finished, m_stoppedByUser=" << m_stoppedByUser;

    publishCommittedEvaluation();

    m_running = false;

    if (m_presenter) {
        m_presenter->setStopButtonEnabled(false);
    }

    // ユーザーによる中止でなければ（正常完了なら）完了メッセージを表示
    if (!m_stoppedByUser && mode == AnalysisCoordinator::RangePositions && m_presenter && m_analysisModel) {
        int totalMoves = m_analysisModel->rowCount();
        m_presenter->showAnalysisComplete(totalMoves);
    }

    emitAnalysisStoppedOnce();
}

void AnalysisFlowController::onResultRowDoubleClicked(int row)
{
    m_resultHandler->showPvBoardDialog(row);
}

void AnalysisFlowController::onResultRowSelected(int row)
{
    if (!m_running && m_analysisModel && row >= 0 && row < m_analysisModel->rowCount()) {
        emit analysisResultRowSelected(m_analysisStartPly + row);
    }
}


void AnalysisFlowController::onEngineError(const QString& msg)
{
    qCWarning(lcAnalysis).noquote() << "Engine error during analysis:" << msg;

    // エラーメッセージを表示
    if (m_err) {
        m_err(tr("エンジンエラー: %1").arg(msg));
    }

    // 解析を停止（analysisStopped は一度だけ通知される）
    stop();
}

// ============================================================
// 解析ダイアログからの開始
// ============================================================

void AnalysisFlowController::start(const Deps& d, KifuAnalysisDialog* dlg)
{
    m_lastStartSucceeded = false;
    if (!dlg) return;
    if (d.line.isEmpty()) {
        if (d.displayError) d.displayError(tr("解析対象の棋譜データが不正です。"));
        return;
    }
    const auto engines = dlg->engineList();
    const int index = dlg->engineNumber();
    if (index < 0 || index >= engines.size()) {
        if (d.displayError) d.displayError(tr("エンジン選択が不正です。"));
        return;
    }
    AnalysisRequest request;
    request.line = d.line;
    request.lineIndex = d.lineIndex;
    request.enginePath = engines.at(index).path;
    request.engineName = dlg->engineName();
    const int last = d.line.moveCount();
    request.options.startPly = dlg->initPosition() ? 0 : qBound(0, dlg->startPly(), last);
    request.options.endPly = dlg->initPosition() ? last : qBound(request.options.startPly, dlg->endPly(), last);
    request.options.movetimeMs = dlg->byoyomiSec() * 1000;
    m_lastStartSucceeded = start(d, std::move(request));
}

bool AnalysisFlowController::runWithDialog(const Deps& d, QWidget* parent)
{
    if (d.line.isEmpty()) {
        if (d.displayError) d.displayError(tr("解析対象の棋譜データが不正です。"));
        return false;
    }
    KifuAnalysisDialog dlg(parent);
    dlg.setMaxPly(d.line.moveCount());
    if (dlg.exec() != QDialog::Accepted) return false;
    start(d, &dlg);
    return m_lastStartSucceeded;
}

// ============================================================
// 局面ごとの準備と確定した評価値の通知
// ============================================================

void AnalysisFlowController::publishCommittedEvaluation()
{
    if (m_resultHandler->lastCommittedPly() < 0) return;
    // 定跡など評価値のない結果は表には残し、グラフへの通知は出さない。
    if (m_resultHandler->lastCommittedScoreCp() != std::numeric_limits<int>::min()) {
        emit analysisProgressReported(m_resultHandler->lastCommittedPly(), m_resultHandler->lastCommittedScoreCp(),
                                      m_resultHandler->lastCommittedMate());
    }
    m_resultHandler->resetLastCommitted();
}

void AnalysisFlowController::onPositionPrepared(int ply, const QString& /*sfen*/)
{
    publishCommittedEvaluation();
    emit analysisPositionPrepared(ply);
}
