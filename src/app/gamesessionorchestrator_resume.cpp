/// @file gamesessionorchestrator_resume.cpp
/// @brief 中断した対局への復帰と棋譜・UIの同期
#include "gamesessionorchestrator.h"
#include "kifubranchtree.h"
#include "kifubranchnode.h"
#include "kifunavigationstate.h"
#include "kifurecordlistmodel.h"
#include "kifudisplay.h"
#include "kifupresentation.h"
#include "livegamesession.h"
#include "replaycontroller.h"

void GameSessionOrchestrator::onGameInterrupted()
{
    auto* match = m_deps.match ? *m_deps.match : nullptr;
    auto* node = m_deps.navState ? m_deps.navState->currentNode() : nullptr;
    if (!match || !match->interruptedGame() || !node || !node->isTerminal()) return;
    m_interruptedNodeId = node->nodeId();
    if (m_deps.setResumeAvailable) m_deps.setResumeAvailable(true);
}

void GameSessionOrchestrator::discardInterruptedGame()
{
    if (m_interruptedNodeId < 0) return;
    m_interruptedNodeId = -1;
    if (m_deps.match && *m_deps.match) (*m_deps.match)->discardInterruptedGame();
    if (m_deps.setResumeAvailable) m_deps.setResumeAvailable(false);
}

void GameSessionOrchestrator::handleResumeGame()
{
    auto* match = m_deps.match ? *m_deps.match : nullptr;
    auto* start = m_deps.gameStart ? *m_deps.gameStart : nullptr;
    auto* tree = m_deps.branchTree;
    auto* nav = m_deps.navState;
    if (!match || !start || !match->interruptedGame() || !tree || !nav
        || !m_deps.liveSession || !m_deps.kifuModel) return;
    auto* terminal = tree->nodeAt(m_interruptedNodeId);
    if (!terminal || !terminal->isTerminal() || !terminal->parent()) {
        discardInterruptedGame();
        return;
    }
    auto* node = terminal->parent();
    const auto state = *match->interruptedGame();
    m_interruptedNodeId = -1;
    if (m_deps.setResumeAvailable) m_deps.setResumeAvailable(false);

    // 閲覧位置にかかわらず中断した手の直前へ戻す。削除前に全選択を退避する。
    nav->setCurrentNode(node);
    nav->resetPreferredLineIndex();
    nav->clearLineSelectionMemory();
    nav->rememberPathSelections(node);
    m_deps.liveSession->discard();
    tree->removeLeaf(terminal);
    m_deps.liveSession->startFromNode(node);

    m_deps.kifuModel->clearAllItems();
    const auto path = tree->pathToNode(node);
    for (auto* entry : path) {
        auto* item = new KifuDisplay(entry->displayText(), entry->timeText(), entry->comment());
        if (entry->parent()) {
            item->beforeSfen = entry->parent()->sfen();
            item->usiMove = KifuPresentation::usiMove(entry->move());
        }
        m_deps.kifuModel->appendItem(item);
    }
    if (m_deps.playMode) *m_deps.playMode = state.options.mode;
    if (m_deps.currentSfenStr) *m_deps.currentSfenStr = state.options.sfenStart;
    if (m_deps.currentSelectedPly) *m_deps.currentSelectedPly = node->ply();
    if (m_deps.replayController) m_deps.replayController->setReplayMode(false);
    if (m_deps.prepareResumeUi) m_deps.prepareResumeUi(node->ply());
    start->resumeInterruptedGame();
}
