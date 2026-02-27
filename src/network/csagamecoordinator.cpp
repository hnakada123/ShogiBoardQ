/// @file csagamecoordinator.cpp
/// @brief CSA通信対局コーディネータクラスの実装

#include "csagamecoordinator.h"
#include "csamoveconverter.h"
#include "csaenginecontroller.h"
#include "kifurecordlistmodel.h"
#include "shogigamecontroller.h"
#include "shogiview.h"
#include "shogiclock.h"
#include "shogiboard.h"
#include "boardinteractioncontroller.h"
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
// 時間管理ヘルパー
// ============================================================

void CsaGameCoordinator::updateTimeTracking(bool isBlackMove, int consumedTimeMs)
{
    if (isBlackMove) {
        m_blackTotalTimeMs += consumedTimeMs;
        m_blackRemainingMs -= consumedTimeMs;
        if (m_blackRemainingMs < 0) m_blackRemainingMs = 0;
    } else {
        m_whiteTotalTimeMs += consumedTimeMs;
        m_whiteRemainingMs -= consumedTimeMs;
        if (m_whiteRemainingMs < 0) m_whiteRemainingMs = 0;
    }
}

void CsaGameCoordinator::syncClockAfterMove(bool startMyTurnClock)
{
    if (!m_clock) return;

    int blackRemainSec = m_blackRemainingMs / 1000;
    int whiteRemainSec = m_whiteRemainingMs / 1000;
    int byoyomiSec = m_gameSummary.byoyomi * m_gameSummary.timeUnitMs() / 1000;
    int incrementSec = m_gameSummary.increment * m_gameSummary.timeUnitMs() / 1000;

    m_clock->setPlayerTimes(blackRemainSec, whiteRemainSec,
                            byoyomiSec, byoyomiSec,
                            incrementSec, incrementSec,
                            true);

    if (startMyTurnClock) {
        m_clock->setCurrentPlayer(m_isBlackSide ? 1 : 2);
    } else {
        m_clock->setCurrentPlayer(m_isBlackSide ? 2 : 1);
    }
    m_clock->startClock();
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
        startEngineThinking();
    }
}

void CsaGameCoordinator::onGameRejected(const QString& gameId, const QString& rejector)
{
    emit logMessage(tr("対局が拒否されました (ID: %1, 拒否者: %2)").arg(gameId, rejector), true);
    setGameState(GameState::WaitingForMatch);
}

void CsaGameCoordinator::onMoveReceived(const QString& move, int consumedTimeMs)
{
    emit logMessage(tr("相手の指し手: %1 (消費時間: %2ms)").arg(move).arg(consumedTimeMs));

    bool isBlackMove = (move.length() > 0 && move[0] == QLatin1Char('+'));

    updateTimeTracking(isBlackMove, consumedTimeMs);
    syncClockAfterMove(true);  // 次は自分の手番

    // CSA形式から座標を抽出（ハイライト用）
    QPoint from, to;
    int fromFile = 0, fromRank = 0;
    int toFile = 0, toRank = 0;
    if (move.length() >= 5) {
        fromFile = move[1].digitValue();
        fromRank = move[2].digitValue();
        toFile = move[3].digitValue();
        toRank = move[4].digitValue();

        if (toFile < 1 || toFile > 9 || toRank < 1 || toRank > 9) {
            qCWarning(lcNetwork) << "Invalid CSA move coordinates: toFile=" << toFile << "toRank=" << toRank << "move=" << move;
            emit logMessage(tr("不正な座標の指し手を受信しました: %1").arg(move), true);
            emit errorOccurred(tr("サーバーからの指し手の座標が不正です: %1").arg(move));
            setGameState(GameState::Error);
            return;
        }

        if (fromFile == 0 && fromRank == 0) {
            from = QPoint(-1, -1);
        } else {
            from = QPoint(fromFile, fromRank);
        }
        to = QPoint(toFile, toRank);
    }

    // 成り判定（盤面更新前に行う）
    bool isPromotion = false;
    if (move.length() >= 7 && fromFile != 0 && fromRank != 0) {
        QString destPiece = move.mid(5, 2);
        static const QStringList promotedPieces = {
            QStringLiteral("TO"), QStringLiteral("NY"), QStringLiteral("NK"),
            QStringLiteral("NG"), QStringLiteral("UM"), QStringLiteral("RY")
        };
        if (promotedPieces.contains(destPiece)) {
            if (m_gameController && m_gameController->board()) {
                Piece srcPiece = m_gameController->board()->getPieceCharacter(fromFile, fromRank);
                static const QString unpromoted = QStringLiteral("PLNSBRplnsbr");
                if (unpromoted.contains(pieceToChar(srcPiece))) {
                    isPromotion = true;
                }
            }
        }
    }

    if (!CsaMoveConverter::applyMoveToBoard(move, m_gameController, m_usiMoves, m_sfenHistory, m_moveCount)) {
        emit logMessage(tr("指し手の適用に失敗しました: %1").arg(move), true);
        emit errorOccurred(tr("サーバーからの指し手を盤面に適用できません: %1").arg(move));
        setGameState(GameState::Error);
        return;
    }

    if (m_view) m_view->update();

    m_moveCount++;
    m_isMyTurn = true;
    emit turnChanged(true);

    emit moveHighlightRequested(from, to);

    QString usiMove = CsaMoveConverter::csaToUsi(move);
    QString prettyMove = CsaMoveConverter::csaToPretty(move, isPromotion, m_prevToFile, m_prevToRank, m_moveCount - 1);

    m_prevToFile = toFile;
    m_prevToRank = toRank;

    emit moveMade(move, usiMove, prettyMove, consumedTimeMs);

    if (m_playerType == PlayerType::Engine) {
        startEngineThinking();
    }
}

