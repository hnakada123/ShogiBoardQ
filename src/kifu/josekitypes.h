/// @file josekitypes.h
/// @brief 定跡データの値型（定跡手）

#ifndef JOSEKITYPES_H
#define JOSEKITYPES_H

#include <QList>
#include <QString>

/**
 * @brief 定跡エントリの指し手情報
 */
struct JosekiMove {
    QString move;       ///< 指し手（USI形式）
    QString nextMove;   ///< 次の指し手（USI形式。無ければ "none"）
    int value = 0;      ///< 評価値
    int depth = 0;      ///< 深さ
    int frequency = 0;  ///< 出現頻度
    QString comment;    ///< コメント
};

#endif // JOSEKITYPES_H
