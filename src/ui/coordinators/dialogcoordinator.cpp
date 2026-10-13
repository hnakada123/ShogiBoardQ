/// @file dialogcoordinator.cpp
/// @brief ダイアログコーディネータクラスの実装

#include "dialogcoordinator.h"
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
#include "matchcoordinator.h"
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

DialogCoordinator::DialogCoordinator(QWidget* parentWidget, QObject* parent)
    : QObject(parent)
    , m_parentWidget(parentWidget)
{
}

DialogCoordinator::~DialogCoordinator() = default;

void DialogCoordinator::updateDeps(const Deps& deps)
{
    m_match = deps.matchCoordinator;
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

void DialogCoordinator::showGameOverMessage(const QString& title, const QString& message)
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
    d.match = m_match;
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
    directParams.previousFileTo = params.previousFileTo;
    directParams.previousRankTo = params.previousRankTo;
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
    d.match = m_match;
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
    const QString fallbackSfen = (m_considerationCtx.currentSfenStr && !m_considerationCtx.currentSfenStr->isEmpty())
        ? *m_considerationCtx.currentSfenStr
        : (m_considerationCtx.startSfenStr ? *m_considerationCtx.startSfenStr : QString());
    const auto resolved = ConsiderationPositionResolver::resolve(line, line.currentPly, fallbackSfen);

    // 手番表示は検討する局面の手番に合わせる
    if (m_considerationCtx.gameController && !line.isEmpty()) {
        const QString sfen = line.sfens.value(line.currentPly);
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
    params.previousFileTo = resolved.previousFileTo;
    params.previousRankTo = resolved.previousRankTo;
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
    params.fallbackSfen = m_tsumeSearchCtx.startSfenStr ? *m_tsumeSearchCtx.startSfenStr : QString();
    showTsumeSearchDialog(params);
}

void DialogCoordinator::showFlowError(const QString& message)
{
    qCWarning(lcUi).noquote() << "Flow error:" << message;
    QMessageBox::warning(m_parentWidget, tr("エラー"), message);
}
