/// @file dialogcoordinator_analysis.cpp
/// @brief 棋譜解析の条件・分岐コンテキストの収集と解析フローへの橋渡し

#include "dialogcoordinator.h"
#include "analysisflowcontroller.h"
#include "evaluationchartwidget.h"
#include "gameinfopanecontroller.h"
#include "kifubranchtree.h"
#include "kifuloadcoordinator.h"
#include "kifunavigationstate.h"
#include "logcategories.h"
#include "usimoveconverter.h"

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
    params.sfenRecord = m_kifuAnalysisCtx.sfenRecord;
    params.recordModel = m_kifuAnalysisCtx.recordModel;
    params.activePly = m_kifuAnalysisCtx.activePly ? *m_kifuAnalysisCtx.activePly : 0;
    params.gameController = m_kifuAnalysisCtx.gameController;

    // 読み込んだ棋譜の本譜には終局行（投了など）の局面も入っている。直前と同じ局面なので解析しない。
    // 分岐ツリーの本譜と手数が一致する場合だけ、末尾の終局行を除く。
    QStringList mainSfens;
    if (params.sfenRecord && m_kifuAnalysisCtx.branchTree) {
        const auto lines = m_kifuAnalysisCtx.branchTree->allLines();
        if (!lines.isEmpty() && lines.first().nodes.size() == params.sfenRecord->size()) {
            const auto& nodes = lines.first().nodes;
            qsizetype positions = nodes.size();
            while (positions > 1 && nodes.at(positions - 1) && nodes.at(positions - 1)->isTerminal()) {
                --positions;
            }
            if (positions < params.sfenRecord->size()) {
                mainSfens = params.sfenRecord->mid(0, positions);
                params.sfenRecord = &mainSfens;
            }
        }
    }

    // USI形式の指し手リストを取得（コンテキストから、またはKifuLoadCoordinatorから）
    // 注: コンテキストのusiMovesが空の場合はKifuLoadCoordinatorから取得
    // 重要: sfenRecordとusiMovesの整合性をチェック（sfenRecord.size() == usiMoves.size() + 1）
    const qsizetype sfenSize = params.sfenRecord ? params.sfenRecord->size() : 0;

    if (m_kifuAnalysisCtx.gameUsiMoves && !m_kifuAnalysisCtx.gameUsiMoves->isEmpty()) {
        // 対局時のUSI形式指し手リストを使用
        const qsizetype usiSize = m_kifuAnalysisCtx.gameUsiMoves->size();
        if (sfenSize == usiSize + 1) {
            params.usiMoves = m_kifuAnalysisCtx.gameUsiMoves;
            qCDebug(lcUi).noquote() << "using ctx.gameUsiMoves (from game), size=" << usiSize;
        } else {
            // 整合性がないためusiMovesを使用しない（フォールバックを使用）
            qCDebug(lcUi).noquote() << "ctx.gameUsiMoves size mismatch: sfenSize=" << sfenSize
                               << " usiSize=" << usiSize << " (expected sfenSize == usiSize + 1)";
        }
    } else if (m_kifuAnalysisCtx.kifuLoadCoordinator) {
        // 棋譜読み込み時のUSI形式指し手リストを使用
        QStringList* kifuUsiMoves = m_kifuAnalysisCtx.kifuLoadCoordinator->kifuUsiMovesPtr();
        const qsizetype usiSize = kifuUsiMoves ? kifuUsiMoves->size() : 0;
        if (sfenSize == usiSize + 1) {
            params.usiMoves = kifuUsiMoves;
            qCDebug(lcUi).noquote() << "using kifuLoadCoordinator->kifuUsiMovesPtr(), size=" << usiSize;
        } else {
            // 整合性がないためusiMovesを使用しない（フォールバックを使用）
            qCDebug(lcUi).noquote() << "kifuUsiMoves size mismatch: sfenSize=" << sfenSize
                               << " usiSize=" << usiSize << " (expected sfenSize == usiSize + 1)";
        }
    }
    qCDebug(lcUi).noquote() << "showKifuAnalysisDialogFromContext: params.usiMoves="
                       << params.usiMoves << " size=" << (params.usiMoves ? params.usiMoves->size() : -1);

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

    // 分岐選択中は、本譜の履歴ではなく表示中のラインを解析する。
    // Flow が開始時にコピーするので、条件ダイアログのキャンセルは前回結果に影響しない。
    QStringList branchSfens;
    QStringList branchMoves;
    if (m_kifuAnalysisCtx.branchTree && m_kifuAnalysisCtx.navState) {
        const int lineIndex = m_kifuAnalysisCtx.navState->currentLineIndex();
        const auto lines = m_kifuAnalysisCtx.branchTree->allLines();
        if (lineIndex > 0 && lineIndex < lines.size()) {
            for (const auto* node : lines.at(lineIndex).nodes) {
                if (!node || node->isTerminal() || node->sfen().isEmpty()) break;
                branchSfens.append(node->sfen());
            }
            if (!branchSfens.isEmpty()) {
                branchMoves = UsiMoveConverter::fromSfenRecord(branchSfens);
                params.sfenRecord = &branchSfens;
                params.usiMoves = &branchMoves;
                params.lineIndex = lineIndex;
            }
        }
    }
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
