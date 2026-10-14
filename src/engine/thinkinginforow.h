#ifndef THINKINGINFOROW_H
#define THINKINGINFOROW_H

/// @file thinkinginforow.h
/// @brief 思考タブ・検討タブの1行分の表示データ（info 1件を表示用に変換したもの）

#include <QMetaType>
#include <QString>
#include <QStringList>

/**
 * @brief info 1件を思考タブの1行に変換した値
 *
 * ThinkingInfoPresenter が UsiInfo から作り、Usi が思考タブ・検討タブのモデルに入れる。
 */
struct ThinkingInfoRow {
    QString time;          ///< 経過時間(ms)
    QString depth;         ///< 探索深さ
    QString nodes;         ///< ノード数
    QString score;         ///< 評価値の表示文字列（"120++" / "3手詰" など）
    QString pvKanji;       ///< 漢字表記の読み筋
    QString usiPv;         ///< 局面に合った手までの USI 形式の読み筋（スペース区切り）
    QStringList enginePv;  ///< エンジンが送った読み筋（UsiInfo::pv。局面との照合前）
    QString baseSfen;      ///< 読み筋の開始局面
    int multipv = 1;       ///< MultiPV 番号（1 が最善）
    int scoreCp = 0;       ///< 評価値（整数。詰みは ±31111）
};

Q_DECLARE_METATYPE(ThinkingInfoRow)

#endif // THINKINGINFOROW_H
