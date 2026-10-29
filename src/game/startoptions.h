#ifndef STARTOPTIONS_H
#define STARTOPTIONS_H

/// @file startoptions.h
/// @brief 対局を始める条件（対局者・手順・時間の決まり・最大手数・自動保存・定跡）
///
/// MatchCoordinator に依存せずに対局開始設定を参照できる軽量ヘッダ。
/// MatchCoordinator::StartOptions は本型の using エイリアス。

#include <QString>
#include <array>
#include <utility>

#include "josekibook.h"
#include "josekiplayoptions.h"
#include "matchautosave.h"
#include "matchplayers.h"
#include "recordline.h"
#include "timecontrol.h"
#include <QMetaType>

/// 開始前の処理を、表示中の文字列から推測せず明示する。
struct GameStartPreparation {
    enum class Mode { NewGame, CurrentPosition };
    Mode mode = Mode::NewGame;
    QString rootSfen;
    int startNodeId = -1; ///< CurrentPosition の開始ノード。-1 は棋譜のない単独局面
};
Q_DECLARE_METATYPE(GameStartPreparation)

/// 対局の時間の決まり。持ち時間がどちらにも無ければ（rules.isUnlimited()）消費時間だけを数える
struct MatchTimeSettings {
    TimeControl rules;          ///< 先手・後手の持ち時間の決まり
    bool loseOnTimeout = true;  ///< 時間切れを負けとするか
};

/// @brief 対局開始のための設定パラメータ
/// @note MatchCoordinator::StartOptions は本型の using エイリアス。
struct MatchStartOptions {
    MatchPlayers players;                  ///< 対局者（人間・エンジンと名前）
    /// 開始する手順と手数（過去の対局から推測しない）。対局は currentPly の局面から始める
    RecordLine recordLine;
    /// 時間の決まり（司令塔が開始の局面の手番で時計に入れる）
    MatchTimeSettings time;

    int maxMoves = 0;                      ///< 最大手数（0=無制限）

    MatchAutoSave autoSave;                ///< 終局した棋譜の自動保存

    /// 対局で使う定跡（対局を始めたときに定跡ウィンドウで開いていた定跡の写し。中身は共有する）
    JosekiBook josekiBook;

    /// 先手（下手）か後手（上手）のエンジンの設定（人間の側は使わない）
    struct EngineSeat {
        QString path;                ///< エンジンの実行パス
        JosekiPlayOptions joseki;    ///< 定跡を使う条件
    };
    std::array<EngineSeat, 2> engines;     ///< [0] が先手（下手）。engine(Turn) で引く

    EngineSeat& engine(Turn side) { return engines[side == Turn::Black ? 0 : 1]; }
    const EngineSeat& engine(Turn side) const { return engines[side == Turn::Black ? 0 : 1]; }
    /// どちらかのエンジンが定跡を使う
    bool usesJoseki() const { return engines[0].joseki.enabled || engines[1].joseki.enabled; }
    /// 先手と後手の対局者・エンジンの設定・持ち時間を入れ替える（連続対局で1局ごとに手番を替える。
    /// エンジンごとに時間差を付けた対局でも、各エンジンが同じ持ち時間で指し続ける）
    void swapSides()
    {
        players.swapSides();
        std::swap(engines[0], engines[1]);
        time.rules.swapSides();
    }
};

#endif // STARTOPTIONS_H
