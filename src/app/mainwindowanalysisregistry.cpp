/// @file mainwindowanalysisregistry.cpp
/// @brief Analysis系（解析・検討モード・エンジン連携）の ensure* 実装
///
/// MainWindowServiceRegistry のメソッドを実装する分割ファイル。

#include "mainwindowserviceregistry.h"
#include "kifunavigationstate.h"
#include "mainwindow.h"

#include "kifuloadcoordinator.h"            // IWYU pragma: keep (QPointer の完全型)
#include "pvclickcontroller.h"
#include "playmodepolicyservice.h"
#include "considerationpositionservice.h"
#include "considerationwiring.h"
#include "usicommandcontroller.h"
#include "engineanalysistab.h"
#include "candidatearrowcontroller.h"

// ---------------------------------------------------------------------------
// 読み筋クリック
// ---------------------------------------------------------------------------

void MainWindowServiceRegistry::ensurePvClickController()
{
    if (m_mw.m_isShuttingDown) return;
    // Lifetime: owned by MainWindow (QObject parent=&m_mw)
    // Created: once on first use, never recreated
    if (!m_mw.m_pvClickController) {
        m_mw.m_pvClickController = new PvClickController(&m_mw);
    }
    m_mw.m_pvClickController->setThinkingModels(m_mw.m_models.thinking1, m_mw.m_models.thinking2);
    m_mw.m_pvClickController->setConsiderationModel(m_mw.m_models.consideration);
    m_mw.m_pvClickController->setLogModels(m_mw.m_models.commLog1, m_mw.m_models.commLog2);

    // 動的状態へのポインタを設定（onPvRowClicked 呼び出し時に自動同期）
    PvClickController::StateRefs stateRefs;
    stateRefs.playMode = &m_mw.m_state.playMode;
    stateRefs.humanName1 = &m_mw.m_player.humanName1;
    stateRefs.humanName2 = &m_mw.m_player.humanName2;
    stateRefs.engineName1 = &m_mw.m_player.engineName1;
    stateRefs.engineName2 = &m_mw.m_player.engineName2;
    stateRefs.currentSfenStr = &m_mw.m_state.currentSfenStr;
    stateRefs.startSfenStr = &m_mw.m_state.startSfenStr;
    stateRefs.currentLine = [this]() {
        return m_mw.m_branchNav.navState ? m_mw.m_branchNav.navState->currentRecordLine() : RecordLine{};
    };
    m_mw.m_pvClickController->setStateRefs(stateRefs);
    m_mw.m_pvClickController->setShogiView(m_mw.m_shogiView);

    if (m_mw.m_analysisTab) {
        connect(m_mw.m_pvClickController, &PvClickController::pvDialogClosed,
                m_mw.m_analysisTab, &EngineAnalysisTab::clearThinkingViewSelection, Qt::UniqueConnection);
    }
}

// ---------------------------------------------------------------------------
// 検討局面サービス
// ---------------------------------------------------------------------------

void MainWindowServiceRegistry::ensureConsiderationPositionService()
{
    if (m_mw.m_isShuttingDown) return;
    if (!m_mw.m_considerationPositionService) {
        // Lifetime: owned by MainWindow (QObject parent=&m_mw)
        // Created: once on first use, never recreated
        m_mw.m_considerationPositionService = new ConsiderationPositionService(&m_mw);
    }

    ConsiderationPositionService::Deps deps;
    deps.playMode = &m_mw.m_state.playMode;
    deps.startSfenStr = &m_mw.m_state.startSfenStr;
    deps.navState = m_mw.m_branchNav.navState;
    deps.match = m_mw.m_match;
    deps.analysisTab = m_mw.m_analysisTab;
    m_mw.m_considerationPositionService->updateDeps(deps);
}

// ---------------------------------------------------------------------------
// 検討モード配線
// ---------------------------------------------------------------------------

void MainWindowServiceRegistry::ensureConsiderationWiring()
{
    if (m_mw.m_isShuttingDown) return;

    ConsiderationWiring::Deps deps;
    deps.parentWidget = &m_mw;
    deps.considerationTabManager = m_mw.m_analysisTab ? m_mw.m_analysisTab->considerationTabManager() : nullptr;
    deps.thinkingInfo1 = m_mw.m_analysisTab ? m_mw.m_analysisTab->info1() : nullptr;
    deps.shogiView = m_mw.m_shogiView;
    deps.match = m_mw.m_match;
    deps.dialogCoordinator = m_mw.m_dialogCoordinator;
    deps.considerationModel = m_mw.m_models.consideration;
    deps.commLogModel = m_mw.m_models.commLog1;
    deps.playMode = &m_mw.m_state.playMode;
    deps.currentSfenStr = &m_mw.m_state.currentSfenStr;
    deps.ensureDialogCoordinator = [this]() {
        ensureDialogCoordinator();
        if (m_mw.m_considerationWiring)
            m_mw.m_considerationWiring->setDialogCoordinator(m_mw.m_dialogCoordinator);
    };

    if (!m_mw.m_considerationWiring) {
        // Lifetime: owned by MainWindow (QObject parent=&m_mw)
        // Created: once on first use, never recreated
        m_mw.m_considerationWiring = new ConsiderationWiring(deps, &m_mw);
    } else {
        m_mw.m_considerationWiring->updateDeps(deps);
    }
}

// ---------------------------------------------------------------------------
// USIコマンドコントローラ
// ---------------------------------------------------------------------------

void MainWindowServiceRegistry::ensureUsiCommandController()
{
    if (m_mw.m_isShuttingDown) return;
    if (!m_mw.m_usiCommandController) {
        m_mw.m_usiCommandController = std::make_unique<UsiCommandController>();
    }
    // 司令塔は起動の途中で作られるので、送信のたびに引く（作った後は変わらない）
    m_mw.m_usiCommandController->setMatchProvider([this]() { return m_mw.m_match; });
    m_mw.m_usiCommandController->setAnalysisTab(m_mw.m_analysisTab);
    if (m_mw.m_analysisTab && m_mw.m_shogiView) {
        m_mw.m_analysisTab->setArrowController(CandidateArrowController::forView(m_mw.m_shogiView));
    }
}
