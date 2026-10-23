#ifndef THINKINGINFOROW_H
#define THINKINGINFOROW_H

/// @file thinkinginforow.h
/// @brief 思考タブ・検討タブの1行分の表示データ（info 1件を表示用に変換したもの）

#include <QMetaType>
#include <QString>

#include "pvtext.h"
#include "usiscore.h"

/**
 * @brief info 1件を思考の表の1行にした値
 *
 * ThinkingInfoFormatter が UsiInfo から作る。表示する文字列（評価値の「3手詰」・読み筋の表記など）は表のモデルが作る。
 */
struct ThinkingInfoRow {
    qint64 timeMs = -1;    ///< 経過時間(ms)。無ければ -1
    int depth = -1;        ///< 探索深さ。無ければ -1
    qint64 nodes = -1;     ///< ノード数。無ければ -1
    UsiScore score;        ///< 評価値（手番側から見た値）
    PvText pv;             ///< 読み筋（探索の局面の上で指した手と、指せなかった語）
    QString lastUsiMove;   ///< 探索の局面に至った指し手（USI 形式。読み筋盤の強調に使う）
    int multipv = 1;       ///< MultiPV 番号（1 が最善）
};

Q_DECLARE_METATYPE(ThinkingInfoRow)

#endif // THINKINGINFOROW_H
