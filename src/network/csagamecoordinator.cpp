/// @file csagamecoordinator.cpp
/// @brief CSA通信対局コーディネータクラスの実装

#include "csagamecoordinator.h"
#include "csamoveconverter.h"
#include "csaenginecontroller.h"
#include "shogigamecontroller.h"
#include "shogiclock.h"
#include "shogiboard.h"
#include "logcategories.h"

#include <QRegularExpression>

CsaGameCoordinator::CsaGameCoordinator(const Deps& deps, QObject* parent)
    : QObject(parent)
    , m_client(new CsaClient(this))
    , m_reconnectTimer(new QTimer(this))
    , m_verdictTimer(new QTimer(this))
{
    m_verdictTimer->setSingleShot(true);
    m_verdictTimer->setInterval(kVerdictTimeoutMs);
    connect(m_verdictTimer, &QTimer::timeout, this, &CsaGameCoordinator::onVerdictTimeout);
    m_reconnectTimer->setSingleShot(true);
    m_reconnectTimer->setInterval(kReconnectDelayMs);
    connect(m_reconnectTimer, &QTimer::timeout,
            this, &CsaGameCoordinator::reconnectForNextGame);
    connect(m_client, &CsaClient::connectionStateChanged,
            this, &CsaGameCoordinator::onConnectionStateChanged);
    connect(m_client, &CsaClient::errorOccurred,
            this, &CsaGameCoordinator::onClientError);
    connect(m_client, &CsaClient::commandRejected,
            this, &CsaGameCoordinator::onCommandRejected);
    connect(m_client, &CsaClient::loginSucceeded,
            this, &CsaGameCoordinator::onLoginSucceeded);
    connect(m_client, &CsaClient::loginFailed,
            this, &CsaGameCoordinator::onLoginFailed);
    connect(m_client, &CsaClient::logoutCompleted,
            this, &CsaGameCoordinator::onLogoutCompleted);
    connect(m_client, &CsaClient::gameSummaryReceived,
            this, &CsaGameCoordinator::onGameSummaryReceived);
    connect(m_client, &CsaClient::gameStarted,
            this, &CsaGameCoordinator::onGameStarted);
    connect(m_client, &CsaClient::gameRejected,
            this, &CsaGameCoordinator::onGameRejected);
    connect(m_client, &CsaClient::moveReceived,
            this, &CsaGameCoordinator::onMoveReceived);
    connect(m_client, &CsaClient::moveConfirmed,
            this, &CsaGameCoordinator::onMoveConfirmed);
    connect(m_client, &CsaClient::gameEnded,
            this, &CsaGameCoordinator::onServerGameEnded);
    connect(m_client, &CsaClient::gameInterrupted,
            this, &CsaGameCoordinator::onGameInterrupted);
    connect(m_client, &CsaClient::rawMessageReceived,
            this, &CsaGameCoordinator::onRawMessageReceived);
    connect(m_client, &CsaClient::rawMessageSent,
            this, &CsaGameCoordinator::onRawMessageSent);
    m_gameController = deps.gameController;
    m_clock = deps.clock;
    m_engines = deps.engines;
}

CsaGameCoordinator::~CsaGameCoordinator()
{
    cleanup();
}

void CsaGameCoordinator::startGame(const StartOptions& options)
{
    if (m_gameState != GameState::Idle && m_gameState != GameState::Error
        && m_gameState != GameState::GameOver) {
        emit errorOccurred(tr("対局中は新しい対局を開始できません"));
        return;
    }

    cleanup();
    m_options = options;
    m_playerType = options.playerType;
    m_engineNeedsRestart = false;
    m_completedGames = 0;
    m_thinkingMarginMs = qMax(0, options.thinkingMarginMs);
    m_client->setKeepAliveEnabled(options.keepAliveEnabled);
    m_client->setKeepAliveIntervalMs(options.keepAliveIntervalMs);
    m_client->setTcpKeepAliveEnabled(options.tcpKeepAliveEnabled);
    connectForGame();
}

void CsaGameCoordinator::stopGame()
{
    m_reconnectTimer->stop();
    if (m_gameState == GameState::InGame && m_client->isMyTurn()) {
        performResign();
    } else if (m_gameState == GameState::WaitingForAgree) {
        // 同意前に離れるときは対局条件を拒否してから（仕様 3.3）
        m_client->reject(m_progress.summary.gameId);
    }
    setGameState(GameState::Idle);
    if (m_clock) m_clock->stopClock();
    cleanup();
}

