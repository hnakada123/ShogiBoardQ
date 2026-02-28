/// @file csagamecoordinator.cpp
/// @brief CSA通信対局コーディネータクラスの実装

#include "csagamecoordinator.h"
#include "csamoveprogresshandler.h"
#include "csamoveconverter.h"
#include "csaenginecontroller.h"
#include "kifurecordlistmodel.h"
#include "shogigamecontroller.h"
#include "boardinteractioncontroller.h"
#include "shogiview.h"
#include "shogiclock.h"
#include "shogiboard.h"
#include "sfencsapositionconverter.h"
#include "logcategories.h"

#include <QRegularExpression>

// ============================================================
// 初期化・破棄
// ============================================================

CsaGameCoordinator::CsaGameCoordinator(QObject* parent)
    : QObject(parent)
    , m_client(new CsaClient(this))
{
    connect(m_client, &CsaClient::connectionStateChanged,
            this, &CsaGameCoordinator::onConnectionStateChanged);
    connect(m_client, &CsaClient::errorOccurred,
            this, &CsaGameCoordinator::onClientError);
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
            this, &CsaGameCoordinator::onClientGameEnded);
    connect(m_client, &CsaClient::gameInterrupted,
            this, &CsaGameCoordinator::onGameInterrupted);
    connect(m_client, &CsaClient::rawMessageReceived,
            this, &CsaGameCoordinator::onRawMessageReceived);
    connect(m_client, &CsaClient::rawMessageSent,
            this, &CsaGameCoordinator::onRawMessageSent);
}

CsaGameCoordinator::~CsaGameCoordinator()
{
    cleanup();
}

void CsaGameCoordinator::setDependencies(const Dependencies& deps)
{
    m_gameController = deps.gameController;
    m_view = deps.view;
    m_clock = deps.clock;
    m_boardController = deps.boardController;
    m_recordModel = deps.recordModel;
    m_sfenHistory = deps.sfenRecord;
    m_gameMoves = deps.gameMoves;
    m_usiCommLog = deps.usiCommLog;
    m_engineThinking = deps.engineThinking;
}

// ============================================================
// 対局制御
// ============================================================

void CsaGameCoordinator::startGame(const StartOptions& options)
{
    if (m_gameState != GameState::Idle && m_gameState != GameState::Error) {
        emit errorOccurred(tr("対局中は新しい対局を開始できません"));
        return;
    }

    m_options = options;
    m_playerType = options.playerType;
    m_moveCount = 0;
    m_blackTotalTimeMs = 0;
    m_whiteTotalTimeMs = 0;
    m_prevToFile = 0;
    m_prevToRank = 0;
    m_resignConsumedTimeMs = 0;
    m_usiMoves.clear();
    if (m_sfenHistory) m_sfenHistory->clear();
    if (m_gameMoves) m_gameMoves->clear();

    m_client->setCsaVersion(options.csaVersion);

    setGameState(GameState::Connecting);
    emit logMessage(tr("サーバー %1:%2 に接続中...").arg(options.host).arg(options.port));
    m_client->connectToServer(options.host, options.port);
}

void CsaGameCoordinator::stopGame()
{
    performResign();
    cleanup();
    setGameState(GameState::Idle);
}

bool CsaGameCoordinator::isMyTurn() const
{
    return m_isMyTurn;
}

bool CsaGameCoordinator::isBlackSide() const
{
    return m_isBlackSide;
}

void CsaGameCoordinator::onHumanMove(const QPoint& from, const QPoint& to, bool promote)
{
    qCDebug(lcNetwork) << "onHumanMove called: from=" << from << "to=" << to << "promote=" << promote;
    qCDebug(lcNetwork) << "gameState=" << static_cast<int>(m_gameState)
                       << "isMyTurn=" << m_isMyTurn
                       << "playerType=" << static_cast<int>(m_playerType)
                       << "isBlackSide=" << m_isBlackSide;

    if (m_gameState != GameState::InGame) {
        qCDebug(lcNetwork) << "Not in game state, returning";
        return;
    }

    if (!m_isMyTurn) {
        qCDebug(lcNetwork) << "Not my turn, returning";
        emit errorOccurred(tr("相手の手番です"));
        return;
    }

    if (m_playerType != PlayerType::Human) {
        qCDebug(lcNetwork) << "Not human player, returning";
        return;
    }

    ShogiBoard* board = m_gameController ? m_gameController->board() : nullptr;
    QString csaMove = CsaMoveConverter::boardToCSA(from, to, promote, m_isBlackSide, board);
    if (csaMove.isEmpty()) {
        qCWarning(lcNetwork) << "Failed to build CSA move from board coordinates";
        emit errorOccurred(tr("CSA指し手の生成に失敗しました。"));
        emit logMessage(tr("CSA指し手の生成に失敗しました。"), true);
        return;
    }
    qCDebug(lcNetwork) << "Generated CSA move:" << csaMove;

    m_client->sendMove(csaMove);
    qCDebug(lcNetwork) << "Move sent to server";
}

