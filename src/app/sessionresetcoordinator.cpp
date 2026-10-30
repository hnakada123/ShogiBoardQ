/// @file sessionresetcoordinator.cpp
/// @brief 棋譜と表示の作り直し

#include "sessionresetcoordinator.h"

#include "enginedisplaypresenter.h"
#include "evaluationgraphpresenter.h"
#include "kifubranchnode.h"
#include "kifubranchtree.h"
#include "kifudocument.h"
#include "kifuloader.h"
#include "livegamesession.h"
#include "usiposition.h"

KifuBranchNode* SessionResetCoordinator::prepareGameStart(const GameStartPreparation& preparation)
{
    KifuBranchTree* tree = m_deps.document->tree();
    const bool fromCurrent = preparation.mode == GameStartPreparation::Mode::CurrentPosition;
    KifuBranchNode* node = fromCurrent ? tree->nodeAt(preparation.startNodeId) : nullptr;
    // 存在しないノードから別の分岐や同じSFENを探し直さない
    if (fromCurrent && preparation.startNodeId >= 0 && !node) return nullptr;

    m_deps.liveSession->discard();
    // 終局の行からは指せないので直前の手から始める。中断の行は再開で作り直すので消す
    if (node && node->isTerminal() && node->parent()) {
        KifuBranchNode* terminal = node;
        node = node->parent();
        if (terminal->terminalType() == TerminalType::Interrupt && terminal->childCount() == 0)
            tree->removeLeaf(terminal);
    }
    if (!node) {
        // 新しい棋譜（前の棋譜の対局の記録・上書き保存先は残さない）。対局情報は利用者が対局の前に
        // 書いた項目（棋戦・場所など）を残し、対局者・日時・持ち時間は対局の開始で作り直す。
        // 評価値グラフとエンジンの表示は、文書の置き換えを受けて消える
        m_deps.document->startNew(UsiPosition::sfenOf(preparation.rootSfen), /*keepHeader=*/true);
        node = tree->root();
    } else {
        // 現在局面から続けるときは、それまでの評価値を残す
        if (m_deps.engineDisplay) m_deps.engineDisplay->clearAll();
        m_deps.evalGraph->trimToPly(node->ply());
    }

    m_deps.liveSession->startFromNode(node);
    return node;
}

void SessionResetCoordinator::resetToNewRecord(const QString& sfen)
{
    // 読み込み途中の棋譜は捨てる（ツリーはここで作り直す）
    m_deps.kifuLoader->cancelLoad();
    m_deps.liveSession->discard();
    // 評価値グラフ・エンジンの表示は、文書の置き換えを受けて消える
    m_deps.document->startNew(sfen);
}
