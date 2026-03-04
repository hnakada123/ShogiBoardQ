/// @file csagamecoordinator_setup.cpp
/// @brief CSA通信対局コーディネータのセットアップ系実装

#include "csagamecoordinator.h"

#include "csaenginecontroller.h"
#include "csamoveconverter.h"
#include "csamoveprogresshandler.h"
#include "logcategories.h"
#include "sfencsapositionconverter.h"
#include "sfenutils.h"
#include "shogiboard.h"
#include "shogiclock.h"
#include "shogigamecontroller.h"
#include "shogiview.h"

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

    int replayMoveCount = 0;
    for (const QString& move : std::as_const(m_gameSummary.moves)) {
        if (!CsaMoveConverter::applyMoveToBoard(move, m_gameController, m_usiMoves, m_sfenHistory,
                                                replayMoveCount)) {
            qCWarning(lcNetwork).noquote()
                << "Failed to replay CSA move from Game_Summary:" << move;
            emit logMessage(tr("初期局面の指し手再生に失敗しました: %1").arg(move), true);
            emit errorOccurred(tr("サーバーから受信した初期局面データが不正です: %1").arg(move));
            setGameState(GameState::Error);
            return;
        }
        ++replayMoveCount;
    }
    m_moveCount = replayMoveCount;

    if (m_view) {
        m_view->update();
    }
}

void CsaGameCoordinator::setupClock()
{
    if (!m_clock) {
        return;
    }

    const int timeUnitMs = m_gameSummary.timeUnitMs();
    const int totalTimeBlackUnits = m_gameSummary.hasIndividualTime
        ? m_gameSummary.totalTimeBlack
        : m_gameSummary.totalTime;
    const int totalTimeWhiteUnits = m_gameSummary.hasIndividualTime
        ? m_gameSummary.totalTimeWhite
        : m_gameSummary.totalTime;
    const int byoyomiBlackUnits = m_gameSummary.hasIndividualTime
        ? m_gameSummary.byoyomiBlack
        : m_gameSummary.byoyomi;
    const int byoyomiWhiteUnits = m_gameSummary.hasIndividualTime
        ? m_gameSummary.byoyomiWhite
        : m_gameSummary.byoyomi;

    const int totalTimeBlackSec = totalTimeBlackUnits * timeUnitMs / 1000;
    const int totalTimeWhiteSec = totalTimeWhiteUnits * timeUnitMs / 1000;
    const int byoyomiBlackSec = byoyomiBlackUnits * timeUnitMs / 1000;
    const int byoyomiWhiteSec = byoyomiWhiteUnits * timeUnitMs / 1000;
    const int incrementSec = m_gameSummary.increment * timeUnitMs / 1000;

    m_clock->setPlayerTimes(totalTimeBlackSec, totalTimeWhiteSec,
                            byoyomiBlackSec, byoyomiWhiteSec,
                            incrementSec, incrementSec,
                            true);

    m_initialBlackTimeMs = totalTimeBlackSec * 1000;
    m_initialWhiteTimeMs = totalTimeWhiteSec * 1000;
    m_blackRemainingMs = m_initialBlackTimeMs;
    m_whiteRemainingMs = m_initialWhiteTimeMs;

    const bool blackToMove = (m_gameSummary.toMove == QStringLiteral("+"));
    m_clock->setCurrentPlayer(blackToMove ? 1 : 2);
    m_clock->startClock();
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
