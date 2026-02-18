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

void sendQuitPair(Usi* engine, const QString& result, const RawSender& sendRaw)
{
    if (!engine || !sendRaw) {
        return;
    }
    sendRaw(engine, QStringLiteral("gameover ") + result);
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
        const QString result = loserIsP1 ? QStringLiteral("win") : QStringLiteral("lose");
        sendQuitPair(usi1, result, sendRaw);
        return;
    }

    if (isEvE(playMode)) {
        Usi* winner = loserIsP1 ? usi2 : usi1;
        Usi* loser = loserIsP1 ? usi1 : usi2;
        if (loser) {
            sendRaw(loser, QStringLiteral("gameover lose"));
        }
        sendQuitPair(winner, QStringLiteral("win"), sendRaw);
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
            sendQuitPair(usi1, QStringLiteral("draw"), sendRaw);
            return;
        }
        const QString result = loserIsP1 ? QStringLiteral("win") : QStringLiteral("lose");
        sendQuitPair(usi1, result, sendRaw);
        return;
    }

    if (isEvE(playMode)) {
        if (isDraw) {
            sendQuitPair(usi1, QStringLiteral("draw"), sendRaw);
            sendQuitPair(usi2, QStringLiteral("draw"), sendRaw);
            return;
        }
        Usi* winner = loserIsP1 ? usi2 : usi1;
        Usi* loser = loserIsP1 ? usi1 : usi2;
        sendQuitPair(winner, QStringLiteral("win"), sendRaw);
        sendQuitPair(loser, QStringLiteral("lose"), sendRaw);
    }
}

} // namespace EngineGameOverNotifier
