/// @file kifubranchtree.cpp
/// @brief 分岐ツリーデータモデルクラスの実装

#include "kifubranchtree.h"
#include "sfenutils.h"
#include <algorithm>
#include <memory>
#include <utility>
#include "kifubranchnode.h"
#include <QMap>
#include <QStringList>

KifuBranchTree::KifuBranchTree(QObject* parent)
    : QObject(parent)
{
}

// 破棄中は受け手が無効なことがあるので知らせない（ノードはルートとともに解放される）
KifuBranchTree::~KifuBranchTree() = default;

void KifuBranchTree::clear()
{
    m_linesCache.clear();  // ダングリングポインタ防止
    invalidateLineCache();
    m_nodeById.clear();
    m_root.reset();
    // 同じツリーの寿命中はIDを再利用しない。古いカーソルが新しいノードを指すのを防ぐ。
    notify({Change::Kind::Reset});
}

void KifuBranchTree::setRootSfen(const QString& sfen)
{
    beginBatchUpdate();
    clear();
    m_root = createNode();
    m_root->record().sfen = sfen;
    endBatchUpdate();
}

std::unique_ptr<KifuBranchNode> KifuBranchTree::createNode()
{
    auto node = std::make_unique<KifuBranchNode>();
    node->m_nodeId = m_nextNodeId++;
    m_nodeById.insert(node->m_nodeId, node.get());
    return node;
}

KifuBranchNode* KifuBranchTree::addRecord(KifuBranchNode* parent, const RecordEntry& entry)
{
    if (!parent || parent->isTerminal() || nodeAt(parent->nodeId()) != parent) return nullptr;
    std::unique_ptr<KifuBranchNode> created = createNode();
    created->m_record = entry;
    if (entry.isTerminal()) {
        // 終局の行は盤を変えない。局面は直前のもの、指し手は持たない
        created->m_record.sfen = parent->sfen();
        created->m_record.move = ShogiMove{};
    }
    KifuBranchNode* node = parent->appendChild(std::move(created));
    invalidateLineCache();
    notify({Change::Kind::Appended, parent->nodeId(), node->nodeId()});
    return node;
}

void KifuBranchTree::beginBatchUpdate()
{
    ++m_batchDepth;
}

void KifuBranchTree::endBatchUpdate()
{
    if (m_batchDepth <= 0) {
        // begin と対応しない end は無視する
        m_batchDepth = 0;
        return;
    }
    if (--m_batchDepth > 0) return;

    const QList<Change> changes = std::exchange(m_pendingChanges, {});
    const bool rebuilt = std::any_of(changes.cbegin(), changes.cend(),
                                     [](const Change& c) { return c.kind == Change::Kind::Reset; });
    if (rebuilt) {
        emitChange({Change::Kind::Reset});
        return;
    }
    for (const Change& change : changes) emitChange(change);
}

void KifuBranchTree::notify(const Change& change)
{
    if (m_batchDepth > 0) {
        m_pendingChanges.append(change);
        return;
    }
    emitChange(change);
}

void KifuBranchTree::emitChange(const Change& change)
{
    switch (change.kind) {
    case Change::Kind::Appended:
        emit nodeAppended(change.parentId, change.nodeId);
        break;
    case Change::Kind::Reordered:
        emit childrenReordered(change.parentId);
        break;
    case Change::Kind::Removed:
        emit subtreeRemoved(change.parentId, change.nodeId);
        break;
    case Change::Kind::Reset:
        emit reset();
        break;
    }
}

KifuBranchNode* KifuBranchTree::nodeAt(int nodeId) const
{
    return m_nodeById.value(nodeId, nullptr);
}

KifuBranchNode* KifuBranchTree::nodeOnLine(int lineEndNodeId, int ply) const
{
    KifuBranchNode* node = nodeAt(lineEndNodeId);
    while (node && node->ply() > ply && node->parent()) node = node->parent();
    return node;
}

