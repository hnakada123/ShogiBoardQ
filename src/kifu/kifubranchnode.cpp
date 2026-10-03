/// @file kifubranchnode.cpp
/// @brief 分岐ツリーノードクラスの実装

#include "kifubranchnode.h"

// ============================================================
// 自由関数
// ============================================================

TerminalType detectTerminalType(const QString& displayText)
{
    if (displayText.contains(QStringLiteral("投了"))) return TerminalType::Resign;
    if (displayText.contains(QStringLiteral("詰み"))) return TerminalType::Checkmate;
    if (displayText.contains(QStringLiteral("千日手"))) return TerminalType::Repetition;
    if (displayText.contains(QStringLiteral("持将棋"))) return TerminalType::Impasse;
    // 棋譜ファイルの「切れ負け」と、対局終了処理の「時間切れ」を同じ終局として扱う。
    if (displayText.contains(QStringLiteral("切れ負け"))
        || displayText.contains(QStringLiteral("時間切れ"))) return TerminalType::Timeout;
    if (displayText.contains(QStringLiteral("反則勝ち"))) return TerminalType::IllegalWin;
    if (displayText.contains(QStringLiteral("反則負け"))) return TerminalType::IllegalLoss;
    if (displayText.contains(QStringLiteral("不戦敗"))) return TerminalType::Forfeit;
    if (displayText.contains(QStringLiteral("不戦勝"))) return TerminalType::Forfeit;
    if (displayText.contains(QStringLiteral("中断"))) return TerminalType::Interrupt;
    if (displayText.contains(QStringLiteral("不詰"))) return TerminalType::NoCheckmate;
    if (displayText.contains(QStringLiteral("入玉勝ち"))
        || displayText.contains(QStringLiteral("宣言勝ち"))) return TerminalType::DeclarationWin;
    if (displayText.contains(QStringLiteral("引き分け"))) return TerminalType::Draw;
    if (displayText.contains(QStringLiteral("最大手数到達"))) return TerminalType::MaxMoves;
    if (displayText.contains(QStringLiteral("エラー"))) return TerminalType::Error;
    return TerminalType::None;
}

// ============================================================
// 初期化
// ============================================================

KifuBranchNode::KifuBranchNode() = default;

KifuBranchNode::~KifuBranchNode()
{
    // 子ノードは所有しない（KifuBranchTreeが一括管理）
}

// ============================================================
// ツリー構造操作
// ============================================================

void KifuBranchNode::addChild(KifuBranchNode* child)
{
    if (child && !m_children.contains(child)) {
        m_children.append(child);
        child->setParent(this);
    }
}

void KifuBranchNode::removeChild(KifuBranchNode* child)
{
    if (m_children.removeOne(child)) {
        child->setParent(nullptr);
    }
}

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

bool KifuBranchNode::isMainLine() const
{
    if (m_parent == nullptr) {
        return true;
    }
    // 親の最初の子が自分なら本譜
    const auto& siblings = m_parent->children();
    return !siblings.isEmpty() && siblings.first() == this;
}

int KifuBranchNode::lineIndex() const
{
    // ルートから辿って、最初の分岐点での子インデックスを返す
    // 本譜（常に最初の子）を辿っていれば0、分岐があれば1以降

    QList<const KifuBranchNode*> path;
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

QList<KifuBranchNode*> KifuBranchNode::siblings() const
{
    QList<KifuBranchNode*> result;
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
