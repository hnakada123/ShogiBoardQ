#ifndef CONSIDERATIONPOSITIONRESOLVER_H
#define CONSIDERATIONPOSITIONRESOLVER_H

/// @file considerationpositionresolver.h
/// @brief 検討モード向け局面解決ロジックの定義

#include <QString>

#include "recordline.h"

/**
 * @brief 検討する局面と、読み筋の「同」の判定に使う直前の手を、表示中の手順から決める
 *
 * 局面列・指し手列を別々の一覧から探さずに、分岐ツリーの手順（RecordLine）だけを見る。
 */
class ConsiderationPositionResolver
{
public:
    struct UpdateParams {
        QString position;        ///< USI の position コマンド
        int previousFileTo = 0;  ///< 直前の手の移動先の筋（1〜9。開始局面は0）
        int previousRankTo = 0;  ///< 直前の手の移動先の段（1〜9。開始局面は0）
        QString lastUsiMove;     ///< 直前の手（USI。開始局面は空）
    };

    /**
     * @brief 手順 line の row 手目の後の局面を検討する
     * @param fallbackSfen 手順が無いとき（分岐ツリーが空）に検討する局面（空なら平手）
     */
    static UpdateParams resolve(const RecordLine& line, int row, const QString& fallbackSfen = {});
};

#endif // CONSIDERATIONPOSITIONRESOLVER_H
