#ifndef ENGINEGAMEOVERNOTIFIER_H
#define ENGINEGAMEOVERNOTIFIER_H

#include <functional>
#include <QString>

#include "playmode.h"
#include "shogitypes.h"

class Usi;

namespace EngineGameOverNotifier {

using RawSender = std::function<void(Usi*, const QString&)>;

void notifyResignation(PlayMode playMode,
                       bool loserIsP1,
                       Usi* usi1,
                       Usi* usi2,
                       const RawSender& sendRaw);

void notifyNyugyoku(PlayMode playMode,
                    bool isDraw,
                    bool loserIsP1,
                    Usi* usi1,
                    Usi* usi2,
                    const RawSender& sendRaw);

} // namespace EngineGameOverNotifier

#endif // ENGINEGAMEOVERNOTIFIER_H
