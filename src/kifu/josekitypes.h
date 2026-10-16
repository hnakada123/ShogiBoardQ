/// @file josekitypes.h
/// @brief 定跡データの値型（定跡手・局面・棋譜からのマージ候補）

#ifndef JOSEKITYPES_H
#define JOSEKITYPES_H

#include <QList>
#include <QString>

/**
 * @brief 定跡エントリの指し手情報
 */
struct JosekiMove {
    QString move;       ///< 指し手（USI形式）
    QString nextMove;   ///< 次の指し手（USI形式）
    int value;          ///< 評価値
    int depth;          ///< 深さ
    int frequency;      ///< 出現頻度
    QString comment;    ///< コメント
};

/**
 * @brief 棋譜からの定跡マージ用エントリ
 */
struct KifuMergeEntry {
    int ply;              ///< 手数
    QString sfen;         ///< この手を指す前の局面SFEN
    QString usiMove;      ///< USI形式の指し手
    QString japaneseMove; ///< 日本語表記の指し手
    bool isCurrentMove;   ///< 現在選択中の指し手かどうか
};

#endif // JOSEKITYPES_H
