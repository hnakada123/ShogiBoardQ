#ifndef STARTOPTIONS_H
#define STARTOPTIONS_H

/// @file startoptions.h
/// @brief 対局開始設定パラメータ構造体の定義
///
/// MatchCoordinator に依存せずに対局開始設定を参照できる軽量ヘッダ。
/// MatchCoordinator::StartOptions は本型の using エイリアス。

#include <QString>

#include "playmode.h"

/// @brief 対局開始のための設定パラメータ
/// @note MatchCoordinator::StartOptions は本型の using エイリアス。
struct MatchStartOptions {
    PlayMode mode = PlayMode::NotStarted;  ///< 対局モード
    QString  sfenStart;                    ///< 開始SFEN

    QString engineName1;                   ///< エンジン1表示名
    QString enginePath1;                   ///< エンジン1実行パス
    QString engineName2;                   ///< エンジン2表示名
    QString enginePath2;                   ///< エンジン2実行パス

    bool engineIsP1 = false;               ///< エンジンが先手か（HvE）
    bool engineIsP2 = false;               ///< エンジンが後手か（HvE）

    int maxMoves = 0;                      ///< 最大手数（0=無制限）

    bool autoSaveKifu = false;             ///< 棋譜自動保存フラグ
    QString kifuSaveDir;                   ///< 棋譜保存ディレクトリ

    QString humanName1;                    ///< 先手の人間対局者名
    QString humanName2;                    ///< 後手の人間対局者名
};

#endif // STARTOPTIONS_H
