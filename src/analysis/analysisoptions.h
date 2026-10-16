#ifndef ANALYSISOPTIONS_H
#define ANALYSISOPTIONS_H

/// @file analysisoptions.h
/// @brief 検討・詰み探索の開始オプション

#include <QString>

#include "usiposition.h"

/// 検討・詰み探索の開始オプション
struct AnalysisOptions {
    enum class Mode { Consideration, Tsume };

    Mode     mode = Mode::Consideration;
    QString  enginePath;                ///< エンジン実行ファイル
    QString  engineName;                ///< 表示用エンジン名
    UsiPosition position;               ///< 探索する局面（そこに至った直前の手は読み筋の「同」の判定に使う）
    int      byoyomiMs = 0;             ///< 0=無制限、>0=ms
    int      multiPV   = 1;             ///< MultiPV（候補手の数）
};

#endif // ANALYSISOPTIONS_H
