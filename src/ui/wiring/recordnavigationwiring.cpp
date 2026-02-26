/// @file recordnavigationwiring.cpp
/// @brief 棋譜ナビゲーション配線クラスの実装

#include "recordnavigationwiring.h"
#include "mainwindow.h"
#include "recordnavigationhandler.h"
#include "kifunavigationcoordinator.h"
#include "considerationpositionservice.h"
#include "uistatepolicymanager.h"
#include "mainwindowserviceregistry.h"

RecordNavigationWiring::RecordNavigationWiring(QObject* parent)
    : QObject(parent)
{}

void RecordNavigationWiring::ensure(const Deps& deps)
{
    m_mainWindow = deps.mainWindow;

    const bool firstTime = !m_handler;
    if (firstTime) {
        m_handler = new RecordNavigationHandler(this);
        wireSignals();
    }

    bindDeps(deps);
}

void RecordNavigationWiring::wireSignals()
{
    m_mainWindow->ensureKifuNavigationCoordinator();
    connect(m_handler, &RecordNavigationHandler::boardSyncRequired,
            m_mainWindow->m_kifuNavCoordinator, &KifuNavigationCoordinator::syncBoardAndHighlightsAtRow);
    connect(m_handler, &RecordNavigationHandler::branchBoardSyncRequired,
            m_mainWindow, &MainWindow::loadBoardWithHighlights);
    m_mainWindow->m_registry->ensureUiStatePolicyManager();
    connect(m_handler, &RecordNavigationHandler::enableArrowButtonsRequired,
            m_mainWindow->m_uiStatePolicy, &UiStatePolicyManager::enableNavigationIfAllowed);
    connect(m_handler, &RecordNavigationHandler::turnUpdateRequired,
            m_mainWindow, &MainWindow::setCurrentTurn);
    connect(m_handler, &RecordNavigationHandler::josekiUpdateRequired,
            m_mainWindow, &MainWindow::updateJosekiWindow);
    m_mainWindow->ensureConsiderationPositionService();
    connect(m_handler, &RecordNavigationHandler::buildPositionRequired,
            m_mainWindow->m_considerationPositionService, &ConsiderationPositionService::handleBuildPositionRequired);
}

void RecordNavigationWiring::bindDeps(const Deps& deps)
{
    RecordNavigationHandler::Deps handlerDeps;
    handlerDeps.navState = deps.navState;
    handlerDeps.branchTree = deps.branchTree;
    handlerDeps.displayCoordinator = deps.displayCoordinator;
    handlerDeps.kifuRecordModel = deps.kifuRecordModel;
    handlerDeps.shogiView = deps.shogiView;
    handlerDeps.evalGraphController = deps.evalGraphController;
    handlerDeps.sfenRecord = deps.sfenRecord;
    handlerDeps.activePly = deps.activePly;
    handlerDeps.currentSelectedPly = deps.currentSelectedPly;
    handlerDeps.currentMoveIndex = deps.currentMoveIndex;
    handlerDeps.currentSfenStr = deps.currentSfenStr;
    handlerDeps.skipBoardSyncForBranchNav = deps.skipBoardSyncForBranchNav;
    handlerDeps.csaGameCoordinator = deps.csaGameCoordinator;
    handlerDeps.playMode = deps.playMode;
    handlerDeps.match = deps.match;
    m_handler->updateDeps(handlerDeps);
}
