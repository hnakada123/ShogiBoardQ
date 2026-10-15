#ifndef APPSTATE_H
#define APPSTATE_H

/// @file appstate.h
/// @brief アプリ全体で共有する対局・対局者・棋譜の状態。置き場はここ1つだけ

#include <QList>
#include <QString>
#include <QStringList>

#include "playmode.h"
#include "appactivitystate.h"
#include "sfenutils.h"
#include "shogimove.h"
#include "shogitypes.h"

/**
 * @brief MainWindow が束ねる部品どうしで共有する状態
 *
 * 所有者は MainWindowComposer。部品はレジストリ経由ではなく、
 * 組み立て時にここへのポインタ（または個々のフィールドへのポインタ）を受け取る。
 * 棋譜の正本は分岐ツリー（KifuBranchTree）で、sfenRecord と gameMoves は対局中だけ使う作業用の履歴である。
 */
struct AppState {
    // --- 対局 ---
    AppActivityState activity;
    const PlayMode& playMode = activity.playModeRef();     ///< 読み取り専用の互換ビュー
    QString startSfenStr = SfenUtils::hirateSfen();        ///< 開始局面（完全な SFEN）
    QString currentSfenStr = QStringLiteral("startpos");   ///< 表示中の局面
    bool errorOccurred = false;                            ///< エラー表示中（通知の重複を避ける）

    // --- 対局者 ---
    QString humanName1;
    QString humanName2;
    QString engineName1;
    QString engineName2;
    Turn bottomSide = Turn::Black;                         ///< 盤の手前に表示している側（盤の反転で入れ替わる）

    // --- 棋譜（対局中の作業用。正本は分岐ツリー） ---
    QStringList sfenRecord;                                ///< 対局の局面列。対局開始時に表示中の手順から作り直す
    QList<ShogiMove> gameMoves;                            ///< 対局の指し手列（局面列とそろえる）
    bool onMainRowGuard = false;                           ///< 棋譜欄の行変更を自分で起こしている間 true
    QString saveFileName;                                  ///< 上書き保存先。読み込み成功時に更新、貼り付け時にクリア
};

#endif // APPSTATE_H
