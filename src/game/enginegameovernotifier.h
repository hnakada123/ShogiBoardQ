#ifndef ENGINEGAMEOVERNOTIFIER_H
#define ENGINEGAMEOVERNOTIFIER_H

#include "playmode.h"
#include "shogitypes.h"

class Usi;

namespace EngineGameOverNotifier {

/// 投了・反則・時間切れなどの勝敗をエンジンに知らせる（loser: 負けた側）
void notifyResignation(PlayMode playMode,
                       Turn loser,
                       Usi* usi1,
                       Usi* usi2);

/// 入玉宣言の結果をエンジンに知らせる（loser: 負けた側。isDraw なら使わない）
void notifyNyugyoku(PlayMode playMode,
                    bool isDraw,
                    Turn loser,
                    Usi* usi1,
                    Usi* usi2);

} // namespace EngineGameOverNotifier

#endif // ENGINEGAMEOVERNOTIFIER_H
