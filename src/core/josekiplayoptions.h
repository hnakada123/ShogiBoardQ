#ifndef JOSEKIPLAYOPTIONS_H
#define JOSEKIPLAYOPTIONS_H

/// @file josekiplayoptions.h
/// @brief 対局でエンジンの代わりに定跡手を指す条件（対局者ごと）

#include <QtGlobal>

/**
 * @brief 対局で定跡を使う条件
 *
 * 意味はやねうら王の定跡オプションに合わせる（手数＝BookMoves、評価値の差＝BookEvalDiff、
 * 評価値の下限＝BookEvalBlackLimit・BookEvalWhiteLimit、深さの下限＝BookDepthLimit、
 * 出現頻度に比例＝ConsiderBookMoveCount）。設定に項目ごとに保存するので、選び方は番号（Method の値）で持つ。
 */
struct JosekiPlayOptions {
    /// 手の選び方
    enum Method : int {
        ByFrequency = 0,  ///< 出現頻度に比例した確率で選ぶ（出現頻度がすべて0なら等確率）
        ByValue = 1,      ///< 最善の評価値から valueDiff 以内の手を等確率で選ぶ
        FirstMove = 2,    ///< 表の先頭の手
    };

    bool enabled = false;          ///< 定跡を使う
    int maxPly = 40;               ///< この手数（これから指す手の手数）まで定跡を使う
    int method = ByFrequency;      ///< 手の選び方（Method）
    int valueDiff = 30;            ///< 「評価値で選ぶ」で、最善の評価値からの差の上限
    bool useMinValue = false;      ///< 評価値の下限を使う
    int minValue = 0;              ///< 指す側から見た評価値の下限
    int minDepth = 0;              ///< 深さの下限
    double minRatioPercent = 0.0;  ///< 出現頻度の比率（その局面の出現頻度の合計に対する割合）の下限（%）
    int minFrequency = 0;          ///< 出現頻度の下限

    /// 各項目を範囲に収めた値
    JosekiPlayOptions normalized() const
    {
        JosekiPlayOptions options = *this;
        options.maxPly = qBound(1, maxPly, 9999);
        options.method = (method == ByValue || method == FirstMove) ? method : ByFrequency;
        options.valueDiff = qBound(0, valueDiff, 99999);
        options.minValue = qBound(-99999, minValue, 99999);
        options.minDepth = qBound(0, minDepth, 9999);
        options.minRatioPercent = qBound(0.0, minRatioPercent, 100.0);
        options.minFrequency = qBound(0, minFrequency, 999999999);
        return options;
    }

    friend bool operator==(const JosekiPlayOptions &a, const JosekiPlayOptions &b)
    {
        return a.enabled == b.enabled && a.maxPly == b.maxPly && a.method == b.method && a.valueDiff == b.valueDiff
            && a.useMinValue == b.useMinValue && a.minValue == b.minValue && a.minDepth == b.minDepth
            && qFuzzyCompare(1.0 + a.minRatioPercent, 1.0 + b.minRatioPercent) && a.minFrequency == b.minFrequency;
    }
    friend bool operator!=(const JosekiPlayOptions &a, const JosekiPlayOptions &b) { return !(a == b); }
};

#endif // JOSEKIPLAYOPTIONS_H
