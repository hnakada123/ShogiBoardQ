/// @file csagamecoordinator.cpp
/// @brief CSA通信対局コーディネータクラスの実装

#include "csagamecoordinator.h"
#include "csamoveconverter.h"
#include "csaverdictwait.h"
#include "matchengineseat.h"
#include "matchrecordsink.h"
#include "usi.h"
#include "gameboard.h"
#include "shogiclock.h"
#include "shogiboard.h"
#include "logcategories.h"

CsaGameCoordinator::CsaGameCoordinator(const Deps& deps, QObject* parent)
    : QObject(parent)
    , m_client(new CsaClient(this))
    , m_series(new CsaGameSeries(this))
    , m_verdict(new CsaVerdictWait(kVerdictTimeoutMs, this))
    , m_agreeDelay(new QTimer(this))
{
    connect(m_verdict, &CsaVerdictWait::timedOut, this, &CsaGameCoordinator::onVerdictTimedOut);
    connect(m_series, &CsaGameSeries::reconnectDue, this, &CsaGameCoordinator::reconnectForNextGame);
    m_agreeDelay->setSingleShot(true);
    connect(m_agreeDelay, &QTimer::timeout, this, &CsaGameCoordinator::agreeToSummary);
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
    m_series->begin(options.gameLimit, options.reconnectAfterGame);
    m_client->setKeepAliveEnabled(options.keepAliveEnabled);
    m_client->setKeepAliveIntervalMs(options.keepAliveIntervalMs);
    m_client->setTcpKeepAliveEnabled(options.tcpKeepAliveEnabled);
    connectForGame();
}

