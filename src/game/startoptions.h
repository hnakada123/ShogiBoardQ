#ifndef STARTOPTIONS_H
#define STARTOPTIONS_H

/// @file startoptions.h
/// @brief 対局開始設定パラメータ構造体の定義
///
/// MatchCoordinator に依存せずに対局開始設定を参照できる軽量ヘッダ。
/// MatchCoordinator::StartOptions は本型の using エイリアス。

#include <QString>

#include "playmode.h"
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
    PlayMode mode = PlayMode::NotStarted;  ///< 対局モード
    RecordLine recordLine;                 ///< 開始する手順と手数（過去の対局から推測しない）
    QString  sfenStart;                    ///< 開始SFEN

    QString engineName1;                   ///< エンジン1表示名
    QString enginePath1;                   ///< エンジン1実行パス
    QString engineName2;                   ///< エンジン2表示名
    QString enginePath2;                   ///< エンジン2実行パス

    int maxMoves = 0;                      ///< 最大手数（0=無制限）

    bool autoSaveKifu = false;             ///< 棋譜自動保存フラグ
    QString kifuSaveDir;                   ///< 棋譜保存ディレクトリ

    QString humanName1;                    ///< 先手の人間対局者名
    QString humanName2;                    ///< 後手の人間対局者名
    bool resumeFromBreakOff = false;        ///< 中断した手の途中から再開する
};

#endif // STARTOPTIONS_H