bool CsaGameCoordinator::isMyTurn() const
{
    return turnState() == TurnState::MyTurn && m_pendingFailureText.isEmpty();
}

bool CsaGameCoordinator::isBlackSide() const
{
    return m_progress.isBlackSide;
}

CsaGameCoordinator::TurnState CsaGameCoordinator::turnState() const
{
    // 手番はサーバーとのやり取りを解釈するクライアントが持つ。対局中でなければ自分の手番ではない
    if (m_gameState != GameState::InGame) return TurnState::OpponentTurn;
    if (m_client->awaitingMoveConfirmation()) return TurnState::AwaitingConfirmation;
    return m_client->isMyTurn() ? TurnState::MyTurn : TurnState::OpponentTurn;
}

void CsaGameCoordinator::onHumanMove(const QPoint& from, const QPoint& to)
{
    if (m_gameState != GameState::InGame || !isMyTurn() || !isHumanPlayer() || !m_gameController)
        return;
    const auto move = m_gameController->prepareMove(from, to);
    // 成り選択のモーダル処理中に対局が終了することもある。
    if (!move || m_gameState != GameState::InGame || !isMyTurn()) return;
    const QString csaMove = CsaMoveConverter::toCsa(*move, m_progress.isBlackSide ? Turn::Black : Turn::White,
                                                    *m_gameController->board());
    if (!csaMove.isEmpty() && m_client->sendMove(csaMove)) emit turnChanged(false);
}

void CsaGameCoordinator::onResign()
{
    performResign();
}

void CsaGameCoordinator::performResign()
{
    if (m_gameState != GameState::InGame) return;
    if (!isMyTurn()) {
        emit errorOccurred(tr("相手の手番です"));
        return;
    }

    if (m_clock) {
        m_clock->stopClock();
        m_progress.resignConsumedMs = static_cast<int>(m_clock->considerationMs(m_progress.isBlackSide ? Turn::Black : Turn::White));
        qCDebug(lcNetwork) << "Resign consumed time from clock:" << m_progress.resignConsumedMs << "ms";
    }
    m_client->resign();
}

void CsaGameCoordinator::requestChudan()
{
    if (m_gameState != GameState::InGame) return;
    if (!isMyTurn()) {
        emit errorOccurred(tr("相手の手番です"));
        return;
    }
    m_client->requestChudan();
}

void CsaGameCoordinator::declareWin()
{
    if (m_gameState != GameState::InGame) return;
    if (!isMyTurn()) {
        emit errorOccurred(tr("相手の手番です"));
        return;
    }
    if (!m_progress.summary.allowsJishogiDeclaration()) {
        // 仕様 3.2.1: Declaration に Jishogi が無ければ入玉宣言ルールは採用されていない
        emit logMessage(tr("対局条件に入玉宣言ルール（Declaration:Jishogi）がありません。宣言は反則負けと判定される可能性があります"), true);
    }
    m_client->declareWin();
}

void CsaGameCoordinator::forceImmediateMove()
{
    if (m_gameState != GameState::InGame || m_playerType != PlayerType::Engine || !isMyTurn()) return;
    if (m_engineController) m_engineController->stopThinking();
}

void CsaGameCoordinator::onConnectionStateChanged(CsaClient::ConnectionState state)
{
    emit connectionStateChanged(isConnected());
    switch (state) {
    case CsaClient::ConnectionState::Connected:
        emit logMessage(tr("接続完了。ログイン中..."));
        setGameState(GameState::LoggingIn);
        m_client->login(m_options.username, m_options.password);
        break;

    case CsaClient::ConnectionState::Disconnected:
        if (m_gameState == GameState::InGame) {
            m_lastGameEndedByServer = false;
            onClientGameEnded(CsaClient::GameResult::Chudan, CsaClient::GameEndCause::Chudan, 0);
            break;
        }
        if (m_gameState == GameState::GameOver) {
            // shogi-server は終局のたびに切断する。対局数が残っていれば接続し直して次を待つ
            if (m_options.reconnectAfterGame && m_lastGameEndedByServer && moreGamesWanted()) {
                emit logMessage(tr("サーバーが切断しました。%1秒後に再接続します").arg(kReconnectDelayMs / 1000));
                m_reconnectTimer->start();
            }
            break;
        }
        if (m_gameState != GameState::Idle) {
            emit logMessage(tr("サーバーから切断されました"), true);
            setGameState(GameState::Error);
        }
        break;

    default:
        break;
    }
}

