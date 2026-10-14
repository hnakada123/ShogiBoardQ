/// @file dialogcoordinatorwiring.cpp
/// @brief DialogCoordinator の生成・配線・コンテキスト設定の実装

#include "dialogcoordinatorwiring.h"
#include "dialogcoordinator.h"
#include "dialogorchestrator.h"
#include "evaluationchartwidget.h"
#include "engineanalysistab.h"
#include "kifunavigationcontroller.h"
#include "logcategories.h"

#include <limits>

DialogCoordinatorWiring::DialogCoordinatorWiring(QObject* parent)
    : QObject(parent)
{}

void DialogCoordinatorWiring::wire(const Deps& deps)
{
    if (m_coordinator) return;

    m_evalChartWidget = deps.evalChart;
    m_analysisTab = deps.analysisTab;
    m_playMode = deps.playMode;
    m_kifuNavController = deps.kifuNavController;

    m_coordinator = new DialogCoordinator(deps.parentWidget, deps.parentWidget);
    wireSignals(deps);

    DialogCoordinator::Deps dcDeps;
    dcDeps.matchCoordinator = deps.match;
    dcDeps.considerationTabManager = deps.analysisTab ? deps.analysisTab->considerationTabManager() : nullptr;
    m_coordinator->updateDeps(dcDeps);
    bindContexts(deps);
}

void DialogCoordinatorWiring::wireSignals(const Deps& deps)
{
    // 検討モード関連シグナルをオーケストレータ経由で接続
    DialogOrchestrator::wireConsiderationSignals(m_coordinator, deps.considerationWiring);

    // 解析進捗シグナルを自身のスロットに接続
    connect(m_coordinator, &DialogCoordinator::analysisProgressReported,
            this, &DialogCoordinatorWiring::onKifuAnalysisProgress);

    // 解析結果行選択シグナルを自身のスロットに接続
    connect(m_coordinator, &DialogCoordinator::analysisResultRowSelected,
            this, &DialogCoordinatorWiring::onKifuAnalysisResultRowSelected);

    connect(m_coordinator, &DialogCoordinator::analysisStarted,
            this, &DialogCoordinatorWiring::onKifuAnalysisStarted);
    if (m_evalChartWidget) {
        connect(m_evalChartWidget, &EvaluationChartWidget::analysisPlyClicked,
                this, &DialogCoordinatorWiring::onAnalysisPositionSelected, Qt::UniqueConnection);
    }

    // UI状態遷移シグナルをオーケストレータ経由で接続
    DialogOrchestrator::wireUiStateSignals(m_coordinator, deps.uiStatePolicy);
}

void DialogCoordinatorWiring::bindContexts(const Deps& deps)
{
    // 検討コンテキストを設定
    DialogCoordinator::ConsiderationContext conCtx;
    conCtx.gameController = deps.gameController;
    conCtx.startSfenStr = deps.startSfenStr;
    conCtx.currentSfenStr = deps.currentSfenStr;
    conCtx.navState = deps.navState;
    conCtx.considerationModel = deps.considerationModel;
    m_coordinator->setConsiderationContext(conCtx);

    // 詰み探索コンテキストを設定
    DialogCoordinator::TsumeSearchContext tsumeCtx;
    tsumeCtx.navState = deps.navState;
    tsumeCtx.startSfenStr = deps.startSfenStr;
    m_coordinator->setTsumeSearchContext(tsumeCtx);

    // 棋譜解析コンテキストを設定
    DialogCoordinator::KifuAnalysisContext kifuCtx;
    kifuCtx.recordModel = deps.kifuRecordModel;
    kifuCtx.navState = deps.navState;
    kifuCtx.gameController = deps.gameController;
    kifuCtx.gameInfoController = deps.gameInfoController;
    kifuCtx.evalChart = deps.evalChart;
    kifuCtx.presenter = deps.presenter;
    kifuCtx.shogiView = deps.shogiView;
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

void DialogCoordinatorWiring::onKifuAnalysisProgress(int ply, int scoreCp, const QString& mate)
{
    qCDebug(lcUi) << "onKifuAnalysisProgress: ply=" << ply << "scoreCp=" << scoreCp;

    // 1) 棋譜欄の該当行をハイライトし、盤面を更新
    onAnalysisPositionSelected(m_coordinator ? m_coordinator->analysisLineIndex() : 0, ply);

    // 2) 評価値グラフに評価値をプロット（バッチ更新で描画負荷を軽減）
    static constexpr int POSITION_ONLY_MARKER = std::numeric_limits<int>::min();
    if (scoreCp != POSITION_ONLY_MARKER && m_evalChartWidget) {
        m_evalChartWidget->appendScoreP1Buffered(ply, scoreCp, false, mate);
    }
}

void DialogCoordinatorWiring::onKifuAnalysisResultRowSelected(int row)
{
    qCDebug(lcUi) << "onKifuAnalysisResultRowSelected: row=" << row;

    const int ply = row;

    // 1) 棋譜欄の該当行をハイライトし、盤面を更新
    onAnalysisPositionSelected(m_coordinator ? m_coordinator->analysisLineIndex() : 0, ply);

}

void DialogCoordinatorWiring::onKifuAnalysisStarted(int lineIndex)
{
    if (m_evalChartWidget) m_evalChartWidget->setAnalysisLineIndex(lineIndex);
}

void DialogCoordinatorWiring::onAnalysisPositionSelected(int lineIndex, int ply)
{
    if (m_kifuNavController) m_kifuNavController->goToLine(lineIndex, ply);
}
