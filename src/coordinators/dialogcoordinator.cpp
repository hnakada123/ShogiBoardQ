#include "dialogcoordinator.h"

#include <QWidget>
#include <QMessageBox>
#include <QDebug>

#include "aboutcoordinator.h"
#include "enginesettingscoordinator.h"
#include "promotionflow.h"
#include "considerationflowcontroller.h"
#include "tsumesearchflowcontroller.h"
#include "analysisflowcontroller.h"
#include "matchcoordinator.h"
#include "shogigamecontroller.h"
#include "kifuanalysislistmodel.h"
#include "engineanalysistab.h"
#include "usi.h"
#include "usicommlogmodel.h"
#include "shogienginethinkingmodel.h"

DialogCoordinator::DialogCoordinator(QWidget* parentWidget, QObject* parent)
    : QObject(parent)
    , m_parentWidget(parentWidget)
{
}

DialogCoordinator::~DialogCoordinator() = default;

// --------------------------------------------------------
// 依存オブジェクトの設定
// --------------------------------------------------------

void DialogCoordinator::setMatchCoordinator(MatchCoordinator* match)
{
    m_match = match;
}

void DialogCoordinator::setGameController(ShogiGameController* gc)
{
    m_gc = gc;
}

void DialogCoordinator::setUsiEngine(Usi* usi)
{
    m_usi = usi;
}

void DialogCoordinator::setLogModel(UsiCommLogModel* logModel)
{
    m_logModel = logModel;
}

void DialogCoordinator::setThinkingModel(ShogiEngineThinkingModel* thinkingModel)
{
    m_thinkingModel = thinkingModel;
}

void DialogCoordinator::setAnalysisModel(KifuAnalysisListModel* model)
{
    m_analysisModel = model;
}

void DialogCoordinator::setAnalysisTab(EngineAnalysisTab* tab)
{
    m_analysisTab = tab;
}

// --------------------------------------------------------
// 情報ダイアログ
// --------------------------------------------------------

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
    EngineSettingsCoordinator::openDialog(m_parentWidget);
}

// --------------------------------------------------------
// ゲームプレイ関連ダイアログ
// --------------------------------------------------------

bool DialogCoordinator::showPromotionDialog()
{
    return PromotionFlow::askPromote(m_parentWidget);
}

void DialogCoordinator::showGameOverMessage(const QString& title, const QString& message)
{
    QMessageBox::information(m_parentWidget, title, message);
}

// --------------------------------------------------------
// 解析関連ダイアログ
// --------------------------------------------------------

void DialogCoordinator::showConsiderationDialog(const ConsiderationParams& params)
{
    qDebug().noquote() << "[DialogCoord] showConsiderationDialog: position=" << params.position;

    Q_EMIT considerationModeStarted();

    // Flow に一任
    ConsiderationFlowController* flow = new ConsiderationFlowController(this);
    ConsiderationFlowController::Deps d;
    d.match = m_match;
    d.onError = [this](const QString& msg) { showFlowError(msg); };

    flow->runWithDialog(d, m_parentWidget, params.position);
}

void DialogCoordinator::showTsumeSearchDialog(const TsumeSearchParams& params)
{
    qDebug().noquote() << "[DialogCoord] showTsumeSearchDialog: currentMoveIndex=" << params.currentMoveIndex;

    Q_EMIT tsumeSearchModeStarted();

    // Flow に一任
    TsumeSearchFlowController* flow = new TsumeSearchFlowController(this);

    TsumeSearchFlowController::Deps d;
    d.match = m_match;
    d.sfenRecord = params.sfenRecord;
    d.startSfenStr = params.startSfenStr;
    d.positionStrList = params.positionStrList;
    d.currentMoveIndex = qMax(0, params.currentMoveIndex);
    d.onError = [this](const QString& msg) { showFlowError(msg); };

    flow->runWithDialog(d, m_parentWidget);
}

void DialogCoordinator::showKifuAnalysisDialog(const KifuAnalysisParams& params)
{
    qDebug().noquote() << "[DialogCoord] showKifuAnalysisDialog: activePly=" << params.activePly;

    Q_EMIT analysisModeStarted();

    // Flow の用意（遅延生成）
    if (!m_analysisFlow) {
        m_analysisFlow = new AnalysisFlowController(this);
    }

    // 依存を詰めて Flow へ一任
    AnalysisFlowController::Deps d;
    d.sfenRecord = params.sfenRecord;
    d.moveRecords = params.moveRecords;
    d.recordModel = params.recordModel;
    d.analysisModel = m_analysisModel;
    d.analysisTab = m_analysisTab;
    d.usi = m_usi;
    d.logModel = m_logModel;
    d.thinkingModel = m_thinkingModel;
    d.gameController = params.gameController;  // 盤面情報取得用
    d.activePly = params.activePly;
    d.displayError = [this](const QString& msg) { showFlowError(msg); };

    m_analysisFlow->runWithDialog(d, m_parentWidget);
}

void DialogCoordinator::stopKifuAnalysis()
{
    qDebug().noquote() << "[DialogCoord] stopKifuAnalysis called";
    if (m_analysisFlow) {
        m_analysisFlow->stop();
    }
}

bool DialogCoordinator::isKifuAnalysisRunning() const
{
    return m_analysisFlow && m_analysisFlow->isRunning();
}

// --------------------------------------------------------
// エラー表示
// --------------------------------------------------------

void DialogCoordinator::showErrorMessage(const QString& message)
{
    QMessageBox::critical(m_parentWidget, tr("Error"), message);
}

void DialogCoordinator::showFlowError(const QString& message)
{
    qWarning().noquote() << "[DialogCoord] Flow error:" << message;
    QMessageBox::warning(m_parentWidget, tr("エラー"), message);
}