void CsaGameCoordinator::onResign()
{
    performResign();
}

void CsaGameCoordinator::performResign()
{
    if (m_gameState != GameState::InGame) return;

    if (m_clock) {
        m_clock->stopClock();
        m_resignConsumedTimeMs = m_isBlackSide
            ? m_clock->getPlayer1ConsiderationTimeMs()
            : m_clock->getPlayer2ConsiderationTimeMs();
        qCDebug(lcNetwork) << "Resign consumed time from clock:" << m_resignConsumedTimeMs << "ms";
    }
    m_client->resign();
}

// ============================================================
// CSAクライアント シグナルハンドラ
// ============================================================

void CsaGameCoordinator::onConnectionStateChanged(CsaClient::ConnectionState state)
{
    switch (state) {
    case CsaClient::ConnectionState::Connected:
        emit logMessage(tr("接続完了。ログイン中..."));
        setGameState(GameState::LoggingIn);
        m_client->login(m_options.username, m_options.password);
        break;

    case CsaClient::ConnectionState::Disconnected:
        if (m_gameState != GameState::Idle && m_gameState != GameState::GameOver) {
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
    emit errorOccurred(message);
    emit logMessage(tr("エラー: %1").arg(message), true);
}

void CsaGameCoordinator::onLoginSucceeded()
{
    emit logMessage(tr("ログイン成功。対局待ち中..."));
    setGameState(GameState::WaitingForMatch);

    if (m_playerType == PlayerType::Engine) {
        if (!m_engineController) {
            m_engineController = new CsaEngineController(this);
            connect(m_engineController, &CsaEngineController::logMessage,
                    this, &CsaGameCoordinator::logMessage);
            connect(m_engineController, &CsaEngineController::resignRequested,
                    this, &CsaGameCoordinator::onEngineControllerResign);
        }
        CsaEngineController::InitParams params;
        params.enginePath = m_options.enginePath;
        params.engineName = m_options.engineName;
        params.engineNumber = m_options.engineNumber;
        params.gameController = m_gameController;
        params.commLog = m_usiCommLog;
        params.thinkingModel = m_engineThinking;
        m_engineController->initialize(params);
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

void CsaGameCoordinator::onGameSummaryReceived(const CsaClient::GameSummary& summary)
{
    m_gameSummary = summary;
    m_isBlackSide = summary.isBlackTurn();

    emit logMessage(tr("対局条件を受信しました"));
    emit logMessage(tr("先手: %1, 後手: %2").arg(summary.blackName, summary.whiteName));
    emit logMessage(tr("持時間: %1秒, 秒読み: %2秒")
                        .arg(summary.totalTime)
                        .arg(summary.byoyomi));

    setGameState(GameState::WaitingForAgree);

    emit logMessage(tr("対局条件に同意します..."));
    m_client->agree(summary.gameId);
}

void CsaGameCoordinator::onGameStarted(const QString& gameId)
{
    Q_UNUSED(gameId)

    emit logMessage(tr("対局開始！"));
    setGameState(GameState::InGame);

    setupInitialPosition();

    // setupClock()はm_isMyTurnを参照するため、先に設定する
    m_isMyTurn = (m_gameSummary.myTurn == m_gameSummary.toMove);

    setupClock();

    emit gameStarted(m_gameSummary.blackName, m_gameSummary.whiteName);
    emit turnChanged(m_isMyTurn);

    if (m_playerType == PlayerType::Engine && m_isMyTurn) {
        ensureMoveProgressHandler();
        m_moveProgressHandler->startEngineThinking();
    }
}

void CsaGameCoordinator::onGameRejected(const QString& gameId, const QString& rejector)
{
    emit logMessage(tr("対局が拒否されました (ID: %1, 拒否者: %2)").arg(gameId, rejector), true);
    setGameState(GameState::WaitingForMatch);
}

void CsaGameCoordinator::onMoveReceived(const QString& move, int consumedTimeMs)
{
    ensureMoveProgressHandler();
    m_moveProgressHandler->handleMoveReceived(move, consumedTimeMs);
}

void CsaGameCoordinator::onMoveConfirmed(const QString& move, int consumedTimeMs)
{
    ensureMoveProgressHandler();
    m_moveProgressHandler->handleMoveConfirmed(move, consumedTimeMs);
}

void CsaGameCoordinator::onClientGameEnded(CsaClient::GameResult result, CsaClient::GameEndCause cause, int consumedTimeMs)
{
    qCDebug(lcNetwork).noquote() << "onClientGameEnded ENTER:"
                                << "result=" << static_cast<int>(result)
                                << "cause=" << static_cast<int>(cause)
                                << "consumedTimeMs=" << consumedTimeMs
                                << "m_isBlackSide=" << m_isBlackSide;

    if (m_clock) {
        m_clock->stopClock();
    }

    // サーバーからの消費時間が0の場合、クロックから計算する
    int actualConsumedTimeMs = consumedTimeMs;
    if (consumedTimeMs == 0) {
        if (cause == CsaClient::GameEndCause::Resign &&
            result == CsaClient::GameResult::Lose && m_resignConsumedTimeMs > 0) {
            actualConsumedTimeMs = m_resignConsumedTimeMs;
            qCDebug(lcNetwork).noquote() << "Using saved resign consumed time (my resign):" << actualConsumedTimeMs << "ms";
        } else if (cause == CsaClient::GameEndCause::Resign &&
                   result == CsaClient::GameResult::Win && m_clock) {
            qint64 opponentClockRemainingMs = m_isBlackSide
                ? m_clock->getPlayer2TimeIntMs()
                : m_clock->getPlayer1TimeIntMs();
            int opponentPreviousConsumedMs = m_isBlackSide
                ? m_whiteTotalTimeMs
                : m_blackTotalTimeMs;

            actualConsumedTimeMs = static_cast<int>(m_initialTimeMs - opponentClockRemainingMs - opponentPreviousConsumedMs);
            if (actualConsumedTimeMs < 0) actualConsumedTimeMs = 0;

            qCDebug(lcNetwork).noquote() << "Calculated opponent resign consumed time:" << actualConsumedTimeMs << "ms";
        }
    }

    QString resultStr = CsaMoveConverter::gameResultToString(result);
    QString causeStr = CsaMoveConverter::gameEndCauseToString(cause);

    emit logMessage(tr("対局終了: %1 (%2)").arg(resultStr, causeStr));
    setGameState(GameState::GameOver);
    emit gameEnded(resultStr, causeStr, actualConsumedTimeMs);

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
    }
}

void CsaGameCoordinator::onGameInterrupted()
{
    emit logMessage(tr("対局が中断されました"));
    setGameState(GameState::GameOver);
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

void CsaGameCoordinator::setGameState(GameState state)
{
    if (m_gameState != state) {
        m_gameState = state;
        emit gameStateChanged(state);
    }
}

// ============================================================
// 局面・時計セットアップ
// ============================================================

void CsaGameCoordinator::setupInitialPosition()
{
    if (!m_gameController || !m_gameController->board()) {
        return;
    }

    const QString hiratePosition =
        QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");

    QString parseError;
    auto startSfen = SfenCsaPositionConverter::fromCsaPositionLines(
        m_gameSummary.positionLines, &parseError);
    if (!startSfen || startSfen->isEmpty()) {
        qCWarning(lcNetwork).noquote()
            << "CSA position parse failed. fallback to hirate. error=" << parseError;
        startSfen = hiratePosition;
    }
    m_gameController->board()->setSfen(*startSfen);

    QString boardSfen = m_gameController->board()->convertBoardToSfen();
    QString standSfen = m_gameController->board()->convertStandToSfen();
    const QString currentPlayerStr = turnToSfen(m_gameController->board()->currentPlayer());
    m_startSfen = QString("%1 %2 %3 1").arg(boardSfen, currentPlayerStr, standSfen);

    if (m_sfenHistory) {
        m_sfenHistory->clear();
        m_sfenHistory->append(m_startSfen);
    }

    const bool isHirate = (m_startSfen.trimmed() == hiratePosition);
    m_positionStr = isHirate
        ? QStringLiteral("position startpos")
        : QStringLiteral("position sfen %1").arg(m_startSfen);
    m_usiMoves.clear();

    for (const QString& move : std::as_const(m_gameSummary.moves)) {
        CsaMoveConverter::applyMoveToBoard(move, m_gameController, m_usiMoves, m_sfenHistory, m_moveCount);
    }

    if (m_view) {
        m_view->update();
    }
}

void CsaGameCoordinator::setupClock()
{
    if (!m_clock) {
        return;
    }

    int timeUnitMs = m_gameSummary.timeUnitMs();
    int totalTimeSec = m_gameSummary.totalTime * timeUnitMs / 1000;
    int byoyomiSec = m_gameSummary.byoyomi * timeUnitMs / 1000;
    int incrementSec = m_gameSummary.increment * timeUnitMs / 1000;

    m_clock->setPlayerTimes(totalTimeSec, totalTimeSec,
                            byoyomiSec, byoyomiSec,
                            incrementSec, incrementSec,
                            true);

    m_initialTimeMs = totalTimeSec * 1000;
    m_blackRemainingMs = m_initialTimeMs;
    m_whiteRemainingMs = m_initialTimeMs;

    bool blackToMove = (m_gameSummary.toMove == QStringLiteral("+"));
    m_clock->setCurrentPlayer(blackToMove ? 1 : 2);
    m_clock->startClock();
}

// ============================================================
// クリーンアップ・ヘルパー
// ============================================================

void CsaGameCoordinator::ensureMoveProgressHandler()
{
    if (m_moveProgressHandler) return;
    m_moveProgressHandler = std::make_unique<CsaMoveProgressHandler>();

    CsaMoveProgressHandler::Refs refs;
    refs.gameController = &m_gameController;
    refs.view = &m_view;
    refs.clock = &m_clock;
    refs.engineController = &m_engineController;
    refs.client = m_client;
    refs.gameState = &m_gameState;
    refs.playerType = &m_playerType;
    refs.isBlackSide = &m_isBlackSide;
    refs.isMyTurn = &m_isMyTurn;
    refs.moveCount = &m_moveCount;
    refs.prevToFile = &m_prevToFile;
    refs.prevToRank = &m_prevToRank;
    refs.blackTotalTimeMs = &m_blackTotalTimeMs;
    refs.whiteTotalTimeMs = &m_whiteTotalTimeMs;
    refs.blackRemainingMs = &m_blackRemainingMs;
    refs.whiteRemainingMs = &m_whiteRemainingMs;
    refs.usiMoves = &m_usiMoves;
    refs.sfenHistory = &m_sfenHistory;
    refs.positionStr = &m_positionStr;
    refs.gameSummary = &m_gameSummary;
    m_moveProgressHandler->setRefs(refs);

    CsaMoveProgressHandler::Hooks hooks;
    hooks.setGameState = [this](GameState s) { setGameState(s); };
    hooks.logMessage = [this](const QString& msg, bool isError) {
        emit logMessage(msg, isError);
    };
    hooks.errorOccurred = [this](const QString& msg) { emit errorOccurred(msg); };
    hooks.moveMade = [this](const QString& csa, const QString& usi,
                            const QString& pretty, int ms) {
        emit moveMade(csa, usi, pretty, ms);
    };
    hooks.turnChanged = [this](bool myTurn) { emit turnChanged(myTurn); };
    hooks.moveHighlightRequested = [this](const QPoint& f, const QPoint& t) {
        emit moveHighlightRequested(f, t);
    };
    hooks.engineScoreUpdated = [this](int scoreCp, int ply) {
        emit engineScoreUpdated(scoreCp, ply);
    };
    hooks.performResign = [this]() { performResign(); };
    m_moveProgressHandler->setHooks(hooks);
}

void CsaGameCoordinator::cleanup()
{
    if (m_engineController) {
        m_engineController->cleanup();
    }
    m_client->disconnectFromServer();
}

void CsaGameCoordinator::sendRawCommand(const QString& command)
{
    if (!m_client) {
        qCWarning(lcNetwork) << "sendRawCommand: client is null";
        return;
    }

    qCDebug(lcNetwork) << "Sending raw command:" << command;
    m_client->sendRawCommand(command);
}
