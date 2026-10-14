/// @file gameendhandler.cpp
/// @brief 終局処理ハンドラの実装

#include "gameendhandler.h"
#include "nyugyokujudgement.h"
#include "shogiboard.h"
#include "shogigamecontroller.h"
#include "shogiclock.h"
#include "usi.h"
#include "enginegameovernotifier.h"
#include "sennichitedetector.h"
#include "enginevsenginestrategy.h"
#include "logcategories.h"

#include <QDateTime>

GameEndHandler::GameEndHandler(QObject* parent)
    : QObject(parent)
{
}

void GameEndHandler::setRefs(const Refs& refs)
{
    m_refs = refs;
}

void GameEndHandler::setHooks(const Hooks& hooks)
{
    m_hooks = hooks;
}

// --- private ヘルパー ---

void GameEndHandler::sendRawToEngineHelper(Usi* which, const QString& cmd)
{
    if (!which) return;
    which->sendRaw(cmd);
}

// --- 投了 ---

void GameEndHandler::handleResign()
{
    if (m_refs.gameOver->isOver) {
        qCDebug(lcGame) << "handleResign: already game over, ignoring";
        return;
    }

    GameEndInfo info;
    info.cause = Cause::Resignation;
    info.loser = (m_refs.gc && m_refs.gc->currentPlayer() == Turn::Black)
                     ? Turn::Black : Turn::White;

    auto rawSender = [this](Usi* which, const QString& cmd) {
        sendRawToEngineHelper(which, cmd);
    };
    Usi* u1 = m_refs.usi1Provider ? m_refs.usi1Provider() : nullptr;
    Usi* u2 = m_refs.usi2Provider ? m_refs.usi2Provider() : nullptr;
    EngineGameOverNotifier::notifyResignation(
        *m_refs.playMode, info.loser, u1, u2, rawSender);

    setGameOver(info, true);
    displayResultsAndUpdateGui(info);
}

void GameEndHandler::handleEngineResign(Turn side)
{
    if (m_refs.gameOver->isOver) return;
    if (m_refs.clock) m_refs.clock->stopClock();

    GameEndInfo info;
    info.cause = Cause::Resignation;
    info.loser = side;

    auto rawSender = [this](Usi* which, const QString& cmd) {
        sendRawToEngineHelper(which, cmd);
    };
    Usi* u1 = m_refs.usi1Provider ? m_refs.usi1Provider() : nullptr;
    Usi* u2 = m_refs.usi2Provider ? m_refs.usi2Provider() : nullptr;
    EngineGameOverNotifier::notifyResignation(
        *m_refs.playMode, info.loser, u1, u2, rawSender);
    if (u1) u1->setSquelchResignLogging(true);
    if (u2) u2->setSquelchResignLogging(true);

    setGameOver(info, true);
    displayResultsAndUpdateGui(info);
}

void GameEndHandler::handleEngineIllegalMove(Turn side)
{
    if (m_refs.gameOver->isOver) return;
    if (m_refs.clock) m_refs.clock->stopClock();

    // エンジンが反則手を返した。盤には指さずに、そのエンジンの反則負けとする
    GameEndInfo info;
    info.cause = Cause::IllegalMove;
    info.loser = side;

    auto rawSender = [this](Usi* which, const QString& cmd) {
        sendRawToEngineHelper(which, cmd);
    };
    Usi* u1 = m_refs.usi1Provider ? m_refs.usi1Provider() : nullptr;
    Usi* u2 = m_refs.usi2Provider ? m_refs.usi2Provider() : nullptr;
    // 投了と同じく、負けた側に gameover lose、相手に gameover win を送る
    EngineGameOverNotifier::notifyResignation(
        *m_refs.playMode, info.loser, u1, u2, rawSender);
    if (u1) u1->setSquelchResignLogging(true);
    if (u2) u2->setSquelchResignLogging(true);

    setGameOver(info, true);
    displayResultsAndUpdateGui(info);
}

