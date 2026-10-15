/// @file csagamewiring.cpp
/// @brief CSA通信対局の配線と画面への反映を担当するクラスの実装

#include "csagamewiring.h"

#include "boardinteractioncontroller.h"
#include "boardsetupcontroller.h"
#include "csagamedialog.h"
#include "csamoveconverter.h"
#include "csawaitingdialog.h"
#include "engineanalysistab.h"
#include "matchrecordsink.h"
#include "logcategories.h"
#include "shogiview.h"
#include "timecontrolcontroller.h"
#include "uinotificationservice.h"
#include "uistatepolicymanager.h"

#include <QStatusBar>
#include "appactivitystate.h"
#include "csagamecoordinator.h"
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QMessageBox>
#include <QRegularExpression>

CsaGameWiring::CsaGameWiring(const Dependencies& deps, QObject* parent)
    : QObject(parent)
    , m_coordinator(deps.coordinator)
    , m_gameController(deps.gameController)
    , m_shogiView(deps.shogiView)
    , m_boardController(deps.boardController)
    , m_statusBar(deps.statusBar)
    , m_analysisTab(deps.analysisTab)
    , m_boardSetupController(deps.boardSetupController)
    , m_usiCommLog(deps.usiCommLog)
    , m_engineThinking(deps.engineThinking)
    , m_timeController(deps.timeController)
    , m_activity(deps.activity)
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

    // 司令塔のシグナルを実際の受け手へ直接つなぐ
    connect(c, &CsaGameCoordinator::recordEntryReady, this, &CsaGameWiring::onRecordEntryReady, Qt::UniqueConnection);
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
    if (m_boardController) disconnect(c, nullptr, m_boardController, nullptr);
    if (m_notifService) disconnect(c, nullptr, m_notifService, nullptr);
    if (m_analysisTab) {
        disconnect(c, nullptr, m_analysisTab, nullptr);
        disconnect(m_analysisTab, nullptr, c, nullptr);
    }
    if (m_boardSetupController) disconnect(m_boardSetupController, nullptr, c, nullptr);
}

void CsaGameWiring::wireExternalSignals(UiStatePolicyManager* uiPolicy,
                                        MatchRecordSink* recordSink,
                                        UiNotificationService* notifService)
{
    unwire();
    m_uiPolicy = uiPolicy;
    m_recordSink = recordSink;
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
        deps.clock = m_timeController ? m_timeController->clock() : nullptr;
        deps.usiCommLog = m_usiCommLog;
        deps.engineThinking = m_engineThinking;
        m_coordinator->setDependencies(deps);
        wire();
    }

    // 対局開始オプションを設定（詳細設定もここで控える）
    const CsaGameCoordinator::StartOptions options = buildStartOptions(dialog);

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
        setActivity(AppActivityState::Activity::Idle);
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
    setActivity(AppActivityState::Activity::Idle);
    if (m_statusBar) m_statusBar->showMessage(tr("通信対局をキャンセルしました"), 3000);
}

// ============================================================
// 司令塔からの通知
// ============================================================

void CsaGameWiring::onRecordEntryReady(const RecordEntry& entry)
{
    if (m_recordSink) m_recordSink->appendRecord(entry);
}

