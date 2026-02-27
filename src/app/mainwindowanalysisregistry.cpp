/// @file mainwindowanalysisregistry.cpp
/// @brief Analysis系の ensure* 実装

#include "mainwindowanalysisregistry.h"
#include "mainwindow.h"
#include "mainwindowserviceregistry.h"
#include "mainwindowcompositionroot.h"
#include "mainwindowdepsfactory.h"

#include "pvclickcontroller.h"
#include "considerationpositionservice.h"
#include "analysisresultspresenter.h"
#include "considerationwiring.h"
#include "usicommandcontroller.h"
#include "engineanalysistab.h"

MainWindowAnalysisRegistry::MainWindowAnalysisRegistry(MainWindow& mw,
                                                       MainWindowServiceRegistry& registry,
                                                       QObject* parent)
    : QObject(parent)
    , m_mw(mw)
    , m_registry(registry)
{
}

// ---------------------------------------------------------------------------
// 読み筋クリック
// ---------------------------------------------------------------------------

void MainWindowAnalysisRegistry::ensurePvClickController()
{
    if (m_mw.m_pvClickController) return;
    m_mw.m_compositionRoot->ensurePvClickController(m_mw.buildRuntimeRefs(), &m_mw, m_mw.m_pvClickController);

    // 動的状態へのポインタを設定（onPvRowClicked 呼び出し時に自動同期）
    PvClickController::StateRefs stateRefs;
    stateRefs.playMode = &m_mw.m_state.playMode;
    stateRefs.humanName1 = &m_mw.m_player.humanName1;
    stateRefs.humanName2 = &m_mw.m_player.humanName2;
    stateRefs.engineName1 = &m_mw.m_player.engineName1;
    stateRefs.engineName2 = &m_mw.m_player.engineName2;
    stateRefs.currentSfenStr = &m_mw.m_state.currentSfenStr;
    stateRefs.startSfenStr = &m_mw.m_state.startSfenStr;
    stateRefs.currentMoveIndex = &m_mw.m_state.currentMoveIndex;
    stateRefs.considerationModel = &m_mw.m_models.consideration;
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

void MainWindowAnalysisRegistry::ensureConsiderationPositionService()
{
    if (!m_mw.m_considerationPositionService) {
        m_mw.m_considerationPositionService = new ConsiderationPositionService(&m_mw);
    }

    ConsiderationPositionService::Deps deps;
    deps.playMode = &m_mw.m_state.playMode;
    deps.positionStrList = &m_mw.m_kifu.positionStrList;
    deps.gameUsiMoves = &m_mw.m_kifu.gameUsiMoves;
    deps.gameMoves = &m_mw.m_kifu.gameMoves;
    deps.startSfenStr = &m_mw.m_state.startSfenStr;
    deps.kifuLoadCoordinator = m_mw.m_kifuLoadCoordinator;
    deps.kifuRecordModel = m_mw.m_models.kifuRecord;
    deps.branchTree = m_mw.m_branchNav.branchTree;
    deps.navState = m_mw.m_branchNav.navState;
    deps.match = m_mw.m_match;
    deps.analysisTab = m_mw.m_analysisTab;
    m_mw.m_considerationPositionService->updateDeps(deps);
}

// ---------------------------------------------------------------------------
// 解析結果プレゼンター
// ---------------------------------------------------------------------------

void MainWindowAnalysisRegistry::ensureAnalysisPresenter()
{
    if (!m_mw.m_analysisPresenter)
        m_mw.m_analysisPresenter = std::make_unique<AnalysisResultsPresenter>();
}

// ---------------------------------------------------------------------------
// 検討モード配線
// ---------------------------------------------------------------------------

void MainWindowAnalysisRegistry::ensureConsiderationWiring()
{
    if (m_mw.m_considerationWiring) return;

    MainWindowDepsFactory::ConsiderationWiringCallbacks cbs;
    cbs.ensureDialogCoordinator = [this]() {
        m_registry.ensureDialogCoordinator();
        if (m_mw.m_considerationWiring)
            m_mw.m_considerationWiring->setDialogCoordinator(m_mw.m_dialogCoordinator);
    };

    m_mw.m_compositionRoot->ensureConsiderationWiring(m_mw.buildRuntimeRefs(), cbs, &m_mw, m_mw.m_considerationWiring);
}

// ---------------------------------------------------------------------------
// USIコマンドコントローラ
// ---------------------------------------------------------------------------

void MainWindowAnalysisRegistry::ensureUsiCommandController()
{
    if (!m_mw.m_usiCommandController) {
        m_mw.m_usiCommandController = std::make_unique<UsiCommandController>();
    }
    m_mw.m_usiCommandController->setMatchCoordinator(m_mw.m_match);
    m_mw.m_usiCommandController->setAnalysisTab(m_mw.m_analysisTab);
}
