#ifndef STARTOPTIONS_H
#define STARTOPTIONS_H

/// @file startoptions.h
/// @brief 対局開始設定パラメータ構造体の定義
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
#include <QMetaType>

/// 開始前の処理を、表示中の文字列から推測せず明示する。
struct GameStartPreparation {
    enum class Mode { NewGame, CurrentPosition };
    Mode mode = Mode::NewGame;
    QString rootSfen;
    int startNodeId = -1; ///< CurrentPosition の開始ノード。-1 は棋譜のない単独局面
};
Q_DECLARE_METATYPE(GameStartPreparation)

/// @brief 対局開始のための設定パラメータ
/// @note MatchCoordinator::StartOptions は本型の using エイリアス。
struct MatchStartOptions {
    MatchPlayers players;                  ///< 対局者（人間・エンジンと名前）
    RecordLine recordLine;                 ///< 開始する手順と手数（過去の対局から推測しない）
    QString  sfenStart;                    ///< 開始SFEN

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
    /// 先手と後手の対局者とエンジンの設定を入れ替える（連続対局で1局ごとに手番を替える）
    void swapSides()
    {
        players.swapSides();
        std::swap(engines[0], engines[1]);
    }
};

#endif // STARTOPTIONS_H
