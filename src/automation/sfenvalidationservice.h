#ifndef SFENVALIDATIONSERVICE_H
#define SFENVALIDATIONSERVICE_H

/// @file sfenvalidationservice.h
/// @brief SFEN 文字列の妥当性検査（自動化・CLI 用、GUI 非依存）

#include <QJsonObject>
#include <QMap>
#include <QString>
#include <QStringList>

/// SFEN の検査結果
struct SfenValidation {
    bool valid = false;               ///< 局面として使えるか
    QStringList errors;               ///< 不正な理由（valid=false のとき 1 件以上）
    QString normalizedSfen;           ///< 正規化した SFEN（valid のとき）
    QString turn;                     ///< "b" / "w"
    int moveNumber = 0;               ///< 手数カウンタ
    bool blackKing = false;           ///< 先手玉があるか
    bool whiteKing = false;           ///< 後手玉があるか
    bool inCheck = false;             ///< 手番側の玉に王手がかかっているか
    bool opponentInCheck = false;     ///< 手番でない側の玉に王手がかかっているか（不正局面）
    int legalMoveCount = -1;          ///< 手番側の合法手数（計算できないとき -1）
    QMap<QString, int> blackHand;     ///< 先手持駒（"R","B","G","S","N","L","P" → 枚数）
    QMap<QString, int> whiteHand;     ///< 後手持駒

    /// JSON 表現（snake_case）
    QJsonObject toJson() const;
};

/**
 * @brief SFEN を検査し、手番・持駒・王手・合法手数を返す
 *
 * 構文は `ShogiBoard::parseSfen`、盤面と持駒の整合（駒数上限、玉の重複）と
 * 王手・合法手は内蔵の Hayanagi `shogi::Position` で調べる。
 * 詰将棋の慣例に合わせ、攻方の玉が無い局面も有効とする（両玉が無い局面は無効）。
 */
class SfenValidationService
{
public:
    /// "startpos" と "position sfen ..." 形式も受け付ける
    static SfenValidation validate(const QString& sfen);

    /// 局面に USI 手を順に適用した後の SFEN を返す。不正な手があれば空文字列と error
    static QString applyUsiMoves(const QString& sfen, const QStringList& moves, QString* error = nullptr);
};

#endif // SFENVALIDATIONSERVICE_H
