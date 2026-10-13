#ifndef USIMOVE_H
#define USIMOVE_H

/// @file usimove.h
/// @brief USI 形式と1対1に対応する指し手の値型（ヘッダーだけで完結する。盤・棋譜・エンジンの各層で使う）

#include <QString>
#include <QStringView>
#include <optional>

#include "shogitypes.h"

/// 盤上のマス。筋・段とも1〜9で、0 は「なし」を表す
struct BoardSquare {
    int file = 0;
    int rank = 0;

    constexpr bool isValid() const { return file >= 1 && file <= 9 && rank >= 1 && rank <= 9; }

    friend constexpr bool operator==(BoardSquare a, BoardSquare b) { return a.file == b.file && a.rank == b.rank; }
    friend constexpr bool operator!=(BoardSquare a, BoardSquare b) { return !(a == b); }
};

/**
 * @brief USI の指し手（"7g7f"・"8h2b+"・"P*5e"）と1対1に対応する値
 *
 * 動かす駒・取る駒は持たない（局面から決まる）。成るかどうかも指し手の一部として持ち、
 * 盤やゲーム制御の状態（共有の成りフラグなど）に頼らない。
 */
struct UsiMove {
    BoardSquare from;          ///< 移動元（駒打ちでは無効）
    BoardSquare to;            ///< 移動先
    Piece drop = Piece::None;  ///< 駒打ちの駒（先手の生駒で表す。盤上の移動では None）
    bool promote = false;      ///< 成る

    bool isDrop() const { return drop != Piece::None; }

    /// 盤上の移動なら移動元・移動先が盤上にあり、駒打ちなら打てる駒種で成らない
    inline bool isValid() const;

    /// 指す側の色にした駒打ちの駒
    Piece dropPieceFor(Turn mover) const { return mover == Turn::Black ? drop : toWhite(drop); }

    /// USI 文字列を読む。前後の空白は無視する。形式が違えば nullopt
    static inline std::optional<UsiMove> parse(QStringView text);

    /// USI 文字列にする（無効な指し手は空文字列）
    inline QString toString() const;

    friend bool operator==(const UsiMove& a, const UsiMove& b)
    {
        return a.from == b.from && a.to == b.to && a.drop == b.drop && a.promote == b.promote;
    }
    friend bool operator!=(const UsiMove& a, const UsiMove& b) { return !(a == b); }
};

namespace UsiNotation {

/// USI のマス（"7g" など）を読む
inline std::optional<BoardSquare> parseSquare(QStringView text)
{
    if (text.size() != 2) return std::nullopt;
    const char16_t file = text.at(0).unicode();
    const char16_t rank = text.at(1).unicode();
    if (file < u'1' || file > u'9' || rank < u'a' || rank > u'i') return std::nullopt;
    return BoardSquare{file - u'0', rank - u'a' + 1};
}

/// USI のマスの表記にする（無効なマスは空文字列）
inline QString squareText(BoardSquare square)
{
    if (!square.isValid()) return {};
    return QString(QChar(u'0' + square.file)) + QChar(u'a' + square.rank - 1);
}

/// USI の駒打ちの駒文字（PLNSGBR、大小文字を問わない）を先手の生駒にする
inline Piece dropPieceFromChar(QChar ch)
{
    switch (ch.toUpper().unicode()) {
    case u'P': return Piece::BlackPawn;
    case u'L': return Piece::BlackLance;
    case u'N': return Piece::BlackKnight;
    case u'S': return Piece::BlackSilver;
    case u'G': return Piece::BlackGold;
    case u'B': return Piece::BlackBishop;
    case u'R': return Piece::BlackRook;
    default:   return Piece::None;
    }
}

} // namespace UsiNotation

bool UsiMove::isValid() const
{
    if (!to.isValid()) return false;
    if (isDrop()) return !promote && UsiNotation::dropPieceFromChar(pieceToChar(drop)) == drop;
    return from.isValid() && from != to;
}

std::optional<UsiMove> UsiMove::parse(QStringView text)
{
    text = text.trimmed();
    UsiMove move;
    if (text.size() == 4 && text.at(1) == QLatin1Char('*')) {
        move.drop = UsiNotation::dropPieceFromChar(text.at(0));
        const auto to = UsiNotation::parseSquare(text.mid(2, 2));
        if (move.drop == Piece::None || !to) return std::nullopt;
        move.to = *to;
        return move;
    }
    if (text.size() != 4 && text.size() != 5) return std::nullopt;
    if (text.size() == 5 && text.at(4) != QLatin1Char('+')) return std::nullopt;
    const auto from = UsiNotation::parseSquare(text.mid(0, 2));
    const auto to = UsiNotation::parseSquare(text.mid(2, 2));
    if (!from || !to || *from == *to) return std::nullopt;
    move.from = *from;
    move.to = *to;
    move.promote = (text.size() == 5);
    return move;
}

QString UsiMove::toString() const
{
    if (!isValid()) return {};
    if (isDrop()) return pieceToChar(drop) + QStringLiteral("*") + UsiNotation::squareText(to);
    QString text = UsiNotation::squareText(from) + UsiNotation::squareText(to);
    if (promote) text += QLatin1Char('+');
    return text;
}

#endif // USIMOVE_H
