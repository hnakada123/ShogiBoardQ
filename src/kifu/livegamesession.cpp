/// @file livegamesession.cpp
/// @brief ライブ対局セッション管理クラスの実装

#include "livegamesession.h"
#include "kifupresentation.h"
#include "kifubranchtree.h"

#include "logcategories.h"

#include <limits>
#include <utility>

LiveGameSession::LiveGameSession(QObject* parent)
    : QObject(parent)
{
}

void LiveGameSession::setTree(KifuBranchTree* tree)
{
    m_tree = tree;
}

void LiveGameSession::startFromNode(KifuBranchNode* branchPoint)
{
    qCDebug(lcKifu).noquote() << "startFromNode ENTER branchPoint="
                       << (branchPoint ? "yes" : "null")
                       << "branchPointPly=" << (branchPoint ? branchPoint->ply() : -1)
                       << "branchPointSfen=" << (branchPoint ? branchPoint->sfen().left(60) : "(null)");

    if (m_active) {
        qCWarning(lcKifu) << "Session already active";
        return;
    }

    if (branchPoint != nullptr && !canStartFrom(branchPoint)) {
        qCWarning(lcKifu) << "Cannot start from terminal node";
        return;
    }

    reset();
    m_active = true;
    m_branchPoint = branchPoint;

    // 起点のSFENを記録
    if (m_branchPoint != nullptr) {
        m_anchorSfen = m_branchPoint->sfen();
        qCDebug(lcKifu).noquote() << "startFromNode: recorded anchor sfen from branchPoint";
    } else if (m_tree != nullptr && m_tree->root() != nullptr) {
        m_anchorSfen = m_tree->root()->sfen();
        qCDebug(lcKifu).noquote() << "startFromNode: recorded anchor sfen from root";
    }

    qCDebug(lcKifu).noquote() << "startFromNode: anchorSfen=" << anchorSfen().left(60);

    emit sessionStarted(m_branchPoint);

    qCDebug(lcKifu).noquote() << "startFromNode LEAVE";
}

void LiveGameSession::startFromRoot()
{
    startFromNode(nullptr);
}

int LiveGameSession::anchorPly() const
{
    if (m_branchPoint == nullptr) {
        return 0;
    }
    return m_branchPoint->ply();
}

QString LiveGameSession::anchorSfen() const
{
    return m_anchorSfen;
}

bool LiveGameSession::canStartFrom(KifuBranchNode* node)
{
    if (node == nullptr) {
        return true;  // ルートからは開始可能
    }
    return !node->isTerminal();
}

void LiveGameSession::addMove(const ShogiMove& move, const QString& displayText,
                               const QString& sfen, const QString& elapsed)
{
    appendRecord(RecordEntry::imported(move, displayText, sfen, elapsed));
}