RecordLine KifuBranchTree::toRecordLine(const QList<KifuBranchNode*>& nodes, int currentPly)
{
    RecordLine line;
    for (const KifuBranchNode* node : std::as_const(nodes)) {
        if (node == nullptr || node->isTerminal()) continue;
        if (node->ply() == 0) line.startSfen = node->sfen();
        else line.entries << node->record();
    }
    line.currentPly = qBound(0, currentPly, line.moveCount());
    return line;
}

QList<KifuBranchNode*> KifuBranchTree::mainLine() const
{
    QList<KifuBranchNode*> result;
    if (m_root == nullptr) {
        return result;
    }

    KifuBranchNode* node = m_root.get();
    while (node != nullptr) {
        result.append(node);
        if (node->childCount() == 0) {
            break;
        }
        // 最初の子が本譜
        node = node->childAt(0);
    }
    return result;
}

QList<KifuBranchNode*> KifuBranchTree::pathToNode(KifuBranchNode* node) const
{
    QList<KifuBranchNode*> path;
    while (node != nullptr) {
        path.append(node);
        node = node->parent();
    }
    std::reverse(path.begin(), path.end());
    return path;
}

QList<BranchLine> KifuBranchTree::allLines() const
{
    if (!m_linesCacheDirty) {
        return m_linesCache;
    }

    m_linesCache.clear();
    if (m_root == nullptr) {
        m_linesCacheDirty = false;
        return m_linesCache;
    }

    QList<KifuBranchNode*> currentPath;
    int lineIndex = 0;
    collectLinesRecursive(m_root.get(), currentPath, m_linesCache, lineIndex);

    m_linesCacheDirty = false;
    return m_linesCache;
}

void KifuBranchTree::invalidateLineCache()
{
    m_linesCacheDirty = true;
    ++m_revision;
}

void KifuBranchTree::collectLinesRecursive(KifuBranchNode* node,
                                           QList<KifuBranchNode*>& currentPath,
                                           QList<BranchLine>& lines,
                                           int& lineIndex) const
{
    currentPath.append(node);

    if (node->childCount() == 0) {
        // 葉ノード - このパスを1つのラインとして記録
        BranchLine line;
        line.lineIndex = lineIndex;
        line.nodes = currentPath;

        if (lineIndex == 0) {
            line.branchPly = 0;
            line.branchPoint = nullptr;
        } else {
            // 分岐点を探す（最後に見つかった分岐点を使用）
            // 例: Line 2 (７七角分岐) の場合、3手目で本譜から分岐し、
            // さらに5手目でLine 1から分岐する。この場合、branchPointは
            // 5手目の親（4手目の「△８四歩」）であるべき。
            for (int i = 0; i < currentPath.size(); ++i) {
                KifuBranchNode* n = currentPath.at(i);
                if (n->parent() != nullptr && n->parent()->childCount() > 1) {
                    // このノードの親が分岐点
                    if (!n->isMainLine()) {
                        line.branchPly = n->ply();
                        line.branchPoint = n->parent();
                        // breakしない: 最後に見つかった分岐点を使う
                    }
                }
            }
        }

        lines.append(line);
        lineIndex++;
    } else {
        // 子ノードを再帰的に処理
        // 最初の子（本譜）を先に処理
        for (int i = 0; i < node->childCount(); ++i) {
            KifuBranchNode* child = node->childAt(i);
            collectLinesRecursive(child, currentPath, lines, lineIndex);
        }
    }

    currentPath.removeLast();
}

int KifuBranchTree::lineCount() const
{
    return static_cast<int>(allLines().size());
}

bool KifuBranchTree::hasBranch(KifuBranchNode* node) const
{
    if (node == nullptr) {
        return false;
    }
    return node->childCount() > 1;
}