void GameEndHandler::handleTimeout(Turn loser)
{
    if (m_refs.gameOver->isOver) return;

    const GameEndInfo info{Cause::Timeout, loser};
    setGameOver(info, true);
    if (m_refs.clock) {
        m_refs.clock->markGameOver();
        m_refs.clock->stopClock();
    }
    if (m_hooks.disarmHumanTimerIfNeeded) m_hooks.disarmHumanTimerIfNeeded();

    const auto notify = [loser](Usi* engine, Turn side) {
        if (!engine) return;
        engine->setSquelchResignLogging(true);
        engine->cancelCurrentOperation();
        engine->sendStopCommand();
        engine->sendGameOverCommand(side == loser ? GameOverResult::Lose : GameOverResult::Win);
        engine->sendQuitCommand();
    };
    Usi* u1 = m_refs.usi1Provider ? m_refs.usi1Provider() : nullptr;
    Usi* u2 = m_refs.usi2Provider ? m_refs.usi2Provider() : nullptr;
    const PlayMode mode = *m_refs.playMode;
    if (mode == PlayMode::EvenEngineVsEngine || mode == PlayMode::HandicapEngineVsEngine) {
        notify(u1, Turn::Black);
        notify(u2, Turn::White);
    } else if (mode == PlayMode::EvenEngineVsHuman || mode == PlayMode::HandicapEngineVsHuman) {
        notify(u1, Turn::Black);
    } else if (mode == PlayMode::EvenHumanVsEngine || mode == PlayMode::HandicapHumanVsEngine) {
        notify(u1, Turn::White);
    }
    displayResultsAndUpdateGui(info);
}

void GameEndHandler::handleEngineWin(Turn side)
{
    if (m_refs.gameOver->isOver) return;
    const Turn declarer = side;
    const ShogiBoard* board = m_refs.gc ? m_refs.gc->board() : nullptr;
    if (!board) {
        handleNyugyokuDeclaration(declarer, true, false);
        if (m_refs.gameOver->isOver && m_hooks.showGameOverDialog) {
            m_hooks.showGameOverDialog(tr("対局終了"), resultMessage(m_refs.gameOver->lastInfo));
        }
        return;
    }

    // エンジンの入玉宣言（bestmove win）も人間の宣言と同じく、盤面から条件と点数を判定する。
    // 条件を満たさなければ宣言したエンジンの負け、24点法の24〜30点は持将棋。
    const NyugyokuJudgement::Result result =
        NyugyokuJudgement::judgeEngineDeclaration(*board, declarer == Turn::Black, usesHandicapNames());
    handleNyugyokuDeclaration(declarer, result.success, result.isDraw);
    // 人間の宣言は宣言の結果ダイアログで知らせる。エンジンの宣言はここで判定結果を知らせる
    if (m_refs.gameOver->isOver && m_hooks.showGameOverDialog) {
        m_hooks.showGameOverDialog(NyugyokuJudgement::resultTitle(), result.message);
    }
}

// --- 入玉宣言 ---

void GameEndHandler::handleNyugyokuDeclaration(Turn declarer, bool success, bool isDraw)
{
    if (m_refs.gameOver->isOver) return;

    qCInfo(lcGame) << "handleNyugyokuDeclaration(): declarer=" << declarer
                    << " success=" << success << " isDraw=" << isDraw;

    if (m_hooks.disarmHumanTimerIfNeeded) m_hooks.disarmHumanTimerIfNeeded();
    if (m_refs.clock) m_refs.clock->stopClock();

    GameEndInfo info;
    if (isDraw) {
        info.cause = Cause::Jishogi;
        info.loser = declarer;
    } else if (success) {
        info.cause = Cause::NyugyokuWin;
        info.loser = (declarer == Turn::Black) ? Turn::White : Turn::Black;
    } else {
        info.cause = Cause::IllegalMove;
        info.loser = declarer;
    }

    auto rawSender = [this](Usi* which, const QString& cmd) {
        sendRawToEngineHelper(which, cmd);
    };
    Usi* u1 = m_refs.usi1Provider ? m_refs.usi1Provider() : nullptr;
    Usi* u2 = m_refs.usi2Provider ? m_refs.usi2Provider() : nullptr;
    EngineGameOverNotifier::notifyNyugyoku(
        *m_refs.playMode, isDraw, info.loser, u1, u2, rawSender);
    if (u1) u1->setSquelchResignLogging(true);
    if (u2) u2->setSquelchResignLogging(true);

    setGameOver(info, true);
    if (m_hooks.autoSaveKifuIfEnabled) m_hooks.autoSaveKifuIfEnabled();
    emit gameEndProcessed(info);
}

// --- 中断 ---

