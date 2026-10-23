/// @file kifmovegrammar.cpp
/// @brief KIF・KI2 の指し手の文字列の文法の実装

#include "kifmovegrammar.h"

#include "ki2movenotation.h"
#include "shoginotation.h"
#include "shogiposition.h"

namespace {

bool isBlank(QChar ch)
{
    return ch.isSpace() || ch == QChar(u'　');
}

/// 半角・全角の数字（1〜9）。それ以外は0
int digitValue(QChar ch)
{
    const char16_t u = ch.unicode();
    if (u >= u'1' && u <= u'9') return u - u'0';
    return ShogiNotation::fileFromZenkaku(ch);
}

/// "(77)"・"（７７）" の移動元を読む。読めたら i を閉じ括弧の後へ進める
std::optional<BoardSquare> readOrigin(QStringView text, qsizetype& i)
{
    const QChar open = text.at(i);
    if (open != QLatin1Char('(') && open != QChar(u'（')) return std::nullopt;
    if (i + 3 >= text.size()) return std::nullopt;
    const QChar close = text.at(i + 3);
    if (close != QLatin1Char(')') && close != QChar(u'）')) return std::nullopt;
    const BoardSquare from{digitValue(text.at(i + 1)), digitValue(text.at(i + 2))};
    if (!from.isValid()) return std::nullopt;
    i += 3;
    return from;
}

} // namespace

namespace KifMoveGrammar {

std::optional<MoveText> parseMoveText(QStringView text)
{
    qsizetype i = 0;
    const auto skipBlanks = [&text, &i] {
        while (i < text.size() && isBlank(text.at(i))) ++i;
    };
    skipBlanks();
    if (i < text.size() && ShogiNotation::isTurnMark(text.at(i))) ++i;
    skipBlanks();

    MoveText move;
    if (i < text.size() && text.at(i) == QChar(u'同')) {
        move.sameAsPrevious = true;
        ++i;
    } else {
        if (i + 1 >= text.size()) return std::nullopt;
        const int file = digitValue(text.at(i));
        int rank = ShogiNotation::rankFromKanji(text.at(i + 1));
        if (rank == 0) rank = digitValue(text.at(i + 1));
        if (file == 0 || rank == 0) return std::nullopt;
        move.to = BoardSquare{file, rank};
        i += 2;
        skipBlanks();
        // 新聞などの「７六同歩」は、移動先と「同」を両方書く
        if (i < text.size() && text.at(i) == QChar(u'同')) ++i;
    }
    skipBlanks();

    // 駒の名前（成香・成桂・成銀だけ2文字）
    if (i + 1 < text.size() && text.at(i) == QChar(u'成')) {
        move.piece = ShogiNotation::pieceFromName(text.mid(i, 2));
        i += 2;
    } else if (i < text.size()) {
        move.piece = ShogiNotation::pieceFromName(text.mid(i, 1));
        ++i;
    }
    if (move.piece == Piece::None) return std::nullopt;

    static constexpr QStringView kModifiers = u"右左直上引寄";
    for (; i < text.size(); ++i) {
        const QChar ch = text.at(i);
        if (ch == QChar(u'不') && i + 1 < text.size() && text.at(i + 1) == QChar(u'成')) {
            move.promote = false;
            ++i;
        } else if (ch == QChar(u'生')) {
            move.promote = false;
        } else if (ch == QChar(u'成')) {
            move.promote = true;
        } else if (ch == QChar(u'打')) {
            move.drop = true;
        } else if (ch == QChar(u'行')) {
            move.modifiers += QChar(u'上');
        } else if (kModifiers.contains(ch)) {
            move.modifiers += ch;
        } else if (const auto from = readOrigin(text, i)) {
            move.from = from;
        }
    }
    return move;
}

std::optional<UsiMove> resolve(const MoveText& text, const Position& board, Turn mover, BoardSquare previousTo)
{
    const BoardSquare to = text.sameAsPrevious ? previousTo : text.to;
    if (!to.isValid()) return std::nullopt;
    UsiMove move;
    move.to = to;
    if (text.drop) {
        // 打てるのは玉以外の生駒
        if (isPromoted(text.piece) || text.piece == Piece::BlackKing) return std::nullopt;
        move.drop = text.piece;
        return move;
    }
    if (text.from) {
        move.from = *text.from;
        move.promote = text.promote.value_or(false);
        return move;
    }
    return Ki2MoveNotation::resolve(text, board, mover, previousTo);
}

} // namespace KifMoveGrammar
