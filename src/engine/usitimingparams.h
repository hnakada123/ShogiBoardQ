#ifndef USITIMINGPARAMS_H
#define USITIMINGPARAMS_H

/// @file usitimingparams.h
/// @brief USIエンジンへ送信するタイミング関連パラメータの値構造体

#include <QString>

/// USI go コマンドに必要なタイミングパラメータをまとめた値構造体
struct UsiTimingParams {
    int byoyomiMilliSec = 0;        ///< 秒読み（ミリ秒）
    QString btime;                   ///< 先手残り時間（ミリ秒、文字列）
    QString wtime;                   ///< 後手残り時間（ミリ秒、文字列）
    int addEachMoveMilliSec1 = 0;   ///< 先手の1手ごとの加算時間（ミリ秒）
    int addEachMoveMilliSec2 = 0;   ///< 後手の1手ごとの加算時間（ミリ秒）
    bool useByoyomi = false;         ///< 秒読みを使用するか
};

#endif // USITIMINGPARAMS_H
