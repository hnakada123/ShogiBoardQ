/// @file gamesessionorchestrator_resume.cpp
/// @brief 中断した対局への復帰と棋譜・UIの同期
#include "gamesessionorchestrator.h"
#include "kifubranchtree.h"
#include "kifubranchnode.h"
#include "kifunavigationstate.h"
#include "livegamesession.h"
#include "replaycontroller.h"
#include "gamerecordmodel.h"
#include "playerinfowiring.h"
#include "shogiview.h"
#include "timecontrolcontroller.h"
#include "uistatepolicymanager.h"

#include <QDateTime>

void GameSessionOrchestrator::onGameInterrupted()
{
    auto* match = m_deps.match;
    auto* node = m_deps.navState ? m_deps.navState->currentNode() : nullptr;
    if (!match || !match->interruptedGame() || !node || !node->isTerminal()) return;
    m_interruptedNodeId = node->nodeId();
    if (m_deps.uiStatePolicy) m_deps.uiStatePolicy->setResumeAvailable(true);
}

void GameSessionOrchestrator::discardInterruptedGame()
{
    if (m_interruptedNodeId < 0) return;
    m_interruptedNodeId = -1;
    if (m_deps.match) m_deps.match->discardInterruptedGame();
    if (m_deps.uiStatePolicy) m_deps.uiStatePolicy->setResumeAvailable(false);
}

void GameSessionOrchestrator::handleResumeGame()
{
    auto* match = m_deps.match;
    auto* start = m_deps.gameStart;
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
    if (m_deps.uiStatePolicy) m_deps.uiStatePolicy->setResumeAvailable(false);

    // 閲覧位置にかかわらず中断した手の直前へ戻す。削除前に全選択を退避する。
    nav->setCurrentNode(node);
    nav->resetPreferredLineIndex();
    nav->clearLineSelectionMemory();
    nav->rememberPathSelections(node);
    m_deps.liveSession->discard();
    tree->removeLeaf(terminal);
    // sessionStarted の表示同期で、手数・時間見出しを含めた棋譜欄を復元する。
    // ここで簡略な表示に作り直すと列幅が縮み、ウィンドウ全体が再配置される。
    m_deps.liveSession->startFromNode(node);

    m_deps.match->setPlayMode(state.options.mode);
    if (m_deps.currentSfenStr) *m_deps.currentSfenStr = state.options.sfenStart;
    if (m_deps.replayController) m_deps.replayController->setReplayMode(false);
    // 終局表示を解き、盤を操作できるようにして、終局時刻を消す
    if (m_deps.shogiView) {
        m_deps.shogiView->setGameOverStyleLock(false);
        m_deps.shogiView->setMouseClickMode(true);
    }
    if (m_deps.timeController) m_deps.timeController->clearGameEndTime();
    if (m_deps.playerInfoWiring) m_deps.playerInfoWiring->updateGameInfoWithEndTime(QDateTime());
    if (m_deps.gameRecord) m_deps.gameRecord->markDirty();
    start->resumeInterruptedGame();
}
