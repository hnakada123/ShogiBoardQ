/// @file csagamecoordinator.cpp
/// @brief CSA通信対局コーディネータクラスの実装

#include "csagamecoordinator.h"
#include "csamoveconverter.h"
#include "matchengineseat.h"
#include "matchrecordsink.h"
#include "usi.h"
#include "gameboard.h"
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
    m_gameBoard = deps.gameBoard;
    m_clock = deps.clock;
    m_engines = deps.engines;
    m_activity = deps.activity;
}

CsaGameCoordinator::~CsaGameCoordinator()
{
    cleanup();
}

void CsaGameCoordinator::startGame(const StartOptions& options)
{
    const GameState state = gameState();
    if (state != GameState::Idle && state != GameState::Error && state != GameState::GameOver) {
        emit errorOccurred(tr("対局中は新しい対局を開始できません"));
        return;
    }

    cleanup();
    m_options = options;
    m_options.thinkingMarginMs = qMax(0, options.thinkingMarginMs);
    m_completedGames = 0;
    m_client->setKeepAliveEnabled(options.keepAliveEnabled);
    m_client->setKeepAliveIntervalMs(options.keepAliveIntervalMs);
    m_client->setTcpKeepAliveEnabled(options.tcpKeepAliveEnabled);
    connectForGame();
}

void CsaGameCoordinator::stopGame()
{
    m_reconnectTimer->stop();
    if (m_phase == Phase::Playing && m_client->isMyTurn()) performResign();
    setPhase(Phase::Off);
    if (m_clock) m_clock->stopClock();
    m_activityLease.reset();
    // 同意前の対局条件は、切断の前に拒否する（仕様 3.3。CsaClient::disconnectFromServer）
    cleanup();
}

CsaGameCoordinator::GameState CsaGameCoordinator::gameState() const
{
    switch (m_phase) {
    case Phase::Off:
        return GameState::Idle;
    case Phase::Failed:
        return GameState::Error;
    case Phase::Playing:
        return GameState::InGame;
    case Phase::Active:
    case Phase::Ended:
        break;
    }
    // 接続の段階はクライアントの状態で決まる
    switch (m_client->connectionState()) {
    case CsaClient::ConnectionState::Disconnected:
        // 終局の後にサーバーが切ったときは、再接続の判断のために GameOver のまま。それ以外は続けられない
        return m_phase == Phase::Ended ? GameState::GameOver : GameState::Error;
    case CsaClient::ConnectionState::Connecting:
        return GameState::Connecting;
    case CsaClient::ConnectionState::Connected:
        return GameState::LoggingIn;
    case CsaClient::ConnectionState::LoggedIn:
    case CsaClient::ConnectionState::WaitingForGame:
        return GameState::WaitingForMatch;
    case CsaClient::ConnectionState::GameReady:
        return GameState::WaitingForAgree;
    case CsaClient::ConnectionState::InGame:
        return GameState::InGame;
    case CsaClient::ConnectionState::GameOver:
        return GameState::GameOver;
    }
    return GameState::Error;
}

void CsaGameCoordinator::setPhase(Phase phase)
{
    m_phase = phase;
    publishGameState();
}

void CsaGameCoordinator::publishGameState()
{
    const GameState state = gameState();
    if (state == m_publishedState) return;
    m_publishedState = state;
    emit gameStateChanged(state);
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
    if (m_phase != Phase::Playing) return TurnState::OpponentTurn;
    if (m_client->awaitingMoveConfirmation()) return TurnState::AwaitingConfirmation;
    return m_client->isMyTurn() ? TurnState::MyTurn : TurnState::OpponentTurn;
}

void CsaGameCoordinator::onHumanMove(const UsiMove& move)
{
    // 成り選択のモーダル処理中に対局が終了することもあるので、届いた時点の状態で確かめる
    if (m_phase != Phase::Playing || !isMyTurn() || !isHumanPlayer() || !m_gameBoard)
        return;
    // 盤は動かさない（サーバーの確認を受けてから指す）。合法手だけを送る
    if (!m_gameBoard->isLegal(move)) return;
    const QString csaMove = CsaMoveConverter::toCsa(move, m_progress.isBlackSide ? Turn::Black : Turn::White,
                                                    m_gameBoard->board()->position());
    if (!csaMove.isEmpty() && m_client->sendMove(csaMove)) emit turnChanged(false);
}

void CsaGameCoordinator::onResign()
{
    performResign();
}

void CsaGameCoordinator::performResign()
{
    if (m_phase != Phase::Playing) return;
    if (!isMyTurn()) {
        emit errorOccurred(tr("相手の手番です"));
        return;
    }

    if (m_clock) m_clock->stopClock();
    m_progress.resignConsumedMs = static_cast<int>(elapsedThisTurn(m_progress.isBlackSide ? Turn::Black : Turn::White));
    qCDebug(lcNetwork) << "Resign consumed time:" << m_progress.resignConsumedMs << "ms";
    m_client->resign();
}

