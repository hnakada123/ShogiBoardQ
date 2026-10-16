/// @file considerationuicontroller.cpp
/// @brief 検討タブの操作と、検討中の表示（経過時間・候補手の数・矢印）の実装

#include "considerationuicontroller.h"

#include "analysissession.h"
#include "candidatearrowpresenter.h"
#include "changeenginesettingsdialog.h"
#include "considerationpositionresolver.h"
#include "considerationtabmanager.h"
#include "enginelistsettings.h"
#include "engineinfowidget.h"
#include "errorbus.h"
#include "kifunavigationstate.h"
#include "logcategories.h"

ConsiderationUiController::ConsiderationUiController(const Deps& deps, QObject* parent)
    : QObject(parent)
    , m_parentWidget(deps.parentWidget)
    , m_considerationTabManager(deps.considerationTabManager)
    , m_thinkingInfo1(deps.thinkingInfo1)
    , m_arrows(deps.arrows)
    , m_session(deps.session)
    , m_navState(deps.navState)
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
        // 開始の要求は MainWindowComposer が startConsideration へつなぐ
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
    if (!m_session || !m_navState || !m_considerationTabManager) return;
    // エンジン破棄中の場合は検討開始を拒否（ハングアップ防止）
    if (m_session->isShutdownInProgress()) {
        qCDebug(lcUi) << "startConsideration: engine shutdown in progress, ignoring";
        return;
    }
    ConsiderationTabManager* tab = m_considerationTabManager;
    const QString selectedName = tab->selectedEngineName();
    if (selectedName.isEmpty()) {
        ErrorBus::instance().postMessage(ErrorBus::ErrorLevel::Error, tr("将棋エンジンが選択されていません。"));
        return;
    }
    // エンジンは名前（検討タブで選んだもの）で引く。番号で引くと、タブの一覧を作った後に
    // 登録が変わったときに名前とパスが食い違う
    const QList<EngineListSettings::EngineEntry> engines = EngineListSettings::loadEngines();
    const int index = EngineListSettings::indexOfName(engines, selectedName);
    if (index < 0 || engines.at(index).path.isEmpty()) {
        ErrorBus::instance().postMessage(ErrorBus::ErrorLevel::Warning, tr("検討エンジンの選択が不正です。"));
        return;
    }
    tab->saveConsiderationTabSettings();

    // 表示中の手順（分岐ツリー）から、検討する局面と直前の手を決める
    const RecordLine line = m_navState->currentRecordLine();
    const UsiPosition position = ConsiderationPositionResolver::resolve(line, line.currentPly, m_navState->currentSfen());

    if (!m_considerationModel) m_considerationModel = tab->considerationModel();
    AnalysisOptions opt;
    opt.mode = AnalysisOptions::Mode::Consideration;
    opt.enginePath = engines.at(index).path;
    opt.engineName = engines.at(index).name;
    opt.position = position;
    opt.byoyomiMs = tab->isUnlimitedTime() ? 0 : tab->byoyomiSec() * 1000;   // 0 は無制限
    opt.multiPV = tab->considerationMultiPV();
    qCDebug(lcUi).noquote() << "startConsideration: engine=" << opt.engineName
                            << "position=" << opt.position.command().left(50);

    m_considerationActive = true;
    attachModels();
    if (!m_session->start(opt)) {
        m_considerationActive = false;
        updateArrows();
        return;
    }
    // 経過時間は、検討が局面ごとに探索を始めるたびに数え直す（considerationSearchStarted）
    tab->setConsiderationRunning(true);
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

void ConsiderationUiController::attachModels()
{
    if (!m_considerationTabManager || !m_considerationModel) {
        updateArrows();
        return;
    }
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
    if (!m_arrows) return;
    const bool show = m_showArrows && (!m_considerationTabManager
        || m_considerationTabManager->isShowArrowsChecked());
    m_arrows->setConsiderationState(
        m_considerationActive, show, m_considerationModel);
}
