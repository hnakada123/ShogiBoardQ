#ifndef THINKINGINFOROW_H
#define THINKINGINFOROW_H

/// @file thinkinginforow.h
/// @brief 思考タブ・検討タブの1行分の表示データ（info 1件を表示用に変換したもの）

#include <QMetaType>
#include <QString>
#include <QStringList>

#include "usiscore.h"

/**
 * @brief info 1件を思考の表の1行にした値
 *
 * ThinkingInfoFormatter が UsiInfo から作る。表示する文字列（評価値の「3手詰」など）は表のモデルが作る。
 */
struct ThinkingInfoRow {
    qint64 timeMs = -1;    ///< 経過時間(ms)。無ければ -1
    int depth = -1;        ///< 探索深さ。無ければ -1
    qint64 nodes = -1;     ///< ノード数。無ければ -1
    UsiScore score;        ///< 評価値（手番側から見た値）
    QString pvKanji;       ///< 漢字表記の読み筋
    QString usiPv;         ///< 局面に合った手までの USI 形式の読み筋（スペース区切り）
    QStringList enginePv;  ///< エンジンが送った読み筋（UsiInfo::pv。局面との照合前）
    QString baseSfen;      ///< 読み筋の開始局面
    QString lastUsiMove;   ///< 開始局面に至った指し手（USI 形式）
    int multipv = 1;       ///< MultiPV 番号（1 が最善）
};

Q_DECLARE_METATYPE(ThinkingInfoRow)

#endif // THINKINGINFOROW_H
