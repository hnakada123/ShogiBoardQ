#ifndef INTERRUPTEDGAMESTATE_H
#define INTERRUPTEDGAMESTATE_H

/// @file interruptedgamestate.h
/// @brief 中断したローカル対局の状態（アプリ起動中だけ保持する）

#include "recordline.h"
#include "shogiclock.h"
#include "startoptions.h"

/// 中断したローカル対局の状態。再開すると、この手順と時計から続ける
struct InterruptedGameState {
    MatchStartOptions options;
    ShogiClock::Snapshot clock;
    RecordLine record;           ///< 中断までの対局の手順
    BoardSquare previousTo;      ///< 「同」表記の基準（直前の手の移動先）
};

#endif