void CsaGameCoordinator::requestChudan()
{
    if (m_phase != Phase::Playing) return;
    if (!isMyTurn()) {
        emit errorOccurred(tr("相手の手番です"));
        return;
    }
    m_client->requestChudan();
}

void CsaGameCoordinator::declareWin()
{
    if (m_phase != Phase::Playing) return;
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
    if (m_phase != Phase::Playing || isHumanPlayer() || !isMyTurn()) return;
    if (Usi* engine = m_engineSeat ? m_engineSeat->engine() : nullptr) engine->stop();
}

void CsaGameCoordinator::onConnectionStateChanged(CsaClient::ConnectionState state)
{
    emit connectionStateChanged(isConnected());
    switch (state) {
    case CsaClient::ConnectionState::Connected:
        emit logMessage(tr("接続完了。ログイン中..."));
        publishGameState();
        m_client->login(m_options.username, m_options.password);
        return;

    case CsaClient::ConnectionState::Disconnected:
        switch (m_phase) {
        case Phase::Playing:
            // 対局中の切断は中断として終局させる
            m_lastGameEndedByServer = false;
            onClientGameEnded(CsaClient::GameResult::Chudan, CsaClient::GameEndCause::Chudan, 0);
            return;
        case Phase::Ended:
            // shogi-server は終局のたびに切断する。対局数が残っていれば接続し直して次を待つ
            if (m_options.reconnectAfterGame && m_lastGameEndedByServer && moreGamesWanted()) {
                emit logMessage(tr("サーバーが切断しました。%1秒後に再接続します").arg(kReconnectDelayMs / 1000));
                m_reconnectTimer->start();
            }
            break;
        case Phase::Active:
            emit logMessage(tr("サーバーから切断されました"), true);
            break;
        case Phase::Off:
        case Phase::Failed:
            break;
        }
        break;

    default:
        break;
    }
    publishGameState();
}

void CsaGameCoordinator::onClientError(const QString& message)
{
    emit logMessage(tr("エラー: %1").arg(message), true);
    if (m_phase == Phase::Playing) {
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
    if (!isHumanPlayer()) startEngine();
}

void CsaGameCoordinator::onLoginFailed(const QString& reason)
{
    emit logMessage(tr("ログイン失敗: %1").arg(reason), true);
    emit errorOccurred(reason);
    setPhase(Phase::Failed);
    m_client->disconnectFromServer();
}

void CsaGameCoordinator::onLogoutCompleted()
{
    emit logMessage(tr("ログアウト完了"));
    setPhase(Phase::Off);
}

void CsaGameCoordinator::onGameStarted(const QString& gameId)
{
    Q_UNUSED(gameId)

    emit logMessage(tr("対局開始！"));
    setPhase(Phase::Playing);

    if (!setupInitialPosition()) return;

    const CsaClient::GameSummary& summary = m_progress.summary;
    const bool myTurn = m_client->isMyTurn();

    setupClock();

    // 棋譜を作り直して再開局面の既存手順を書いてから、通信対局中にする（ローカル対局の
    // MatchCoordinator::start と同じ順。同じ接続での次局もここを通る）
    if (m_sink) {
        m_sink->beginRecord(recordStart());
        for (const RecordEntry& entry : std::as_const(m_progress.record.entries)) m_sink->appendRecord(entry);
    }
    if (m_activity) m_activityLease = m_activity->begin(AppActivityState::Activity::DuringCsaGame, this);
    emit gameStarted(summary.blackName, summary.whiteName);
    emit turnChanged(myTurn);

    if (!isHumanPlayer() && myTurn) startEngineThinking();
}

MatchRecordStart CsaGameCoordinator::recordStart() const
{
    // 対局者はサーバーが知らせた名前（エンジンで指していても対局者名で記録する）、持ち時間はサーバーの対局条件
    MatchRecordStart start;
    start.startSfen = m_progress.record.startSfen;
    start.players.seat(Turn::Black).humanName = m_progress.summary.blackName;
    start.players.seat(Turn::White).humanName = m_progress.summary.whiteName;
    start.timeControl = m_progress.summary.timeControl();
    start.autoSave = m_options.autoSave;
    return start;
}

void CsaGameCoordinator::onGameRejected(const QString& gameId, const QString& rejector)
{
    // 拒否した双方は対局待ちに戻る（クライアントの状態が先に変わっている）
    emit logMessage(tr("対局が拒否されました (ID: %1, 拒否者: %2)").arg(gameId, rejector), true);
}

void CsaGameCoordinator::onClientGameEnded(CsaClient::GameResult result, CsaClient::GameEndCause cause, int consumedTimeMs)
{
    // 解釈できなかった指し手の裁定待ちは、終局で解消する
    clearPendingVerdict();

    if (m_phase != Phase::Playing) return;
    const bool isBlackSide = m_progress.isBlackSide;
    // 連続王手の千日手は終局した局面の手番で決まる
    const bool blackToMove = m_progress.sideToMove() == Turn::Black;
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
        } else if (cause == CsaClient::GameEndCause::Resign && result == CsaClient::GameResult::Win) {
            // 相手の手番が始まってから投了までの時間（残り時間から逆算すると、加算・秒読みで狂う）
            actualConsumedTimeMs = static_cast<int>(elapsedThisTurn(isBlackSide ? Turn::White : Turn::Black));

            qCDebug(lcNetwork).noquote() << "Calculated opponent resign consumed time:" << actualConsumedTimeMs << "ms";
        }
    }

    const QString resultStr = CsaClient::gameResultText(result);
    const QString causeStr = CsaClient::gameEndCauseText(cause);

    emit logMessage(tr("対局終了: %1 (%2)").arg(resultStr, causeStr));
    setPhase(Phase::Ended);

    // 終局行を棋譜へ書く（対局の手順は終局の行を含めない）
    if (m_sink) m_sink->appendRecord(m_progress.endRecord(result, cause, blackToMove, actualConsumedTimeMs));

    // エンジンに gameover を送って終える（ローカル対局の終局と同じ手順。同じ接続で次の対局条件が来たら起動し直す）
    if (m_engineSeat) {
        switch (result) {
        case CsaClient::GameResult::Win:
            m_engineSeat->finish(GameOverResult::Win);
            break;
        case CsaClient::GameResult::Lose:
            m_engineSeat->finish(GameOverResult::Lose);
            break;
        case CsaClient::GameResult::Draw:
            m_engineSeat->finish(GameOverResult::Draw);
            break;
        case CsaClient::GameResult::Censored:
        case CsaClient::GameResult::Chudan:
        case CsaClient::GameResult::Unknown:
            break;
        }
        m_engineSeat->release();
    }
    // 対局数を数えてログアウトの要否を先に決める。終局の通知を受ける側（結果の表示）は
    // moreGamesWanted() で続行の有無を見られ、結果のダイアログが LOGOUT を遅らせない
    ++m_completedGames;
    handleGameCountAfterEnd();
    // 棋譜を確定して自動保存し（次の対局が始まる前に書き出す）、待機に戻す
    if (m_sink) m_sink->finishRecord(/*autoSave=*/true);
    m_activityLease.reset();
    emit gameEnded(result, cause, actualConsumedTimeMs);
}

