#ifndef ENGINEGAMEOVERNOTIFIER_H
#define ENGINEGAMEOVERNOTIFIER_H

#include <functional>
#include <QString>

#include "playmode.h"
#include "shogitypes.h"

class Usi;

namespace EngineGameOverNotifier {

using RawSender = std::function<void(Usi*, const QString&)>;

/// 投了・反則・時間切れなどの勝敗をエンジンに知らせる（loser: 負けた側）
void notifyResignation(PlayMode playMode,
                       Turn loser,
                       Usi* usi1,
                       Usi* usi2,
                       const RawSender& sendRaw);

/// 入玉宣言の結果をエンジンに知らせる（loser: 負けた側。isDraw なら使わない）
void notifyNyugyoku(PlayMode playMode,
                    bool isDraw,
                    Turn loser,
                    Usi* usi1,
                    Usi* usi2,
                    const RawSender& sendRaw);

} // namespace EngineGameOverNotifier

#endif // ENGINEGAMEOVERNOTIFIER_H
