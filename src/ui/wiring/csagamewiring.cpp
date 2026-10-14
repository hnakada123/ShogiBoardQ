/// @file csagamewiring.cpp
/// @brief CSA通信対局の配線と画面への反映を担当するクラスの実装

#include "csagamewiring.h"

#include "boardinteractioncontroller.h"
#include "boardsetupcontroller.h"
#include "csagamedialog.h"
#include "csamoveconverter.h"
#include "csawaitingdialog.h"
#include "engineanalysistab.h"
#include "gamerecordupdateservice.h"
#include "logcategories.h"
#include "shogiview.h"
#include "timecontrolcontroller.h"
#include "uinotificationservice.h"
#include "uistatepolicymanager.h"

#include <QStatusBar>

CsaGameWiring::CsaGameWiring(const Dependencies& deps, QObject* parent)
    : QObject(parent)
    , m_coordinator(deps.coordinator)
    , m_gameController(deps.gameController)
    , m_shogiView(deps.shogiView)
    , m_boardController(deps.boardController)
    , m_statusBar(deps.statusBar)
    , m_sfenHistory(deps.sfenRecord)
    , m_gameMoves(deps.gameMoves)
    , m_analysisTab(deps.analysisTab)
    , m_boardSetupController(deps.boardSetupController)
    , m_usiCommLog(deps.usiCommLog)
    , m_engineThinking(deps.engineThinking)
    , m_timeController(deps.timeController)
    , m_playMode(deps.playMode)
    , m_parentWidget(deps.parentWidget)
    , m_prepareRecord(deps.prepareRecord)
    , m_recordGameEnd(deps.recordGameEnd)
    , m_saveKifuToPath(deps.saveKifuToPath)
    , m_setBoardFlipped(deps.setBoardFlipped)
{
}

// ============================================================
// 配線
// ============================================================

void CsaGameWiring::setCoordinator(CsaGameCoordinator* coordinator)
{
    if (m_coordinator == coordinator) return;
    unwire();
    m_coordinator = coordinator;
}

void CsaGameWiring::wire()
{
    CsaGameCoordinator* const c = m_coordinator;
    if (!c) {
        qCWarning(lcUi) << "wire: coordinator is null";
        return;
    }

    // 画面の切り替え（開始・終局・手番・ログのステータスバー表示）
    connect(c, &CsaGameCoordinator::gameStarted, this, &CsaGameWiring::onGameStarted, Qt::UniqueConnection);
    connect(c, &CsaGameCoordinator::gameEnded, this, &CsaGameWiring::onGameEnded, Qt::UniqueConnection);
    connect(c, &CsaGameCoordinator::turnChanged, this, &CsaGameWiring::onTurnChanged, Qt::UniqueConnection);
    connect(c, &CsaGameCoordinator::logMessage, this, &CsaGameWiring::onLogMessage, Qt::UniqueConnection);
    connect(c, &CsaGameCoordinator::moveAppliedToBoard,
            this, &CsaGameWiring::moveAppliedToBoard, Qt::UniqueConnection);

    // 司令塔のシグナルを実際の受け手へ直接つなぐ
    if (m_recordService) {
        connect(c, &CsaGameCoordinator::recordEntryReady,
                m_recordService, &GameRecordUpdateService::appendRecord, Qt::UniqueConnection);
        connect(c, &CsaGameCoordinator::recordEntryReady,
                m_recordService, &GameRecordUpdateService::updateCurrentSfenAfterMove, Qt::UniqueConnection);
    }
    if (m_boardController) {
        connect(c, &CsaGameCoordinator::moveHighlightRequested,
                m_boardController, &BoardInteractionController::showMoveHighlights, Qt::UniqueConnection);
    }
    if (m_notifService) {
        connect(c, &CsaGameCoordinator::errorOccurred,
                m_notifService, &UiNotificationService::displayErrorMessage, Qt::UniqueConnection);
    }
    if (m_analysisTab) {
        connect(c, &CsaGameCoordinator::connectionStateChanged,
                m_analysisTab, &EngineAnalysisTab::setCsaConnected, Qt::UniqueConnection);
        connect(c, &CsaGameCoordinator::csaCommLogAppended,
                m_analysisTab, &EngineAnalysisTab::appendCsaLog, Qt::UniqueConnection);
        connect(m_analysisTab, &EngineAnalysisTab::csaRawCommandRequested,
                c, &CsaGameCoordinator::sendRawCommand, Qt::UniqueConnection);
    }
    if (m_boardSetupController) {
        connect(m_boardSetupController, &BoardSetupController::csaMoveRequested,
                c, &CsaGameCoordinator::onHumanMove, Qt::UniqueConnection);
    }
}