void CsaGameCoordinator::stopGame()
{
    m_series->stop();
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

bool CsaGameCoordinator::sessionContinues() const
{
    switch (m_phase) {
    case Phase::Off:
    case Phase::Failed:
        return false;
    case Phase::Playing:
        return true;
    case Phase::Active:
        // 接続・ログイン・対局条件を待つ間。接続できなかった・切れたときは続けない（gameState は Error）
        return m_client->connectionState() != CsaClient::ConnectionState::Disconnected;
    case Phase::Ended:
        // 次の対局を待つ（接続したまま、または再接続の待ち）
        return moreGamesWanted() && (m_client->connectionState() != CsaClient::ConnectionState::Disconnected
                                     || m_series->isReconnectPending());
    }
    return false;
}

void CsaGameCoordinator::updateActivity()
{
    if (!m_activity || m_phase == Phase::Playing) return;
    if (!sessionContinues()) {
        m_activityLease.reset();
        return;
    }
    // 局と局の間も通信対局を続けているので、待機中に戻さない（棋譜の置き換え・開始系の操作を止める）
    if (!m_activityLease.isCurrent() || !m_activity->is(AppActivityState::Activity::DuringCsaConnection))
        m_activityLease = m_activity->begin(AppActivityState::Activity::DuringCsaConnection, this);
}

void CsaGameCoordinator::publishGameState()
{
    updateActivity();
    const GameState state = gameState();
    if (state == m_publishedState) return;
    m_publishedState = state;
    emit gameStateChanged(state);
}

bool CsaGameCoordinator::isMyTurn() const
{
    return turnState() == TurnState::MyTurn && !m_verdict->isPending();
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

bool CsaGameCoordinator::sendHumanMove(const UsiMove& move)
{
    // 成り選択のモーダル処理中に対局が終了することもあるので、届いた時点の状態で確かめる
    if (m_phase != Phase::Playing || !isMyTurn() || !isHumanPlayer() || !m_gameBoard)
        return false;
    // 盤は動かさない（サーバーの確認を受けてから指す）。合法手だけを送る
    if (!m_gameBoard->isLegal(move)) return false;
    const QString csaMove = CsaMoveConverter::toCsa(move, m_progress.isBlackSide ? Turn::Black : Turn::White,
                                                    m_gameBoard->board()->position());
    if (csaMove.isEmpty() || !m_client->sendMove(csaMove)) return false;
    // 駒を置いた時点に合わせる: 自分の時計を止め（消費時間はサーバーが決めるので、確認で残り時間を合わせ直す。
    // 相手の時計は確認を受けてから進める）、駒音を鳴らす。盤の表示は盤の操作が指した後にする
    if (m_clock) m_clock->stopClock();
    m_gameBoard->notifyPiecePlaced(m_progress.isBlackSide ? Turn::Black : Turn::White, m_progress.moveCount() + 1);
    return true;
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
    // ローカル対局と同じく、エンジンにすぐ指させる（初期化の途中なら、始めた探索の後で止める）
    if (Usi* engine = m_engineSeat ? m_engineSeat->engine() : nullptr) engine->requestImmediateMove();
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
            endGame(CsaClient::GameResult::Chudan, CsaClient::GameEndCause::Chudan, 0, false);
            return;
        case Phase::Ended:
            // shogi-server は終局のたびに切断する。対局数が残っていれば接続し直して次を待つ
            if (m_series->scheduleReconnect()) {
                emit logMessage(tr("サーバーが切断しました。%1秒後に再接続します")
                                    .arg(CsaGameSeries::kReconnectDelayMs / 1000));
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
        endGame(CsaClient::GameResult::Chudan, CsaClient::GameEndCause::Chudan, 0, false);
        return;
    }
    emit errorOccurred(message);
}

void CsaGameCoordinator::onServerGameEnded(CsaClient::GameResult result, CsaClient::GameEndCause cause,
                                           int consumedTimeMs)
{
    endGame(result, cause, consumedTimeMs, true);
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
    // 段階と接続を片付けてから知らせる（知らせのダイアログの間に次の受信行を処理しない。セキュリティ監査 V1）
    setPhase(Phase::Failed);
    m_client->disconnectFromServer();
    emit errorOccurred(reason);
}

void CsaGameCoordinator::onLogoutCompleted()
{
    emit logMessage(tr("ログアウト完了"));
    setPhase(Phase::Off);
}

void CsaGameCoordinator::onGameStarted(const QString& gameId)
{
    Q_UNUSED(gameId)

    // 通信対局を続けていないのに届いた START では、棋譜を置き換えない（セキュリティ監査 V11）
    if (m_phase == Phase::Off || m_phase == Phase::Failed) {
        m_client->disconnectFromServer();
        return;
    }

    emit logMessage(tr("対局開始！"));
    setPhase(Phase::Playing);

    setupInitialPosition();

    const CsaGameSummary& summary = m_progress.summary;
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
    if (!isHumanPlayer())
        emit matchEngineStarted(m_progress.isBlackSide ? Turn::Black : Turn::White, m_options.engineName);

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

void CsaGameCoordinator::endGame(CsaClient::GameResult result, CsaClient::GameEndCause cause, int consumedTimeMs,
                                 bool endedByServer)
{
    // 解釈できなかった指し手の裁定待ちは、終局で解消する
    m_verdict->clear();

    if (m_phase != Phase::Playing) return;
    m_series->setLastEndedByServer(endedByServer);
    const bool isBlackSide = m_progress.isBlackSide;
    // 連続王手の千日手は終局した局面の手番で決まる
    const bool blackToMove = m_progress.sideToMove() == Turn::Black;
    qCDebug(lcNetwork).noquote() << "endGame:"
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

    // 結果の表示（ステータスバー・ダイアログ）は gameEnded を受ける ui の CsaGameUiController が出す
    qCInfo(lcNetwork) << "Game ended:" << result << cause;
    // 対局の段階だけを変え、動作状態は棋譜を確定してから変える（MatchCoordinator と同じ順）
    m_phase = Phase::Ended;

    // 終局行を棋譜へ書く（対局の手順は終局の行を含めない）
    if (m_sink) m_sink->appendRecord(m_progress.endRecord(result, cause, blackToMove, actualConsumedTimeMs));

    // エンジンに gameover を送って終える（ローカル対局の終局と同じ手順と同じ対応 MatchEnding。最大手数・中断は draw。
    // 同じ接続で次の対局条件が来たら起動し直す）
    if (m_engineSeat) {
        m_engineSeat->finish(MatchEnding::engineResult(m_progress.endInfo(result, cause),
                                                       isBlackSide ? Turn::Black : Turn::White));
        m_engineSeat->release();
    }
    m_series->finishGame();
    m_lastGameEnd.start();
    // 棋譜を確定して自動保存し（次の対局が始まる前に書き出す）、対局の間の動作状態を終える。
    // 次の対局を待つなら通信対局の接続中に、続けなければ待機に戻す
    if (m_sink) m_sink->finishRecord(/*autoSave=*/true);
    publishGameState();
    // ログアウトは終局の通知の前に送る。受ける側（結果の表示）は moreGamesWanted() で続行の有無を見られ、
    // 結果のダイアログが LOGOUT を遅らせない
    handleGameCountAfterEnd();
    emit gameEnded(result, cause, actualConsumedTimeMs);
}

MatchSearchTiming CsaGameCoordinator::snapshot(Turn side, bool pondering) const
{
    // 先読みも自分の持ち時間を読む。残り時間は着手ごとにサーバーが示した値で、自分の手をサーバーが確認するまでは
    // 今の手の消費を引いていないので、手番側の経過時間を引く（ローカル対局と同じ換算）
    UsiGoTimeInput input = m_progress.engineTiming(pondering, elapsedThisTurn(m_progress.sideToMove()),
                                                   m_options.thinkingMarginMs);
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

// 送受信の行は通信ログ（csaCommLogAppended）にだけ出す。logMessage は状態の変化と異常の知らせで、ステータスバーと
// 待機ダイアログに出る。1行ごとに出すと、対局中は1手ごとにステータスバーを描き直すことになる
void CsaGameCoordinator::onRawMessageReceived(const QString& message)
{
    emit csaCommLogAppended(QStringLiteral("▶ ") + message);
}

void CsaGameCoordinator::onRawMessageSent(const QString& message)
{
    // LOGIN のパスワードはクライアントが伏せてから知らせる（CsaClient::sendMessage）
    emit csaCommLogAppended(QStringLiteral("◀ ") + message);
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
    agreeToSummary();
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
    if (!inGame) setPhase(Phase::Failed);
    // 同意できなくなった対局条件は、切断の前に拒否する（CsaClient::disconnectFromServer）
    m_client->disconnectFromServer();
    // 対局中に続行できなくなったら中断として終局させ、画面を対局中のまま残さない。
    // 切断の通知で既に終局していれば何もしない
    if (inGame) {
        endGame(CsaClient::GameResult::Chudan, CsaClient::GameEndCause::Chudan, 0, false);
    }
    emit errorOccurred(message);
}
