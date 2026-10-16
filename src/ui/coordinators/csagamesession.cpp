/// @file csagamesession.cpp
/// @brief CSA 通信対局の開始と、通信対局の進行の画面への反映の実装

#include "csagamesession.h"

#include "boardinteractioncontroller.h"
#include "boardmovecontroller.h"
#include "csagamecoordinator.h"
#include "csagamedialog.h"
#include "csawaitingdialog.h"
#include "csalogpanel.h"
#include "evaluationchartwidget.h"
#include "kifufilecontroller.h"
#include "logcategories.h"
#include "mainwindowappearancecontroller.h"
#include "matchrecordsink.h"
#include "shogiview.h"
#include "uinotificationservice.h"

#include <QMessageBox>
#include <QStatusBar>

CsaGameSession::CsaGameSession(const Deps& deps, QObject* parent)
    : QObject(parent)
    , m_deps(deps)
{
    Q_ASSERT(m_deps.coordinator && m_deps.activity && m_deps.recordSink);
    CsaGameCoordinator* const c = m_deps.coordinator;

    // 対局の開始・終局・手番・ログの反映
    connect(c, &CsaGameCoordinator::gameStarted, this, &CsaGameSession::onGameStarted);
    connect(c, &CsaGameCoordinator::gameEnded, this, &CsaGameSession::onGameEnded);
    connect(c, &CsaGameCoordinator::turnChanged, this, &CsaGameSession::onTurnChanged);
    connect(c, &CsaGameCoordinator::logMessage, this, &CsaGameSession::onLogMessage);
    connect(c, &CsaGameCoordinator::recordEntryReady, this, &CsaGameSession::onRecordEntryReady);
    connect(c, &CsaGameCoordinator::engineScoreUpdated, this, &CsaGameSession::onEngineScoreUpdated);

    // 司令塔と画面の部品を直接つなぐ
    if (m_deps.boardController) {
        connect(c, &CsaGameCoordinator::moveHighlightRequested,
                m_deps.boardController, &BoardInteractionController::showMoveHighlights);
    }
    if (m_deps.boardMoves) {
        connect(m_deps.boardMoves, &BoardMoveController::csaMoveRequested, c, &CsaGameCoordinator::onHumanMove);
    }
    if (m_deps.notifications) {
        connect(c, &CsaGameCoordinator::errorOccurred,
                m_deps.notifications, &UiNotificationService::displayErrorMessage);
    }
    if (CsaLogPanel* log = m_deps.csaLog) {
        connect(c, &CsaGameCoordinator::connectionStateChanged, log, &CsaLogPanel::setConnected);
        connect(c, &CsaGameCoordinator::csaCommLogAppended, log, &CsaLogPanel::append);
        connect(log, &CsaLogPanel::csaRawCommandRequested, c, &CsaGameCoordinator::sendRawCommand);
    }
}

// ============================================================
// 対局の開始
// ============================================================

bool CsaGameSession::startGame(const CsaGameDialog::Selection& selection)
{
    const CsaGameCoordinator::StartOptions& options = selection.start;
    m_autoSaveEnabled = selection.autoSaveEnabled;
    m_autoSaveDir = selection.autoSaveDir;
    m_autoSaveFormat = selection.autoSaveFormat;
    m_autoFlipBoard = selection.autoFlipBoard;

    CsaGameCoordinator* const c = m_deps.coordinator;
    CsaWaitingDialog waitingDialog(c, m_deps.parentWidget);
    connect(&waitingDialog, &CsaWaitingDialog::cancelRequested, this, &CsaGameSession::onWaitingCancelled);

    // 待機ダイアログがシグナルを受け取れるようになってから始める
    c->startGame(options);
    // 対局開始またはエラーまで待つ
    const bool started = waitingDialog.exec() == QDialog::Accepted
        && c->gameState() == CsaGameCoordinator::GameState::InGame;
    if (!started) {
        m_deps.activity->setActivity(AppActivityState::Activity::Idle);
        if (c->gameState() != CsaGameCoordinator::GameState::Idle) c->stopGame();
    }
    return started;
}

