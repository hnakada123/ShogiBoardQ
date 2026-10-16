#ifndef ANALYSISOPTIONS_H
#define ANALYSISOPTIONS_H

/// @file analysisoptions.h
/// @brief 検討・詰み探索の開始オプション

#include <QString>

#include "usimove.h"

/// 検討・詰み探索の開始オプション
struct AnalysisOptions {
    enum class Mode { Consideration, Tsume };

    Mode     mode = Mode::Consideration;
    QString  enginePath;                ///< エンジン実行ファイル
    QString  engineName;                ///< 表示用エンジン名
    QString  positionStr;               ///< "position sfen ... [moves ...]" の完全文字列
    int      byoyomiMs = 0;             ///< 0=無制限、>0=ms
    int      multiPV   = 1;             ///< MultiPV（候補手の数）
    BoardSquare previousTo;             ///< 局面に至った手の移動先（読み筋の「同」の判定に使う）
    QString  lastUsiMove;               ///< 開始局面に至った最後の指し手（USI形式）
};

#endif // ANALYSISOPTIONS_H
