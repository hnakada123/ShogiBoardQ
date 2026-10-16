/// @file sessionresetcoordinator.cpp
/// @brief 棋譜と表示の作り直し

#include "sessionresetcoordinator.h"

#include "considerationtabmanager.h"
#include "csalogpanel.h"
#include "engineinfowidget.h"
#include "evaluationgraphpresenter.h"
#include "kifuanalysislistmodel.h"
#include "kifubranchnode.h"
#include "kifubranchtree.h"
#include "kifudocument.h"
#include "kifuloadcoordinator.h"
#include "livegamesession.h"
#include "sfenutils.h"
#include "shogienginethinkingmodel.h"
#include "thinkingpanel.h"
#include "usicommlogmodel.h"
#include "usilogpanel.h"

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
        // 書いた項目（棋戦・場所など）を残し、対局者・日時・持ち時間は対局の開始で作り直す
        m_deps.document->startNew(SfenUtils::normalizeStart(preparation.rootSfen), /*keepHeader=*/true);
        node = tree->root();
    }

    clearEngineDisplays();
    // 現在局面から続けるときは、それまでの評価値を残す
    if (fromCurrent) m_deps.evalGraph->trimToPly(node->ply());
    else m_deps.evalGraph->clear();

    m_deps.liveSession->startFromNode(node);
    return node;
}

void SessionResetCoordinator::resetToNewRecord(const QString& sfen, bool keepHeader)
{
    // 読み込み途中の棋譜は捨てる（ツリーはここで作り直す）
    m_deps.kifuLoader->cancelLoad();
    m_deps.liveSession->discard();
    m_deps.document->startNew(sfen, keepHeader);
    m_deps.evalGraph->setEngine1Name({});
    m_deps.evalGraph->setEngine2Name({});
}

void SessionResetCoordinator::clearBeforeKifuLoad()
{
    clearEngineDisplays();
    m_deps.evalGraph->clear();
}

void SessionResetCoordinator::clearEngineDisplays()
{
    for (UsiCommLogModel* log : {m_deps.commLog1, m_deps.commLog2}) {
        if (log) log->clear();
    }
    for (ShogiEngineThinkingModel* thinking : {m_deps.thinking1, m_deps.thinking2, m_deps.consideration}) {
        if (thinking) thinking->clearAllItems();
    }
    if (m_deps.analysisModel) m_deps.analysisModel->clearAllItems();

    if (UsiLogPanel* log = m_deps.usiLog) log->clear();
    if (CsaLogPanel* log = m_deps.csaLog) log->clear();
    if (ThinkingPanel* panel = m_deps.thinkingPanel) {
        panel->info1()->setDisplayNameFallback(QString());
        panel->info2()->setDisplayNameFallback(QString());
    }
    if (ConsiderationTabManager* consideration = m_deps.considerationTab) {
        if (consideration->considerationInfo()) consideration->considerationInfo()->setModel(nullptr);
        consideration->setConsiderationEngineName(QString());
        consideration->resetElapsedTimer();
        consideration->setConsiderationRunning(false);
    }
}