std::optional<int> KifuBranchTree::findLineIndexForNode(KifuBranchNode* node) const
{
    if (node == nullptr) {
        return std::nullopt;
    }

    QList<BranchLine> lines = allLines();

    // まず、このノードが終端であるラインを探す（子がないノード）
    if (node->childCount() == 0) {
        for (const BranchLine& line : std::as_const(lines)) {
            if (!line.nodes.isEmpty() && line.nodes.last() == node) {
                return line.lineIndex;
            }
        }
    }

    // 次に、このノードの後に分岐があるかチェック
    // ノードから終端までのパスを辿り、その終端が属するラインを返す
    KifuBranchNode* current = node;
    while (current->childCount() > 0) {
        // 最初の子を辿る（このノードが属する「主要な」パス）
        current = current->childAt(0);
    }

    // 終端ノードが属するラインを探す
    for (const BranchLine& line : std::as_const(lines)) {
        if (!line.nodes.isEmpty() && line.nodes.last() == current) {
            // このラインに対象ノードが含まれているか確認
            if (line.nodes.contains(node)) {
                return line.lineIndex;
            }
        }
    }

    // フォールバック: ノードを含む最初のラインを返す
    for (const BranchLine& line : std::as_const(lines)) {
        if (line.nodes.contains(node)) {
            return line.lineIndex;
        }
    }

    return std::nullopt;
}

void KifuBranchTree::setComment(int nodeId, const QString& comment)
{
    KifuBranchNode* node = nodeAt(nodeId);
    if (node != nullptr) {
        node->record().comment = comment;
    }
}

// === 手順の取り出し ===

RecordLine KifuBranchTree::exportLine(int lineIndex) const
{
    RecordLine result;
    const QList<BranchLine> lines = allLines();
    if (lineIndex < 0 || lineIndex >= lines.size()) return result;

    for (const KifuBranchNode* node : std::as_const(lines.at(lineIndex).nodes)) {
        if (node->ply() == 0) {
            result.startSfen = node->sfen();
            result.openingComment = node->comment();
            result.openingBookmark = node->bookmark();
        } else {
            result.entries << node->record();
        }
    }
    result.currentPly = result.moveCount();
    return result;
}

QStringList KifuBranchTree::sfenListForLine(int lineIndex) const
{
    QStringList result;

    QList<BranchLine> lines = allLines();
    if (lineIndex < 0 || lineIndex >= lines.size()) {
        return result;
    }

    const BranchLine& line = lines.at(lineIndex);
    for (KifuBranchNode* node : std::as_const(line.nodes)) {
        result.append(node->sfen());
    }

    return result;
}

bool KifuBranchTree::removeLeaf(KifuBranchNode* node)
{
    return node != nullptr && node->childCount() == 0 && removeSubtree(node);
}

// ============================================================
// 分岐の編集
// ============================================================

bool KifuBranchTree::moveChildSilently(KifuBranchNode* child, int index)
{
    if (child == nullptr || child->parent() == nullptr || nodeAt(child->nodeId()) != child) return false;
    KifuBranchNode* parent = child->parent();
    if (parent->indexOfChild(child) == index || !parent->moveChild(child, index)) return false;
    invalidateLineCache();
    return true;
}

bool KifuBranchTree::moveChild(KifuBranchNode* child, int index)
{
    if (!moveChildSilently(child, index)) return false;
    notify({Change::Kind::Reordered, child->parent()->nodeId()});
    return true;
}

bool KifuBranchTree::promoteToMainLine(KifuBranchNode* node)
{
    if (node == nullptr || nodeAt(node->nodeId()) != node) return false;
    // 経路上で並びを変えた親のうち、最もルートに近いもの（その先の並びの変更も含む）
    KifuBranchNode* topmost = nullptr;
    const QList<KifuBranchNode*> path = pathToNode(node);
    for (KifuBranchNode* n : path) {
        if (n->parent() != nullptr && !n->isMainLine() && moveChildSilently(n, 0) && topmost == nullptr)
            topmost = n->parent();
    }
    if (topmost == nullptr) return false;
    notify({Change::Kind::Reordered, topmost->nodeId()});
    return true;
}

