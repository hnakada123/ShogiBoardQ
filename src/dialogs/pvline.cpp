/// @file pvline.cpp
/// @brief 読み筋盤に出す読み筋の実装

#include "pvline.h"

#include "boardcoordinates.h"
#include "kifupresentation.h"
#include "sfenutils.h"
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

QString PvLine::ending() const
{
    return KifuPresentation::pvEnding(m_pv);
}

void PvLine::setArrival(const QString& lastMove)
{
    m_lastMove = lastMove;
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
    // 至った手が分かれば、それを指したのは開始局面の手番の相手（分からなければハイライトしない）
    if (const std::optional<UsiMove> move = UsiMove::parse(m_lastMove))
        return moveHighlight(*move, opponent(SfenUtils::sideToMove(m_pv.baseSfen())));
    return {};
}