void CsaGameWiring::unwire()
{
    CsaGameCoordinator* const c = m_coordinator;
    if (!c) return;
    disconnect(c, nullptr, this, nullptr);
    if (m_recordService) disconnect(c, nullptr, m_recordService, nullptr);
    if (m_boardController) disconnect(c, nullptr, m_boardController, nullptr);
    if (m_notifService) disconnect(c, nullptr, m_notifService, nullptr);
    if (m_analysisTab) {
        disconnect(c, nullptr, m_analysisTab, nullptr);
        disconnect(m_analysisTab, nullptr, c, nullptr);
    }
    if (m_boardSetupController) disconnect(m_boardSetupController, nullptr, c, nullptr);
}

void CsaGameWiring::wireExternalSignals(UiStatePolicyManager* uiPolicy,
                                        GameRecordUpdateService* recordService,
                                        UiNotificationService* notifService)
{
    unwire();
    m_uiPolicy = uiPolicy;
    m_recordService = recordService;
    m_notifService = notifService;
    if (m_coordinator) wire();
}

void CsaGameWiring::setAnalysisTab(EngineAnalysisTab* tab)
{
    if (m_analysisTab == tab) return;
    unwire();
    m_analysisTab = tab;
    if (m_coordinator) wire();
}

void CsaGameWiring::setBoardSetupController(BoardSetupController* controller)
{
    if (m_boardSetupController == controller) return;
    unwire();
    m_boardSetupController = controller;
    if (m_coordinator) wire();
}

// ============================================================
// 対局の開始
// ============================================================

bool CsaGameWiring::startCsaGame(CsaGameDialog* dialog, QWidget* parent)
{
    if (!dialog) {
        qCWarning(lcUi) << "startCsaGame: dialog is null";
        return false;
    }

    // 通信対局の司令塔は最初の通信対局で作る（ローカル対局しか使わない起動では作らない）
    if (!m_coordinator) {
        m_coordinator = new CsaGameCoordinator(this);
        CsaGameCoordinator::Dependencies deps;
        deps.gameController = m_gameController;
        deps.view = m_shogiView;
        deps.clock = m_timeController ? m_timeController->clock() : nullptr;
        deps.sfenRecord = m_sfenHistory;
        deps.gameMoves = m_gameMoves;
        deps.usiCommLog = m_usiCommLog;
        deps.engineThinking = m_engineThinking;
        m_coordinator->setDependencies(deps);
        wire();
    }

    // 対局開始オプションを設定（詳細設定もここで控える）
    const CsaGameCoordinator::StartOptions options = buildStartOptions(dialog);
    setPlayMode(PlayMode::CsaNetworkMode);

    CsaWaitingDialog waitingDialog(m_coordinator, parent);
    connect(&waitingDialog, &CsaWaitingDialog::cancelRequested,
            this, &CsaGameWiring::onWaitingCancelled);

    // 対局を開始（シグナルがCsaWaitingDialogに届くようになった後に開始）
    m_coordinator->startGame(options);

    // 待機ダイアログを表示（対局開始またはエラーまでブロック）
    const int result = waitingDialog.exec();
    const bool started = (result == QDialog::Accepted && m_coordinator
                          && m_coordinator->gameState() == CsaGameCoordinator::GameState::InGame);
    if (!started) {
        setPlayMode(PlayMode::NotStarted);
        if (m_coordinator && m_coordinator->gameState() != CsaGameCoordinator::GameState::Idle) {
            m_coordinator->stopGame();
        }
    }
    return started;
}