void CsaGameCoordinator::onClientError(const QString& message)
{
    emit logMessage(tr("エラー: %1").arg(message), true);
    if (m_gameState == GameState::InGame) {
        m_lastGameEndedByServer = false;
        onClientGameEnded(CsaClient::GameResult::Chudan, CsaClient::GameEndCause::Chudan, 0);
        return;
    }
    emit errorOccurred(message);
}

void CsaGameCoordinator::onServerGameEnded(CsaClient::GameResult result, CsaClient::GameEndCause cause,
                                           int consumedTimeMs)
{
    m_lastGameEndedByServer = true;
    onClientGameEnded(result, cause, consumedTimeMs);
}

void CsaGameCoordinator::onCommandRejected(const QString& reason)
{
    // 手番外や確認待ちの送信を取りやめただけなので、対局は続ける
    emit logMessage(reason, true);
}

void CsaGameCoordinator::onLoginSucceeded()
{
    emit logMessage(tr("ログイン成功。対局待ち中..."));
    setGameState(GameState::WaitingForMatch);

    if (m_playerType == PlayerType::Engine) {
        startEngine();
    }
}

void CsaGameCoordinator::onLoginFailed(const QString& reason)
{
    emit logMessage(tr("ログイン失敗: %1").arg(reason), true);
    emit errorOccurred(reason);
    setGameState(GameState::Error);
    m_client->disconnectFromServer();
}

void CsaGameCoordinator::onLogoutCompleted()
{
    emit logMessage(tr("ログアウト完了"));
    setGameState(GameState::Idle);
}

void CsaGameCoordinator::onGameStarted(const QString& gameId)
{
    Q_UNUSED(gameId)

    emit logMessage(tr("対局開始！"));
    setGameState(GameState::InGame);

    setupInitialPosition();
    if (m_gameState == GameState::Error) {
        return;
    }

    const CsaClient::GameSummary& summary = m_progress.summary;
    const bool myTurn = m_client->isMyTurn();

    setupClock();

    // 始まった時点の手順は再開局面の既存手順だけ
    emit gameStarted(summary.blackName, summary.whiteName, m_progress.record.entries);
    emit turnChanged(myTurn);

    if (m_playerType == PlayerType::Engine && myTurn) {
        startEngineThinking();
    }
}

void CsaGameCoordinator::onGameRejected(const QString& gameId, const QString& rejector)
{
    emit logMessage(tr("対局が拒否されました (ID: %1, 拒否者: %2)").arg(gameId, rejector), true);
    setGameState(GameState::WaitingForMatch);
}

void CsaGameCoordinator::onClientGameEnded(CsaClient::GameResult result, CsaClient::GameEndCause cause, int consumedTimeMs)
{
    // 解釈できなかった指し手の裁定待ちは、終局で解消する
    clearPendingVerdict();

    if (m_gameState != GameState::InGame) return;
    const bool isBlackSide = m_progress.isBlackSide;
    // 連続王手の千日手は終局した局面の手番で決まるので、最後の局面から手番を取る
    const QString lastSfen = m_progress.record.lastSfen();
    const bool blackToMove = lastSfen.isEmpty() ? (m_client->isMyTurn() == isBlackSide)
                                                : (lastSfen.section(QLatin1Char(' '), 1, 1) != QLatin1String("w"));
    qCDebug(lcNetwork).noquote() << "onClientGameEnded ENTER:"
                                << "result=" << static_cast<int>(result)
                                << "cause=" << static_cast<int>(cause)
                                << "consumedTimeMs=" << consumedTimeMs
                                << "isBlackSide=" << isBlackSide;

    if (m_clock) {
        m_clock->stopClock();
    }

    // サーバーからの消費時間が0の場合、クロックから計算する
    int actualConsumedTimeMs = consumedTimeMs;
    if (consumedTimeMs == 0) {
        if (cause == CsaClient::GameEndCause::Resign &&
            result == CsaClient::GameResult::Lose && m_progress.resignConsumedMs > 0) {
            actualConsumedTimeMs = m_progress.resignConsumedMs;
            qCDebug(lcNetwork).noquote() << "Using saved resign consumed time (my resign):" << actualConsumedTimeMs << "ms";
        } else if (cause == CsaClient::GameEndCause::Resign &&
                   result == CsaClient::GameResult::Win && m_clock) {
            const qint64 opponentClockRemainingMs = m_clock->remainingMs(isBlackSide ? Turn::White : Turn::Black);
            const int opponentInitialTimeMs = m_progress.summary.totalTimeMs(!isBlackSide);
            const int opponentPreviousConsumedMs = m_progress.totalMs(!isBlackSide);

            actualConsumedTimeMs = static_cast<int>(
                static_cast<qint64>(opponentInitialTimeMs)
                - opponentClockRemainingMs
                - opponentPreviousConsumedMs);
            if (actualConsumedTimeMs < 0) actualConsumedTimeMs = 0;

            qCDebug(lcNetwork).noquote() << "Calculated opponent resign consumed time:" << actualConsumedTimeMs << "ms";
        }
    }

    const QString resultStr = CsaClient::gameResultText(result);
    const QString causeStr = CsaClient::gameEndCauseText(cause);

    emit logMessage(tr("対局終了: %1 (%2)").arg(resultStr, causeStr));
    setGameState(GameState::GameOver);

    // 終局行を棋譜へ渡す（対局の手順は終局の行を含めない）
    emit recordEntryReady(m_progress.endRecord(result, cause, blackToMove, actualConsumedTimeMs));

    // エンジンにgameoverとquitコマンドを送信
    if (m_engineController && m_engineController->isInitialized()) {
        switch (result) {
        case CsaClient::GameResult::Win:
            m_engineController->sendGameOver(true);
            break;
        case CsaClient::GameResult::Lose:
            m_engineController->sendGameOver(false);
            break;
        case CsaClient::GameResult::Draw:
        default:
            m_engineController->sendQuit();
            break;
        }
        // 同じ接続で次の対局条件が来たら、エンジンを起動し直す
        m_engineNeedsRestart = true;
    }
    // 対局数を数えてログアウトの要否を先に決める。終局の通知を受ける側（結果の表示・自動保存）は
    // moreGamesWanted() で続行の有無を見られ、結果のダイアログが LOGOUT を遅らせない
    ++m_completedGames;
    handleGameCountAfterEnd();
    emit gameEnded(result, cause, actualConsumedTimeMs);
}

