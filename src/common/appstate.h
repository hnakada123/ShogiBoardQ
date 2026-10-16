#ifndef APPSTATE_H
#define APPSTATE_H

/// @file appstate.h
/// @brief アプリ全体で共有する対局・対局者・棋譜の状態。置き場はここ1つだけ

#include <QString>

#include "appactivitystate.h"
#include "matchplayers.h"

/**
 * @brief MainWindow が束ねる部品どうしで共有する状態
 *
 * 所有者は MainWindowComposer。部品はレジストリ経由ではなく、
 * 組み立て時にここへのポインタを受け取る（個々のフィールドへのポインタは渡さない）。
 * 棋譜の正本は分岐ツリー（KifuBranchTree）、表示中の局面は KifuNavigationState が持つ（ここには写さない）。
 * 対局中の手順は対局の司令塔（MatchCoordinator::record()）が持つ。
 */
struct AppState {
    // --- 対局 ---
    AppActivityState activity;

    // --- 対局者 ---
    MatchPlayers players;                                  ///< 最後に始めた対局の対局者（名前と人間・エンジン）

    // --- 棋譜（正本は分岐ツリー） ---
    QString saveFileName;                                  ///< 上書き保存先。読み込み成功時に更新、貼り付け時にクリア

    /// 対局中なら対局者、対局していなければ nullptr（検討・解析・通信対局のエンジンは対局者ではない）
    const MatchPlayers* currentMatchPlayers() const
    {
        return activity.is(AppActivityState::Activity::DuringGame) ? &players : nullptr;
    }
};

#endif // APPSTATE_H