MatchSearchTiming CsaGameCoordinator::snapshot(Turn side, bool /*pondering*/) const
{
    // 先読みも自分の持ち時間を読む。残り時間は着手ごとにサーバーが示した値
    UsiGoTimeInput input = m_progress.engineTiming(m_options.thinkingMarginMs);
    input.sideToMove = side;
    return MatchSearchTiming::fromInput(input);
}

qint64 CsaGameCoordinator::elapsedThisTurn(Turn side) const
{
    return side == m_progress.sideToMove() && m_turnTimer.isValid() ? m_turnTimer.elapsed() : 0;
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

void CsaGameCoordinator::onEngineResigned()
{
    if (m_phase == Phase::Playing) {
        emit logMessage(tr("エンジンが投了を選択しました"));
        performResign();
    }
}

void CsaGameCoordinator::onEngineDeclaredWin()
{
    if (m_phase == Phase::Playing) {
        // 宣言の成否はサーバーが判定する
        emit logMessage(tr("エンジンが入玉宣言を選択しました"));
        declareWin();
    }
}

void CsaGameCoordinator::onEngineInitialized()
{
    emit logMessage(tr("エンジン %1 を起動しました").arg(m_options.engineName));
    if (gameState() == GameState::WaitingForAgree) m_client->agree(m_progress.summary.gameId);
}

void CsaGameCoordinator::onEngineError(const QString& message)
{
    if (m_engineSeat) m_engineSeat->release();
    emit logMessage(message, true);
    failGame(message);
}

void CsaGameCoordinator::failGame(const QString& message)
{
    const bool inGame = (m_phase == Phase::Playing);
    m_lastGameEndedByServer = false;
    if (!inGame) setPhase(Phase::Failed);
    // 同意できなくなった対局条件は、切断の前に拒否する（CsaClient::disconnectFromServer）
    m_client->disconnectFromServer();
    // 対局中に続行できなくなったら中断として終局させ、画面を対局中のまま残さない。
    // 切断の通知で既に終局していれば何もしない
    if (inGame) {
        onClientGameEnded(CsaClient::GameResult::Chudan, CsaClient::GameEndCause::Chudan, 0);
    }
    emit errorOccurred(message);
}
