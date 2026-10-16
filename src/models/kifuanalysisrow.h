#ifndef KIFUANALYSISROW_H
#define KIFUANALYSISROW_H

/// @file kifuanalysisrow.h
/// @brief 棋譜解析の結果の表の1行（1局面の解析結果）

#include <QString>
#include <optional>

#include "usiscore.h"

/**
 * @brief 棋譜解析の結果の表の1行（作った後は変えない値）
 *
 * 評価値は先手から見た値で持つ。表示する文字列（表記の設定に合わせた手・読み筋、「mate 3」・形勢）は
 * 表のモデル（KifuAnalysisListModel）が作る。
 */
struct KifuAnalysisRow {
    QString move;                             ///< 指した手（棋譜の表記）
    QString lastUsiMove;                      ///< 指した手（USI。読み筋の盤で強調する）
    QString beforeSfen;                       ///< 指す前の局面
    QString sfen;                             ///< 指した後の局面（読み筋の始まり）
    UsiScore evaluation;                      ///< 先手から見た評価値（定跡・未取得は None）
    std::optional<int> evaluationDifference;  ///< 直前の解析局面からの評価値の増減（比べられなければ nullopt）
    QString pv;                               ///< 読み筋（漢字表記）
    QString usiPv;                            ///< 読み筋（USI。空白区切り）
    QString candidateMove;                    ///< 候補手（直前の行の読み筋の最初の手。漢字表記）
    QString candidateUsi;                     ///< 候補手（USI）
    QString candidateSfen;                    ///< 候補手を指す局面

    /// 指した手が直前の局面の候補手と同じか
    bool matchesCandidate() const
    {
        return !lastUsiMove.isEmpty() && !candidateUsi.isEmpty() && !beforeSfen.isEmpty()
            && lastUsiMove == candidateUsi && beforeSfen == candidateSfen;
    }
};

#endif // KIFUANALYSISROW_H
