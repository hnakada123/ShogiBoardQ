/// @file pvline.cpp
/// @brief 読み筋盤に出す読み筋の実装

#include "pvline.h"

#include "boardcoordinates.h"
#include "kifmovetext.h"
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

PvLine::PvLine(const QString& baseSfen, const QStringList& pvMoves)
    : m_baseSfen(baseSfen)
    , m_moves(pvMoves)
{
    Position position;
    if (!position.assignSfen(baseSfen)) return;
    BoardSquare previousTo;
    for (const QString& text : pvMoves) {
        const std::optional<UsiMove> move = UsiMove::parse(text);
        if (!move) break;
        Step step;
        step.move = *move;
        step.mover = position.sideToMove();
        const Piece moving = move->isDrop() ? move->dropPieceFor(step.mover) : position.pieceAt(move->from);
        // 読み筋の手は合法手かを確かめずに指す（持ち駒の数が合わない駒打ちも指す）
        if (moving == Piece::None || !position.apply(*move, Position::DropRule::Lenient)) break;
        KifMoveText::append(step.japanese, step.mover, moving, *move, previousTo, KifMoveText::SameSquare::Compact);
        step.sfen = position.toSfen();
        previousTo = move->to;
        m_steps.append(step);
    }
}

QStringList PvLine::positions() const
{
    QStringList result{m_baseSfen};
    for (const Step& step : m_steps) result.append(step.sfen);
    return result;
}

QString PvLine::moveText(int ply) const
{
    if (ply < 1 || ply > m_steps.size()) return {};
    const Step& step = m_steps.at(ply - 1);
    if (KifuPresentation::options().notation == KifuPresentation::Notation::Western) {
        const QString before = ply == 1 ? m_baseSfen : m_steps.at(ply - 2).sfen;
        return KifuPresentation::move(before, step.move.toString(), KifuPresentation::options());
    }
    return step.japanese;
}

QString PvLine::displayPv(const QString& japanese, bool fullOrigin) const
{
    return KifuPresentation::pv(m_baseSfen, m_moves.join(QLatin1Char(' ')), japanese, fullOrigin);
}

void PvLine::setArrival(const QString& lastMove, const QString& previousSfen)
{
    m_lastMove = lastMove;
    m_previousSfen = previousSfen;
}

PvLine::Highlight PvLine::highlight(int ply) const
{
    if (ply == 0) return arrivalHighlight();
    if (ply > m_steps.size()) return {};
    const Step& step = m_steps.at(ply - 1);
    return moveHighlight(step.move, step.mover);
}

PvLine::Highlight PvLine::arrivalHighlight() const
{
    // 至った手が分かれば、それを指したのは開始局面の手番の相手
    if (const std::optional<UsiMove> move = UsiMove::parse(m_lastMove))
        return moveHighlight(*move, opponent(SfenUtils::sideToMove(m_baseSfen)));

    // 至る前の局面があれば、盤の差分から移動元・移動先を決める
    SfenDiffUtils::DiffResult diff;
    if (m_previousSfen.isEmpty() || !SfenDiffUtils::diffBoards(m_previousSfen, m_baseSfen, diff)
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
