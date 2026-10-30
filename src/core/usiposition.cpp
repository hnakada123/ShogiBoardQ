/// @file usiposition.cpp
/// @brief エンジンに探索させる局面の値の実装

#include "usiposition.h"

#include "sfenutils.h"

#include <algorithm>

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
    // 1つの値に手を足していく（手ごとに withMove で写すと、手順の複製で手数の二乗の時間になる）
    std::optional<UsiPosition> position = fromSfen(startSfen);
    if (!position) return std::nullopt;
    position->m_moves.reserve(moves.size());
    for (const UsiMove& move : moves) {
        if (!position->advance(move)) return std::nullopt;
    }
    return position;
}

std::optional<QList<UsiPosition>> UsiPosition::alongMoves(QStringView startSfen, const QList<UsiMove>& moves,
                                                          int fromPly, int toPly)
{
    if (fromPly < 0 || fromPly > toPly || toPly > moves.size()) return std::nullopt;
    std::optional<UsiPosition> running = fromSfen(startSfen);
    if (!running) return std::nullopt;
    QStringList played;
    played.reserve(toPly);
    for (int i = 0; i < toPly; ++i) played << moves.at(i).toString();
    running->m_moves = played;

    QList<UsiPosition> positions;
    positions.reserve(toPly - fromPly + 1);
    for (int ply = 0;; ++ply) {
        if (ply >= fromPly) positions << *running;
        if (ply == toPly) break;
        const UsiMove& move = moves.at(ply);
        if (!running->m_position.apply(move, Position::DropRule::Lenient)) return std::nullopt;
        ++running->m_moveCount;
        running->m_lastMove = move;
    }
    return positions;
}

QString UsiPosition::Words::completeStart() const
{
    return start.count(QLatin1Char(' ')) == 2 ? start + QStringLiteral(" 1") : start;
}

std::optional<UsiPosition::Words> UsiPosition::split(QStringView text)
{
    const auto isKeyword = [](const QString& token, const char* keyword) {
        return token.compare(QLatin1String(keyword), Qt::CaseInsensitive) == 0;
    };
    // 空白・タブ・改行のどれで区切ってもよい
    const QStringList tokens = text.toString().simplified().split(QLatin1Char(' '), Qt::SkipEmptyParts);
    qsizetype i = 0;
    if (i < tokens.size() && isKeyword(tokens.at(i), "position")) ++i;
    if (i >= tokens.size()) return std::nullopt;

    Words words;
    if (isKeyword(tokens.at(i), "startpos")) {
        words.start = SfenUtils::hirateSfen();
        ++i;
    } else {
        if (isKeyword(tokens.at(i), "sfen")) ++i;
        // 盤・手番・持ち駒の3つの欄（"moves" が先に来れば足りない）と、数字だけなら手数の欄
        QStringList fields;
        for (; i < tokens.size() && fields.size() < 3 && !isKeyword(tokens.at(i), "moves"); ++i)
            fields << tokens.at(i);
        if (fields.size() < 3) return std::nullopt;
        if (i < tokens.size()) {
            const QString& ply = tokens.at(i);
            if (std::all_of(ply.cbegin(), ply.cend(), [](QChar c) { return c.isDigit(); })) {
                fields << ply;
                ++i;
            }
        }
        words.start = fields.join(QLatin1Char(' '));
    }
    if (i < tokens.size() && isKeyword(tokens.at(i), "moves")) ++i;
    words.moves = tokens.mid(i);
    return words;
}

std::optional<UsiPosition> UsiPosition::parse(QStringView text, QString* error)
{
    const std::optional<Words> words = split(text);
    if (!words) {
        if (error) *error = QStringLiteral("Unknown position format: %1").arg(text.trimmed().left(80));
        return std::nullopt;
    }
    const std::optional<Position> start = Position::fromSfen(words->completeStart(), error);
    if (!start) return std::nullopt;
    UsiPosition position(*start);
    for (const QString& token : words->moves) {
        const std::optional<UsiMove> move = UsiMove::parse(token);
        if (!move || !position.advance(*move)) {
            if (error) *error = QStringLiteral("Move cannot be applied: %1").arg(token);
            return std::nullopt;
        }
    }
    return position;
}

QString UsiPosition::sfenOf(QStringView text)
{
    const std::optional<Words> words = split(text);
    return words && words->moves.isEmpty() ? words->start : text.trimmed().toString();
}

std::optional<UsiPosition> UsiPosition::withMove(const UsiMove& move) const
{
    UsiPosition next = *this;
    if (!next.advance(move)) return std::nullopt;
    return next;
}

bool UsiPosition::advance(const UsiMove& move)
{
    if (!m_valid) return false;
    // 棋譜の読み込みと同じく、持ち駒が合わない駒打ちも指す（合法手かはエンジンと対局の側で確かめる）
    if (!m_position.apply(move, Position::DropRule::Lenient)) return false;
    // alongMoves の局面は後ろの手も共有しているので、自分の手順の後ろを捨ててから足す
    if (m_moves.size() > m_moveCount) m_moves.resize(m_moveCount);
    m_moves << move.toString();
    ++m_moveCount;
    m_lastMove = move;
    return true;
}

QString UsiPosition::command() const
{
    if (!m_valid) return {};
    return commandFor(m_start == Position::hirate() ? QString() : m_start.toSfen(), moves());
}

QString UsiPosition::commandFor(const QString& startSfen, const QStringList& moves)
{
    const QString start = startSfen.trimmed();
    QString text = (start.isEmpty() || start == SfenUtils::hirateSfen()) ? QStringLiteral("position startpos")
                                                                         : QStringLiteral("position sfen ") + start;
    if (!moves.isEmpty()) text += QStringLiteral(" moves ") + moves.join(QLatin1Char(' '));
    return text;
}
