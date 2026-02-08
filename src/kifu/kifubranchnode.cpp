/// @file kifubranchnode.cpp
/// @brief 分岐ツリーノードクラスの実装
/// @todo remove コメントスタイルガイド適用済み

#include "kifubranchnode.h"

// ============================================================
// 自由関数
// ============================================================

/// @todo remove コメントスタイルガイド適用済み
TerminalType detectTerminalType(const QString& displayText)
{
    if (displayText.contains(QStringLiteral("投了"))) return TerminalType::Resign;
    if (displayText.contains(QStringLiteral("詰み"))) return TerminalType::Checkmate;
    if (displayText.contains(QStringLiteral("千日手"))) return TerminalType::Repetition;
    if (displayText.contains(QStringLiteral("持将棋"))) return TerminalType::Impasse;
    if (displayText.contains(QStringLiteral("切れ負け"))) return TerminalType::Timeout;
    if (displayText.contains(QStringLiteral("反則勝ち"))) return TerminalType::IllegalWin;
    if (displayText.contains(QStringLiteral("反則負け"))) return TerminalType::IllegalLoss;
    if (displayText.contains(QStringLiteral("不戦敗"))) return TerminalType::Forfeit;
    if (displayText.contains(QStringLiteral("不戦勝"))) return TerminalType::Forfeit;
    if (displayText.contains(QStringLiteral("中断"))) return TerminalType::Interrupt;
    if (displayText.contains(QStringLiteral("不詰"))) return TerminalType::NoCheckmate;
    return TerminalType::None;
}

// ============================================================
// 初期化
// ============================================================

/// @todo remove コメントスタイルガイド適用済み
KifuBranchNode::KifuBranchNode()
    : m_nodeId(-1)
    , m_ply(0)
    , m_terminalType(TerminalType::None)
    , m_parent(nullptr)
{
}

/// @todo remove コメントスタイルガイド適用済み
KifuBranchNode::~KifuBranchNode()
{
    // 子ノードは所有しない（KifuBranchTreeが一括管理）
}

// ============================================================
// ツリー構造操作
// ============================================================

/// @todo remove コメントスタイルガイド適用済み
void KifuBranchNode::addChild(KifuBranchNode* child)
{
    if (child && !m_children.contains(child)) {
        m_children.append(child);
        child->setParent(this);
    }
}

/// @todo remove コメントスタイルガイド適用済み
void KifuBranchNode::removeChild(KifuBranchNode* child)
{
    if (child) {
        m_children.removeOne(child);
        if (child->parent() == this) {
            child->setParent(nullptr);
        }
    }
}

/// @todo remove コメントスタイルガイド適用済み
KifuBranchNode* KifuBranchNode::childAt(int index) const
{
    if (index >= 0 && index < m_children.size()) {
        return m_children.at(index);
    }
    return nullptr;
}

// ============================================================
// クエリ
// ============================================================

/// @todo remove コメントスタイルガイド適用済み
bool KifuBranchNode::isMainLine() const
{
    if (m_parent == nullptr) {
        return true;
    }
    // 親の最初の子が自分なら本譜
    const auto& siblings = m_parent->children();
    return !siblings.isEmpty() && siblings.first() == this;
}

/// @todo remove コメントスタイルガイド適用済み
int KifuBranchNode::depth() const
{
    int d = 0;
    const KifuBranchNode* node = this;
    while (node->parent() != nullptr) {
        d++;
        node = node->parent();
    }
    return d;
}

/// @todo remove コメントスタイルガイド適用済み
QString KifuBranchNode::lineName() const
{
    int idx = lineIndex();
    if (idx == 0) {
        return QStringLiteral("本譜");
    }
    return QStringLiteral("分岐%1").arg(idx);
}

/// @todo remove コメントスタイルガイド適用済み
int KifuBranchNode::lineIndex() const
{
    // ルートから辿って、最初の分岐点での子インデックスを返す
    // 本譜（常に最初の子）を辿っていれば0、分岐があれば1以降

    QVector<const KifuBranchNode*> path;
    const KifuBranchNode* node = this;
    while (node != nullptr) {
        path.prepend(node);
        node = node->parent();
    }

    for (int i = 0; i < path.size() - 1; ++i) {
        const KifuBranchNode* current = path.at(i);
        const KifuBranchNode* next = path.at(i + 1);

        if (current->childCount() > 1) {
            const auto& children = current->children();
            for (int j = 0; j < children.size(); ++j) {
                if (children.at(j) == next) {
                    return j;
                }
            }
        }
    }

    return 0;
}

/// @todo remove コメントスタイルガイド適用済み
QVector<KifuBranchNode*> KifuBranchNode::siblings() const
{
    QVector<KifuBranchNode*> result;
    if (m_parent == nullptr) {
        return result;
    }

    for (KifuBranchNode* child : std::as_const(m_parent->m_children)) {
        if (child != this) {
            result.append(child);
        }
    }
    return result;
}
