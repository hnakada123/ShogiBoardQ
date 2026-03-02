/// @file considerationwiring.cpp
/// @brief 検討モード配線クラスの実装

#include "considerationwiring.h"

#include "logcategories.h"

#include "shogiview.h"
#include "matchcoordinator.h"
#include "dialogcoordinator.h"
#include "considerationmodeuicontroller.h"
#include "considerationtabmanager.h"
#include "changeenginesettingsdialog.h"

ConsiderationWiring::ConsiderationWiring(const Deps& deps, QObject* parent)
    : QObject(parent)
    , m_parentWidget(deps.parentWidget)
    , m_considerationTabManager(deps.considerationTabManager)
    , m_thinkingInfo1(deps.thinkingInfo1)
    , m_shogiView(deps.shogiView)
    , m_match(deps.match)
    , m_dialogCoordinator(deps.dialogCoordinator)
    , m_considerationModel(deps.considerationModel)
    , m_commLogModel(deps.commLogModel)
    , m_playMode(deps.playMode)
    , m_currentSfenStr(deps.currentSfenStr)
    , m_ensureDialogCoordinator(deps.ensureDialogCoordinator)
{
}

void ConsiderationWiring::updateDeps(const Deps& deps)
{
    m_parentWidget = deps.parentWidget;
    m_considerationTabManager = deps.considerationTabManager;
    m_thinkingInfo1 = deps.thinkingInfo1;
    m_shogiView = deps.shogiView;
    m_match = deps.match;
    m_dialogCoordinator = deps.dialogCoordinator;
    m_considerationModel = deps.considerationModel;
    m_commLogModel = deps.commLogModel;
    m_playMode = deps.playMode;
    m_currentSfenStr = deps.currentSfenStr;
    if (deps.ensureDialogCoordinator) {
        m_ensureDialogCoordinator = deps.ensureDialogCoordinator;
    }
}

void ConsiderationWiring::setDialogCoordinator(DialogCoordinator* dc)
{
    m_dialogCoordinator = dc;
}

void ConsiderationWiring::ensureUIController()
{
    if (m_uiController) return;

    m_uiController = new ConsiderationModeUIController(this);
    m_uiController->setConsiderationTabManager(m_considerationTabManager);
    m_uiController->setThinkingEngineInfo(m_thinkingInfo1);
    m_uiController->setShogiView(m_shogiView);
    m_uiController->setMatchCoordinator(m_match);
    m_uiController->setConsiderationModel(m_considerationModel);
    m_uiController->setCommLogModel(m_commLogModel);

    // コントローラからのシグナルを接続
    connect(m_uiController, &ConsiderationModeUIController::stopRequested,
            this, &ConsiderationWiring::handleStopRequest);
    connect(m_uiController, &ConsiderationModeUIController::startRequested,
            this, &ConsiderationWiring::displayConsiderationDialog);
    connect(m_uiController, &ConsiderationModeUIController::multiPVChangeRequested,
            this, &ConsiderationWiring::onMultiPVChangeRequested);
}

void ConsiderationWiring::displayConsiderationDialog()
{
    qCDebug(lcUi) << "displayConsiderationDialog ENTER";

    if (!m_playMode) return;

    // エンジン破棄中の場合は検討開始を拒否（ハングアップ防止）
    if (m_match && m_match->isEngineShutdownInProgress()) {
        qCDebug(lcUi) << "displayConsiderationDialog engine shutdown in progress, ignoring";
        return;
    }

    // m_dialogCoordinatorが未初期化の場合、コールバックで遅延初期化を実行
    if (!m_dialogCoordinator && m_ensureDialogCoordinator) {
        m_ensureDialogCoordinator();
    }

    if (m_dialogCoordinator) {
        DialogCoordinator::Deps dcDeps;
        dcDeps.matchCoordinator = m_match;
        dcDeps.considerationTabManager = m_considerationTabManager;
        m_dialogCoordinator->updateDeps(dcDeps);
        // 検討開始前にモードを設定（onSetEngineNamesで検討タブにエンジン名を設定するため）
        const PlayMode previousMode = *m_playMode;
        *m_playMode = PlayMode::ConsiderationMode;
        if (!m_dialogCoordinator->startConsiderationFromContext()) {
            // 検討開始失敗時は元のモードに戻す
            *m_playMode = previousMode;
        }
    }
    qCDebug(lcUi) << "displayConsiderationDialog EXIT";
}

