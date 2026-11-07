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
                             const WrittenMove& written, Position::DropRule rule)
{
    m_lastError.clear();
    m_lastIrregularity.clear();
    if (ended()) {
        m_lastError = QStringLiteral("The line has ended: %1").arg(move.toString());
        return false;
    }
    // 書かれた手番・駒が局面と違う手は、局面の手番・駒に読み替えない（全形式で同じ）
    if (written.side && *written.side != m_position.sideToMove()) {
        m_lastError = QStringLiteral("The move is written for the side not to move: %1").arg(move.toString());
        return false;
    }
    if (written.piece != Piece::None && !move.isDrop()
        && m_position.mismatchOf(move, rule) == Position::MoveMismatch::None
        && toBlack(m_position.pieceAt(move.from)) != written.piece) {
        m_lastError = QStringLiteral("The written piece is not the piece on the source square: %1").arg(move.toString());
        return false;
    }
    // 反則の駒打ちも記録どおり指すが、指す前の局面で何が反則かを残す（呼び出し側が警告にする）
    if (rule == Position::DropRule::Lenient) {
        const Position::MoveMismatch mismatch = m_position.mismatchOf(move, Position::DropRule::RequireHand);
        if (move.isDrop() && mismatch != Position::MoveMismatch::None)
            m_lastIrregularity = MoveRecord::mismatchText(mismatch).arg(move.toString());
    }
    // 対局の着手（GameBoard）と同じ組み立てで、指した手の行を作る
    std::optional<RecordEntry> played = MoveRecord::play(m_position, move, previousTo(), time, notes, rule, &m_lastError);
    if (!played) {
        m_lastIrregularity.clear();
        return false;
    }
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
