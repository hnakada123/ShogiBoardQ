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

void sendQuitPair(Usi* engine, GameOverResult result, const RawSender& sendRaw)
{
    if (!engine || !sendRaw) {
        return;
    }
    // 終局後のbestmoveで着手・先読みを再開させない。quitに応答しない場合も
    // 非同期の終了監視へ引き渡し、アプリを閉じるまでプロセスを保持しない。
    engine->cancelCurrentOperation();
    engine->sendStopCommand();
    sendRaw(engine, QStringLiteral("gameover ") + gameOverResultToString(result));
    engine->cleanupEngineProcessAndThread(false);
}

} // namespace

void notifyResignation(PlayMode playMode,
                       Turn loser,
                       Usi* usi1,
                       Usi* usi2,
                       const RawSender& sendRaw)
{
    if (!sendRaw || isHvH(playMode)) {
        return;
    }

    if (isHvE(playMode)) {
        const bool engineLost = loser == hveEngineSide(playMode);
        sendQuitPair(usi1, engineLost ? GameOverResult::Lose : GameOverResult::Win, sendRaw);
        return;
    }

    if (isEvE(playMode)) {
        Usi* winnerEngine = loser == Turn::Black ? usi2 : usi1;
        Usi* loserEngine = loser == Turn::Black ? usi1 : usi2;
        sendQuitPair(loserEngine, GameOverResult::Lose, sendRaw);
        sendQuitPair(winnerEngine, GameOverResult::Win, sendRaw);
    }
}

void notifyNyugyoku(PlayMode playMode,
                    bool isDraw,
                    Turn loser,
                    Usi* usi1,
                    Usi* usi2,
                    const RawSender& sendRaw)
{
    if (!sendRaw || isHvH(playMode)) {
        return;
    }

    if (isHvE(playMode)) {
        if (isDraw) {
            sendQuitPair(usi1, GameOverResult::Draw, sendRaw);
            return;
        }
        const bool engineLost = loser == hveEngineSide(playMode);
        sendQuitPair(usi1, engineLost ? GameOverResult::Lose : GameOverResult::Win, sendRaw);
        return;
    }

    if (isEvE(playMode)) {
        if (isDraw) {
            sendQuitPair(usi1, GameOverResult::Draw, sendRaw);
            sendQuitPair(usi2, GameOverResult::Draw, sendRaw);
            return;
        }
        Usi* winnerEngine = loser == Turn::Black ? usi2 : usi1;
        Usi* loserEngine = loser == Turn::Black ? usi1 : usi2;
        sendQuitPair(winnerEngine, GameOverResult::Win, sendRaw);
        sendQuitPair(loserEngine, GameOverResult::Lose, sendRaw);
    }
}

} // namespace EngineGameOverNotifier
