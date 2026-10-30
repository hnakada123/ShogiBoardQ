/// @file livegamesession.cpp
/// @brief 対局中の手を分岐ツリーへ書く

#include "livegamesession.h"
#include "kifubranchnode.h"
#include "kifubranchtree.h"
#include "kifunavigationstate.h"
#include "logcategories.h"

#include <limits>

LiveGameSession::LiveGameSession(KifuBranchTree* tree, KifuNavigationState* navState, QObject* parent)
    : QObject(parent)
    , m_tree(tree)
    , m_navState(navState)
{
    Q_ASSERT(m_tree && m_navState);
    connect(m_tree, &KifuBranchTree::subtreeRemoved, this, &LiveGameSession::onTreeChanged);
    connect(m_tree, &KifuBranchTree::reset, this, &LiveGameSession::onTreeChanged);
}

void LiveGameSession::onTreeChanged()
{
    // 棋譜を読み込んだ・新しく作ったなどで、対局を始めた局面がツリーから消えたら記録をやめる
    // （中断して再開を待っている対局も含む）。ノード ID は使い回さないので、消えたかは ID で分かる
    if (m_active && m_tree->nodeAt(m_anchorNodeId) == nullptr) discard();
}

void LiveGameSession::startFromNode(KifuBranchNode* branchPoint)
{
    if (m_active) {
        qCWarning(lcKifu) << "Session already active";
        return;
    }
    if (!canStartFrom(branchPoint)) {
        qCWarning(lcKifu) << "Cannot start from terminal node";
        return;
    }
    reset();
    m_active = true;
    m_branchPoint = branchPoint;
    if (const KifuBranchNode* anchor = liveNode()) m_anchorNodeId = anchor->nodeId();
    // 対局は分岐点（途中局面）か開始局面から始まる。表示中の手をそこに合わせ、それより先の選択は忘れる
    m_navState->resetToNode(liveNode());
    emit activeChanged();
}

bool LiveGameSession::canStartFrom(const KifuBranchNode* node)
{
    return node == nullptr || !node->isTerminal();
}

KifuBranchNode* LiveGameSession::liveNode() const
{
    if (m_liveParent) return m_liveParent;
    return m_branchPoint ? m_branchPoint : m_tree->root();
}

int LiveGameSession::anchorPly() const
{
    return m_branchPoint ? m_branchPoint->ply() : 0;
}

void LiveGameSession::appendRecord(const RecordEntry& entry)
{
    if (!canAddMove()) {
        qCWarning(lcKifu) << "Cannot add move - session not active or already terminated";
        return;
    }
    KifuBranchNode* parent = liveNode();
    if (!parent) return;

    m_tree->beginBatchUpdate();
    // 同じ手が既に子として存在する場合はそのノードを再利用する。
    // 対局後に途中局面へ戻って同じ手を指し直したときに、同一の手の分岐ができるのを防ぐ。
    // 既存ノードのコメント・消費時間はそのまま維持する。
    KifuBranchNode* node = m_tree->findMatchingChild(parent, entry);
    if (!node) {
        // 棋譜の途中から続けた対局の情報は、新しく作った最初の手のコメントに残す
        RecordEntry added = entry;
        if (!m_firstMoveNote.isEmpty()) added.comment = m_firstMoveNote;
        node = m_tree->addRecord(parent, added);
        if (node) {
            m_createdNodeIds.insert(node->nodeId());
            m_firstMoveNote.clear();
        }
    }
    if (node) {
        placeBeforeUndoneSiblings(node);
        m_liveParent = node;
        ++m_moveCount;
        if (entry.isTerminal()) m_hasTerminal = true;
        // 足した手へ移ってからツリーの変更を知らせる（表示は足した手を見て1回だけ描き直す）
        m_navState->setCurrentNode(node);
    }
    m_tree->endBatchUpdate();
}

RecordTime LiveGameSession::continuedTime(const RecordTime& time) const
{
    if (!m_active || !time.valid) return time;
    const KifuBranchNode* parent = liveNode();
    const KifuBranchNode* sameSide = parent ? parent->parent() : nullptr;
    const qint64 previous = sameSide && sameSide->time().valid ? sameSide->time().totalMs : 0;
    return RecordTime::fromMilliseconds(time.elapsedMs, previous > std::numeric_limits<qint64>::max() - time.elapsedMs
        ? std::numeric_limits<qint64>::max() : previous + time.elapsedMs);
}

void LiveGameSession::setFirstMoveNote(const QString& note)
{
    m_firstMoveNote = note;
}

bool LiveGameSession::canUndoMoves(int count) const
{
    if (!m_active || m_hasTerminal || count <= 0 || count > m_moveCount) return false;
    // 対局開始位置（分岐起点・ルート）より前には戻らない。
    const KifuBranchNode* node = m_liveParent;
    for (int i = 0; i < count; ++i) {
        if (node == nullptr || node == m_branchPoint || node == m_tree->root()) return false;
        node = node->parent();
    }
    return true;
}

bool LiveGameSession::undoMoves(int count)
{
    if (!canUndoMoves(count)) return false;
    // 取り消す手順の先頭ノード（巻き戻し先の子）。ノードは削除せず分岐として残す。
    KifuBranchNode* undoneHead = m_liveParent;
    for (int i = 1; i < count; ++i) undoneHead = undoneHead->parent();
    // 以前の棋譜にあった手を再利用して取り消した場合は、その棋譜の並び順を保つ。
    if (m_createdNodeIds.contains(undoneHead->nodeId())) m_undoneNodeIds.insert(undoneHead->nodeId());
    m_liveParent = undoneHead->parent();
    m_moveCount -= count;
    m_navState->setCurrentNode(liveNode());
    return true;
}

void LiveGameSession::placeBeforeUndoneSiblings(KifuBranchNode* node)
{
    if (node == nullptr || node->parent() == nullptr) return;
    m_undoneNodeIds.remove(node->nodeId());
    if (m_undoneNodeIds.isEmpty()) return;
    // 待ったで取り消した手順より前に並べ、実際に指した手順を本譜側にする。
    // 対局前から存在する兄弟（以前の対局の本譜など）の順序は変えない。
    const KifuBranchNode* parent = node->parent();
    const int current = parent->indexOfChild(node);
    for (int i = 0; i < current; ++i) {
        if (m_undoneNodeIds.contains(parent->childAt(i)->nodeId())) {
            m_tree->moveChild(node, i);
            return;
        }
    }
}

KifuBranchNode* LiveGameSession::commit()
{
    if (!m_active) {
        qCWarning(lcKifu) << "Cannot commit - session not active";
        return nullptr;
    }
    KifuBranchNode* result = m_moveCount == 0 ? nullptr : m_liveParent;
    reset();
    emit activeChanged();
    return result;
}

void LiveGameSession::discard()
{
    if (!m_active) return;
    reset();
    emit activeChanged();
}

void LiveGameSession::reset()
{
    m_active = false;
    m_hasTerminal = false;
    m_branchPoint = nullptr;
    m_anchorNodeId = -1;
    m_liveParent = nullptr;
    m_moveCount = 0;
    m_createdNodeIds.clear();
    m_undoneNodeIds.clear();
    m_firstMoveNote.clear();
}
