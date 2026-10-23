/// @file pvline.cpp
/// @brief 読み筋盤に出す読み筋の実装

#include "pvline.h"

#include "boardcoordinates.h"
#include "kifupresentation.h"
#include "sfendiffutils.h"
#include "shogiposition.h"

namespace {

PvLine::Highlight moveHighlight(const UsiMove& move, Turn mover)
{
    PvLine::Highlight highlight;
    const QPoint from = BoardCoordinates::fromSquare(move, mover);
    if (!from.isNull()) highlight.from = from;
    highlight.to = BoardCoordinates::toSquare(move);
    return highlight;
}

} // namespace

PvLine::PvLine(const PvText& pv)
    : m_pv(pv)
    , m_positions{pv.baseSfen()}
{
    Position position;
    if (!position.assignSfen(pv.baseSfen())) return;
    for (const PvText::Step& step : pv.steps()) {
        position.apply(step.move);
        m_positions.append(position.toSfen());
    }
}

QString PvLine::moveText(int ply) const
{
    return KifuPresentation::pvMove(m_pv, ply);
}

QString PvLine::displayPv(bool fullOrigin) const
{
    return KifuPresentation::pv(m_pv, fullOrigin);
}

void PvLine::setArrival(const QString& lastMove, const QString& previousSfen)
{
    m_lastMove = lastMove;
    m_previousSfen = previousSfen;
}

PvLine::Highlight PvLine::highlight(int ply) const
{
    if (ply == 0) return arrivalHighlight();
    if (ply > m_pv.moveCount()) return {};
    const PvText::Step& step = m_pv.steps().at(ply - 1);
    return moveHighlight(step.move, step.mover);
}

PvLine::Highlight PvLine::arrivalHighlight() const
{
    // 至った手が分かれば、それを指したのは開始局面の手番の相手
    if (const std::optional<UsiMove> move = UsiMove::parse(m_lastMove))
        return moveHighlight(*move, opponent(SfenUtils::sideToMove(m_pv.baseSfen())));

    // 至る前の局面があれば、盤の差分から移動元・移動先を決める
    SfenDiffUtils::DiffResult diff;
    if (m_previousSfen.isEmpty() || !SfenDiffUtils::diffBoards(m_previousSfen, m_pv.baseSfen(), diff)
        || !diff.isSingleMovePattern() || !diff.hasTo())
        return {};
    Highlight highlight;
    if (diff.hasFrom()) {
        highlight.from = diff.from + QPoint(1, 1);
    } else if (diff.droppedPiece != Piece::None) {
        // 駒打ちは、打った側の駒台の駒の位置を移動元にする
        const QPoint stand = BoardCoordinates::standSquareOf(diff.droppedPiece);
        if (!stand.isNull()) highlight.from = stand;
    }
    highlight.to = diff.to + QPoint(1, 1);
    return highlight;
}
