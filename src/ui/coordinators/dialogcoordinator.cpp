/// @file dialogcoordinator.cpp
/// @brief ダイアログコーディネータクラスの実装

#include "dialogcoordinator.h"
#include "appstate.h"
#include "kifubranchtree.h"
#include "kifunavigationstate.h"

#include <QWidget>
#include <QMessageBox>
#include "logcategories.h"

#include "aboutcoordinator.h"
#include "engineregistrationdialog.h"
#include "promotionflow.h"
#include "considerationflowcontroller.h"
#include "tsumesearchflowcontroller.h"
#include "analysissession.h"
#include "shogigamecontroller.h"
#include "kifuanalysislistmodel.h"
#include "considerationtabmanager.h"
#include "usi.h"
#include "usicommlogmodel.h"
#include "shogienginethinkingmodel.h"
#include "kifuloadcoordinator.h"
#include "shogimove.h"
#include "kifurecordlistmodel.h"
#include "considerationpositionresolver.h"
#include "sfenutils.h"
#include "shogiview.h"
#include "analysisflowcontroller.h"
#include "evaluationchartwidget.h"
#include "gameinfopanecontroller.h"

DialogCoordinator::DialogCoordinator(QWidget* parentWidget, QObject* parent)
    : QObject(parent)
    , m_parentWidget(parentWidget)
{
}

DialogCoordinator::~DialogCoordinator() = default;

void DialogCoordinator::updateDeps(const Deps& deps)
{
    m_analysisSession = deps.analysisSession;
    m_logModel = deps.logModel;
    m_thinkingModel = deps.thinkingModel;
    m_analysisModel = deps.analysisModel;
    m_considerationTabManager = deps.considerationTabManager;
}

void DialogCoordinator::showVersionInformation()
{
    AboutCoordinator::showVersionDialog(m_parentWidget);
}

void DialogCoordinator::openProjectWebsite()
{
    AboutCoordinator::openProjectWebsite();
}

void DialogCoordinator::showEngineSettingsDialog()
{
    EngineRegistrationDialog dlg(m_parentWidget);
    dlg.exec();
}

bool DialogCoordinator::showPromotionDialog()
{
    return PromotionFlow::askPromote(m_parentWidget);
}

void DialogCoordinator::showInformation(const QString& title, const QString& message)
{
    QMessageBox::information(m_parentWidget, title, message);
}

bool DialogCoordinator::startConsiderationDirect(const ConsiderationDirectParams& params)
{
    qCDebug(lcUi).noquote() << "startConsiderationDirect: position=" << params.position
                       << "engineIndex=" << params.engineIndex
                       << "unlimitedTime=" << params.unlimitedTime
                       << "byoyomiSec=" << params.byoyomiSec
                       << "multiPV=" << params.multiPV;

    // Flow に一任
    ConsiderationFlowController flow(this);
    ConsiderationFlowController::Deps d;
    d.session = m_analysisSession;
    d.onStarted = [this]() {
        Q_EMIT considerationModeStarted();
    };
    d.onError = [this](const QString& msg) { showFlowError(msg); };
    d.considerationModel = params.considerationModel;
    d.onTimeSettingsReady = [this](bool unlimited, int byoyomiSec) {
        Q_EMIT considerationTimeSettingsReady(unlimited, byoyomiSec);
    };
    d.onMultiPVReady = [this](int multiPV) {
        Q_EMIT considerationMultiPVReady(multiPV);
    };

    ConsiderationFlowController::DirectParams directParams;
    directParams.engineIndex = params.engineIndex;
    directParams.engineName = params.engineName;
    directParams.unlimitedTime = params.unlimitedTime;
    directParams.byoyomiSec = params.byoyomiSec;
    directParams.multiPV = params.multiPV;
    directParams.previousTo = params.previousTo;
    directParams.lastUsiMove = params.lastUsiMove;

    return flow.runDirect(d, directParams, params.position);
}

void DialogCoordinator::showTsumeSearchDialog(const TsumeSearchParams& params)
{
    qCDebug(lcUi).noquote() << "showTsumeSearchDialog: ply=" << params.line.currentPly;

    Q_EMIT tsumeSearchModeStarted();

    // Flow に一任
    TsumeSearchFlowController flow(this);

    TsumeSearchFlowController::Deps d;
    d.session = m_analysisSession;
    d.line = params.line;
    d.fallbackSfen = params.fallbackSfen;
    d.onError = [this](const QString& msg) { showFlowError(msg); };

    const bool started = flow.runWithDialog(d, m_parentWidget);
    if (!started) {
        Q_EMIT tsumeSearchModeEnded();
    }
}

void DialogCoordinator::setConsiderationContext(const ConsiderationContext& ctx)
{
    m_considerationCtx = ctx;
}

