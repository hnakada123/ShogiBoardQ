#include "enginegameovernotifier.h"

#include "usi.h"

namespace EngineGameOverNotifier {
namespace {

bool isHvH(PlayMode mode)
{
    return mode == PlayMode::HumanVsHuman;
}

bool isHvE(PlayMode mode)
{
    return mode == PlayMode::EvenHumanVsEngine ||
           mode == PlayMode::EvenEngineVsHuman ||
           mode == PlayMode::HandicapHumanVsEngine ||
           mode == PlayMode::HandicapEngineVsHuman;
}

// 人間対エンジンではエンジンは常に usi1。先手・下手を持つかはモードで決まる
Turn hveEngineSide(PlayMode mode)
{
    return (mode == PlayMode::EvenEngineVsHuman || mode == PlayMode::HandicapEngineVsHuman)
        ? Turn::Black : Turn::White;
}

bool isEvE(PlayMode mode)
{
    return mode == PlayMode::EvenEngineVsEngine ||
           mode == PlayMode::HandicapEngineVsEngine;
}

void sendQuitPair(Usi* engine, GameOverResult result)
{
    if (!engine) {
        return;
    }
    // 終局後のbestmoveで着手・先読みを再開させない。quitに応答しない場合も
    // 非同期の終了監視へ引き渡し、アプリを閉じるまでプロセスを保持しない。
    engine->cancelCurrentOperation();
    engine->sendStopCommand();
    engine->sendGameOverCommand(result);
    engine->cleanupEngineProcessAndThread(false);
}

} // namespace

void notifyResignation(PlayMode playMode,
                       Turn loser,
                       Usi* usi1,
                       Usi* usi2)
{
    if (isHvH(playMode)) {
        return;
    }

    if (isHvE(playMode)) {
        const bool engineLost = loser == hveEngineSide(playMode);
        sendQuitPair(usi1, engineLost ? GameOverResult::Lose : GameOverResult::Win);
        return;
    }

    if (isEvE(playMode)) {
        Usi* winnerEngine = loser == Turn::Black ? usi2 : usi1;
        Usi* loserEngine = loser == Turn::Black ? usi1 : usi2;
        sendQuitPair(loserEngine, GameOverResult::Lose);
        sendQuitPair(winnerEngine, GameOverResult::Win);
    }
}

void notifyNyugyoku(PlayMode playMode,
                    bool isDraw,
                    Turn loser,
                    Usi* usi1,
                    Usi* usi2)
{
    if (isHvH(playMode)) {
        return;
    }

    if (isHvE(playMode)) {
        if (isDraw) {
            sendQuitPair(usi1, GameOverResult::Draw);
            return;
        }
        const bool engineLost = loser == hveEngineSide(playMode);
        sendQuitPair(usi1, engineLost ? GameOverResult::Lose : GameOverResult::Win);
        return;
    }

    if (isEvE(playMode)) {
        if (isDraw) {
            sendQuitPair(usi1, GameOverResult::Draw);
            sendQuitPair(usi2, GameOverResult::Draw);
            return;
        }
        Usi* winnerEngine = loser == Turn::Black ? usi2 : usi1;
        Usi* loserEngine = loser == Turn::Black ? usi1 : usi2;
        sendQuitPair(winnerEngine, GameOverResult::Win);
        sendQuitPair(loserEngine, GameOverResult::Lose);
    }
}

} // namespace EngineGameOverNotifier
