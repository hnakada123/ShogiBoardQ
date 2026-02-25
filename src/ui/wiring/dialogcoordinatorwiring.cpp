/// @file dialogcoordinatorwiring.cpp
/// @brief DialogCoordinator の生成・配線・コンテキスト設定の実装

#include "dialogcoordinatorwiring.h"
#include "dialogcoordinator.h"
#include "dialogorchestrator.h"
#include "mainwindow.h"

DialogCoordinatorWiring::DialogCoordinatorWiring(QObject* parent)
    : QObject(parent)
{}

void DialogCoordinatorWiring::ensure(const Deps& deps)
{
    m_mainWindow = deps.mainWindow;

    const bool firstTime = !m_coordinator;
    if (firstTime) {
        m_coordinator = new DialogCoordinator(deps.parentWidget, deps.parentWidget);
        m_coordinator->setMatchCoordinator(deps.match);
        m_coordinator->setGameController(deps.gameController);
        wireSignals(deps);
    }

    bindContexts(deps);
}

void DialogCoordinatorWiring::wireSignals(const Deps& deps)
{
    // 検討モード関連シグナルをオーケストレータ経由で接続
    ConsiderationWiring* considerationWiring = deps.getConsiderationWiring();
    DialogOrchestrator::wireConsiderationSignals(m_coordinator, considerationWiring);

    // 解析進捗シグナルを接続
    connect(m_coordinator, &DialogCoordinator::analysisProgressReported,
            m_mainWindow, &MainWindow::onKifuAnalysisProgress);

    // 解析結果行選択シグナルを接続（棋譜欄・将棋盤・分岐ツリー連動用）
    connect(m_coordinator, &DialogCoordinator::analysisResultRowSelected,
            m_mainWindow, &MainWindow::onKifuAnalysisResultRowSelected);

    // UI状態遷移シグナルをオーケストレータ経由で接続
    UiStatePolicyManager* uiStatePolicy = deps.getUiStatePolicyManager();
    DialogOrchestrator::wireUiStateSignals(m_coordinator, uiStatePolicy);
}

void DialogCoordinatorWiring::bindContexts(const Deps& deps)
{
    // 検討コンテキストを設定
    DialogCoordinator::ConsiderationContext conCtx;
    conCtx.gameController = deps.gameController;
    conCtx.gameMoves = deps.gameMoves;
    conCtx.currentMoveIndex = deps.currentMoveIndex;
    conCtx.kifuRecordModel = deps.kifuRecordModel;
    conCtx.sfenRecord = deps.sfenRecord;
    conCtx.startSfenStr = deps.startSfenStr;
    conCtx.currentSfenStr = deps.currentSfenStr;
    conCtx.branchTree = deps.branchTree;
    conCtx.navState = deps.navState;
    conCtx.considerationModel = deps.considerationModel;
    conCtx.gameUsiMoves = deps.gameUsiMoves;
    conCtx.kifuLoadCoordinator = deps.kifuLoadCoordinator;
    m_coordinator->setConsiderationContext(conCtx);

    // 詰み探索コンテキストを設定
    DialogCoordinator::TsumeSearchContext tsumeCtx;
    tsumeCtx.sfenRecord = deps.sfenRecord;
    tsumeCtx.startSfenStr = deps.startSfenStr;
    tsumeCtx.positionStrList = deps.positionStrList;
    tsumeCtx.currentMoveIndex = deps.currentMoveIndex;
    tsumeCtx.gameUsiMoves = deps.gameUsiMoves;
    tsumeCtx.kifuLoadCoordinator = deps.kifuLoadCoordinator;
    m_coordinator->setTsumeSearchContext(tsumeCtx);

    // 棋譜解析コンテキストを設定
    DialogCoordinator::KifuAnalysisContext kifuCtx;
    kifuCtx.sfenRecord = deps.sfenRecord;
    kifuCtx.moveRecords = deps.moveRecords;
    kifuCtx.recordModel = deps.kifuRecordModel;
    kifuCtx.activePly = deps.activePly;
    kifuCtx.gameController = deps.gameController;
    kifuCtx.gameInfoController = deps.gameInfoController;
    kifuCtx.kifuLoadCoordinator = deps.kifuLoadCoordinator;
    kifuCtx.evalChart = deps.evalChart;
    kifuCtx.gameUsiMoves = deps.gameUsiMoves;
    kifuCtx.presenter = deps.presenter;
    kifuCtx.getBoardFlipped = deps.getBoardFlipped;
    m_coordinator->setKifuAnalysisContext(kifuCtx);
}
