#ifndef JOSEKILINES_H
#define JOSEKILINES_H

/// @file josekilines.h
/// @brief 定跡と棋譜の手順（定跡を棋譜の変化に展開する・手順が定跡を外れた手を探す）

#include <QList>
#include <QMetaType>
#include <QString>
#include <QStringList>
#include <optional>

#include "recordline.h"
#include "usimove.h"

class JosekiBook;

namespace JosekiLines {

/// 定跡を棋譜の変化に展開する条件
struct ExpandOptions {
    int maxPlies = 8;              ///< 表示中の局面から何手先まで展開するか
    int maxMovesPerPosition = 3;   ///< 1つの局面から展開する定跡手の数（定跡の表の順に取る）
    double minRatioPercent = 0.0;  ///< 比率（局面の出現頻度の合計に対する割合）がこれより低い手は展開しない
    int maxMoves = 3000;           ///< 展開する手の数の上限（超えたところで止める）
};

/// 展開した結果
struct ExpandResult {
    /// 展開した手順（開始局面から行き止まりまで）。先にある手順ほど表の上の手を選んでいる
    QList<RecordLine> lines;
    int moveCount = 0;       ///< 展開した手の数（手順どうしで重なる手は1つと数える）
    int transpositions = 0;  ///< 別の手順で同じ局面に着いたので止めた数
    bool truncated = false;  ///< 手の数の上限に達して止めた
};

/**
 * @brief 定跡を棋譜の変化に展開する
 * @param startSfen 展開を始める局面
 * @param startPly 最初の手の手数
 * @param previous 開始局面に至った手（「同」の表記の基準。無ければ nullopt）
 *
 * 局面の定跡手を表の順にたどり、局面で指せない手と比率が下限より低い手は使わない（出現頻度の合計が0の局面では
 * 比率で絞らない）。別の手順で同じ局面（手数を除く）に着いたら、その手までで止める。
 */
ExpandResult expand(const JosekiBook &book, const QString &startSfen, int startPly, std::optional<UsiMove> previous,
                    const ExpandOptions &options);

/**
 * @brief 手順が定跡を外れた最初の手
 * @param sfens 各手を指す前の局面（[i] が i 番目の手の前。RecordLine::sfens() の形で、余分があってもよい）
 * @param usiMoves 手順の指し手
 * @return 外れた手の位置（0 始まり）。すべて定跡にあれば nullopt
 */
std::optional<int> firstDeviation(const JosekiBook &book, const QStringList &sfens, const QStringList &usiMoves);

} // namespace JosekiLines

Q_DECLARE_METATYPE(JosekiLines::ExpandOptions)

#endif // JOSEKILINES_H
