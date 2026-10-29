#ifndef STARTGAMEDIALOGDATA_H
#define STARTGAMEDIALOGDATA_H

/// @file startgamedialogdata.h
/// @brief 対局開始ダイアログで選んだ値（対局者1人の選択 PlayerChoice と対局の条件 StartGameDialogData）

#include <QString>
#include <array>

#include "registeredengines.h"
#include "handicaps.h"
#include "josekiplayoptions.h"
#include "timecontrol.h"

/// 対局者1人の選択（対局開始ダイアログの「対局者」と時間設定）
struct PlayerChoice {
    enum class Kind { Human, Engine };

    Kind kind = Kind::Human;
    QString humanName;                       ///< 人間の対局者名
    EngineEntry engine;  ///< エンジン（kind が Engine のとき。名前・実行ファイル・作者）
    TimeControlSide time;                    ///< 持ち時間・秒読み・加算
    JosekiPlayOptions joseki;                ///< 定跡を使う条件（kind が Engine のとき。定跡ウィンドウで開いている定跡を使う）

    bool isHuman() const { return kind == Kind::Human; }
    bool isEngine() const { return kind == Kind::Engine; }
};

/**
 * @brief 対局開始ダイアログで選んだ対局の条件
 *
 * dialogs/ 層の StartGameDialog への依存を game/ 層から排除するため、ダイアログの入力値を値として持つ。
 * 変換は app/ 層（GameSession）でのみ行い、game/ 層はこの値だけを受け取る。
 */
struct StartGameDialogData {
    /// 対局者。[0] が先手／下手、[1] が後手／上手（「後手に異なる時間」がなければ時間は先手と同じ）
    std::array<PlayerChoice, 2> players;

    bool isLoseOnTimeout = false;  ///< 時間切れ負けにするか

    /// 開始局面の手合割（Handicaps::kAll の1つ）。nullptr なら表示中の局面から始める
    const Handicaps::Handicap* startingHandicap = &Handicaps::kAll[0];

    // --- 対局オプション ---
    int     maxMoves          = 0;      ///< 最大手数（0=無制限）
    bool    autoSaveKifu      = false;  ///< 棋譜自動保存フラグ
    QString kifuSaveDir;               ///< 棋譜保存ディレクトリ
    bool    isShowHumanInFront = false; ///< 人間を手前に表示するか
    int     consecutiveGames  = 1;     ///< 連続対局数
    bool    isSwitchTurnEachGame = false; ///< 1局ごとに手番入替フラグ

    bool startsFromCurrentPosition() const { return startingHandicap == nullptr; }
    bool hasEngine() const { return players[0].isEngine() || players[1].isEngine(); }
};

#endif // STARTGAMEDIALOGDATA_H
