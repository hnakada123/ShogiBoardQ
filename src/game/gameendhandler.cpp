/// @file gameendhandler.cpp
/// @brief 終局処理ハンドラの実装

#include "gameendhandler.h"
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
    info.loser = (m_refs.gc && m_refs.gc->currentPlayer() == ShogiGameController::Player1)
                     ? Player::P1 : Player::P2;

    auto rawSender = [this](Usi* which, const QString& cmd) {
        sendRawToEngineHelper(which, cmd);
    };
    Usi* u1 = m_refs.usi1Provider ? m_refs.usi1Provider() : nullptr;
    Usi* u2 = m_refs.usi2Provider ? m_refs.usi2Provider() : nullptr;
    EngineGameOverNotifier::notifyResignation(
        *m_refs.playMode, info.loser == Player::P1, u1, u2, rawSender);

    setGameOver(info, info.loser == Player::P1, true);
    displayResultsAndUpdateGui(info);
}

void GameEndHandler::handleEngineResign(int idx)
{
    if (m_refs.gameOver->isOver) return;
    if (m_refs.clock) m_refs.clock->stopClock();

    GameEndInfo info;
    info.cause = Cause::Resignation;
    info.loser = (idx == 1 ? Player::P1 : Player::P2);

    auto rawSender = [this](Usi* which, const QString& cmd) {
        sendRawToEngineHelper(which, cmd);
    };
    Usi* u1 = m_refs.usi1Provider ? m_refs.usi1Provider() : nullptr;
    Usi* u2 = m_refs.usi2Provider ? m_refs.usi2Provider() : nullptr;
    EngineGameOverNotifier::notifyResignation(
        *m_refs.playMode, info.loser == Player::P1, u1, u2, rawSender);
    if (u1) u1->setSquelchResignLogging(true);
    if (u2) u2->setSquelchResignLogging(true);

    const bool loserIsP1 = (info.loser == Player::P1);
    setGameOver(info, loserIsP1, true);
    displayResultsAndUpdateGui(info);
}

void GameEndHandler::handleTimeout(Player loser)
{
    if (m_refs.gameOver->isOver) return;

    const GameEndInfo info{Cause::Timeout, loser};
    setGameOver(info, loser == Player::P1, true);
    if (m_refs.clock) {
        m_refs.clock->markGameOver();
        m_refs.clock->stopClock();
    }
    if (m_hooks.disarmHumanTimerIfNeeded) m_hooks.disarmHumanTimerIfNeeded();

    const auto notify = [loser](Usi* engine, Player side) {
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
        notify(u1, Player::P1);
        notify(u2, Player::P2);
    } else if (mode == PlayMode::EvenEngineVsHuman || mode == PlayMode::HandicapEngineVsHuman) {
        notify(u1, Player::P1);
    } else if (mode == PlayMode::EvenHumanVsEngine || mode == PlayMode::HandicapHumanVsEngine) {
        notify(u1, Player::P2);
    }
    displayResultsAndUpdateGui(info);
}

void GameEndHandler::handleEngineWin(int idx)
{
    if (m_refs.gameOver->isOver) return;
    const Player declarer = (idx == 1 ? Player::P1 : Player::P2);
    handleNyugyokuDeclaration(declarer, true, false);
    // 人間の宣言は宣言の結果ダイアログで知らせる。エンジンの宣言は対局終了の通知で知らせる
    if (m_refs.gameOver->isOver && m_hooks.showGameOverDialog) {
        m_hooks.showGameOverDialog(tr("対局終了"), resultMessage(m_refs.gameOver->lastInfo));
    }
}

// --- 入玉宣言 ---

void GameEndHandler::handleNyugyokuDeclaration(Player declarer, bool success, bool isDraw)
{
    if (m_refs.gameOver->isOver) return;

    qCInfo(lcGame) << "handleNyugyokuDeclaration(): declarer=" << (declarer == Player::P1 ? "P1" : "P2")
                    << " success=" << success << " isDraw=" << isDraw;

    if (m_hooks.disarmHumanTimerIfNeeded) m_hooks.disarmHumanTimerIfNeeded();
    if (m_refs.clock) m_refs.clock->stopClock();

    GameEndInfo info;
    if (isDraw) {
        info.cause = Cause::Jishogi;
        info.loser = declarer;
    } else if (success) {
        info.cause = Cause::NyugyokuWin;
        info.loser = (declarer == Player::P1) ? Player::P2 : Player::P1;
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
        *m_refs.playMode, isDraw, info.loser == Player::P1, u1, u2, rawSender);
    if (u1) u1->setSquelchResignLogging(true);
    if (u2) u2->setSquelchResignLogging(true);

    const bool loserIsP1 = (info.loser == Player::P1);
    setGameOver(info, loserIsP1, true);
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

    const auto gcTurn = m_refs.gc ? m_refs.gc->currentPlayer() : ShogiGameController::NoPlayer;
    m_refs.gameOver->lastInfo.loser = (gcTurn == ShogiGameController::Player1) ? Player::P1 : Player::P2;

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
    info.loser = Player::P1;

    setGameOver(info, true, true);

    appendGameOverLineAndMark(Cause::Jishogi, Player::P1);
    displayResultsAndUpdateGui(info);
}

