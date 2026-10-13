/// @file dialogcoordinator_analysis.cpp
/// @brief 棋譜解析の条件・分岐コンテキストの収集と解析フローへの橋渡し

#include "dialogcoordinator.h"
#include "analysisflowcontroller.h"
#include "evaluationchartwidget.h"
#include "gameinfopanecontroller.h"
#include "kifunavigationstate.h"
#include "kifuloadcoordinator.h"
#include "kifunavigationstate.h"
#include "logcategories.h"

void DialogCoordinator::showKifuAnalysisDialog(const KifuAnalysisParams& params)
{
    qCDebug(lcUi).noquote() << "showKifuAnalysisDialog: activePly=" << params.activePly;

    Q_EMIT analysisModeStarted();

    // Flow の用意（遅延生成）
    if (!m_analysisFlow) {
        m_analysisFlow = new AnalysisFlowController(this);
        // シグナル中継（Flow → DialogCoordinator → MainWindow）
        connect(m_analysisFlow, &AnalysisFlowController::analysisStarted,
                this, &DialogCoordinator::analysisStarted);
        QObject::connect(m_analysisFlow, &AnalysisFlowController::analysisProgressReported,
                         this, &DialogCoordinator::analysisProgressReported, Qt::UniqueConnection);
        QObject::connect(m_analysisFlow, &AnalysisFlowController::analysisResultRowSelected,
                         this, &DialogCoordinator::analysisResultRowSelected, Qt::UniqueConnection);
        QObject::connect(m_analysisFlow, &AnalysisFlowController::analysisStopped,
                         this, &DialogCoordinator::analysisModeEnded, Qt::UniqueConnection);
    }

    // 依存を詰めて Flow へ一任
    AnalysisFlowController::Deps d;
    d.sfenRecord = params.sfenRecord;
    d.recordModel = params.recordModel;
    d.analysisModel = m_analysisModel;
    d.usi = m_usi;
    d.logModel = m_logModel;
    d.thinkingModel = m_thinkingModel;
    d.gameController = params.gameController;  // 盤面情報取得用
    d.presenter = params.presenter;  // 結果表示用プレゼンター
    d.activePly = params.activePly;
    d.lineIndex = params.lineIndex;
    d.blackPlayerName = params.blackPlayerName;
    d.whitePlayerName = params.whitePlayerName;
    d.usiMoves = params.usiMoves;
    d.boardFlipped = params.boardFlipped;
    d.displayError = [this](const QString& msg) { showFlowError(msg); };

    if (m_kifuAnalysisCtx.evalChart) {
        // 条件ダイアログのキャンセル時は対局中の評価値を保持する。
        QObject::connect(m_analysisFlow, &AnalysisFlowController::analysisEngineNameChanged,
                         m_kifuAnalysisCtx.evalChart, &EvaluationChartWidget::clearAll,
                         Qt::UniqueConnection);
        QObject::connect(m_analysisFlow, &AnalysisFlowController::analysisEngineNameChanged,
                         m_kifuAnalysisCtx.evalChart, &EvaluationChartWidget::setEngine1Name,
                         Qt::UniqueConnection);
    }
    const bool started = m_analysisFlow->runWithDialog(d, m_parentWidget);
    if (!started) {
        Q_EMIT analysisModeEnded();
    }
}

void DialogCoordinator::setKifuAnalysisContext(const KifuAnalysisContext& ctx)
{
    m_kifuAnalysisCtx = ctx;
}

void DialogCoordinator::showKifuAnalysisDialogFromContext()
{
    qCDebug(lcUi).noquote() << "showKifuAnalysisDialogFromContext";

    // パラメータを構築
    KifuAnalysisParams params;
    params.recordModel = m_kifuAnalysisCtx.recordModel;
    params.gameController = m_kifuAnalysisCtx.gameController;

    // 表示中の手順（分岐ツリー。分岐を選んでいればその分岐、対局中はライブの手も含む）を解析する。
    // 終局行（投了など）は手順に含まれない。Flow が開始時にコピーするので、ここでの一時的な値でよい
    const RecordLine line = m_kifuAnalysisCtx.navState ? m_kifuAnalysisCtx.navState->currentRecordLine()
                                                       : RecordLine{};
    QStringList lineSfens = line.sfens;
    QStringList lineMoves = line.usiMoves();
    params.sfenRecord = &lineSfens;
    params.usiMoves = &lineMoves;
    params.activePly = line.currentPly;
    params.lineIndex = m_kifuAnalysisCtx.navState ? m_kifuAnalysisCtx.navState->currentLineIndex() : 0;

    // 対局者名を取得（GameInfoPaneControllerから）
    if (m_kifuAnalysisCtx.gameInfoController) {
        const QList<KifGameInfoItem> items = m_kifuAnalysisCtx.gameInfoController->gameInfo();
        extractPlayerNames(items, params.blackPlayerName, params.whitePlayerName);
    }

    // プレゼンターを設定
    params.presenter = m_kifuAnalysisCtx.presenter;

    // GUI本体の盤面反転状態を取得
    if (m_kifuAnalysisCtx.getBoardFlipped) {
        params.boardFlipped = m_kifuAnalysisCtx.getBoardFlipped();
    }

    qCDebug(lcUi).noquote() << "showKifuAnalysisDialogFromContext:"
                       << "blackPlayerName=" << params.blackPlayerName
                       << "whitePlayerName=" << params.whitePlayerName;

    showKifuAnalysisDialog(params);
}

void DialogCoordinator::extractPlayerNames(const QList<KifGameInfoItem>& gameInfo,
                                           QString& outBlackName, QString& outWhiteName)
{
    for (const KifGameInfoItem& item : gameInfo) {
        if (item.key == QStringLiteral("先手") || item.key == QStringLiteral("下手")) {
            outBlackName = item.value;
        } else if (item.key == QStringLiteral("後手") || item.key == QStringLiteral("上手")) {
            outWhiteName = item.value;
        }
    }
}

void DialogCoordinator::stopKifuAnalysis()
{
    qCDebug(lcUi).noquote() << "stopKifuAnalysis called";
    if (m_analysisFlow) {
        m_analysisFlow->stop();
    }
}

int DialogCoordinator::analysisLineIndex() const
{
    return m_analysisFlow ? m_analysisFlow->analysisLineIndex() : 0;
}

bool DialogCoordinator::isKifuAnalysisRunning() const
{
    return m_analysisFlow && m_analysisFlow->isRunning();
}
