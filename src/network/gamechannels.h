#ifndef GAMECHANNELS_H
#define GAMECHANNELS_H

/// @file gamechannels.h
/// @brief ローカル対局と通信対局の窓口の組と、今の窓口

#include <memory>

#include "csagamechannel.h"
#include "gamechannel.h"

class AppActivityState;
class CsaGameCoordinator;
class MatchCoordinator;

/**
 * @brief 対局相手の窓口の組。今の動作状態で使う窓口は active() の1か所で決める
 *
 * 盤の手・投了・中断・人間が指せるかを受ける部品は、対局の種類で分岐せず active() の窓口に聞く。
 * 司令塔の直後に作り、Deps で渡す。司令塔が無ければ、その対局の窓口は無い。
 */
class GameChannels
{
public:
    GameChannels(const AppActivityState* activity, MatchCoordinator* match, CsaGameCoordinator* csaGame);

    /// 対局相手の窓口（対局していなければ nullptr）。通信対局はサーバーに接続している間（局と局の間も）
    GameChannel* active() const;

private:
    const AppActivityState* m_activity = nullptr;
    std::unique_ptr<LocalGameChannel> m_local;
    std::unique_ptr<CsaGameChannel> m_csa;
};

#endif // GAMECHANNELS_H