void LiveGameSession::appendRecord(const RecordEntry& entry)
{
    const auto& displayText = entry.displayText;
    const auto& sfen = entry.sfen;
    qCDebug(lcKifu).noquote() << "addMove ENTER displayText=" << displayText
                       << "sfen=" << sfen.left(60);

    if (!canAddMove()) {
        qCWarning(lcKifu) << "Cannot add move - session not active or already terminated";
        return;
    }

    const int ply = anchorPly() + static_cast<int>(m_entries.size()) + 1;
    qCDebug(lcKifu).noquote() << "addMove: ply=" << ply
                       << "anchorPly=" << anchorPly()
                       << "movesSize=" << m_entries.size();

    // ツリーとセッションの更新が揃ってから変更通知を発行する。
    if (m_tree) m_tree->beginBatchUpdate();
    if (m_tree != nullptr) {
        KifuBranchNode* parent = m_liveParent;
        if (parent == nullptr) {
            parent = (m_branchPoint != nullptr) ? m_branchPoint : m_tree->root();
        }

        qCDebug(lcKifu).noquote() << "addMove: parent node ply="
                           << (parent ? parent->ply() : -1)
                           << "branchPoint ply="
                           << (m_branchPoint ? m_branchPoint->ply() : -1);

        if (parent != nullptr) {
            // 同じ手が既に子として存在する場合はそのノードを再利用する。
            // 対局後に途中局面へ戻って同じ手を指し直したときに、
            // 同一の手を表す兄弟ノード（重複分岐）ができるのを防ぐ。
            // 既存ノードのコメント・消費時間はそのまま維持する。
            KifuBranchNode* existing = m_tree->findMatchingChild(parent, entry);
            if (existing != nullptr) {
                m_liveParent = existing;
                qCDebug(lcKifu).noquote() << "addMove: reused existing node ply=" << existing->ply()
                                   << "displayText=" << existing->displayText();
            } else {
                m_liveParent = m_tree->addRecord(parent, entry);
                if (m_liveParent != nullptr) {
                    m_createdNodeIds.insert(m_liveParent->nodeId());
                    // 棋譜の途中から続けた対局の情報は、新しく作った最初の手に残す
                    if (!m_firstMoveNote.isEmpty()) {
                        m_liveParent->setComment(m_firstMoveNote);
                        m_firstMoveNote.clear();
                    }
                }
                qCDebug(lcKifu).noquote() << "addMove: added to tree, m_liveParent ply="
                                   << (m_liveParent ? m_liveParent->ply() : -1)
                                   << "sfen stored in node="
                                   << (m_liveParent ? m_liveParent->sfen().left(60) : "(null)");
            }
            placeBeforeUndoneSiblings(m_liveParent);
        }
    }

    if (m_tree && !m_liveParent) {
        m_tree->endBatchUpdate();
        return;
    }

    RecordEntry recorded = entry;
    if (m_liveParent) recorded.sfen = m_liveParent->sfen();
    m_entries.append(recorded);

    // 終局手を追加したらセッションは終了状態（これ以上の追加は canAddMove() で拒否）
    if (entry.isTerminal()) {
        m_hasTerminal = true;
    }

    if (m_tree) m_tree->endBatchUpdate();
    emit moveAdded(ply, displayText);

    // 分岐マーク（「+」）は表示中ラインの経路から KifuDisplayCoordinator が計算する。
    // ここで全ラインを走査して計算すると、別ラインにある同じ手数の分岐を拾ってしまう。
    // 棋譜欄は、行の追加と現在行の強調の変更をモデルが知らせるので、ここで描き直しを頼まない。

    qCDebug(lcKifu).noquote() << "addMove LEAVE";
}

RecordTime LiveGameSession::continuedTime(const RecordTime& time) const
{
    if (!m_active || !m_tree || !time.valid) return time;
    const KifuBranchNode* parent = m_liveParent ? m_liveParent : (m_branchPoint ? m_branchPoint : m_tree->root());
    const KifuBranchNode* sameSide = parent ? parent->parent() : nullptr;
    const qint64 previous = sameSide && sameSide->time().valid ? sameSide->time().totalMs : 0;
    return RecordTime::fromMilliseconds(time.elapsedMs, previous > std::numeric_limits<qint64>::max() - time.elapsedMs
        ? std::numeric_limits<qint64>::max() : previous + time.elapsedMs);
}

QString LiveGameSession::continuedElapsedText(const QString& elapsed) const
{
    return continuedTime(RecordTime::fromText(elapsed)).toText();
}

bool LiveGameSession::canUndoMoves(int count) const
{
    if (!m_active || m_hasTerminal || count <= 0 || count > m_entries.size()) {
        return false;
    }
    if (m_tree == nullptr) {
        return true;
    }

    // 対局開始位置（分岐起点・ルート）より前には戻らない。
    KifuBranchNode* node = m_liveParent;
    for (int i = 0; i < count; ++i) {
        if (node == nullptr || node == m_branchPoint || node == m_tree->root()) {
            return false;
        }
        node = node->parent();
    }
    return true;
}

bool LiveGameSession::undoMoves(int count)
{
    if (!canUndoMoves(count)) {
        return false;
    }

    // 取り消す手順の先頭ノード（巻き戻し先の子）。ノードは削除せず分岐として残す。
    KifuBranchNode* undoneHead = m_liveParent;
    for (int i = 1; undoneHead != nullptr && i < count; ++i) {
        undoneHead = undoneHead->parent();
    }
    KifuBranchNode* target = (undoneHead != nullptr) ? undoneHead->parent() : nullptr;
    emit movesAboutToBeUndone(target);

    if (undoneHead != nullptr) {
        // 以前の棋譜にあった手を再利用して取り消した場合は、その棋譜の並び順を保つ。
        if (m_createdNodeIds.contains(undoneHead->nodeId())) {
            m_undoneNodeIds.insert(undoneHead->nodeId());
        }
        m_liveParent = target;
    }
    m_entries.resize(m_entries.size() - count);
    emit movesUndone();
    return true;
}

