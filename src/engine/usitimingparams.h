#ifndef USITIMINGPARAMS_H
#define USITIMINGPARAMS_H

/// @file usitimingparams.h
/// @brief 対局中のエンジンに思考を頼むときの時間の条件

#include <QtGlobal>

#include "timecontrol.h"

/**
 * @brief エンジンに思考を頼むときの時間の条件
 *
 * エンジンに時計（ShogiClock）がつながっていれば、残り時間は思考を始める時点の時計から求める
 * （ローカル対局）。つながっていなければ btimeMs/wtimeMs を使う（CSA 通信対局）。
 * 実際に go へ載せる値は UsiGoTimeCalculator が求める。
 */
struct UsiTimingParams {
    TimeControl timeControl;  ///< 秒読み・加算の決まり
    qint64 btimeMs = 0;       ///< 時計がないときの先手の残り持ち時間（USI の btime の意味）
    qint64 wtimeMs = 0;       ///< 時計がないときの後手の残り持ち時間（USI の wtime の意味）
    qint64 extraMs = 0;       ///< 今の手だけに足す時間（CSA の遅延時間）
    qint64 marginMs = 0;      ///< 通信の遅延に備えて差し引く時間
};

#endif // USITIMINGPARAMS_H
