#include "kifupresentation.h"
/// @file kifubranchtree.cpp
/// @brief 分岐ツリーデータモデルクラスの実装

#include "kifubranchtree.h"
#include <algorithm>
#include <memory>
#include "kifubranchnode.h"
#include <QMap>
#include <QStringList>

KifuBranchTree::KifuBranchTree(QObject* parent)
    : QObject(parent)
{
}

KifuBranchTree::~KifuBranchTree()
{
    // デストラクタでは clear() を呼ばない。
    // clear() は treeChanged() シグナルを発火するが、
    // 破棄中は受信側が無効な状態である可能性があるため、
    // シグナルなしで直接クリーンアップする。
    m_linesCache.clear();  // ダングリングポインタ防止
    qDeleteAll(m_nodeById);
    m_nodeById.clear();
    m_root = nullptr;
}

void KifuBranchTree::clear()
{
    // 全ノードを削除
    // 注意: シグナルはここでは発行しない。
    // 理由: clear() 後に setRootSfen() 等が呼ばれることが多く、
    // 空の状態でシグナルを発行すると受信側で無効なポインタ参照が発生する可能性がある。
    // 呼び出し側が必要に応じてシグナルを発行する。
    m_linesCache.clear();  // メモリ解放 + ダングリングポインタ防止
    invalidateLineCache();
    qDeleteAll(m_nodeById);
    m_nodeById.clear();
    m_root = nullptr;
    // 同じツリーの寿命中はIDを再利用しない。古いカーソルが新しいノードを指すのを防ぐ。
}

void KifuBranchTree::setRootSfen(const QString& sfen)
{
    clear();

    m_root = createNode();
    m_root->setPly(0);
    m_root->setDisplayText(tr("開始局面"));
    m_root->setSfen(sfen);

    notifyTreeChanged();
}

bool KifuBranchTree::removeLeafQuiet(KifuBranchNode* node)
{
    if (node == nullptr || node == m_root || node->parent() == nullptr
        || node->childCount() != 0 || nodeAt(node->nodeId()) != node) {
        return false;
    }

    node->parent()->removeChild(node);
    std::unique_ptr<KifuBranchNode> removed(m_nodeById.take(node->nodeId()));
    m_linesCache.clear();
    invalidateLineCache();
    return true;
}

bool KifuBranchTree::moveChildQuiet(KifuBranchNode* child, int index)
{
    if (child == nullptr || child->parent() == nullptr || nodeAt(child->nodeId()) != child) {
        return false;
    }
    KifuBranchNode* parent = child->parent();
    if (parent->children().indexOf(child) == index) {
        return true;
    }
    if (!parent->moveChild(child, index)) {
        return false;
    }
    invalidateLineCache();
    return true;
}

KifuBranchNode* KifuBranchTree::createNode()
{
    auto* node = new KifuBranchNode();
    node->setNodeId(m_nextNodeId);
    m_nodeById.insert(m_nextNodeId, node);
    m_nextNodeId++;
    return node;
}

KifuBranchNode* KifuBranchTree::addMove(KifuBranchNode* parent, const ShogiMove& move,
                                       const QString& displayText, const QString& sfen,
                                       const QString& timeText)
{
    return addRecord(parent, RecordEntry::played(move, displayText, sfen, RecordTime::fromText(timeText)));
}

KifuBranchNode* KifuBranchTree::addRecord(KifuBranchNode* parent, const RecordEntry& entry)
{
    auto* node = insertRecord(parent, entry);
    if (node) notifyTreeChanged();
    return node;
}

KifuBranchNode* KifuBranchTree::insertRecord(KifuBranchNode* parent, const RecordEntry& entry)
{
    if (!parent || parent->isTerminal()) return nullptr;
    auto* node = createNode();
    node->setPly(parent->ply() + 1);
    RecordEntry record = entry;
    if (record.isTerminal()) {
        // 終局の行は盤を変えない。局面は直前のもの、指し手は持たない
        record.sfen = parent->sfen();
        record.move = ShogiMove{};
    }
    node->setRecord(record);
    parent->addChild(node);
    invalidateLineCache();
    return node;
}

KifuBranchNode* KifuBranchTree::addTerminalMove(KifuBranchNode* parent,
                                                TerminalType type,
                                                Turn side,
                                                const QString& timeText)
{
    return addRecord(parent, RecordEntry::ending(type, side, RecordTime::fromText(timeText)));
}

