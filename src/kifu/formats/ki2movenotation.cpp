/// @file ki2movenotation.cpp
/// @brief KI2 の指し手の文字列と、局面の上の指し手（USI）との対応の実装

#include "ki2movenotation.h"

#include "enginemovevalidator.h"
#include "shogiposition.h"
#include "shoginotation.h"

#include <QList>
#include <QStringList>
#include <algorithm>
#include <utility>

namespace {

/// 指す側から見て前へ動いた段数（正なら上がる、負なら引く、0 なら寄る）
int advance(BoardSquare from, BoardSquare to, Turn mover)
{
    return (mover == Turn::Black) ? from.rank - to.rank : to.rank - from.rank;
}

/// 指す側から見て右にあるほど大きい値（先手から見た右は筋の小さい側）
int rightness(BoardSquare square, Turn mover)
{
    return (mover == Turn::Black) ? -square.file : square.file;
}

/**
 * @brief 修飾語で移動元を絞る
 *
 * 右・左は候補のうち最も右・左の筋の駒、上・引・寄は動く向き、直は真っ直ぐ上がる駒。
 * 修飾語が2つあれば、右・左で絞ってから上・引・寄で絞る（「左上」は左の駒のうち上がる駒）。
 */
QList<BoardSquare> filterSources(QList<BoardSquare> sources, QStringView modifiers, BoardSquare to, Turn mover)
{
    const auto keepIf = [&sources](const auto& predicate) {
        QList<BoardSquare> kept;
        for (const BoardSquare& square : std::as_const(sources)) {
            if (predicate(square)) kept.append(square);
        }
        sources = std::move(kept);
    };
    const auto keepExtreme = [&sources, &keepIf, mover](int sign) {
        if (sources.isEmpty()) return;
        const auto byPosition = [mover, sign](BoardSquare a, BoardSquare b) {
            return sign * rightness(a, mover) < sign * rightness(b, mover);
        };
        const int best = rightness(*std::max_element(sources.cbegin(), sources.cend(), byPosition), mover);
        keepIf([best, mover](BoardSquare square) { return rightness(square, mover) == best; });
    };

    if (modifiers.contains(u'右')) keepExtreme(1);
    if (modifiers.contains(u'左')) keepExtreme(-1);
    if (modifiers.contains(u'上')) keepIf([to, mover](BoardSquare s) { return advance(s, to, mover) > 0; });
    if (modifiers.contains(u'引')) keepIf([to, mover](BoardSquare s) { return advance(s, to, mover) < 0; });
    if (modifiers.contains(u'寄')) keepIf([to, mover](BoardSquare s) { return advance(s, to, mover) == 0; });
    if (modifiers.contains(u'直')) {
        keepIf([to, mover](BoardSquare s) { return s.file == to.file && advance(s, to, mover) > 0; });
    }
    return sources;
}

/**
 * @brief 同じ駒が to へ動ける移動元から、from の駒を区別する修飾語を選ぶ（日本将棋連盟の棋譜の表記方法）
 *
 * 動作（上・引・寄）で区別できればそれを書き、できなければ位置（右・左）で区別する。
 * 金・銀などが真っ直ぐ1段上がるときは「直」。飛・角・竜・馬は「直」を使わず右・左で区別する。
 * 3枚以上で1字で区別できないときは、位置・動作の順に2字で書く（「左上」）。
 */
QString modifierFor(const QList<BoardSquare>& sources, BoardSquare from, BoardSquare to, Piece piece, Turn mover)
{
    if (sources.size() <= 1) return {};

    const int forward = advance(from, to, mover);
    const QString action = forward > 0 ? QStringLiteral("上")
                         : forward < 0 ? QStringLiteral("引") : QStringLiteral("寄");
    const Piece base = demote(toBlack(piece));
    const bool usesStraight = base != Piece::BlackRook && base != Piece::BlackBishop;

    QStringList candidates{action};
    if (usesStraight && forward > 0 && from.file == to.file) candidates << QStringLiteral("直");
    candidates << QStringLiteral("右") << QStringLiteral("左")
               << QStringLiteral("右") + action << QStringLiteral("左") + action;
    for (const QString& modifiers : std::as_const(candidates)) {
        const QList<BoardSquare> kept = filterSources(sources, modifiers, to, mover);
        if (kept.size() == 1 && kept.first() == from) return modifiers;
    }
    return {};
}

/// 指し手の一覧から、文字列が表す手を1つに決める
std::optional<UsiMove> resolveAmong(const QList<UsiMove>& moves, const KifMoveGrammar::MoveText& text,
                                    const Position& board, BoardSquare to, Turn mover)
{
    if (!text.drop) {
        // 盤上の同じ駒の移動。移動元ごとに、書かれた成・不成に合う手を1つ選ぶ
        QList<BoardSquare> sources;
        QList<UsiMove> chosen;
        for (const UsiMove& move : moves) {
            if (move.isDrop() || move.to != to || toBlack(board.pieceAt(move.from)) != text.piece) continue;
            if (text.promote && move.promote != *text.promote) continue;
            const qsizetype index = sources.indexOf(move.from);
            if (index < 0) {
                sources.append(move.from);
                chosen.append(move);
            } else if (!move.promote) {
                // 成・不成を書いていなければ不成の手（成るしかない手は成る手）
                chosen[index] = move;
            }
        }
        if (!sources.isEmpty()) {
            const QList<BoardSquare> kept = filterSources(sources, text.modifiers, to, mover);
            if (kept.size() != 1) return std::nullopt;
            return chosen.at(sources.indexOf(kept.first()));
        }
    }

    // 盤上の同じ駒が動けないときは「打」を省ける
    if (isPromoted(text.piece) || text.promote.value_or(false)) return std::nullopt;
    for (const UsiMove& move : moves) {
        if (move.isDrop() && move.to == to && move.drop == text.piece) return move;
    }
    return std::nullopt;
}

} // namespace

