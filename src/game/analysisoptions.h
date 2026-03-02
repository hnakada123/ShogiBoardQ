#ifndef ANALYSISOPTIONS_H
#define ANALYSISOPTIONS_H

/// @file analysisoptions.h
/// @brief 検討・詰み探索オプション構造体の定義
///
/// MatchCoordinator に依存せずに検討オプションを参照できる軽量ヘッダ。
/// MatchCoordinator::AnalysisOptions は本型の using エイリアス。

#include <QString>

#include "playmode.h"

class ShogiEngineThinkingModel;

/// @brief 検討オプション
/// @note MatchCoordinator::AnalysisOptions は本型の using エイリアス。
struct MatchAnalysisOptions {
    QString  enginePath;                ///< 検討に使うエンジン実行ファイル
    QString  engineName;                ///< 表示用エンジン名
    QString  positionStr;               ///< "position sfen ... [moves ...]" の完全文字列
    int      byoyomiMs = 0;             ///< 0=無制限、>0=ms
    int      multiPV   = 1;             ///< MultiPV（候補手の数）
    PlayMode mode      = PlayMode::ConsiderationMode;  ///< 検討モード
    ShogiEngineThinkingModel* considerationModel = nullptr; ///< 検討タブ用モデル
    int      previousFileTo = 0;        ///< 前回の移動先の筋（1-9, 0=未設定）
    int      previousRankTo = 0;        ///< 前回の移動先の段（1-9, 0=未設定）
    QString  lastUsiMove;               ///< 開始局面に至った最後の指し手（USI形式）
};

#endif // ANALYSISOPTIONS_H
