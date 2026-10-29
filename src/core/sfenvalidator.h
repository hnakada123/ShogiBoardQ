#ifndef SFENVALIDATOR_H
#define SFENVALIDATOR_H

/// @file sfenvalidator.h
/// @brief SFEN 文字列の妥当性検査（局面を値で返す。JSON にするのは headless の SfenValidationJson）

#include <QMap>
#include <QString>
#include <QStringList>
#include <optional>

#include "shogiposition.h"

/// SFEN の検査結果
struct SfenValidation {
    bool valid = false;               ///< 局面として使えるか
    QStringList errors;               ///< 不正な理由（valid=false のとき 1 件以上）
    std::optional<Position> position; ///< 読めた局面（盤と持駒の形が正しいとき。valid でない局面もある）
    QString normalizedSfen;           ///< 正規化した SFEN（position があるとき）
    QString turn;                     ///< "b" / "w"
    int moveNumber = 0;               ///< 手数カウンタ
    bool blackKing = false;           ///< 先手玉があるか
    bool whiteKing = false;           ///< 後手玉があるか
    bool inCheck = false;             ///< 手番側の玉に王手がかかっているか
    bool opponentInCheck = false;     ///< 手番でない側の玉に王手がかかっているか（不正局面）
    int legalMoveCount = -1;          ///< 手番側の合法手数（計算できないとき -1）
    QMap<QString, int> blackHand;     ///< 先手持駒（"R","B","G","S","N","L","P" → 枚数）
    QMap<QString, int> whiteHand;     ///< 後手持駒
};

/**
 * @brief SFEN を検査し、局面と手番・持駒・王手・合法手数を返す
 *
 * 手番と手数の欄はここで、盤面と持駒の文法は Position::fromSfen、局面として成り立つか（駒数上限・玉・
 * 手番でない側への王手）と王手・合法手の数は PositionValidator で調べる。
 * 詰将棋の慣例に合わせ、攻方の玉が無い局面も有効とする（両玉が無い局面は無効）。
 */
namespace SfenValidator {

/// "startpos" と "position sfen ..." 形式も受け付ける
SfenValidation validate(const QString& sfen);

/// 局面に USI 手を順に適用した後の SFEN を返す（合法手だけを指す）。不正な手があれば空文字列と error
QString applyUsiMoves(const QString& sfen, const QStringList& moves, QString* error = nullptr);

} // namespace SfenValidator

#endif // SFENVALIDATOR_H