void KifuBranchTree::forgetSubtree(const KifuBranchNode* node)
{
    QList<const KifuBranchNode*> pending{node};
    while (!pending.isEmpty()) {
        const KifuBranchNode* current = pending.takeLast();
        m_nodeById.remove(current->nodeId());
        for (int i = 0; i < current->childCount(); ++i) pending.append(current->childAt(i));
    }
}

bool KifuBranchTree::removeSubtree(KifuBranchNode* node)
{
    if (node == nullptr || node == m_root.get() || node->parent() == nullptr || nodeAt(node->nodeId()) != node) {
        return false;
    }
    KifuBranchNode* parent = node->parent();
    const int nodeId = node->nodeId();
    forgetSubtree(node);
    m_linesCache.clear();  // ダングリングポインタ防止
    parent->takeChild(node);
    invalidateLineCache();
    notify({Change::Kind::Removed, parent->nodeId(), nodeId});
    return true;
}

int KifuBranchTree::subtreeSize(const KifuBranchNode* node)
{
    if (node == nullptr) return 0;
    int count = 0;
    QList<const KifuBranchNode*> pending{node};
    while (!pending.isEmpty()) {
        const KifuBranchNode* current = pending.takeLast();
        ++count;
        for (int i = 0; i < current->childCount(); ++i) pending.append(current->childAt(i));
    }
    return count;
}

KifuBranchNode* KifuBranchTree::variationHead(KifuBranchNode* node)
{
    for (KifuBranchNode* n = node; n != nullptr && n->parent() != nullptr; n = n->parent()) {
        if (n->parent()->childCount() > 1) return n;
    }
    return nullptr;
}

// ============================================================
// 分岐ツリーデータモデル - 局面SFEN・指し手によるノード検索
// ============================================================

namespace {

/// 手数を除いた局面（盤面・手番・持ち駒。持ち駒の並びは問わない）を比較用キーにする。
/// 盤面・手番・持ち駒の3つの欄が揃っていなければ空文字を返す。
QString positionKey(const QString& sfen)
{
    if (sfen.split(QLatin1Char(' '), Qt::SkipEmptyParts).size() < 3) return QString();
    return SfenUtils::normalizeSfenKey(sfen);
}

/// デフォルト構築された（駒種未設定の）ShogiMove でないか
bool isValidMove(const ShogiMove& move)
{
    return move.isValid();
}

} // namespace

KifuBranchNode* KifuBranchTree::findMatchingChild(KifuBranchNode* parent, const RecordEntry& entry) const
{
    if (parent == nullptr) {
        return nullptr;
    }

    const TerminalType termType = entry.terminal;
    const auto& move = entry.move;
    // 指し手を持たない行（KIF 本譜由来のノードなど）だけ局面で比べる。キーは1回だけ作る
    QString key;
    bool keyMade = false;

    for (int i = 0; i < parent->childCount(); ++i) {
        KifuBranchNode* child = parent->childAt(i);
        if (termType != TerminalType::None) {
            // 終局を宣言する側は必ずしも手番側ではない（反則勝ちなど）。
            if (child->terminalType() == termType && child->terminalSide() == entry.mover) {
                return child;
            }
            continue;
        }

        if (child->isTerminal()) {
            continue;
        }

        // 同じ親から同じ手を指せば同じ局面、違う手なら違う局面なので、両方に指し手があれば手で比べる
        if (isValidMove(move) && isValidMove(child->move())) {
            if (child->move() == move) return child;
            continue;
        }

        // 指し手が欠けていれば、手数を除いた局面で比べる
        if (!keyMade) {
            key = positionKey(entry.sfen);
            keyMade = true;
        }
        if (!key.isEmpty() && key == positionKey(child->sfen())) return child;
    }

    return nullptr;
}
