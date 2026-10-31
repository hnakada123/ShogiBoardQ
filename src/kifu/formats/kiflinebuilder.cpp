/// @file kiflinebuilder.cpp
/// @brief 棋譜の読み込みで1本の手順を組み立てる部品の実装

#include "kiflinebuilder.h"

#include "moverecord.h"

#include <utility>

KifLineBuilder::KifLineBuilder(const Position& start, int startPly, std::optional<UsiMove> previous, LineKind kind)
    : m_position(start)
    , m_previous(previous)
    , m_kind(kind)
{
    m_line.startSfen = start.toSfen();
    m_line.startPly = startPly;
}

bool KifLineBuilder::addMove(const UsiMove& move, const RecordTime& time, const KifMoveText::Notes& notes,
                             Position::DropRule rule)
{
    m_lastError.clear();
    if (ended()) {
        m_lastError = QStringLiteral("The line has ended: %1").arg(move.toString());
        return false;
    }
    // 対局の着手（GameBoard）と同じ組み立てで、指した手の行を作る
    std::optional<RecordEntry> played = MoveRecord::play(m_position, move, previousTo(), time, notes, rule, &m_lastError);
    if (!played) return false;
    RecordEntry& entry = *played;
    if (m_line.entries.isEmpty()) {
        entry.comment = std::exchange(m_pendingComment, {});
        entry.bookmark = std::exchange(m_pendingBookmark, {});
    }
    m_line.entries.append(entry);
    m_previous = move;
    return true;
}

bool KifLineBuilder::correctStartTurn(Turn side)
{
    if (!m_line.entries.isEmpty()) return false;
    m_position.setSideToMove(side);
    m_line.startSfen = m_position.toSfen();
    return true;
}

void KifLineBuilder::addTerminal(TerminalType type, Turn side, const RecordTime& time, const QString& text)
{
    if (ended()) return;
    RecordEntry entry = RecordEntry::ending(type, side, time, text);
    entry.sfen = m_position.toSfen();
    if (m_line.entries.isEmpty()) {
        entry.comment = std::exchange(m_pendingComment, {});
        entry.bookmark = std::exchange(m_pendingBookmark, {});
    }
    m_line.entries.append(entry);
}

void KifLineBuilder::appendText(QString& target, const QString& text)
{
    if (!target.isEmpty()) target += QLatin1Char('\n');
    target += text;
}

void KifLineBuilder::appendComment(const QString& text)
{
    if (!m_line.entries.isEmpty()) appendText(m_line.entries.last().comment, text);
    else appendText(m_kind == LineKind::Mainline ? m_line.openingComment : m_pendingComment, text);
}

void KifLineBuilder::appendBookmark(const QString& text)
{
    if (!m_line.entries.isEmpty()) appendText(m_line.entries.last().bookmark, text);
    else appendText(m_kind == LineKind::Mainline ? m_line.openingBookmark : m_pendingBookmark, text);
}

RecordTime KifLineBuilder::timeFor(Turn side, qint64 elapsedMs, std::optional<qint64> totalMs)
{
    qint64& total = m_totalMs.at(side == Turn::Black ? 0 : 1);
    total = totalMs ? *totalMs : total + qMax<qint64>(0, elapsedMs);
    return RecordTime::fromMilliseconds(elapsedMs, total);
}

void KifLineBuilder::setLastTime(const RecordTime& time)
{
    if (!m_line.entries.isEmpty()) m_line.entries.last().time = time;
}

std::optional<Turn> KifLineBuilder::lastMover() const
{
    if (m_line.entries.isEmpty()) return std::nullopt;
    return m_line.entries.last().mover;
}

RecordLine KifLineBuilder::take()
{
    return std::move(m_line);
}