void CsaGameCoordinator::onMoveConfirmed(const QString& move, int consumedTimeMs)
{
    emit logMessage(tr("指し手確認: %1 (消費時間: %2ms)").arg(move).arg(consumedTimeMs));

    bool isBlackMove = (move.length() > 0 && move[0] == QLatin1Char('+'));

    updateTimeTracking(isBlackMove, consumedTimeMs);
    syncClockAfterMove(false);  // 次は相手の手番

    // USI指し手リストとSFEN記録は更新する必要がある
    QString usiMove = CsaMoveConverter::csaToUsi(move);
    if (!usiMove.isEmpty()) {
        m_usiMoves.append(usiMove);
    }

    // エンジンの指し手の場合のみSFEN記録を更新（人間はvalidateAndMoveで追加済み）
    if (m_playerType == PlayerType::Engine) {
        if (m_gameController && m_gameController->board() && m_sfenHistory) {
            QString boardSfen = m_gameController->board()->convertBoardToSfen();
            QString standSfen = m_gameController->board()->convertStandToSfen();
            QString currentPlayerStr = (m_gameController->currentPlayer() == ShogiGameController::Player1)
                                       ? QStringLiteral("b") : QStringLiteral("w");
            QString fullSfen = QString("%1 %2 %3 %4")
                                   .arg(boardSfen, currentPlayerStr, standSfen)
                                   .arg(m_moveCount + 1);
            m_sfenHistory->append(fullSfen);
        }
    }

    // CSA形式から座標を抽出（ハイライト用）
    QPoint from, to;
    int toFile = 0, toRank = 0;
    if (move.length() >= 5) {
        int fromFile = move[1].digitValue();
        int fromRank = move[2].digitValue();
        toFile = move[3].digitValue();
        toRank = move[4].digitValue();

        const bool invalidTo = (toFile < 1 || toFile > 9 || toRank < 1 || toRank > 9);
        const bool invalidFrom = !((fromFile == 0 && fromRank == 0) ||
                                   (fromFile >= 1 && fromFile <= 9 &&
                                    fromRank >= 1 && fromRank <= 9));
        if (invalidTo || invalidFrom) {
            qCWarning(lcNetwork) << "Invalid confirmed CSA move coordinates:"
                                 << "fromFile=" << fromFile << "fromRank=" << fromRank
                                 << "toFile=" << toFile << "toRank=" << toRank
                                 << "move=" << move;
            emit logMessage(tr("不正な座標の指し手確認を受信しました: %1").arg(move), true);
            emit errorOccurred(tr("サーバーからの指し手確認の座標が不正です: %1").arg(move));
            setGameState(GameState::Error);
            return;
        }

        if (fromFile == 0 && fromRank == 0) {
            from = QPoint(-1, -1);
        } else {
            from = QPoint(fromFile, fromRank);
        }
        to = QPoint(toFile, toRank);
    }

    m_moveCount++;
    m_isMyTurn = false;
    emit turnChanged(false);

    emit moveHighlightRequested(from, to);

    bool isPromotion = m_gameController ? m_gameController->promote() : false;
    QString prettyMove = CsaMoveConverter::csaToPretty(move, isPromotion, m_prevToFile, m_prevToRank, m_moveCount - 1);

    m_prevToFile = toFile;
    m_prevToRank = toRank;

    emit moveMade(move, usiMove, prettyMove, consumedTimeMs);
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

    QString startSfen;
    QString parseError;
    const bool parsed = SfenCsaPositionConverter::fromCsaPositionLines(
        m_gameSummary.positionLines, &startSfen, &parseError);
    if (!parsed || startSfen.isEmpty()) {
        qCWarning(lcNetwork).noquote()
            << "CSA position parse failed. fallback to hirate. error=" << parseError;
        startSfen = hiratePosition;
    }
    m_gameController->board()->setSfen(startSfen);

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
// エンジン思考
// ============================================================

void CsaGameCoordinator::startEngineThinking()
{
    if (!m_engineController || !m_engineController->isInitialized()
        || m_playerType != PlayerType::Engine) {
        return;
    }

    // ポジションコマンドを構築
    QString positionCmd = m_positionStr;
    if (!m_usiMoves.isEmpty()) {
        positionCmd += QStringLiteral(" moves ") + m_usiMoves.join(QLatin1Char(' '));
    }

    // 時間パラメータを計算
    int byoyomiMs = m_gameSummary.byoyomi * m_gameSummary.timeUnitMs();
    int totalTimeMs = m_gameSummary.totalTime * m_gameSummary.timeUnitMs();
    int blackRemainMs = totalTimeMs - m_blackTotalTimeMs;
    int whiteRemainMs = totalTimeMs - m_whiteTotalTimeMs;
    if (blackRemainMs < 0) blackRemainMs = 0;
    if (whiteRemainMs < 0) whiteRemainMs = 0;
    int incMs = m_gameSummary.increment * m_gameSummary.timeUnitMs();

    CsaEngineController::ThinkingParams params;
    params.positionCmd = positionCmd;
    params.byoyomiMs = byoyomiMs;
    params.btimeStr = QString::number(blackRemainMs);
    params.wtimeStr = QString::number(whiteRemainMs);
    params.bincMs = incMs;
    params.wincMs = incMs;
    params.useByoyomi = (byoyomiMs > 0);

    emit logMessage(tr("エンジンが思考中..."));

    auto result = m_engineController->think(params);

    // 投了チェック
    if (result.resign) {
        emit logMessage(tr("エンジンが投了しました"));
        performResign();
        return;
    }

    // 有効な指し手かチェック
    if (!result.valid) {
        emit logMessage(tr("エンジンが有効な指し手を返しませんでした"), true);
        return;
    }

    // CSA形式の指し手を生成（盤面更新前に駒情報を取得）
    ShogiBoard* board = m_gameController ? m_gameController->board() : nullptr;
    if (!board) {
        emit logMessage(tr("盤面が取得できませんでした"), true);
        return;
    }

    int fromFile = result.from.x();
    int fromRank = result.from.y();
    int toFile = result.to.x();
    int toRank = result.to.y();

    QChar turnSign = m_isBlackSide ? QLatin1Char('+') : QLatin1Char('-');
    QString csaPiece;

    bool isDrop = (fromFile >= 10);
    if (isDrop) {
        Piece piece = board->getPieceCharacter(fromFile, fromRank);
        csaPiece = CsaMoveConverter::pieceCharToCsa(piece, false);
        fromFile = 0;
        fromRank = 0;
    } else {
        Piece piece = board->getPieceCharacter(fromFile, fromRank);
        csaPiece = CsaMoveConverter::pieceCharToCsa(piece, result.promote);
    }

    QString csaMove = QString("%1%2%3%4%5%6")
        .arg(turnSign).arg(fromFile).arg(fromRank)
        .arg(toFile).arg(toRank).arg(csaPiece);

    emit logMessage(tr("CSA形式の指し手: %1").arg(csaMove));

    // 盤面を更新
    if (!isDrop) {
        Piece movingPiece = board->getPieceCharacter(result.from.x(), result.from.y());
        Piece capturedPiece = board->getPieceCharacter(toFile, toRank);
        if (capturedPiece != Piece::None) {
            board->addPieceToStand(capturedPiece);
        }
        board->movePieceToSquare(movingPiece, result.from.x(), result.from.y(), toFile, toRank, result.promote);
    } else {
        Piece dropPiece = board->getPieceCharacter(result.from.x(), result.from.y());
        board->decrementPieceOnStand(dropPiece);
        board->movePieceToSquare(dropPiece, 0, 0, toFile, toRank, false);
    }

    m_gameController->changeCurrentPlayer();
    if (m_view) m_view->update();

    // 評価値更新
    int ply = m_moveCount + 1;
    emit engineScoreUpdated(result.scoreCp, ply);

    // サーバーに送信
    m_client->sendMove(csaMove);
}

// ============================================================
// クリーンアップ
// ============================================================

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
