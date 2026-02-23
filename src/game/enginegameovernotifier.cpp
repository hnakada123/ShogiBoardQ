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
    sendRaw(engine, QStringLiteral("gameover ") + gameOverResultToString(result));
    sendRaw(engine, QStringLiteral("quit"));
}

} // namespace

void notifyResignation(PlayMode playMode,
                       bool loserIsP1,
                       Usi* usi1,
                       Usi* usi2,
                       const RawSender& sendRaw)
{
    if (!sendRaw || isHvH(playMode)) {
        return;
    }

    if (isHvE(playMode)) {
        const auto result = loserIsP1 ? GameOverResult::Win : GameOverResult::Lose;
        sendQuitPair(usi1, result, sendRaw);
        return;
    }

    if (isEvE(playMode)) {
        Usi* winner = loserIsP1 ? usi2 : usi1;
        Usi* loser = loserIsP1 ? usi1 : usi2;
        if (loser) {
            sendRaw(loser, QStringLiteral("gameover ") + gameOverResultToString(GameOverResult::Lose));
        }
        sendQuitPair(winner, GameOverResult::Win, sendRaw);
    }
}

void notifyNyugyoku(PlayMode playMode,
                    bool isDraw,
                    bool loserIsP1,
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
        const auto result = loserIsP1 ? GameOverResult::Win : GameOverResult::Lose;
        sendQuitPair(usi1, result, sendRaw);
        return;
    }

    if (isEvE(playMode)) {
        if (isDraw) {
            sendQuitPair(usi1, GameOverResult::Draw, sendRaw);
            sendQuitPair(usi2, GameOverResult::Draw, sendRaw);
            return;
        }
        Usi* winner = loserIsP1 ? usi2 : usi1;
        Usi* loser = loserIsP1 ? usi1 : usi2;
        sendQuitPair(winner, GameOverResult::Win, sendRaw);
        sendQuitPair(loser, GameOverResult::Lose, sendRaw);
    }
}

} // namespace EngineGameOverNotifier