void GameEndHandler::handleBreakOff()
{
    // エンジン投了シグナルの切断は MatchCoordinator 側で実施済み

    if (m_refs.gameOver->isOver) return;

    m_refs.gameOver->isOver = true;
    m_refs.gameOver->when = QDateTime::currentDateTime();
    m_refs.gameOver->hasLast = true;
    m_refs.gameOver->lastInfo.cause = Cause::BreakOff;

    const Turn gcTurn = m_refs.gc ? m_refs.gc->currentPlayer() : Turn::White;
    m_refs.gameOver->lastInfo.loser = gcTurn;

    if (m_refs.clock) {
        m_refs.clock->markGameOver();
        m_refs.clock->stopClock();
    }

    if (m_hooks.disarmHumanTimerIfNeeded) m_hooks.disarmHumanTimerIfNeeded();

    Usi* u1 = m_refs.usi1Provider ? m_refs.usi1Provider() : nullptr;
    Usi* u2 = m_refs.usi2Provider ? m_refs.usi2Provider() : nullptr;

    const bool isEvE =
        (*m_refs.playMode == PlayMode::EvenEngineVsEngine) ||
        (*m_refs.playMode == PlayMode::HandicapEngineVsEngine);

    if (isEvE) {
        if (u1) u1->sendStopCommand();
        if (u2) u2->sendStopCommand();
        if (u1) { u1->sendGameOverCommand(GameOverResult::Draw); u1->setSquelchResignLogging(true); }
        if (u2) { u2->sendGameOverCommand(GameOverResult::Draw); u2->setSquelchResignLogging(true); }
    } else {
        if (Usi* eng = m_hooks.primaryEngine ? m_hooks.primaryEngine() : nullptr) {
            eng->sendStopCommand();
            eng->sendGameOverCommand(GameOverResult::Draw);
            eng->setSquelchResignLogging(true);
        }
    }

    emit gameEnded(m_refs.gameOver->lastInfo);

    appendBreakOffLineAndMark();

    // quitに応答しない場合もterminate/killまで進める。中断後の思考表示は保持する。
    if (u1) u1->cleanupEngineProcessAndThread(false);
    if (u2) u2->cleanupEngineProcessAndThread(false);
}

// --- 持将棋 ---

void GameEndHandler::handleMaxMovesJishogi()
{
    if (m_refs.gameOver->isOver) return;

    qCInfo(lcGame) << "handleMaxMovesJishogi(): max moves reached";

    if (m_hooks.disarmHumanTimerIfNeeded) m_hooks.disarmHumanTimerIfNeeded();

    Usi* u1 = m_refs.usi1Provider ? m_refs.usi1Provider() : nullptr;
    Usi* u2 = m_refs.usi2Provider ? m_refs.usi2Provider() : nullptr;

    const bool isEvE =
        (*m_refs.playMode == PlayMode::EvenEngineVsEngine) ||
        (*m_refs.playMode == PlayMode::HandicapEngineVsEngine);

    if (isEvE) {
        if (u1) { u1->sendGameOverCommand(GameOverResult::Draw); u1->sendQuitCommand(); u1->setSquelchResignLogging(true); }
        if (u2) { u2->sendGameOverCommand(GameOverResult::Draw); u2->sendQuitCommand(); u2->setSquelchResignLogging(true); }
    } else {
        if (Usi* eng = m_hooks.primaryEngine ? m_hooks.primaryEngine() : nullptr) {
            eng->sendGameOverCommand(GameOverResult::Draw);
            eng->sendQuitCommand();
            eng->setSquelchResignLogging(true);
        }
    }

    GameEndInfo info;
    info.cause = Cause::Jishogi;
    info.loser = Turn::Black;

    setGameOver(info, true);

    appendGameOverLineAndMark(Cause::Jishogi, Turn::Black);
    displayResultsAndUpdateGui(info);
}

// --- 千日手 ---

bool GameEndHandler::checkAndHandleSennichite()
{
    if (m_refs.gameOver->isOver) return false;

    const QStringList* rec = m_refs.sfenHistory;
    if (!rec || rec->size() < 4) return false;

    const auto result = SennichiteDetector::check(*rec);
    switch (result) {
    case SennichiteDetector::Result::None:
        return false;
    case SennichiteDetector::Result::Draw:
        handleSennichite();
        return true;
    case SennichiteDetector::Result::ContinuousCheckByBlack:
        handleOuteSennichite(Turn::Black);
        return true;
    case SennichiteDetector::Result::ContinuousCheckByWhite:
        handleOuteSennichite(Turn::White);
        return true;
    }
    return false;
}

void GameEndHandler::handleSennichite()
{
    if (m_refs.gameOver->isOver) return;

    qCInfo(lcGame) << "handleSennichite(): draw by repetition";

    if (m_hooks.disarmHumanTimerIfNeeded) m_hooks.disarmHumanTimerIfNeeded();

    Usi* u1 = m_refs.usi1Provider ? m_refs.usi1Provider() : nullptr;
    Usi* u2 = m_refs.usi2Provider ? m_refs.usi2Provider() : nullptr;

    const bool isEvE =
        (*m_refs.playMode == PlayMode::EvenEngineVsEngine) ||
        (*m_refs.playMode == PlayMode::HandicapEngineVsEngine);

    if (isEvE) {
        if (u1) { u1->sendGameOverCommand(GameOverResult::Draw); u1->sendQuitCommand(); u1->setSquelchResignLogging(true); }
        if (u2) { u2->sendGameOverCommand(GameOverResult::Draw); u2->sendQuitCommand(); u2->setSquelchResignLogging(true); }
    } else {
        if (Usi* eng = m_hooks.primaryEngine ? m_hooks.primaryEngine() : nullptr) {
            eng->sendGameOverCommand(GameOverResult::Draw);
            eng->sendQuitCommand();
            eng->setSquelchResignLogging(true);
        }
    }

    GameEndInfo info;
    info.cause = Cause::Sennichite;
    info.loser = Turn::Black;

    setGameOver(info, true);

    appendGameOverLineAndMark(Cause::Sennichite, Turn::Black);
    displayResultsAndUpdateGui(info);
}