void CsaGameSession::onWaitingCancelled()
{
    qCDebug(lcUi) << "onWaitingCancelled: cancelled by user";
    m_deps.coordinator->stopGame();
    m_deps.activity->setActivity(AppActivityState::Activity::Idle);
    showStatus(tr("通信対局をキャンセルしました"), 3000);
}

// ============================================================
// 司令塔からの通知
// ============================================================

void CsaGameSession::onRecordEntryReady(const RecordEntry& entry)
{
    m_deps.recordSink->appendRecord(entry);
}

void CsaGameSession::onGameStarted(const QString& blackName, const QString& whiteName,
                                   const QList<RecordEntry>& initialRecords)
{
    qCDebug(lcUi) << "onGameStarted:" << blackName << "vs" << whiteName;
    CsaGameCoordinator* const c = m_deps.coordinator;

    // 後手のときは自分側から見る（通常の対局と同じ経路で反転し、評価値グラフや名前表示の向きも揃える）
    if (m_autoFlipBoard && m_deps.appearance && m_deps.shogiView
        && m_deps.shogiView->flipMode() != !c->isBlackSide()) {
        m_deps.appearance->flipBoard();
    }
    // 棋譜と対局情報を作り直す（持ち時間と開始日時はサーバーの対局条件）
    if (m_deps.prepareRecord) m_deps.prepareRecord(c->startSfen(), blackName, whiteName, c->gameSummary().timeControl());
    if (m_deps.shogiView) {
        m_deps.shogiView->chrome()->setBlackPlayerName(QStringLiteral("▲") + blackName);
        m_deps.shogiView->chrome()->setWhitePlayerName(QStringLiteral("▽") + whiteName);
    }
    // 再開局面の既存手順を棋譜へ入れる（以後の着手は司令塔の recordEntryReady が届く）
    for (const RecordEntry& entry : initialRecords) m_deps.recordSink->appendRecord(entry);

    // 時計・再開棋譜の準備が揃ってから通信対局中にする。同じ接続での次局もここを通る
    m_deps.activity->setActivity(AppActivityState::Activity::DuringCsaGame);
    showStatus(tr("CSA通信対局開始: %1 vs %2").arg(blackName, whiteName), 5000);
}

void CsaGameSession::onGameEnded(CsaClient::GameResult result, CsaClient::GameEndCause cause, int consumedTimeMs)
{
    const QString resultText = CsaClient::gameResultText(result);
    const QString causeText = CsaClient::gameEndCauseText(cause);
    qCDebug(lcUi) << "onGameEnded:" << resultText << "(" << causeText << ")" << "consumedTimeMs=" << consumedTimeMs;

    // 終局行は司令塔が棋譜へ渡し済み。終局日時を入れてから、次の対局が始まる前に書き出す
    if (m_deps.recordGameEnd) m_deps.recordGameEnd();
    if (m_autoSaveEnabled && m_deps.kifuFiles) {
        m_deps.kifuFiles->autoSaveKifu(m_autoSaveDir, m_autoSaveFormat.isEmpty() ? QStringLiteral("kif")
                                                                                       : m_autoSaveFormat);
    }

    m_deps.activity->setActivity(AppActivityState::Activity::Idle);
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

void CsaGameSession::onEngineScoreUpdated(const UsiScore& score, int ply)
{
    // 自分のエンジンの評価値を、自分の側の系列に先手から見た値で描く
    if (!m_deps.evalChart) return;
    if (m_deps.coordinator->isBlackSide()) m_deps.evalChart->appendScoreP1(ply, score);
    else m_deps.evalChart->appendScoreP2(ply, score.negated());
}

// ============================================================
// 表示
// ============================================================

void CsaGameSession::showGameEndDialog(const QString& title, const QString& message)
{
    // 対局を続けるときは、結果の表示が次の対局の開始を止めないようにする
    if (m_deps.coordinator->moreGamesWanted()) {
        auto* box = new QMessageBox(QMessageBox::Information, title, message, QMessageBox::Ok, m_deps.parentWidget);
        box->setAttribute(Qt::WA_DeleteOnClose);
        box->setModal(false);
        box->show();
        return;
    }
    QMessageBox::information(m_deps.parentWidget, title, message);
}

void CsaGameSession::showStatus(const QString& message, int timeoutMs) const
{
    if (m_deps.statusBar) m_deps.statusBar->showMessage(message, timeoutMs);
}
