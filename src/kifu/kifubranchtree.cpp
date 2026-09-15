/// @file kifubranchtree.cpp
/// @brief 分岐ツリーデータモデルクラスの実装

#include "kifubranchtree.h"
#include <QMap>
#include <QStringList>
#include <algorithm>

namespace {

/// 持ち駒フィールドを駒種ごとの枚数に分解し、並び順に依存しない形へ正規化する。
/// ShogiBoard は駒種ごとに先後を交互に、SfenPositionTracer は先手分→後手分の順に
/// 出力するため、文字列のままでは同じ持ち駒でも一致しないことがある。
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
    return move.movingPiece != Piece::None;
}

} // namespace

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
    m_linesCacheDirty = true;
    qDeleteAll(m_nodeById);
    m_nodeById.clear();
    m_root = nullptr;
    m_nextNodeId = 1;
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

KifuBranchNode* KifuBranchTree::createNode()
{
    auto* node = new KifuBranchNode();
    node->setNodeId(m_nextNodeId);
    m_nodeById.insert(m_nextNodeId, node);
    m_nextNodeId++;
    return node;
}

KifuBranchNode* KifuBranchTree::addMove(KifuBranchNode* parent,
                                        const ShogiMove& move,
                                        const QString& displayText,
                                        const QString& sfen,
                                        const QString& timeText)
{
    if (parent == nullptr) {
        return nullptr;
    }

    // 終局手には子を追加できない
    if (parent->isTerminal()) {
        return nullptr;
    }

    auto* node = createNode();
    node->setPly(parent->ply() + 1);
    node->setDisplayText(displayText);
    node->setTimeText(timeText);

    // 終局手かどうかを判定
    const TerminalType termType = detectTerminalType(displayText);
    node->setTerminalType(termType);

    if (termType != TerminalType::None) {
        // 終局手は盤面を変化させないため、追加経路によらず親と同じ局面を持たせる。
        // 渡された move は直前の手など無関係な値のことがあるので保持しない。
        node->setSfen(parent->sfen());
        node->setMove(ShogiMove());
    } else {
        node->setSfen(sfen);
        node->setMove(move);
    }

    parent->addChild(node);
    invalidateLineCache();

    notifyTreeChanged();

    return node;
}

KifuBranchNode* KifuBranchTree::addTerminalMove(KifuBranchNode* parent,
                                                TerminalType type,
                                                const QString& displayText,
                                                const QString& timeText)
{
    if (parent == nullptr) {
        return nullptr;
    }

    // 終局手には子を追加できない
    if (parent->isTerminal()) {
        return nullptr;
    }

    auto* node = createNode();
    node->setPly(parent->ply() + 1);
    node->setDisplayText(displayText);
    node->setSfen(parent->sfen());  // 終局手は盤面変化なし
    node->setTimeText(timeText);
    node->setTerminalType(type);

    parent->addChild(node);
    invalidateLineCache();

    notifyTreeChanged();

    return node;
}

KifuBranchNode* KifuBranchTree::addMoveQuiet(KifuBranchNode* parent,
                                              const ShogiMove& move,
                                              const QString& displayText,
                                              const QString& sfen,
                                              const QString& timeText)
{
    if (parent == nullptr) {
        return nullptr;
    }

    // 終局手には子を追加できない
    if (parent->isTerminal()) {
        return nullptr;
    }

    auto* node = createNode();
    node->setPly(parent->ply() + 1);
    node->setDisplayText(displayText);
    node->setTimeText(timeText);

    // 終局手かどうかを判定
    const TerminalType termType = detectTerminalType(displayText);
    node->setTerminalType(termType);

    if (termType != TerminalType::None) {
        // 終局手は盤面を変化させないため、追加経路によらず親と同じ局面を持たせる。
        // 渡された move は直前の手など無関係な値のことがあるので保持しない。
        node->setSfen(parent->sfen());
        node->setMove(ShogiMove());
    } else {
        node->setSfen(sfen);
        node->setMove(move);
    }

    parent->addChild(node);
    invalidateLineCache();

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

KifuBranchNode* KifuBranchTree::findByPlyOnLine(KifuBranchNode* lineEnd, int ply) const
{
    if (lineEnd == nullptr) {
        return nullptr;
    }

    // lineEndからルートまで辿って、指定plyのノードを探す
    QList<KifuBranchNode*> path = pathToNode(lineEnd);
    for (KifuBranchNode* node : std::as_const(path)) {
        if (node->ply() == ply) {
            return node;
        }
    }
    return nullptr;
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
    if (parent == nullptr) {
        return nullptr;
    }

    const TerminalType termType = detectTerminalType(displayText);

    for (KifuBranchNode* child : parent->children()) {
        if (termType != TerminalType::None) {
            // 終局手は種類が同じなら同じ手（同じ親の下なので手番も同じ）
            if (child->terminalType() == termType) {
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

