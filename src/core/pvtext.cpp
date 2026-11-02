/// @file pvtext.cpp
/// @brief 局面の上で指した読み筋の実装

#include "pvtext.h"

#include "shogiposition.h"

namespace {

/// 読み筋の末尾に付く "(57.54%)" のような、指し手でない語か（指し手の形の崩れたものは含めない）
bool isAnnotation(QStringView token)
{
    if (token.size() < 4) return false;
    const QChar head = token.front();
    return !head.isDigit() && !QStringView(u"PLNSGBR").contains(head);
}

} // namespace

PvText PvText::play(const QString& baseSfen, const QStringList& tokens, BoardSquare previousTo)
{
    PvText pv;
    pv.m_baseSfen = baseSfen;
    pv.m_previousTo = previousTo;
    Position board;
    const bool readable = !baseSfen.isEmpty() && board.assignSfen(baseSfen);
    qsizetype index = 0;
    if (readable) {
        pv.m_steps.reserve(tokens.size());
        for (; index < tokens.size(); ++index) {
            const std::optional<UsiMove> move = UsiMove::parse(tokens.at(index));
            if (!move) break;
            // 手番側の手として局面に合わない手（相手の駒・自分の駒を取る・持ち駒にない駒を打つ）から先は指さない
            if (board.mismatchOf(*move) != Position::MoveMismatch::None) break;
            const Turn mover = board.sideToMove();
            const Piece moving = board.describe(*move).movingPiece;
            if (!board.apply(*move)) break;
            pv.m_steps.append(Step{*move, mover, moving});
        }
    }
    pv.m_rest = tokens.mid(index);
    return pv;
}

PvText PvText::play(const QString& baseSfen, const QString& usiText, BoardSquare previousTo)
{
    return play(baseSfen, usiText.split(QLatin1Char(' '), Qt::SkipEmptyParts), previousTo);
}

std::optional<UsiMove> PvText::firstMove() const
{
    if (m_steps.isEmpty()) return std::nullopt;
    return m_steps.constFirst().move;
}

QStringList PvText::usiMoves() const
{
    QStringList moves;
    moves.reserve(m_steps.size());
    for (const Step& step : m_steps) moves.append(step.move.toString());
    return moves;
}

QString PvText::usiText() const
{
    return usiMoves().join(QLatin1Char(' '));
}

TerminalType PvText::declaration() const
{
    if (m_rest.isEmpty()) return TerminalType::None;
    if (m_rest.constFirst() == QLatin1String("resign")) return TerminalType::Resign;
    if (m_rest.constFirst() == QLatin1String("win")) return TerminalType::DeclarationWin;
    return TerminalType::None;
}

QString PvText::annotation() const
{
    return m_rest.size() == 1 && isAnnotation(m_rest.constFirst()) ? m_rest.constFirst() : QString();
}