bool DialogCoordinator::startConsiderationFromContext()
{
    qCDebug(lcUi).noquote() << "startConsiderationFromContext";

    // エンジンが選択されているかチェック
    if (m_considerationTabManager && m_considerationTabManager->selectedEngineName().isEmpty()) {
        QMessageBox::critical(m_parentWidget, tr("エラー"), tr("将棋エンジンが選択されていません。"));
        return false;
    }

    // 表示中の手順（分岐ツリー）から、検討する局面と直前の手を決める
    const RecordLine line = m_considerationCtx.navState ? m_considerationCtx.navState->currentRecordLine()
                                                        : RecordLine{};
    const AppState* state = m_considerationCtx.state;
    const QString fallbackSfen = !state ? QString()
        : (state->currentSfenStr.isEmpty() ? state->startSfenStr : state->currentSfenStr);
    const auto resolved = ConsiderationPositionResolver::resolve(line, line.currentPly, fallbackSfen);

    // 手番表示は検討する局面の手番に合わせる
    if (m_considerationCtx.gameController && !line.isEmpty()) {
        const QString sfen = line.sfenAt(line.currentPly);
        m_considerationCtx.gameController->setCurrentPlayer(sfenToTurn(sfen.section(QLatin1Char(' '), 1, 1)));
    }

    qCDebug(lcUi).noquote() << "startConsiderationFromContext: position=" << resolved.position.left(50);

    // 検討タブの設定を保存
    if (m_considerationTabManager) {
        m_considerationTabManager->saveConsiderationTabSettings();
    }

    // 検討タブの設定を使用して直接検討を開始
    ConsiderationDirectParams params;
    params.position = resolved.position;
    params.engineIndex = m_considerationTabManager ? m_considerationTabManager->selectedEngineIndex() : 0;
    params.engineName = m_considerationTabManager ? m_considerationTabManager->selectedEngineName() : QString();
    params.unlimitedTime = m_considerationTabManager ? m_considerationTabManager->isUnlimitedTime() : true;
    params.byoyomiSec = m_considerationTabManager ? m_considerationTabManager->byoyomiSec() : 20;
    params.multiPV = m_considerationTabManager ? m_considerationTabManager->considerationMultiPV() : 1;
    params.considerationModel = m_considerationCtx.considerationModel;
    params.previousTo = resolved.previousTo;
    params.lastUsiMove = resolved.lastUsiMove;

    qCDebug(lcUi).noquote() << "startConsiderationFromContext calling startConsiderationDirect"
                      << "engineIndex=" << params.engineIndex
                      << "engineName=" << params.engineName
                      << "unlimitedTime=" << params.unlimitedTime
                      << "byoyomiSec=" << params.byoyomiSec
                      << "multiPV=" << params.multiPV;
    const bool started = startConsiderationDirect(params);
    qCDebug(lcUi).noquote() << "startConsiderationFromContext EXIT";
    return started;
}

void DialogCoordinator::setTsumeSearchContext(const TsumeSearchContext& ctx)
{
    m_tsumeSearchCtx = ctx;
}

void DialogCoordinator::showTsumeSearchDialogFromContext()
{
    qCDebug(lcUi).noquote() << "showTsumeSearchDialogFromContext";

    // 表示中の手順（分岐ツリー）の、表示中の局面を探索する
    TsumeSearchParams params;
    if (m_tsumeSearchCtx.navState) params.line = m_tsumeSearchCtx.navState->currentRecordLine();
    params.fallbackSfen = m_tsumeSearchCtx.state ? m_tsumeSearchCtx.state->startSfenStr : QString();
    showTsumeSearchDialog(params);
}

void DialogCoordinator::showFlowError(const QString& message)
{
    qCWarning(lcUi).noquote() << "Flow error:" << message;
    QMessageBox::warning(m_parentWidget, tr("エラー"), message);
}

// ============================================================
// 棋譜解析の条件・分岐コンテキストの収集と解析フローへの橋渡し
// ============================================================

void DialogCoordinator::showKifuAnalysisDialog(const KifuAnalysisParams& params)
{
    qCDebug(lcUi).noquote() << "showKifuAnalysisDialog: activePly=" << params.activePly;

    Q_EMIT analysisModeStarted();

    // Flow の用意（遅延生成）
    if (!m_analysisFlow) {
        m_analysisFlow = new AnalysisFlowController(this);
        connect(m_analysisFlow, &AnalysisFlowController::analysisPositionPrepared,
                this, &DialogCoordinator::analysisPositionPrepared);
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
    d.line = params.line;
    d.analysisModel = m_analysisModel;
    d.logModel = m_logModel;
    d.thinkingModel = m_thinkingModel;
    d.presenter = params.presenter;  // 結果表示用プレゼンター
    d.activePly = params.activePly;
    d.lineIndex = params.lineIndex;
    d.blackPlayerName = params.blackPlayerName;
    d.whitePlayerName = params.whitePlayerName;
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

    // 表示中の手順（分岐ツリー。分岐を選んでいればその分岐、対局中はライブの手も含む）を解析する。
    // 終局行（投了など）は手順に含まれない。Flow が開始時にコピーするので、ここでの一時的な値でよい
    const RecordLine line = m_kifuAnalysisCtx.navState ? m_kifuAnalysisCtx.navState->currentRecordLine()
                                                       : RecordLine{};
    params.line = line;
    // 表示ラベル（手数番号・分岐記号）はこの画面境界で値にする。
    // 解析中に棋譜欄を切り替えても、解析実行はモデルを参照しない。
    if (const auto* model = m_kifuAnalysisCtx.recordModel) {
        for (int ply = 1; ply <= params.line.moveCount() && ply < model->rowCount(); ++ply) {
            if (const auto* item = model->item(ply)) params.line.entries[ply - 1].displayText = item->currentMove();
        }
    }
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
    if (m_kifuAnalysisCtx.shogiView) {
        params.boardFlipped = m_kifuAnalysisCtx.shogiView->flipMode();
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