void GameEndHandler::handleOuteSennichite(Turn loser)
{
    if (m_refs.gameOver->isOver) return;

    const Turn winner = (loser == Turn::Black) ? Turn::White : Turn::Black;

    qCInfo(lcGame) << "handleOuteSennichite(): continuous check by" << loser;

    if (m_hooks.disarmHumanTimerIfNeeded) m_hooks.disarmHumanTimerIfNeeded();

    Usi* u1 = m_refs.usi1Provider ? m_refs.usi1Provider() : nullptr;
    Usi* u2 = m_refs.usi2Provider ? m_refs.usi2Provider() : nullptr;

    const bool isEvE =
        (*m_refs.playMode == PlayMode::EvenEngineVsEngine) ||
        (*m_refs.playMode == PlayMode::HandicapEngineVsEngine);

    if (isEvE) {
        if (u1) {
            const auto result = (winner == Turn::Black) ? GameOverResult::Win : GameOverResult::Lose;
            u1->sendGameOverCommand(result); u1->sendQuitCommand(); u1->setSquelchResignLogging(true);
        }
        if (u2) {
            const auto result = (winner == Turn::White) ? GameOverResult::Win : GameOverResult::Lose;
            u2->sendGameOverCommand(result); u2->sendQuitCommand(); u2->setSquelchResignLogging(true);
        }
    } else {
        if (Usi* eng = m_hooks.primaryEngine ? m_hooks.primaryEngine() : nullptr) {
            const bool engineIsBlack =
                (*m_refs.playMode == PlayMode::EvenEngineVsHuman) ||
                (*m_refs.playMode == PlayMode::HandicapEngineVsHuman);
            const Turn engineSide = engineIsBlack ? Turn::Black : Turn::White;
            const auto result = (winner == engineSide) ? GameOverResult::Win : GameOverResult::Lose;
            eng->sendGameOverCommand(result); eng->sendQuitCommand(); eng->setSquelchResignLogging(true);
        }
    }

    GameEndInfo info;
    info.cause = Cause::OuteSennichite;
    info.loser = loser;

    setGameOver(info, true);

    appendGameOverLineAndMark(Cause::OuteSennichite, loser);
    displayResultsAndUpdateGui(info);
}

// --- 終局状態管理 ---

void GameEndHandler::clearGameOverState()
{
    const bool wasOver = m_refs.gameOver->isOver;
    *m_refs.gameOver = GameOverState{};
    if (wasOver) {
        emit gameOverStateChanged(*m_refs.gameOver);
        qCDebug(lcGame) << "clearGameOverState()";
    }
}

void GameEndHandler::setGameOver(const GameEndInfo& info, bool appendMoveOnce)
{
    if (m_refs.gameOver->isOver) {
        qCDebug(lcGame) << "setGameOver() ignored: already over";
        return;
    }

    qCDebug(lcGame).nospace()
        << "setGameOver cause="
        << ((info.cause==Cause::Timeout)?"Timeout":"Resign")
        << " loser=" << info.loser
        << " appendMoveOnce=" << appendMoveOnce;

    m_refs.gameOver->isOver        = true;
    m_refs.gameOver->hasLast       = true;
    m_refs.gameOver->lastInfo      = info;
    m_refs.gameOver->when          = QDateTime::currentDateTime();

    emit gameOverStateChanged(*m_refs.gameOver);

    if (appendMoveOnce && !m_refs.gameOver->moveAppended) {
        qCDebug(lcGame) << "emit requestAppendGameOverMove";
        emit requestAppendGameOverMove(info);
    }
    // 終局手の追記・ライブセッションの確定後に時刻と UI を更新する。
    emit gameEnded(info);
}

void GameEndHandler::markGameOverMoveAppended()
{
    if (!m_refs.gameOver->isOver) return;
    if (m_refs.gameOver->moveAppended) return;

    m_refs.gameOver->moveAppended = true;
    emit gameOverStateChanged(*m_refs.gameOver);
    qCDebug(lcGame) << "markGameOverMoveAppended()";
}