void CsaGameWiring::onGameStarted(const QString& blackName, const QString& whiteName,
                                  const QList<RecordEntry>& initialRecords)
{
    qCDebug(lcUi) << "onGameStarted:" << blackName << "vs" << whiteName;

    applyAutoFlip();
    // 対局情報と棋譜の書き出しに使う持ち時間と開始日時を、サーバーの対局条件で置き換える
    if (m_timeController && m_coordinator) {
        m_timeController->beginGameWithTimeControl(m_coordinator->gameSummary().timeControl());
    }
    if (m_prepareRecord && m_coordinator) m_prepareRecord(m_coordinator->startSfen(), blackName, whiteName);

    if (m_shogiView) {
        m_shogiView->setBlackPlayerName(QStringLiteral("▲") + blackName);
        m_shogiView->setWhitePlayerName(QStringLiteral("▽") + whiteName);
    }

    // 再開局面の既存手順を棋譜へ入れる（以後の着手は司令塔の recordEntryReady が直接届く）
    if (m_recordSink) {
        for (const RecordEntry& entry : initialRecords) m_recordSink->appendRecord(entry);
    }

    // 時計・再開棋譜の準備が揃ってから公開する。同じ接続での次局もここを通る。
    setActivity(AppActivityState::Activity::DuringCsaGame);
    if (m_uiPolicy) m_uiPolicy->transitionToDuringCsaGame();
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

    setActivity(AppActivityState::Activity::Idle);
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

// ============================================================
// CSA通信対局配線の補助処理（接続ダイアログの詳細設定・棋譜の自動保存・盤の反転・結果の表示）
// ============================================================

namespace {
/// ファイル名に使えない文字を取り除く
QString sanitizeFileNamePart(const QString& text)
{
    static const QRegularExpression forbidden(QStringLiteral("[\\\\/:*?\"<>|\\s]+"));
    QString out = text;
    out.replace(forbidden, QStringLiteral("_"));
    return out.isEmpty() ? QStringLiteral("player") : out;
}
} // namespace

CsaGameCoordinator::StartOptions CsaGameWiring::buildStartOptions(const CsaGameDialog* dialog)
{
    CsaGameCoordinator::StartOptions options;
    options.host = dialog->host();
    options.transport = dialog->useTls() ? CsaClient::Transport::Tls : CsaClient::Transport::Plain;
    options.port = dialog->port();
    options.username = dialog->loginId();
    // shogi-server のゲーム名と手番の希望は LOGIN のパスワード欄に載せる
    options.password = dialog->loginPassword();

    if (dialog->isHuman()) {
        options.playerType = CsaGameCoordinator::PlayerType::Human;
    } else {
        options.playerType = CsaGameCoordinator::PlayerType::Engine;
        options.engineName = dialog->engineName();
        options.enginePath = dialog->engineList().value(dialog->engineNumber()).path;
    }

    options.keepAliveEnabled = dialog->keepAliveEnabled();
    options.keepAliveIntervalMs = dialog->keepAliveIntervalSec() * 1000;
    options.tcpKeepAliveEnabled = dialog->tcpKeepAliveEnabled();
    options.thinkingMarginMs = dialog->thinkingMarginSec() * 1000;
    options.gameLimit = dialog->gameLimit();
    options.reconnectAfterGame = dialog->reconnectAfterGame();

    m_autoSaveEnabled = dialog->autoSaveEnabled();
    m_autoSaveDir = dialog->autoSaveDir();
    m_autoSaveFormat = dialog->autoSaveFormat();
    m_autoFlipBoard = dialog->autoFlipBoard();
    return options;
}

void CsaGameWiring::applyAutoFlip()
{
    if (!m_autoFlipBoard || !m_coordinator || !m_setBoardFlipped) return;
    m_setBoardFlipped(!m_coordinator->isBlackSide());
}

void CsaGameWiring::autoSaveRecord()
{
    if (!m_autoSaveEnabled || !m_coordinator) return;
    if (!m_saveKifuToPath) {
        qCWarning(lcUi) << "autoSaveRecord: no save callback";
        return;
    }
    if (m_autoSaveDir.isEmpty() || !QDir().mkpath(m_autoSaveDir)) {
        const QString text = tr("棋譜の自動保存先を作成できません: %1").arg(m_autoSaveDir);
        qCWarning(lcUi) << text;
        if (m_statusBar) m_statusBar->showMessage(text, 5000);
        return;
    }

    const CsaClient::GameSummary& summary = m_coordinator->gameSummary();
    const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"));
    const QString base = QStringLiteral("%1_%2_vs_%3")
                             .arg(stamp, sanitizeFileNamePart(summary.blackName),
                                  sanitizeFileNamePart(summary.whiteName));
    const QString extension = m_autoSaveFormat.isEmpty() ? QStringLiteral("kif") : m_autoSaveFormat;
    // 同じ秒に終わった対局があっても上書きしない
    const QDir dir(m_autoSaveDir);
    QString path = dir.filePath(base + QLatin1Char('.') + extension);
    for (int serial = 2; QFileInfo::exists(path); ++serial) {
        path = dir.filePath(QStringLiteral("%1_%2.%3").arg(base).arg(serial).arg(extension));
    }

    QString error;
    if (m_saveKifuToPath(path, &error)) {
        qCDebug(lcUi) << "autoSaveRecord: saved" << path;
        if (m_statusBar) m_statusBar->showMessage(tr("棋譜を自動保存しました: %1").arg(path), 5000);
    } else {
        const QString text = tr("棋譜の自動保存に失敗しました: %1").arg(error.isEmpty() ? path : error);
        qCWarning(lcUi) << text;
        if (m_statusBar) m_statusBar->showMessage(text, 5000);
    }
}

void CsaGameWiring::setActivity(AppActivityState::Activity activity)
{
    if (m_activity) m_activity->setActivity(activity);
}

void CsaGameWiring::showGameEndDialog(const QString& title, const QString& message)
{
    // 対局を続けるときは、結果の表示が次の対局の開始を止めないようにする
    if (m_coordinator && m_coordinator->moreGamesWanted()) {
        auto* box = new QMessageBox(QMessageBox::Information, title, message, QMessageBox::Ok, m_parentWidget);
        box->setAttribute(Qt::WA_DeleteOnClose);
        box->setModal(false);
        box->show();
        return;
    }
    QMessageBox::information(m_parentWidget, title, message);
}
