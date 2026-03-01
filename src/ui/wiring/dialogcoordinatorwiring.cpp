/// @file dialogcoordinatorwiring.cpp
/// @brief DialogCoordinator の生成・配線・コンテキスト設定の実装

#include "dialogcoordinatorwiring.h"
#include "dialogcoordinator.h"
#include "dialogorchestrator.h"
#include "evaluationchartwidget.h"
#include "engineanalysistab.h"
#include "logcategories.h"

#include <limits>

DialogCoordinatorWiring::DialogCoordinatorWiring(QObject* parent)
    : QObject(parent)
{}

void DialogCoordinatorWiring::ensure(const Deps& deps)
{
    m_evalChartWidget = deps.evalChartWidget;
    m_analysisTab = deps.analysisTab;
    m_playMode = deps.playMode;
    m_navigateKifuViewToRow = deps.navigateKifuViewToRow;

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

    // 解析進捗シグナルを自身のスロットに接続
    connect(m_coordinator, &DialogCoordinator::analysisProgressReported,
            this, &DialogCoordinatorWiring::onKifuAnalysisProgress);

    // 解析結果行選択シグナルを自身のスロットに接続
    connect(m_coordinator, &DialogCoordinator::analysisResultRowSelected,
            this, &DialogCoordinatorWiring::onKifuAnalysisResultRowSelected);

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

void DialogCoordinatorWiring::cancelKifuAnalysis()
{
    qCDebug(lcUi) << "cancelKifuAnalysis called";

    if (m_coordinator) {
        if (m_coordinator->isKifuAnalysisRunning()) {
            m_coordinator->stopKifuAnalysis();

            if (m_playMode) {
                *m_playMode = PlayMode::NotStarted;
            }

            qCDebug(lcUi) << "cancelKifuAnalysis: analysis cancelled";
        } else {
            qCDebug(lcUi) << "cancelKifuAnalysis: no analysis running";
        }
    }
}

void DialogCoordinatorWiring::onKifuAnalysisProgress(int ply, int scoreCp)
{
    qCDebug(lcUi) << "onKifuAnalysisProgress: ply=" << ply << "scoreCp=" << scoreCp;

    // 1) 棋譜欄の該当行をハイライトし、盤面を更新
    if (m_navigateKifuViewToRow) {
        m_navigateKifuViewToRow(ply);
    }

    // 2) 評価値グラフに評価値をプロット（バッチ更新で描画負荷を軽減）
    static constexpr int POSITION_ONLY_MARKER = std::numeric_limits<int>::min();
    if (scoreCp != POSITION_ONLY_MARKER && m_evalChartWidget) {
        m_evalChartWidget->appendScoreP1Buffered(ply, scoreCp, false);
    }
}

void DialogCoordinatorWiring::onKifuAnalysisResultRowSelected(int row)
{
    qCDebug(lcUi) << "onKifuAnalysisResultRowSelected: row=" << row;

    const int ply = row;

    // 1) 棋譜欄の該当行をハイライトし、盤面を更新
    if (m_navigateKifuViewToRow) {
        m_navigateKifuViewToRow(ply);
    }

    // 2) 分岐ツリーの該当手数をハイライト
    if (m_analysisTab) {
        m_analysisTab->highlightBranchTreeAt(/*row=*/0, ply, /*centerOn=*/true);
    }
}
