/// @file boardsyncpresenter.cpp
/// @brief 盤を表示中の手に合わせるプレゼンタの実装

#include "boardsyncpresenter.h"
#include "boardcoordinates.h"
#include "boardinteractioncontroller.h"
#include "kifubranchnode.h"
#include "kifunavigationstate.h"
#include "shogiboard.h"
#include "shogigamecontroller.h"

BoardSyncPresenter::BoardSyncPresenter(const Deps& deps, QObject* parent)
    : QObject(parent)
    , m_deps(deps)
{
    Q_ASSERT(m_deps.gc && m_deps.bic && m_deps.navState);

    connect(m_deps.navState, &KifuNavigationState::changed, this, &BoardSyncPresenter::syncToCurrentNode);
}

void BoardSyncPresenter::syncToCurrentNode()
{
    const KifuBranchNode* node = m_deps.navState->currentNode();
    ShogiBoard* board = m_deps.gc->board();
    if (!node) return;

    // 局面が違うときだけ入れ替える（終局の行は直前の局面を持つ）
    const QString sfen = node->sfen();
    if (!sfen.isEmpty() && board->toSfen() != sfen) board->setSfen(sfen);

    // 局面が変わったので、選びかけた駒は取り消す
    m_deps.bic->cancelPendingClick();
    if (node->isActualMove()) {
        const ShogiMove move = node->move();
        m_deps.bic->showMoveHighlights(BoardCoordinates::fromSquare(move.move, move.mover()),
                                       BoardCoordinates::toSquare(move.move));
    } else {
        m_deps.bic->clearAllHighlights();
    }
}
