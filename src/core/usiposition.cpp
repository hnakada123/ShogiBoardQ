/// @file usiposition.cpp
/// @brief エンジンに探索させる局面の値の実装

#include "usiposition.h"

#include "sfenutils.h"

UsiPosition::UsiPosition(const Position& start, std::optional<UsiMove> lastMove)
    : m_valid(true)
    , m_start(start)
    , m_position(start)
    , m_lastMove(lastMove)
{
}

std::optional<UsiPosition> UsiPosition::fromSfen(QStringView sfen, std::optional<UsiMove> lastMove)
{
    const QStringView text = sfen.trimmed();
    if (text == u"startpos") return UsiPosition(Position::hirate(), lastMove);
    const std::optional<Position> start = Position::fromSfen(text);
    if (!start) return std::nullopt;
    return UsiPosition(*start, lastMove);
}

std::optional<UsiPosition> UsiPosition::fromMoves(QStringView startSfen, const QList<UsiMove>& moves)
{
    std::optional<UsiPosition> position = fromSfen(startSfen);
    for (const UsiMove& move : moves) {
        if (!position) return std::nullopt;
        position = position->withMove(move);
    }
    return position;
}

std::optional<UsiPosition> UsiPosition::parse(QStringView text, QString* error)
{
    QStringView rest = text.trimmed();
    if (rest.startsWith(u"position ")) rest = rest.sliced(9).trimmed();
    QStringView movesPart;
    if (const qsizetype moves = rest.indexOf(u" moves"); moves >= 0) {
        movesPart = rest.sliced(moves + 6).trimmed();
        rest = rest.left(moves).trimmed();
    } else if (rest.endsWith(u"moves")) {
        rest = rest.chopped(5).trimmed();
    }
    if (rest.startsWith(u"sfen ")) rest = rest.sliced(5).trimmed();

    std::optional<UsiPosition> position;
    if (rest == u"startpos") {
        position = UsiPosition(Position::hirate());
    } else if (const std::optional<Position> start = rest.count(QLatin1Char(' ')) == 2
                   ? Position::fromSfen(rest.toString() + QStringLiteral(" 1"), error)
                   : Position::fromSfen(rest, error)) {
        position = UsiPosition(*start);
    } else {
        return std::nullopt;
    }
    for (const QStringView token : movesPart.tokenize(QLatin1Char(' '), Qt::SkipEmptyParts)) {
        const std::optional<UsiMove> move = UsiMove::parse(token);
        std::optional<UsiPosition> next = move ? position->withMove(*move) : std::nullopt;
        if (!next) {
            if (error) *error = QStringLiteral("Move cannot be applied: %1").arg(token);
            return std::nullopt;
        }
        position = std::move(next);
    }
    return position;
}

std::optional<UsiPosition> UsiPosition::withMove(const UsiMove& move) const
{
    if (!m_valid) return std::nullopt;
    UsiPosition next = *this;
    // 棋譜の読み込みと同じく、持ち駒が合わない駒打ちも指す（合法手かはエンジンと対局の側で確かめる）
    if (!next.m_position.apply(move, Position::DropRule::Lenient)) return std::nullopt;
    next.m_moves << move.toString();
    next.m_lastMove = move;
    return next;
}

QString UsiPosition::command() const
{
    if (!m_valid) return {};
    return commandFor(m_start == Position::hirate() ? QString() : m_start.toSfen(), m_moves);
}

QString UsiPosition::commandFor(const QString& startSfen, const QStringList& moves)
{
    const QString start = startSfen.trimmed();
    QString text = (start.isEmpty() || start == SfenUtils::hirateSfen()) ? QStringLiteral("position startpos")
                                                                         : QStringLiteral("position sfen ") + start;
    if (!moves.isEmpty()) text += QStringLiteral(" moves ") + moves.join(QLatin1Char(' '));
    return text;
}
