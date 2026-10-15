/// @file considerationuicontroller.cpp
/// @brief 検討タブの操作と、検討中の表示（経過時間・候補手の数・矢印）の実装

#include "considerationuicontroller.h"

#include "analysissession.h"
#include "candidatearrowcontroller.h"
#include "changeenginesettingsdialog.h"
#include "considerationtabmanager.h"
#include "dialogcoordinator.h"
#include "engineinfowidget.h"
#include "logcategories.h"
#include "shogiview.h"

ConsiderationUiController::ConsiderationUiController(const Deps& deps, QObject* parent)
    : QObject(parent)
    , m_parentWidget(deps.parentWidget)
    , m_considerationTabManager(deps.considerationTabManager)
    , m_thinkingInfo1(deps.thinkingInfo1)
    , m_shogiView(deps.shogiView)
    , m_session(deps.session)
    , m_considerationModel(deps.considerationModel)
    , m_commLogModel(deps.commLogModel)
{
    if (m_considerationTabManager) {
        connect(m_considerationTabManager, &ConsiderationTabManager::showArrowsChanged,
                this, &ConsiderationUiController::onShowArrowsChanged);
        connect(m_considerationTabManager, &ConsiderationTabManager::considerationMultiPVChanged,
                this, &ConsiderationUiController::onMultiPVChanged);
        connect(m_considerationTabManager, &ConsiderationTabManager::stopConsiderationRequested,
                this, &ConsiderationUiController::stopConsideration);
        // 開始の要求は AnalysisTabWiring が startConsideration へつなぐ
    }
    if (m_session) {
        connect(m_session, &AnalysisSession::considerationModeEnded,
                this, &ConsiderationUiController::onModeEnded);
        connect(m_session, &AnalysisSession::considerationWaitingStarted,
                this, &ConsiderationUiController::onWaitingStarted);
    }
}

// ============================================================
// 検討タブの操作
// ============================================================

void ConsiderationUiController::startConsideration()
{
    if (!m_session) return;
    // エンジン破棄中の場合は検討開始を拒否（ハングアップ防止）
    if (m_session->isShutdownInProgress()) {
        qCDebug(lcUi) << "startConsideration: engine shutdown in progress, ignoring";
        return;
    }
    if (m_dialogCoordinator) m_dialogCoordinator->startConsiderationFromContext();
}

void ConsiderationUiController::onEngineSettingsRequested(int engineNumber, const QString& engineName)
{
    qCDebug(lcUi) << "onEngineSettingsRequested engineNumber=" << engineNumber << " engineName=" << engineName;
    ChangeEngineSettingsDialog dialog(m_parentWidget);
    dialog.setEngineName(engineName);
    dialog.setupEngineOptionsDialog();
    dialog.exec();
}

void ConsiderationUiController::onEngineChanged(int engineIndex, const QString& engineName)
{
    qCDebug(lcUi) << "onEngineChanged engineIndex=" << engineIndex << " engineName=" << engineName;
    // 検討中でなければ、次に始めるときに選んだエンジンを使う
    if (!m_session || !m_session->isConsidering()) return;
    // 今の検討を中止して新しいエンジンで始め直す
    m_session->stop();
    startConsideration();
}

void ConsiderationUiController::stopConsideration()
{
    if (m_session) m_session->stop();
}

void ConsiderationUiController::onMultiPVChanged(int value)
{
    qCDebug(lcAnalysis).noquote() << "onMultiPVChanged: value=" << value;
    if (m_session && m_session->isConsidering()) m_session->updateMultiPV(value);
    if (m_considerationActive && m_considerationTabManager) m_considerationTabManager->startElapsedTimer();
}

void ConsiderationUiController::onShowArrowsChanged(bool checked)
{
    m_showArrows = checked;
    updateArrows();
}

// ============================================================
// 検討の状態の表示
// ============================================================

void ConsiderationUiController::onModeStarted()
{
    m_considerationActive = true;
    qCDebug(lcAnalysis).noquote() << "Initializing consideration mode";

    // 起動時に検討タブのモデルが無かった場合は、検討タブから最新のモデルを取る
    if (!m_considerationModel && m_considerationTabManager) {
        m_considerationModel = m_considerationTabManager->considerationModel();
    }
    if (!m_considerationTabManager || !m_considerationModel) return;

    m_considerationTabManager->setConsiderationThinkingModel(m_considerationModel);
    if (m_commLogModel) {
        // 検討タブと思考タブのエンジン欄に通信ログをつなぐ
        if (m_considerationTabManager->considerationInfo())
            m_considerationTabManager->considerationInfo()->setModel(m_commLogModel);
        if (m_thinkingInfo1) m_thinkingInfo1->setModel(m_commLogModel);
    }
    // 検討と対局で共有する矢印コントローラへ表示元を設定
    updateArrows();
}

void ConsiderationUiController::onTimeSettingsReady(bool unlimited, int byoyomiSec)
{
    qCDebug(lcAnalysis).noquote() << "onTimeSettingsReady: unlimited=" << unlimited << "byoyomiSec=" << byoyomiSec;
    if (!m_considerationTabManager) return;
    m_considerationTabManager->setConsiderationTimeLimit(unlimited, byoyomiSec);
    m_considerationTabManager->startElapsedTimer();
    m_considerationTabManager->setConsiderationRunning(true);
}

void ConsiderationUiController::onDialogMultiPVReady(int multiPV)
{
    if (m_considerationTabManager) m_considerationTabManager->setConsiderationMultiPV(multiPV);
}

void ConsiderationUiController::onModeEnded()
{
    m_considerationActive = false;
    qCDebug(lcAnalysis).noquote() << "consideration mode ended";
    if (m_considerationTabManager) {
        m_considerationTabManager->stopElapsedTimer();
        m_considerationTabManager->setConsiderationRunning(false);
    }
    // 検討終了時に矢印を消す
    updateArrows();
}

void ConsiderationUiController::onWaitingStarted()
{
    if (m_considerationTabManager) m_considerationTabManager->stopElapsedTimer();
}

void ConsiderationUiController::updateArrows()
{
    if (!m_shogiView) return;
    const bool show = m_showArrows && (!m_considerationTabManager
        || m_considerationTabManager->isShowArrowsChecked());
    CandidateArrowController::forView(m_shogiView)->setConsiderationState(
        m_considerationActive, show, m_considerationModel);
}
