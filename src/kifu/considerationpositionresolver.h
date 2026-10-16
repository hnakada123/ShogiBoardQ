#ifndef CONSIDERATIONPOSITIONRESOLVER_H
#define CONSIDERATIONPOSITIONRESOLVER_H

/// @file considerationpositionresolver.h
/// @brief 検討モード向け局面解決ロジックの定義

#include <QString>

#include "recordline.h"
#include "usiposition.h"

/**
 * @brief 検討する局面（読み筋の「同」の判定に使う直前の手を含む）を、表示中の手順から決める
 *
 * 局面列・指し手列を別々の一覧から探さずに、分岐ツリーの手順（RecordLine）だけを見る。
 */
class ConsiderationPositionResolver
{
public:
    /**
     * @brief 手順 line の row 手目の後の局面を検討する
     * @param fallbackSfen 手順が無いとき（分岐ツリーが空）に検討する局面（空なら平手）
     */
    static UsiPosition resolve(const RecordLine& line, int row, const QString& fallbackSfen = {});
};

#endif // CONSIDERATIONPOSITIONRESOLVER_H
