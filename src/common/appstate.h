#ifndef APPSTATE_H
#define APPSTATE_H

/// @file appstate.h
/// @brief アプリ全体で共有する動作状態。置き場はここ1つだけ

#include "appactivitystate.h"

/**
 * @brief MainWindow が束ねる部品どうしで共有する状態
 *
 * 所有者は MainWindowComposer。部品はレジストリ経由ではなく、
 * 組み立て時にここへのポインタを受け取る（個々のフィールドへのポインタは渡さない）。
 * 棋譜と、それに付く対局情報・対局者・上書き保存先は棋譜の文書（KifuDocument）、
 * 表示中の局面は KifuNavigationState、対局中の手順は対局の司令塔（MatchCoordinator::record()）が持つ（ここには写さない）。
 */
struct AppState {
    AppActivityState activity;
};

#endif // APPSTATE_H