KifuBranchNode* KifuBranchTree::addMoveQuiet(KifuBranchNode* parent,
                                              const ShogiMove& move,
                                              const QString& displayText,
                                              const QString& sfen,
                                              const QString& timeText)
{
    return insertRecord(parent, RecordEntry::played(move, displayText, sfen, RecordTime::fromText(timeText)));
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

    --m_batchDepth;
    if (m_batchDepth == 0 && m_treeChangedPending) {
        m_treeChangedPending = false;
        emit treeChanged();
    }
}

void KifuBranchTree::notifyTreeChanged()
{
    if (m_batchDepth > 0) {
        m_treeChangedPending = true;
        return;
    }
    emit treeChanged();
}

KifuBranchNode* KifuBranchTree::nodeAt(int nodeId) const
{
    return m_nodeById.value(nodeId, nullptr);
}

KifuBranchNode* KifuBranchTree::findByPlyOnMainLine(int ply) const
{
    if (m_root == nullptr) {
        return nullptr;
    }

    // 本譜を辿る
    KifuBranchNode* node = m_root;
    while (node != nullptr) {
        if (node->ply() == ply) {
            return node;
        }
        if (node->childCount() == 0) {
            break;
        }
        // 最初の子が本譜
        node = node->childAt(0);
    }
    return nullptr;
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

    KifuBranchNode* node = m_root;
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
    collectLinesRecursive(m_root, currentPath, m_linesCache, lineIndex);

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
            line.name = QStringLiteral("本譜");
            line.branchPly = 0;
            line.branchPoint = nullptr;
        } else {
            line.name = QStringLiteral("分岐%1").arg(lineIndex);
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

QSet<int> KifuBranchTree::branchablePlysOnLine(const BranchLine& line) const
{
    QSet<int> result;
    for (KifuBranchNode* node : std::as_const(line.nodes)) {
        if (node->childCount() > 1) {
            // このノードの子の手数を分岐点として記録
            result.insert(node->ply() + 1);
        }
    }
    return result;
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
        node->setComment(comment);
    }
}

// === データ抽出（ResolvedRow互換）===

QList<KifDisplayItem> KifuBranchTree::displayItemsForLine(int lineIndex) const
{
    QList<KifDisplayItem> result;

    QList<BranchLine> lines = allLines();
    if (lineIndex < 0 || lineIndex >= lines.size()) {
        return result;
    }

    const BranchLine& line = lines.at(lineIndex);
    for (KifuBranchNode* node : std::as_const(line.nodes)) {
        KifDisplayItem item;
        item.prettyMove = node->displayText();
        item.beforeSfen = node->parent() ? node->parent()->sfen() : QString();
        item.usiMove = node->isTerminal() ? QString() : KifuPresentation::usiMove(node->move());
        item.terminal = node->isTerminal();
        item.terminalType = node->terminalType();
        item.terminalSide = node->terminalSide();
        item.comment = node->comment();
        item.bookmark = node->bookmark();
        item.timeText = node->timeText();
        item.ply = node->ply();
        result.append(item);
    }

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
    if (!removeLeafQuiet(node)) return false;
    notifyTreeChanged();
    return true;
}

// ============================================================
// 分岐ツリーの編集（本譜にする・並べ替え・削除）の実装
// ============================================================

// ============================================================
// 分岐の編集
// ============================================================

bool KifuBranchTree::moveChild(KifuBranchNode* child, int index)
{
    if (child == nullptr || child->parent() == nullptr) return false;
    if (child->parent()->children().indexOf(child) == index) return false;
    if (!moveChildQuiet(child, index)) return false;
    notifyTreeChanged();
    return true;
}

bool KifuBranchTree::promoteToMainLine(KifuBranchNode* node)
{
    if (node == nullptr || nodeAt(node->nodeId()) != node) return false;
    bool changed = false;
    const QList<KifuBranchNode*> path = pathToNode(node);
    for (KifuBranchNode* n : path) {
        if (n->parent() != nullptr && !n->isMainLine()) {
            changed = moveChildQuiet(n, 0) || changed;
        }
    }
    if (changed) notifyTreeChanged();
    return changed;
}

