#ifndef TSUMEEDITTYPES_H
#define TSUMEEDITTYPES_H

/// @file tsumeedittypes.h
/// @brief 詰将棋の候補局面を作る操作が受け渡しする値（駒数の上限・作った候補）

#include <QString>

/// 盤そのもの（State）と盤面の規則は tsumeeditboard.h。こちらは Hayanagi の型を使わないので、
/// 候補局面を作る操作を使う側のヘッダーからも include できる。
namespace TsumeEditBoard {

/// 作る局面の駒数と盤面構成の上限
struct Limits {
    int maxAttackPieces = 20;      ///< 攻方の盤上+持駒の合計上限（玉除く）
    int maxDefendPieces = 14;      ///< 玉以外の玉方盤上駒の上限
    int maxTotalBoardPieces = 30;  ///< 盤上の総駒数上限（玉含む）
    bool addRemainingToDefenderHand = true; ///< 未使用駒をすべて玉方持駒へ入れる（40駒保存則）
};

/// 逆算・盤面編集・余詰リペアで作った候補局面
struct Candidate {
    QString sfen;              ///< 正規化済みの攻方（先手）手番SFEN
    QString tag;               ///< 操作種別（ログ・統計用）
    QString intendedFirstMove; ///< 逆算時に想定した攻方の初手（USI、不明なら空）
};

} // namespace TsumeEditBoard

#endif // TSUMEEDITTYPES_H
