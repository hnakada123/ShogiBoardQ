/// @file csagamecoordinator_setup.cpp
/// @brief CSA通信対局コーディネータのセットアップ系実装

#include "csagamecoordinator.h"

#include "csaenginecontroller.h"
#include "csamoveconverter.h"
#include "csamoveprogresshandler.h"
#include "logcategories.h"
#include "parsecommon.h"
#include "sfencsapositionconverter.h"
#include "sfenutils.h"
#include "shogiboard.h"
#include "shogiclock.h"
#include "shogigamecontroller.h"
#include "shogiview.h"

namespace {
bool isInitialPromotionMove(const QString& csaMove, ShogiGameController* gameController)
{
    if (!gameController || !gameController->board() || csaMove.length() < 7) {
        return false;
    }

    const int fromFile = csaMove[1].digitValue();
    const int fromRank = csaMove[2].digitValue();
    if (fromFile == 0 && fromRank == 0) {
        return false;
    }

    const QString destPiece = csaMove.mid(5, 2);
    static const QStringList promotedPieces = {
        QStringLiteral("TO"), QStringLiteral("NY"), QStringLiteral("NK"),
        QStringLiteral("NG"), QStringLiteral("UM"), QStringLiteral("RY")
    };
    if (!promotedPieces.contains(destPiece)) {
        return false;
    }

    const Piece srcPiece = gameController->board()->pieceCharacter(fromFile, fromRank);
    static const QString unpromoted = QStringLiteral("PLNSBRplnsbr");
    return unpromoted.contains(pieceToChar(srcPiece));
}
} // namespace

void CsaGameCoordinator::setGameState(GameState state)
{
    if (m_gameState != state) {
        m_gameState = state;
        emit gameStateChanged(state);
    }
}

void CsaGameCoordinator::setupInitialPosition()
{
    if (!m_gameController || !m_gameController->board()) {
        return;
    }

    const QString hiratePosition = SfenUtils::hirateSfen();

    QString parseError;
    auto startSfen = SfenCsaPositionConverter::fromCsaPositionLines(
        m_gameSummary.positionLines, &parseError);
    if (!startSfen || startSfen->isEmpty()) {
        qCWarning(lcNetwork).noquote()
            << "CSA position parse failed. fallback to hirate. error=" << parseError;
        startSfen = hiratePosition;
    }
    m_gameController->board()->setSfen(*startSfen);
    m_gameController->setCurrentPlayer(m_gameController->board()->currentPlayer() == Turn::Black
        ? ShogiGameController::Player1 : ShogiGameController::Player2);
    if (m_gameMoves) m_gameMoves->clear();

    const QString boardSfen = m_gameController->board()->convertBoardToSfen();
    const QString standSfen = m_gameController->board()->convertStandToSfen();
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
    m_initialPrettyMoves.clear();
    m_prevToFile = 0;
    m_prevToRank = 0;

    int replayMoveCount = 0;
    for (const QString& move : std::as_const(m_gameSummary.moves)) {
        const bool isPromotion = isInitialPromotionMove(move, m_gameController);
        const QString prettyMove = CsaMoveConverter::csaToPretty(
            move, isPromotion, m_prevToFile, m_prevToRank, replayMoveCount);

        if (!CsaMoveConverter::applyMoveToBoard(move, m_gameController, m_usiMoves, m_sfenHistory,
                                                replayMoveCount)) {
            qCWarning(lcNetwork).noquote()
                << "Failed to replay CSA move from Game_Summary:" << move;
            emit logMessage(tr("初期局面の指し手再生に失敗しました: %1").arg(move), true);
            emit errorOccurred(tr("サーバーから受信した初期局面データが不正です: %1").arg(move));
            setGameState(GameState::Error);
            return;
        }

        if (move.length() >= 5) {
            m_prevToFile = move[3].digitValue();
            m_prevToRank = move[4].digitValue();
        }
        m_initialPrettyMoves.append(prettyMove);
        ++replayMoveCount;
    }
    m_moveCount = replayMoveCount;

    if (m_view) {
        m_view->update();
    }
}

