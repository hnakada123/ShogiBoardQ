/// @file josekitypes.h
/// @brief 定跡データの値型（定跡手）

#ifndef JOSEKITYPES_H
#define JOSEKITYPES_H

#include <QList>
#include <QString>
#include <QtGlobal>

#include <optional>

/**
 * @brief 定跡エントリの指し手情報
 */
struct JosekiMove {
    QString move;       ///< 指し手（USI形式）
    std::optional<QString> nextMove;   ///< 予想応手（USI形式。無ければ nullopt。定跡ファイルには "none" と書く）
    int value = 0;      ///< 評価値
    int depth = 0;      ///< 深さ
    int frequency = 0;  ///< 出現頻度
    QString comment;    ///< コメント
};

/// 局面の定跡手の出現頻度の合計（負の値は0とみなす）
inline qint64 frequencyTotal(const QList<JosekiMove>& moves)
{
    qint64 total = 0;
    for (const JosekiMove& move : moves) total += qMax(move.frequency, 0);
    return total;
}

/// 手の比率（局面の出現頻度の合計 total に対する割合、%）。合計が0の局面に比率は無い
inline std::optional<double> frequencyRatioPercent(const JosekiMove& move, qint64 total)
{
    if (total <= 0) return std::nullopt;
    return 100.0 * qMax(move.frequency, 0) / static_cast<double>(total);
}

/// 比率の下限を満たすか。比率の無い局面（出現頻度を持たない定跡）は比率で絞らない。
/// 表の比率（小数1桁）と同じ値が下限ちょうどでも使えるように、わずかな誤差は許す
inline bool meetsMinRatio(const JosekiMove& move, qint64 total, double minRatioPercent)
{
    const std::optional<double> ratio = frequencyRatioPercent(move, total);
    return !ratio || *ratio + 1e-9 >= minRatioPercent;
}

#endif // JOSEKITYPES_H
