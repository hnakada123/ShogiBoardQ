#ifndef JOSEKIMOVEPICKER_H
#define JOSEKIMOVEPICKER_H

/// @file josekimovepicker.h
/// @brief 対局で指す定跡手を、対局者の条件（JosekiPlayOptions）に合わせて選ぶ

#include <QList>
#include <QString>
#include <optional>

#include "josekiplayoptions.h"
#include "josekitypes.h"

class JosekiBook;
class QRandomGenerator;

/// 対局で選んだ定跡手
struct JosekiPick {
    JosekiMove move;            ///< 選んだ定跡手
    double ratioPercent = 0.0;  ///< その局面の出現頻度の合計に対する割合（%。合計が0なら0）
};

namespace JosekiMovePicker {

/// 局面 sfen（手数付き）の定跡手のうち、条件に合い、その局面で指せる手（表の順）。手数の条件は見ない
QList<JosekiMove> candidates(const QList<JosekiMove> &moves, const QString &sfen, const JosekiPlayOptions &options);

/**
 * @brief 局面 sfen（手数付き）で指す定跡手を選ぶ
 *
 * 定跡を使わない・SFEN の手数が条件の手数を超えた・条件に合う手が無いときは nullopt。
 * 選び方は options.method（出現頻度に比例・評価値で選ぶ・先頭の手）。
 */
std::optional<JosekiPick> pick(const JosekiBook &book, const QString &sfen, const JosekiPlayOptions &options,
                               QRandomGenerator &random);

} // namespace JosekiMovePicker

#endif // JOSEKIMOVEPICKER_H