void CsaGameWiring::onWaitingCancelled()
{
    qCDebug(lcUi) << "onWaitingCancelled: cancelled by user";
    if (m_coordinator) m_coordinator->stopGame();
    setPlayMode(PlayMode::NotStarted);
    if (m_statusBar) m_statusBar->showMessage(tr("通信対局をキャンセルしました"), 3000);
}

// ============================================================
// 司令塔からの通知
// ============================================================

void CsaGameWiring::onGameStarted(const QString& blackName, const QString& whiteName,
                                  const QList<RecordEntry>& initialRecords)
{
    qCDebug(lcUi) << "onGameStarted:" << blackName << "vs" << whiteName;

    // 終局後も同じ接続で次の対局が始まるので、盤面入力の振り分けを通信対局に戻す
    setPlayMode(PlayMode::CsaNetworkMode);
    applyAutoFlip();
    // 対局情報と棋譜の書き出しに使う持ち時間と開始日時を、サーバーの対局条件で置き換える
    if (m_timeController && m_coordinator) {
        m_timeController->beginGameWithTimeControl(m_coordinator->gameSummary().timeControl());
    }
    if (m_prepareRecord && m_coordinator) m_prepareRecord(m_coordinator->startSfen(), blackName, whiteName);
    if (m_uiPolicy) m_uiPolicy->transitionToDuringCsaGame();

    if (m_shogiView) {
        m_shogiView->setBlackPlayerName(QStringLiteral("▲") + blackName);
        m_shogiView->setWhitePlayerName(QStringLiteral("▽") + whiteName);
    }

    // 再開局面の既存手順を棋譜へ入れる（以後の着手は司令塔の recordEntryReady が直接届く）
    if (m_recordService) {
        for (const RecordEntry& entry : initialRecords) m_recordService->appendRecord(entry);
        m_recordService->updateCurrentSfenAfterMove();
    }

    if (m_shogiView) m_shogiView->update();
    if (m_statusBar) m_statusBar->showMessage(tr("CSA通信対局開始: %1 vs %2").arg(blackName, whiteName), 5000);
}

void CsaGameWiring::onGameEnded(CsaClient::GameResult result, CsaClient::GameEndCause cause, int consumedTimeMs)
{
    const QString resultText = CsaMoveConverter::gameResultToString(result);
    const QString causeText = CsaMoveConverter::gameEndCauseToString(cause);
    qCDebug(lcUi) << "onGameEnded:" << resultText << "(" << causeText << ")" << "consumedTimeMs=" << consumedTimeMs;

    // 終局行は司令塔が棋譜へ渡し済み。終局日時を入れてから、次の対局が始まる前に書き出す
    if (m_recordGameEnd) m_recordGameEnd();
    autoSaveRecord();

    setPlayMode(PlayMode::NotStarted);
    if (m_uiPolicy) m_uiPolicy->transitionToIdle();
    showGameEndDialog(tr("対局終了"), tr("対局が終了しました。\n\n結果: %1\n原因: %2").arg(resultText, causeText));
    if (m_statusBar) m_statusBar->showMessage(tr("対局終了: %1 (%2)").arg(resultText, causeText), 5000);
}

void CsaGameWiring::onTurnChanged(bool isMyTurn)
{
    if (!m_coordinator || !m_statusBar) return;
    const bool isBlackTurn = m_coordinator->isBlackSide() ? isMyTurn : !isMyTurn;
    m_statusBar->showMessage(isBlackTurn ? tr("先手番") : tr("後手番"), 2000);
}

void CsaGameWiring::onLogMessage(const QString& message, bool isError)
{
    if (isError) qCWarning(lcUi) << message;
    else qCDebug(lcUi) << message;
    if (m_statusBar) m_statusBar->showMessage(message, 3000);
}
