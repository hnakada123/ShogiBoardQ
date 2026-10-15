#ifndef STARTOPTIONS_H
#define STARTOPTIONS_H

/// @file startoptions.h
/// @brief 対局開始設定パラメータ構造体の定義
///
/// MatchCoordinator に依存せずに対局開始設定を参照できる軽量ヘッダ。
/// MatchCoordinator::StartOptions は本型の using エイリアス。

#include <QString>

#include "matchplayers.h"
#include "recordline.h"
#include <QMetaType>

/// 開始前の処理を、表示中の文字列から推測せず明示する。
struct GameStartPreparation {
    enum class Mode { NewGame, CurrentPosition, Reset };
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

    QString enginePath1;                   ///< 先手（下手）のエンジンの実行パス
    QString enginePath2;                   ///< 後手（上手）のエンジンの実行パス

    int maxMoves = 0;                      ///< 最大手数（0=無制限）

    bool autoSaveKifu = false;             ///< 棋譜自動保存フラグ
    QString kifuSaveDir;                   ///< 棋譜保存ディレクトリ

    bool resumeFromBreakOff = false;        ///< 中断した手の途中から再開する
};

#endif // STARTOPTIONS_H