void CsaGameCoordinator::setupClock()
{
    const CsaClient::GameSummary& s = m_gameSummary;

    // 再開局面（仕様 3.2.3）: 初期局面と規定の持時間から、Game_Summary の指し手が
    // 既に完了した状態にする。各手の消費時間を引き、各手番の開始時に加算時間を足す
    int remainingBlackMs = s.totalTimeMs(true);
    int remainingWhiteMs = s.totalTimeMs(false);
    int consumedBlackMs = 0;
    int consumedWhiteMs = 0;
    for (int i = 0; i < s.moves.size(); ++i) {
        const bool black = s.moves.at(i).startsWith(QLatin1Char('+'));
        int& remaining = black ? remainingBlackMs : remainingWhiteMs;
        int& consumed = black ? consumedBlackMs : consumedWhiteMs;
        const int consumedMs = s.positionMoveConsumedMs(i);
        remaining += s.incrementMs(black);
        consumed += consumedMs;
        // 残り時間を超えた分は秒読みで指したものとして、残りは 0 にする（仕様 3.2.2.1）
        remaining = qMax(0, remaining - consumedMs);
    }

    const bool blackToMove = (s.toMove == QStringLiteral("+"));
    // 手番側には、これから始まる手番の加算時間を足す
    if (blackToMove) remainingBlackMs += s.incrementMs(true);
    else remainingWhiteMs += s.incrementMs(false);

    m_initialBlackTimeMs = s.totalTimeMs(true);
    m_initialWhiteTimeMs = s.totalTimeMs(false);
    m_blackTotalTimeMs = consumedBlackMs;
    m_whiteTotalTimeMs = consumedWhiteMs;
    m_blackRemainingMs = remainingBlackMs;
    m_whiteRemainingMs = remainingWhiteMs;

    if (!m_clock) {
        return;
    }

    CsaMoveProgressHandler::applyClockTimes(m_clock, s, remainingBlackMs, remainingWhiteMs);
    // 終局の裁定はサーバーを正とし、表示用時計の時間切れで対局を終了しない。
    m_clock->setLoseOnTimeout(false);
    m_clock->setCurrentPlayer(blackToMove ? 1 : 2);
    m_clock->startClock();
}


// ============================================================
// 対局条件の確認とエンジン起動
// ============================================================

void CsaGameCoordinator::startEngine()
{
    if (!m_engineController) {
        m_engineController = new CsaEngineController(this);
        connect(m_engineController, &CsaEngineController::logMessage,
                this, &CsaGameCoordinator::logMessage);
        connect(m_engineController, &CsaEngineController::initialized,
                this, &CsaGameCoordinator::onEngineControllerInitialized);
        connect(m_engineController, &CsaEngineController::engineError,
                this, &CsaGameCoordinator::onEngineControllerError);
        connect(m_engineController, &CsaEngineController::resignRequested,
                this, &CsaGameCoordinator::onEngineControllerResign);
        connect(m_engineController, &CsaEngineController::winDeclarationRequested,
                this, &CsaGameCoordinator::onEngineControllerWinDeclaration);
    }
    CsaEngineController::InitParams params;
    params.enginePath = m_options.enginePath;
    params.engineName = m_options.engineName;
    params.engineNumber = m_options.engineNumber;
    params.gameController = m_gameController;
    params.commLog = m_usiCommLog;
    params.thinkingModel = m_engineThinking;
    // USI初期化の待機中にもTCPのGame_Summaryが届く。readyokを待って合意する。
    m_initializingEngine = true;
    m_engineNeedsRestart = false;
    m_engineController->initialize(params);
}

QString CsaGameCoordinator::unsupportedConditionReason(const CsaClient::GameSummary& summary,
                                                      PlayerType playerType)
{
    // Format は Position の書式と指し手表記を決める。将棋以外は解釈できない
    if (!summary.format.isEmpty() && !summary.format.startsWith(QStringLiteral("Shogi"))) {
        return tr("未対応の Format です: %1").arg(summary.format);
    }
    // 時間無制限ではエンジンに思考時間を与えられない（ローカル対局と同じ方針）
    if (playerType == PlayerType::Engine && summary.isTimeUnlimited()) {
        return tr("時間無制限の対局はエンジンでは行えません");
    }
    return QString();
}

void CsaGameCoordinator::logGameConditions(const CsaClient::GameSummary& summary)
{
    if (summary.isTimeUnlimited()) {
        emit logMessage(tr("持時間: 無制限"));
    } else {
        for (const bool black : {true, false}) {
            if (!black && !summary.hasIndividualTime) break;
            const CsaClient::TimeControl& t = summary.timeFor(black);
            QString text = tr("持時間: %1秒, 秒読み: %2秒, 加算: %3秒")
                               .arg(t.totalTimeMs() / 1000)
                               .arg(t.byoyomiMs() / 1000)
                               .arg(t.incrementMs() / 1000);
            if (t.delay > 0) text += tr(", 遅延: %1秒").arg(t.delayMs() / 1000);
            if (t.leastTimePerMove > 0) text += tr(", 最少消費: %1秒").arg(t.leastTimePerMoveMs() / 1000);
            if (summary.hasIndividualTime) {
                text = (black ? tr("先手の") : tr("後手の")) + text;
            }
            emit logMessage(text);
        }
    }
    if (summary.maxMoves > 0) {
        emit logMessage(tr("最大手数: %1手").arg(summary.maxMoves));
    }
    emit logMessage(summary.allowsJishogiDeclaration()
                        ? tr("入玉宣言ルール: %1").arg(summary.declaration)
                        : tr("入玉宣言ルール: なし"));
    if (!summary.protocolVersion.isEmpty() && !summary.protocolVersion.startsWith(QStringLiteral("1."))) {
        emit logMessage(tr("未知のプロトコルバージョンです: %1").arg(summary.protocolVersion), true);
    }
    if (summary.protocolMode != QStringLiteral("Server")) {
        emit logMessage(tr("Protocol_Mode が Server ではありません: %1").arg(summary.protocolMode), true);
    }
}