void ConsiderationWiring::onEngineSettingsRequested(int engineNumber, const QString& engineName)
{
    qCDebug(lcUi) << "onEngineSettingsRequested engineNumber=" << engineNumber
                   << " engineName=" << engineName;

    ChangeEngineSettingsDialog dialog(m_parentWidget);
    dialog.setEngineNumber(engineNumber);
    dialog.setEngineName(engineName);
    dialog.setupEngineOptionsDialog();

    if (dialog.exec() == QDialog::Rejected) {
        return;
    }
}

void ConsiderationWiring::onEngineChanged(int engineIndex, const QString& engineName)
{
    qCDebug(lcUi) << "onEngineChanged engineIndex=" << engineIndex
                   << " engineName=" << engineName;

    if (!m_playMode) return;

    // 検討モード中でなければ何もしない
    if (*m_playMode != PlayMode::ConsiderationMode) {
        qCDebug(lcUi) << "onEngineChanged not in consideration mode, ignoring";
        return;
    }

    // 現在の検討を中止して新しいエンジンで再開
    if (m_match) {
        m_match->stopAnalysisEngine();
    }

    // 新しいエンジンで検討を開始
    displayConsiderationDialog();
}

void ConsiderationWiring::onModeStarted()
{
    // m_considerationModel が初期化時に null だった場合、
    // ConsiderationTabManager から最新のモデルを取得して補完する
    if (!m_considerationModel && m_considerationTabManager) {
        m_considerationModel = m_considerationTabManager->considerationModel();
    }

    ensureUIController();
    if (m_uiController) {
        m_uiController->setConsiderationTabManager(m_considerationTabManager);
        m_uiController->setThinkingEngineInfo(m_thinkingInfo1);
        m_uiController->setConsiderationModel(m_considerationModel);
        m_uiController->setCommLogModel(m_commLogModel);
        m_uiController->onModeStarted();
    }
}

void ConsiderationWiring::onTimeSettingsReady(bool unlimited, int byoyomiSec)
{
    ensureUIController();
    if (m_uiController) {
        m_uiController->setConsiderationTabManager(m_considerationTabManager);
        m_uiController->setThinkingEngineInfo(m_thinkingInfo1);
        m_uiController->setMatchCoordinator(m_match);
        m_uiController->onTimeSettingsReady(unlimited, byoyomiSec);
    }
}

void ConsiderationWiring::onDialogMultiPVReady(int multiPV)
{
    ensureUIController();
    if (m_uiController) {
        m_uiController->setConsiderationTabManager(m_considerationTabManager);
        m_uiController->setThinkingEngineInfo(m_thinkingInfo1);
        m_uiController->onDialogMultiPVReady(multiPV);
    }
}

void ConsiderationWiring::onMultiPVChangeRequested(int value)
{
    if (m_match && m_playMode && *m_playMode == PlayMode::ConsiderationMode) {
        m_match->updateConsiderationMultiPV(value);
    }
}

void ConsiderationWiring::updateArrows()
{
    ensureUIController();
    if (m_uiController) {
        m_uiController->setShogiView(m_shogiView);
        m_uiController->setConsiderationTabManager(m_considerationTabManager);
        m_uiController->setThinkingEngineInfo(m_thinkingInfo1);
        m_uiController->setConsiderationModel(m_considerationModel);
        if (m_currentSfenStr) {
            m_uiController->setCurrentSfenStr(*m_currentSfenStr);
        }
        m_uiController->updateArrows();
    }
}

void ConsiderationWiring::onShowArrowsChanged(bool checked)
{
    ensureUIController();
    if (m_uiController) {
        m_uiController->onShowArrowsChanged(checked);
    }
}

bool ConsiderationWiring::updatePositionIfNeeded(int row, const QString& newPosition,
                                                  const QList<ShogiMove>* gameMoves,
                                                  KifuRecordListModel* kifuRecordModel)
{
    ensureUIController();
    if (m_uiController) {
        m_uiController->setMatchCoordinator(m_match);
        m_uiController->setConsiderationTabManager(m_considerationTabManager);
        m_uiController->setThinkingEngineInfo(m_thinkingInfo1);
        return m_uiController->updatePositionIfInConsiderationMode(
            row, newPosition, gameMoves, kifuRecordModel);
    }
    return false;
}

void ConsiderationWiring::handleStopRequest()
{
    if (m_match) {
        m_match->stopAnalysisEngine();
    }
}
