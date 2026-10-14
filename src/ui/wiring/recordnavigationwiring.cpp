/// @file recordnavigationwiring.cpp
/// @brief 棋譜ナビゲーション配線クラスの実装

#include "recordnavigationwiring.h"

#include "boardloadservice.h"
#include "considerationpositionservice.h"
#include "josekiwindowwiring.h"
#include "kifunavigationcoordinator.h"
#include "recordnavigationhandler.h"
#include "turnstatesyncservice.h"
#include "uistatepolicymanager.h"

RecordNavigationWiring::RecordNavigationWiring(QObject* parent)
    : QObject(parent)
{}

void RecordNavigationWiring::wire(const Deps& deps, const WiringTargets& targets)
{
    if (m_handler) return;
    m_handler = new RecordNavigationHandler(this);

    RecordNavigationHandler::Deps hd;
    hd.navState = deps.navState;
    hd.branchTree = deps.branchTree;
    hd.displayCoordinator = deps.displayCoordinator;
    hd.kifuRecordModel = deps.kifuRecordModel;
    hd.shogiView = deps.shogiView;
    hd.evalGraphController = deps.evalGraphController;
    hd.currentSfenStr = deps.currentSfenStr;
    hd.skipBoardSyncForBranchNav = deps.skipBoardSyncForBranchNav;
    hd.csaGameWiring = deps.csaGameWiring;
    hd.playMode = deps.playMode;
    hd.match = deps.match;
    m_handler->updateDeps(hd);

    if (targets.kifuNav) {
        connect(m_handler, &RecordNavigationHandler::boardSyncRequired,
                targets.kifuNav, &KifuNavigationCoordinator::syncBoardAndHighlightsAtRow);
    }
    if (targets.boardLoad) {
        connect(m_handler, &RecordNavigationHandler::branchBoardSyncRequired,
                targets.boardLoad, &BoardLoadService::loadWithHighlights);
    }
    if (targets.uiStatePolicy) {
        connect(m_handler, &RecordNavigationHandler::enableArrowButtonsRequired,
                targets.uiStatePolicy, &UiStatePolicyManager::enableNavigationIfAllowed);
    }
    if (targets.turnSync) {
        connect(m_handler, &RecordNavigationHandler::turnUpdateRequired,
                targets.turnSync, &TurnStateSyncService::setCurrentTurn);
    }
    if (targets.josekiWiring) {
        connect(m_handler, &RecordNavigationHandler::josekiUpdateRequired,
                targets.josekiWiring, &JosekiWindowWiring::updateJosekiWindow);
    }
    if (targets.considerationPosition) {
        connect(m_handler, &RecordNavigationHandler::buildPositionRequired,
                targets.considerationPosition, &ConsiderationPositionService::handleBuildPositionRequired);
    }
}