void CsaGameCoordinator::onGameSummaryReceived(const CsaClient::GameSummary& summary)
{
    m_gameSummary = summary;
    m_isBlackSide = summary.isBlackTurn();
    // 終局後も同じ接続で次の対局条件を受けるので、前局の記録はここで消す
    resetGameTracking();

    emit logMessage(tr("対局条件を受信しました"));
    emit logMessage(tr("先手: %1, 後手: %2").arg(summary.blackName, summary.whiteName));
    logGameConditions(summary);

    setGameState(GameState::WaitingForAgree);

    if (!moreGamesWanted()) {
        emit logMessage(tr("予定の対局数を終えているので、対局条件を拒否します"), true);
        m_client->reject(summary.gameId);
        setGameState(GameState::WaitingForMatch);
        m_client->disconnectFromServer();
        return;
    }

    const QString rejectReason = unsupportedConditionReason(summary, m_playerType);
    if (!rejectReason.isEmpty()) {
        emit logMessage(tr("対局条件を拒否します: %1").arg(rejectReason), true);
        m_client->reject(summary.gameId);
        setGameState(GameState::WaitingForMatch);
        return;
    }

    if (m_playerType == PlayerType::Engine
        && (m_engineNeedsRestart || !m_engineController || !m_engineController->isInitialized())) {
        // 前局の終局で quit したエンジンを起動し直す
        startEngine();
    }

    emit logMessage(tr("対局条件に同意します..."));
    if (!m_initializingEngine) m_client->agree(summary.gameId);
}

QString CsaGameCoordinator::initialMoveElapsedText(int index) const
{
    // 再開局面の各手の消費時間と、その側の累計（「１手 / 合計」の表記）
    const CsaClient::GameSummary& s = m_gameSummary;
    if (index < 0 || index >= s.moves.size()) return QString();
    const bool black = s.moves.at(index).startsWith(QLatin1Char('+'));
    int totalMs = 0;
    for (int i = 0; i <= index; ++i) {
        if (s.moves.at(i).startsWith(QLatin1Char('+')) == black) totalMs += s.positionMoveConsumedMs(i);
    }
    return KifuParseCommon::formatTimeText(s.positionMoveConsumedMs(index), totalMs);
}

// ============================================================
// 対局数と再接続
// ============================================================

bool CsaGameCoordinator::moreGamesWanted() const
{
    return m_options.gameLimit <= 0 || m_completedGames < m_options.gameLimit;
}

void CsaGameCoordinator::connectForGame()
{
    m_lastGameEndedByServer = false;
    setGameState(GameState::Connecting);
    emit logMessage(tr("サーバー %1:%2 に接続中...").arg(m_options.host).arg(m_options.port));
    m_client->connectToServer(m_options.host, m_options.port);
}

void CsaGameCoordinator::handleGameCountAfterEnd()
{
    if (!moreGamesWanted()) {
        emit logMessage(tr("予定の対局数（%1局）を終えたのでログアウトします").arg(m_completedGames));
        m_client->disconnectFromServer();
        return;
    }
    const QString progress = m_options.gameLimit > 0
        ? tr("%1/%2局").arg(m_completedGames).arg(m_options.gameLimit)
        : tr("%1局目まで終了").arg(m_completedGames);
    if (m_client->isConnected()) {
        emit logMessage(tr("次の対局条件を待ちます（%1）").arg(progress));
    }
}

void CsaGameCoordinator::reconnectForNextGame()
{
    if (m_gameState != GameState::GameOver || !moreGamesWanted()) return;
    if (m_client->connectionState() != CsaClient::ConnectionState::Disconnected) return;
    emit logMessage(tr("次の対局のために再接続します（%1局目まで終了）").arg(m_completedGames));
    connectForGame();
}

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
    refs.thinkingMarginMs = &m_thinkingMarginMs;
    m_moveProgressHandler->setRefs(refs);

    CsaMoveProgressHandler::Hooks hooks;
    hooks.logMessage = [this](const QString& msg, bool isError) {
        emit logMessage(msg, isError);
    };
    hooks.failGame = [this](const QString& msg) { failGame(msg); };
    hooks.moveMade = [this](const QString& csa, const QString& usi,
                            const QString& pretty, int ms) {
        emit moveMade(csa, usi, pretty, ms);
    };
    hooks.turnChanged = [this](bool myTurn) { emit turnChanged(myTurn); };
    hooks.moveHighlightRequested = [this](const QPoint& f, const QPoint& t) {
        emit moveHighlightRequested(f, t);
    };
    hooks.moveAppliedToBoard = [this]() { emit moveAppliedToBoard(); };
    hooks.engineScoreUpdated = [this](int scoreCp, int ply, const QString& mate) {
        emit engineScoreUpdated(scoreCp, ply, mate);
    };
    hooks.performResign = [this]() { performResign(); };
    m_moveProgressHandler->setHooks(hooks);
}

void CsaGameCoordinator::cleanup()
{
    if (m_moveProgressHandler) {
        m_moveProgressHandler->clearPendingVerdict();
    }
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

    m_client->sendRawCommand(command);
}
