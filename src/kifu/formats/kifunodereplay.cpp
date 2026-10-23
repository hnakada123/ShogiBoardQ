/// @file kifunodereplay.cpp
/// @brief 棋譜の手順のノードを局面と一緒にたどる処理の実装

#include "kifunodereplay.h"

#include "kifubranchnode.h"

KifuNodeReplay::KifuNodeReplay(const QString& startSfen, const QString& fallbackSfen)
{
    m_synced = m_board.assignSfen(startSfen) || (!fallbackSfen.isEmpty() && m_board.assignSfen(fallbackSfen));
    m_mover = m_board.sideToMove();
}

KifuNodeReplay KifuNodeReplay::after(const KifuBranchNode* node)
{
    KifuNodeReplay replay(node->sfen());
    if (node->isActualMove() && node->move().move.isValid()) replay.m_previousTo = node->move().move.to;
    return replay;
}

bool KifuNodeReplay::canWrite(const KifuBranchNode* node) const
{
    return m_synced && node->isActualMove() && node->move().move.isValid();
}

bool KifuNodeReplay::advance(const KifuBranchNode* node)
{
    if (!node->isActualMove()) return true;
    const UsiMove move = node->move().move;
    const bool applied = canWrite(node) && m_board.applyAs(move, m_mover);
    if (applied) {
        m_mover = opponent(m_mover);
    } else {
        resync(node);
    }
    m_previousTo = move.isValid() ? move.to : BoardSquare{};
    return applied;
}

KifuNodeReplay KifuNodeReplay::branchFrom(const KifuBranchNode* parent) const
{
    KifuNodeReplay copy = *this;
    if (!copy.m_synced && parent) copy.resync(parent);
    return copy;
}

void KifuNodeReplay::resync(const KifuBranchNode* node)
{
    m_synced = m_board.assignSfen(node->sfen());
    m_mover = m_synced ? m_board.sideToMove() : opponent(m_mover);
}