void LiveGameSession::placeBeforeUndoneSiblings(KifuBranchNode* node)
{
    if (node == nullptr || node->parent() == nullptr) {
        return;
    }
    m_undoneNodeIds.remove(node->nodeId());
    if (m_undoneNodeIds.isEmpty()) {
        return;
    }

    // 待ったで取り消した手順より前に並べ、実際に指した手順を本譜側にする。
    // 対局前から存在する兄弟（以前の対局の本譜など）の順序は変えない。
    const QList<KifuBranchNode*>& siblings = node->parent()->children();
    const qsizetype current = siblings.indexOf(node);
    for (qsizetype i = 0; i < current; ++i) {
        if (m_undoneNodeIds.contains(siblings.at(i)->nodeId())) {
            m_tree->moveChildQuiet(node, static_cast<int>(i));
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

    if (m_tree == nullptr) {
        qCWarning(lcKifu) << "Cannot commit - no tree set";
        discard();
        return nullptr;
    }

    // 追加する指し手がない場合は何もしない
    if (m_entries.isEmpty()) {
        discard();
        return nullptr;
    }

    // リアルタイム追加済みの場合は既存ノードを返す
    if (m_liveParent != nullptr) {
        KifuBranchNode* result = m_liveParent;
        reset();
        emit sessionCommitted(result);
        return result;
    }

    // 分岐起点を決定
    KifuBranchNode* parent = m_branchPoint;
    if (parent == nullptr) {
        parent = m_tree->root();
    }

    if (parent == nullptr) {
        qCWarning(lcKifu) << "Cannot commit - no root node";
        discard();
        return nullptr;
    }

    KifuBranchNode* lastNode = parent;

    // 各指し手をツリーに追加（treeChanged は最後に1回だけ発火させる）
    m_tree->beginBatchUpdate();
    for (const RecordEntry& entry : std::as_const(m_entries)) {
        lastNode = m_tree->addRecord(lastNode, entry);
        if (!lastNode) break;
    }
    m_tree->endBatchUpdate();

    KifuBranchNode* result = lastNode;
    reset();

    emit sessionCommitted(result);
    return result;
}

void LiveGameSession::discard()
{
    reset();
    emit sessionDiscarded();
}

int LiveGameSession::totalPly() const
{
    return anchorPly() + static_cast<int>(m_entries.size());
}

QString LiveGameSession::currentSfen() const
{
    return m_entries.isEmpty() ? m_anchorSfen : m_entries.last().sfen;
}

QList<KifDisplayItem> LiveGameSession::moves() const
{
    QList<KifDisplayItem> result;
    result.reserve(m_entries.size());
    QString before = m_anchorSfen;
    int ply = anchorPly();
    for (const auto& entry : m_entries) {
        KifDisplayItem item;
        item.prettyMove = entry.displayText;
        item.beforeSfen = before;
        item.terminal = entry.isTerminal();
        item.usiMove = entry.isTerminal() ? QString() : KifuPresentation::usiMove(entry.move);
        item.timeText = entry.time.toText();
        item.ply = ++ply;
        result.append(item);
        before = entry.sfen;
    }
    return result;
}

int LiveGameSession::currentLineIndex() const
{
    if (m_tree == nullptr) {
        return 0;
    }

    KifuBranchNode* node = m_liveParent;
    if (node == nullptr) {
        node = (m_branchPoint != nullptr) ? m_branchPoint : m_tree->root();
    }

    if (node == nullptr) {
        return 0;
    }

    const auto lineIndex = m_tree->findLineIndexForNode(node);
    if (lineIndex.has_value()) {
        return *lineIndex;
    }

    return node->lineIndex();
}

void LiveGameSession::reset()
{
    m_active = false;
    m_hasTerminal = false;
    m_branchPoint = nullptr;
    m_liveParent = nullptr;
    m_entries.clear();
    m_anchorSfen.clear();
    m_createdNodeIds.clear();
    m_undoneNodeIds.clear();
    m_firstMoveNote.clear();
}

void LiveGameSession::setFirstMoveNote(const QString& note)
{
    m_firstMoveNote = note;
}
