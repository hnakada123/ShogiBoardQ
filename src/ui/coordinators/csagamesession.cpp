/// @file csagamesession.cpp
/// @brief CSA 通信対局の開始と、通信対局の進行の画面への反映の実装

#include "csagamesession.h"

#include "csagamecoordinator.h"
#include "csagamedialog.h"
#include "csawaitingdialog.h"
#include "kifufilecontroller.h"
#include "logcategories.h"
#include "mainwindowappearancecontroller.h"
#include "notificationpresenter.h"
#include "shogiview.h"

#include <QStatusBar>

CsaGameSession::CsaGameSession(const Deps& deps, QObject* parent)
    : QObject(parent)
    , m_deps(deps)
{
    Q_ASSERT(m_deps.coordinator);
    CsaGameCoordinator* const c = m_deps.coordinator;

    // 対局の開始・終局・手番・ログの反映
    connect(c, &CsaGameCoordinator::gameStarted, this, &CsaGameSession::onGameStarted);
    connect(c, &CsaGameCoordinator::gameEnded, this, &CsaGameSession::onGameEnded);
    connect(c, &CsaGameCoordinator::turnChanged, this, &CsaGameSession::onTurnChanged);
    connect(c, &CsaGameCoordinator::logMessage, this, &CsaGameSession::onLogMessage);
}

// ============================================================
// 対局の開始
// ============================================================

void CsaGameSession::openGameDialog()
{
    if (!m_gameDialog) m_gameDialog = new CsaGameDialog(m_deps.parentWidget);
    m_gameDialog->refreshChoices();
    if (m_gameDialog->exec() != QDialog::Accepted) return;
    if (m_deps.kifuFiles && !m_deps.kifuFiles->confirmDiscardUnsaved()) return;
    // 待機ダイアログを出して対局が始まるまで待つ（始まった後はこのクラスが進める）
    startGame(m_gameDialog->selection());
}

bool CsaGameSession::startGame(const CsaGameDialog::Selection& selection)
{
    const CsaGameCoordinator::StartOptions& options = selection.start;
    m_autoFlipBoard = selection.autoFlipBoard;

    CsaGameCoordinator* const c = m_deps.coordinator;
    CsaWaitingDialog waitingDialog(c, m_deps.parentWidget);
    connect(&waitingDialog, &CsaWaitingDialog::cancelRequested, this, &CsaGameSession::onWaitingCancelled);

    // 待機ダイアログがシグナルを受け取れるようになってから始める
    c->startGame(options);
    // 対局開始またはエラーまで待つ
    const bool started = waitingDialog.exec() == QDialog::Accepted
        && c->gameState() == CsaGameCoordinator::GameState::InGame;
    if (!started && c->gameState() != CsaGameCoordinator::GameState::Idle) c->stopGame();
    return started;
}

void CsaGameSession::onWaitingCancelled()
{
    qCDebug(lcUi) << "onWaitingCancelled: cancelled by user";
    m_deps.coordinator->stopGame();
    showStatus(tr("通信対局をキャンセルしました"), 3000);
}

// ============================================================
// 司令塔からの通知
// ============================================================

void CsaGameSession::onGameStarted(const QString& blackName, const QString& whiteName)
{
    qCDebug(lcUi) << "onGameStarted:" << blackName << "vs" << whiteName;
    CsaGameCoordinator* const c = m_deps.coordinator;

    // 後手のときは自分側から見る（通常の対局と同じ経路で反転し、評価値グラフや名前表示の向きも揃える）
    if (m_autoFlipBoard && m_deps.appearance && m_deps.shogiView
        && m_deps.shogiView->flipMode() != !c->isBlackSide()) {
        m_deps.appearance->flipBoard();
    }
    // 棋譜・対局者の表示と動作状態は、司令塔がこの知らせの前に整えている（同じ接続での次局もここを通る）
    showStatus(tr("CSA通信対局開始: %1 vs %2").arg(blackName, whiteName), 5000);
}

void CsaGameSession::onGameEnded(CsaClient::GameResult result, CsaClient::GameEndCause cause, int consumedTimeMs)
{
    const QString resultText = CsaClient::gameResultText(result);
    const QString causeText = CsaClient::gameEndCauseText(cause);
    qCDebug(lcUi) << "onGameEnded:" << resultText << "(" << causeText << ")" << "consumedTimeMs=" << consumedTimeMs;

    // 棋譜の確定・終了日時・自動保存と待機への切り替えは、司令塔がこの知らせの前に済ませている
    showGameEndDialog(tr("対局終了"), tr("対局が終了しました。\n\n結果: %1\n原因: %2").arg(resultText, causeText));
    showStatus(tr("対局終了: %1 (%2)").arg(resultText, causeText), 5000);
}

void CsaGameSession::onTurnChanged(bool isMyTurn)
{
    const bool isBlackTurn = m_deps.coordinator->isBlackSide() ? isMyTurn : !isMyTurn;
    showStatus(isBlackTurn ? tr("先手番") : tr("後手番"), 2000);
}

void CsaGameSession::onLogMessage(const QString& message, bool isError)
{
    if (isError) qCWarning(lcUi) << message;
    else qCDebug(lcUi) << message;
    showStatus(message, 3000);
}

// ============================================================
// 表示
// ============================================================

void CsaGameSession::showGameEndDialog(const QString& title, const QString& message)
{
    // 対局を続けるときは、結果の表示が次の対局の開始を止めないようにする
    NotificationPresenter::information(m_deps.parentWidget, title, message,
                                       m_deps.coordinator->moreGamesWanted() ? NotificationPresenter::Wait::No
                                                                             : NotificationPresenter::Wait::UntilClosed);
}

void CsaGameSession::showStatus(const QString& message, int timeoutMs) const
{
    if (m_deps.statusBar) m_deps.statusBar->showMessage(message, timeoutMs);
}