// --- 千日手 ---

bool GameEndHandler::checkAndHandleSennichite()
{
    if (m_refs.gameOver->isOver) return false;

    const QStringList* rec = m_refs.sfenHistory;
    if (m_refs.strategyProvider) {
        if (auto* eve = dynamic_cast<EngineVsEngineStrategy*>(m_refs.strategyProvider())) {
            rec = eve->sfenRecordForEvE();
        }
    }
    if (!rec || rec->size() < 4) return false;

    const auto result = SennichiteDetector::check(*rec);
    switch (result) {
    case SennichiteDetector::Result::None:
        return false;
    case SennichiteDetector::Result::Draw:
        handleSennichite();
        return true;
    case SennichiteDetector::Result::ContinuousCheckByP1:
        handleOuteSennichite(true);
        return true;
    case SennichiteDetector::Result::ContinuousCheckByP2:
        handleOuteSennichite(false);
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
    info.loser = Player::P1;

    setGameOver(info, true, true);

    appendGameOverLineAndMark(Cause::Sennichite, Player::P1);
    displayResultsAndUpdateGui(info);
}

void GameEndHandler::handleOuteSennichite(bool p1Loses)
{
    if (m_refs.gameOver->isOver) return;

    const Player loser = p1Loses ? Player::P1 : Player::P2;
    const Player winner = p1Loses ? Player::P2 : Player::P1;

    qCInfo(lcGame) << "handleOuteSennichite(): continuous check by"
                   << (p1Loses ? "P1(sente)" : "P2(gote)");

    if (m_hooks.disarmHumanTimerIfNeeded) m_hooks.disarmHumanTimerIfNeeded();

    Usi* u1 = m_refs.usi1Provider ? m_refs.usi1Provider() : nullptr;
    Usi* u2 = m_refs.usi2Provider ? m_refs.usi2Provider() : nullptr;

    const bool isEvE =
        (*m_refs.playMode == PlayMode::EvenEngineVsEngine) ||
        (*m_refs.playMode == PlayMode::HandicapEngineVsEngine);

    if (isEvE) {
        if (u1) {
            const auto result = (winner == Player::P1) ? GameOverResult::Win : GameOverResult::Lose;
            u1->sendGameOverCommand(result); u1->sendQuitCommand(); u1->setSquelchResignLogging(true);
        }
        if (u2) {
            const auto result = (winner == Player::P2) ? GameOverResult::Win : GameOverResult::Lose;
            u2->sendGameOverCommand(result); u2->sendQuitCommand(); u2->setSquelchResignLogging(true);
        }
    } else {
        if (Usi* eng = m_hooks.primaryEngine ? m_hooks.primaryEngine() : nullptr) {
            const bool engineIsP1 =
                (*m_refs.playMode == PlayMode::EvenEngineVsHuman) ||
                (*m_refs.playMode == PlayMode::HandicapEngineVsHuman);
            const Player engineSide = engineIsP1 ? Player::P1 : Player::P2;
            const auto result = (winner == engineSide) ? GameOverResult::Win : GameOverResult::Lose;
            eng->sendGameOverCommand(result); eng->sendQuitCommand(); eng->setSquelchResignLogging(true);
        }
    }

    GameEndInfo info;
    info.cause = Cause::OuteSennichite;
    info.loser = loser;

    setGameOver(info, p1Loses, true);

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

void GameEndHandler::setGameOver(const GameEndInfo& info, bool loserIsP1, bool appendMoveOnce)
{
    if (m_refs.gameOver->isOver) {
        qCDebug(lcGame) << "setGameOver() ignored: already over";
        return;
    }

    qCDebug(lcGame).nospace()
        << "setGameOver cause="
        << ((info.cause==Cause::Timeout)?"Timeout":"Resign")
        << " loser=" << ((info.loser==Player::P1)?"P1":"P2")
        << " appendMoveOnce=" << appendMoveOnce;

    m_refs.gameOver->isOver        = true;
    m_refs.gameOver->hasLast       = true;
    m_refs.gameOver->lastLoserIsP1 = loserIsP1;
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