void CsaGameCoordinator::onGameInterrupted()
{
    emit logMessage(tr("対局が中断されました"));
}

void CsaGameCoordinator::onRawMessageReceived(const QString& message)
{
    emit logMessage(tr("[RECV] %1").arg(message));
    emit csaCommLogAppended(QStringLiteral("▶ ") + message);
}

void CsaGameCoordinator::onRawMessageSent(const QString& message)
{
    emit logMessage(tr("[SEND] %1").arg(message));
    QString displayMsg = message;
    if (displayMsg.startsWith(QStringLiteral("LOGIN "))) {
        static const QRegularExpression kWhitespaceRe(QStringLiteral("\\s+"));
        const QStringList parts = displayMsg.split(kWhitespaceRe, Qt::SkipEmptyParts);
        if (parts.size() >= 3) {
            QStringList masked = parts;
            masked[2] = QStringLiteral("*****");
            displayMsg = masked.join(QLatin1Char(' '));
        } else {
            displayMsg = QStringLiteral("LOGIN *****");
        }
    }
    emit csaCommLogAppended(QStringLiteral("◀ ") + displayMsg);
}

void CsaGameCoordinator::onEngineControllerResign()
{
    if (m_gameState == GameState::InGame) {
        emit logMessage(tr("エンジンが投了を選択しました"));
        performResign();
    }
}

void CsaGameCoordinator::onEngineControllerWinDeclaration()
{
    if (m_gameState == GameState::InGame) {
        // 宣言の成否はサーバーが判定する
        emit logMessage(tr("エンジンが入玉宣言を選択しました"));
        declareWin();
    }
}

void CsaGameCoordinator::onEngineControllerInitialized()
{
    m_initializingEngine = false;
    if (m_gameState == GameState::WaitingForAgree) m_client->agree(m_progress.summary.gameId);
}

void CsaGameCoordinator::onEngineControllerError(const QString& message)
{
    m_initializingEngine = false;
    failGame(message);
}

void CsaGameCoordinator::failGame(const QString& message)
{
    const bool inGame = (m_gameState == GameState::InGame);
    m_lastGameEndedByServer = false;
    if (m_gameState == GameState::WaitingForAgree) {
        // 同意できなくなった対局条件は拒否してから離れる
        m_client->reject(m_progress.summary.gameId);
    }
    if (!inGame) setGameState(GameState::Error);
    m_client->disconnectFromServer();
    // 対局中に続行できなくなったら中断として終局させ、画面を対局中のまま残さない。
    // 切断の通知で既に終局していれば何もしない
    if (inGame) {
        onClientGameEnded(CsaClient::GameResult::Chudan, CsaClient::GameEndCause::Chudan, 0);
    }
    emit errorOccurred(message);
}
