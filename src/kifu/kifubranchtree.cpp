/// @file kifubranchtree.cpp
/// @brief 分岐ツリーデータモデルクラスの実装

#include "kifubranchtree.h"

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
    m_root->setDisplayText(QStringLiteral("開始局面"));
    m_root->setSfen(sfen);

    emit treeChanged();
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
    node->setSfen(sfen);
    node->setMove(move);
    node->setTimeText(timeText);

    // 終局手かどうかを判定
    TerminalType termType = detectTerminalType(displayText);
    node->setTerminalType(termType);

    parent->addChild(node);

    emit nodeAdded(node);
    emit treeChanged();

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

    emit nodeAdded(node);
    emit treeChanged();

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
    node->setSfen(sfen);
    node->setMove(move);
    node->setTimeText(timeText);

    // 終局手かどうかを判定
    TerminalType termType = detectTerminalType(displayText);
    node->setTerminalType(termType);

    parent->addChild(node);

    // nodeAdded のみ発火、treeChanged は発火しない
    emit nodeAdded(node);

    return node;
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
    QVector<KifuBranchNode*> path = pathToNode(lineEnd);
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

KifuBranchNode* KifuBranchTree::findBySfen(const QString& sfen) const
{
    if (sfen.isEmpty()) {
        return nullptr;
    }

    // 入力SFENから手数部分を除去
    QString targetSfen = sfen;
    qsizetype lastSpace = targetSfen.lastIndexOf(QLatin1Char(' '));
    if (lastSpace > 0) {
        // 最後のスペース以降が数字のみなら手数部分として除去
        QString suffix = targetSfen.mid(lastSpace + 1);
        bool isNumber = false;
        suffix.toInt(&isNumber);
        if (isNumber) {
            targetSfen = targetSfen.left(lastSpace);
        }
    }

    // 全ノードを検索
    for (KifuBranchNode* node : std::as_const(m_nodeById)) {
        QString nodeSfen = node->sfen();
        if (nodeSfen.isEmpty()) {
            continue;
        }

        // ノードのSFENからも手数部分を除去して比較
        qsizetype nodeLastSpace = nodeSfen.lastIndexOf(QLatin1Char(' '));
        if (nodeLastSpace > 0) {
            QString nodeSuffix = nodeSfen.mid(nodeLastSpace + 1);
            bool isNumber = false;
            nodeSuffix.toInt(&isNumber);
            if (isNumber) {
                nodeSfen = nodeSfen.left(nodeLastSpace);
            }
        }

        if (nodeSfen == targetSfen) {
            return node;
        }
    }

    return nullptr;
}

QVector<KifuBranchNode*> KifuBranchTree::mainLine() const
{
    QVector<KifuBranchNode*> result;
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

QVector<KifuBranchNode*> KifuBranchTree::pathToNode(KifuBranchNode* node) const
{
    QVector<KifuBranchNode*> path;
    while (node != nullptr) {
        path.prepend(node);
        node = node->parent();
    }
    return path;
}

QVector<KifuBranchNode*> KifuBranchTree::branchesAt(KifuBranchNode* node) const
{
    QVector<KifuBranchNode*> result;
    if (node == nullptr || node->parent() == nullptr) {
        return result;
    }

    // 親の全ての子（自分を含む）を返す
    KifuBranchNode* parent = node->parent();
    for (KifuBranchNode* child : std::as_const(parent->children())) {
        result.append(child);
    }
    return result;
}

QVector<BranchLine> KifuBranchTree::allLines() const
{
    QVector<BranchLine> lines;
    if (m_root == nullptr) {
        return lines;
    }

    QVector<KifuBranchNode*> currentPath;
    int lineIndex = 0;
    collectLinesRecursive(m_root, currentPath, lines, lineIndex);

    return lines;
}

void KifuBranchTree::collectLinesRecursive(KifuBranchNode* node,
                                           QVector<KifuBranchNode*>& currentPath,
                                           QVector<BranchLine>& lines,
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

int KifuBranchTree::findLineIndexForNode(KifuBranchNode* node) const
{
    if (node == nullptr) {
        return -1;
    }

    QVector<BranchLine> lines = allLines();

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

    return -1;
}

void KifuBranchTree::setComment(int nodeId, const QString& comment)
{
    KifuBranchNode* node = nodeAt(nodeId);
    if (node != nullptr) {
        node->setComment(comment);
        emit commentChanged(nodeId, comment);
    }
}

// === データ抽出（ResolvedRow互換）===

QList<KifDisplayItem> KifuBranchTree::getDisplayItemsForLine(int lineIndex) const
{
    QList<KifDisplayItem> result;

    QVector<BranchLine> lines = allLines();
    if (lineIndex < 0 || lineIndex >= lines.size()) {
        return result;
    }

    const BranchLine& line = lines.at(lineIndex);
    for (KifuBranchNode* node : std::as_const(line.nodes)) {
        KifDisplayItem item;
        item.prettyMove = node->displayText();
        item.comment = node->comment();
        item.timeText = node->timeText();
        result.append(item);
    }

    return result;
}

QStringList KifuBranchTree::getSfenListForLine(int lineIndex) const
{
    QStringList result;

    QVector<BranchLine> lines = allLines();
    if (lineIndex < 0 || lineIndex >= lines.size()) {
        return result;
    }

    const BranchLine& line = lines.at(lineIndex);
    for (KifuBranchNode* node : std::as_const(line.nodes)) {
        result.append(node->sfen());
    }

    return result;
}

QVector<ShogiMove> KifuBranchTree::getGameMovesForLine(int lineIndex) const
{
    QVector<ShogiMove> result;

    QVector<BranchLine> lines = allLines();
    if (lineIndex < 0 || lineIndex >= lines.size()) {
        return result;
    }

    const BranchLine& line = lines.at(lineIndex);
    for (KifuBranchNode* node : std::as_const(line.nodes)) {
        // ply=0（開始局面）と終局手は含めない
        if (node->isActualMove()) {
            result.append(node->move());
        }
    }

    return result;
}

QStringList KifuBranchTree::getCommentsForLine(int lineIndex) const
{
    QStringList result;

    QVector<BranchLine> lines = allLines();
    if (lineIndex < 0 || lineIndex >= lines.size()) {
        return result;
    }

    const BranchLine& line = lines.at(lineIndex);
    for (KifuBranchNode* node : std::as_const(line.nodes)) {
        result.append(node->comment());
    }

    return result;
}