bool KifuBranchTree::removeSubtree(KifuBranchNode* node)
{
    if (node == nullptr || node == m_root || node->parent() == nullptr || nodeAt(node->nodeId()) != node) {
        return false;
    }
    node->parent()->removeChild(node);

    // 子のリストは非所有なので、子を取り出してから親を解放する
    QList<KifuBranchNode*> pending{node};
    while (!pending.isEmpty()) {
        KifuBranchNode* current = pending.takeLast();
        pending.append(current->children());
        std::unique_ptr<KifuBranchNode> removed(m_nodeById.take(current->nodeId()));
    }
    m_linesCache.clear();
    invalidateLineCache();
    notifyTreeChanged();
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
        for (const KifuBranchNode* child : current->children()) pending.append(child);
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

/// 持ち駒フィールドを駒種ごとの枚数に分解し、並び順に依存しない形へ正規化する。
/// 以前の版の盤は駒種ごとに先後を交互に書いていたので、文字列のままでは
/// 同じ持ち駒でも一致しないことがある（その版で保存した局面も比べられるようにする）。
QString normalizeHands(const QString& hands)
{
    QMap<QChar, int> counts;
    int pending = 0;
    for (const QChar ch : hands) {
        if (ch.isDigit()) {
            pending = pending * 10 + ch.digitValue();
        } else if (ch != QLatin1Char('-')) {
            counts[ch] += (pending > 0) ? pending : 1;
            pending = 0;
        }
    }

    QString out;
    for (auto it = counts.cbegin(); it != counts.cend(); ++it) {
        if (it.value() > 1) {
            out += QString::number(it.value());
        }
        out += it.key();
    }
    return out.isEmpty() ? QStringLiteral("-") : out;
}

/// 手数を除いた局面（盤面・手番・正規化した持ち駒）を比較用キーにする。
/// 盤面・手番・持ち駒が揃っていない不完全な SFEN は空文字を返す。
QString positionKey(const QString& sfen)
{
    const QStringList parts = sfen.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (parts.size() < 3) {
        return QString();
    }
    return parts.at(0) + QLatin1Char(' ') + parts.at(1) + QLatin1Char(' ') + normalizeHands(parts.at(2));
}

/// 手数を除いた局面（盤面・手番・持ち駒）が一致するか。どちらかが不完全なら false
bool isSamePosition(const QString& sfenA, const QString& sfenB)
{
    const QString keyA = positionKey(sfenA);
    return !keyA.isEmpty() && keyA == positionKey(sfenB);
}

/// デフォルト構築された（駒種未設定の）ShogiMove でないか
bool isValidMove(const ShogiMove& move)
{
    return move.isValid();
}

} // namespace

KifuBranchNode* KifuBranchTree::findBySfen(const QString& sfen, int ply) const
{
    if (m_root == nullptr) {
        return nullptr;
    }

    const QString targetKey = positionKey(sfen);
    if (targetKey.isEmpty()) {
        return nullptr;
    }

    // QHash の走査順に依存しないよう、ルートから深さ優先（先頭の子＝本譜を優先）で探す。
    // 同じ局面が複数のラインにある場合は本譜、次に分岐順で先のラインが選ばれる。
    QList<KifuBranchNode*> stack;
    stack.append(m_root);
    while (!stack.isEmpty()) {
        KifuBranchNode* node = stack.takeLast();

        // 終局手ノードは親と同じ局面を持つため対象外（局面を表すのは親ノード）
        if (!node->isTerminal()
            && (ply < 0 || node->ply() == ply)
            && positionKey(node->sfen()) == targetKey) {
            return node;
        }

        const QList<KifuBranchNode*>& children = node->children();
        for (qsizetype i = children.size() - 1; i >= 0; --i) {
            stack.append(children.at(i));
        }
    }

    return nullptr;
}

KifuBranchNode* KifuBranchTree::findMatchingChild(KifuBranchNode* parent,
                                                  const ShogiMove& move,
                                                  const QString& displayText,
                                                  const QString& sfen) const
{
    return findMatchingChild(parent, RecordEntry::played(move, displayText, sfen));
}

KifuBranchNode* KifuBranchTree::findMatchingChild(KifuBranchNode* parent, const RecordEntry& entry) const
{
    if (parent == nullptr) {
        return nullptr;
    }

    const TerminalType termType = entry.terminal;
    const auto& sfen = entry.sfen;
    const auto& move = entry.move;

    for (KifuBranchNode* child : parent->children()) {
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

        // 同じ親から異なる手を指して同じ局面になることはないので、局面一致＝同じ手
        if (isSamePosition(child->sfen(), sfen)) {
            return child;
        }

        // KIF 本譜由来のノードなど SFEN が欠けている場合に備えて ShogiMove でも比較する
        if (isValidMove(move) && isValidMove(child->move()) && child->move() == move) {
            return child;
        }
    }

    return nullptr;
}