namespace Ki2MoveNotation {

std::optional<UsiMove> resolve(const KifMoveGrammar::MoveText& text, const Position& board, Turn mover,
                               BoardSquare previousTo)
{
    const BoardSquare to = text.sameAsPrevious ? previousTo : text.to;
    if (!to.isValid() || text.piece == Piece::None) return std::nullopt;

    const auto position = EngineMoveValidator::load(board, mover);
    const auto legal = resolveAmong(EngineMoveValidator::legalMoves(position), text, board, to, mover);
    if (legal) return legal;
    // 合法手で決まらないのは、反則手を書いた棋譜。駒の動きだけで決める
    return resolveAmong(EngineMoveValidator::pseudoLegalMoves(position), text, board, to, mover);
}

Annotation annotate(const Position& board, Turn mover, const UsiMove& move)
{
    const Piece moving = move.isDrop() ? move.drop : toBlack(board.pieceAt(move.from));

    // 読み込みと同じく、反則手は駒の動きだけで決めた手の中で区別する
    const auto position = EngineMoveValidator::load(board, mover);
    QList<UsiMove> moves = EngineMoveValidator::legalMoves(position);
    if (!moves.contains(move)) moves = EngineMoveValidator::pseudoLegalMoves(position);

    // 同じ駒で同じマスへ動ける移動元（成り・不成は区別しない）
    QList<BoardSquare> sources;
    Annotation annotation;
    for (const UsiMove& candidate : std::as_const(moves)) {
        if (candidate.isDrop() || candidate.to != move.to) continue;
        if (toBlack(board.pieceAt(candidate.from)) != moving) continue;
        if (!sources.contains(candidate.from)) sources.append(candidate.from);
        if (!move.isDrop() && candidate.from == move.from && candidate.promote) annotation.canPromote = true;
    }

    annotation.sources = sources;
    if (move.isDrop()) {
        if (!sources.isEmpty()) annotation.modifiers = QStringLiteral("打");
    } else {
        annotation.modifiers = modifierFor(sources, move.from, move.to, moving, mover);
    }
    return annotation;
}

QString format(const Position& board, Turn mover, const UsiMove& move, BoardSquare previousTo)
{
    const Piece moving = move.isDrop() ? move.drop : toBlack(board.pieceAt(move.from));

    QString text = (mover == Turn::Black) ? QStringLiteral("▲") : QStringLiteral("△");
    if (previousTo.isValid() && previousTo == move.to) {
        text += QStringLiteral("同　");
    } else {
        text += ShogiNotation::fileZenkaku(move.to.file) + ShogiNotation::rankKanji(move.to.rank);
    }
    text += ShogiNotation::movePieceName(moving);

    const Annotation annotation = annotate(board, mover, move);
    text += annotation.modifiers;
    if (move.promote) {
        text += QStringLiteral("成");
    } else if (annotation.canPromote) {
        text += QStringLiteral("不成");
    }
    return text;
}

} // namespace Ki2MoveNotation
