#include "livegamesession.h"
#include "kifubranchtree.h"

#include <QDebug>

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
    if (m_active) {
        qWarning() << "[LiveGameSession] Session already active";
        return;
    }

    if (branchPoint != nullptr && !canStartFrom(branchPoint)) {
        qWarning() << "[LiveGameSession] Cannot start from terminal node";
        return;
    }

    reset();
    m_active = true;
    m_branchPoint = branchPoint;

    // 起点のSFENを記録
    if (m_branchPoint != nullptr) {
        m_sfens.append(m_branchPoint->sfen());
    } else if (m_tree != nullptr && m_tree->root() != nullptr) {
        m_sfens.append(m_tree->root()->sfen());
    }

    qDebug() << "[LiveGameSession] Started from ply" << anchorPly();
    emit sessionStarted(m_branchPoint);
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
    if (m_sfens.isEmpty()) {
        return QString();
    }
    return m_sfens.first();
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
    if (!canAddMove()) {
        qWarning() << "[LiveGameSession] Cannot add move - session not active or already terminated";
        return;
    }

    const int ply = anchorPly() + static_cast<int>(m_moves.size()) + 1;

    KifDisplayItem item;
    item.prettyMove = displayText;
    item.timeText = elapsed;
    item.ply = ply;
    m_moves.append(item);

    m_gameMoves.append(move);
    m_sfens.append(sfen);

    qDebug() << "[LiveGameSession] Added move at ply" << ply << ":" << displayText;
    emit moveAdded(ply, displayText);
}

void LiveGameSession::addTerminalMove(TerminalType type, const QString& displayText,
                                       const QString& elapsed)
{
    if (!canAddMove()) {
        qWarning() << "[LiveGameSession] Cannot add terminal - session not active or already terminated";
        return;
    }

    const int ply = anchorPly() + static_cast<int>(m_moves.size()) + 1;

    KifDisplayItem item;
    item.prettyMove = displayText;
    item.timeText = elapsed;
    item.ply = ply;
    m_moves.append(item);

    // 終局手にはShogiMoveは無効
    m_gameMoves.append(ShogiMove());

    // SFENは前回と同じ（盤面は変化しない）
    if (!m_sfens.isEmpty()) {
        m_sfens.append(m_sfens.last());
    }

    m_hasTerminal = true;

    qDebug() << "[LiveGameSession] Added terminal at ply" << ply << ":" << displayText;
    emit terminalAdded(type);
}

KifuBranchNode* LiveGameSession::commit()
{
    if (!m_active) {
        qWarning() << "[LiveGameSession] Cannot commit - session not active";
        return nullptr;
    }

    if (m_tree == nullptr) {
        qWarning() << "[LiveGameSession] Cannot commit - no tree set";
        discard();
        return nullptr;
    }

    // 追加する指し手がない場合は何もしない
    if (m_moves.isEmpty()) {
        qDebug() << "[LiveGameSession] No moves to commit";
        discard();
        return nullptr;
    }

    // 分岐起点を決定
    KifuBranchNode* parent = m_branchPoint;
    if (parent == nullptr) {
        parent = m_tree->root();
    }

    if (parent == nullptr) {
        qWarning() << "[LiveGameSession] Cannot commit - no root node";
        discard();
        return nullptr;
    }

    KifuBranchNode* lastNode = parent;

    // 各指し手をツリーに追加
    for (int i = 0; i < m_moves.size(); ++i) {
        const KifDisplayItem& item = m_moves.at(i);
        const ShogiMove& move = m_gameMoves.at(i);
        const QString sfen = (i + 1 < m_sfens.size()) ? m_sfens.at(i + 1) : QString();

        // 終局手かどうかを判定
        TerminalType termType = detectTerminalType(item.prettyMove);

        if (termType != TerminalType::None) {
            // 終局手
            lastNode = m_tree->addTerminalMove(lastNode, termType, item.prettyMove, item.timeText);
        } else {
            // 通常の指し手
            lastNode = m_tree->addMove(lastNode, move, item.prettyMove, sfen, item.timeText);
        }
    }

    qDebug() << "[LiveGameSession] Committed" << m_moves.size() << "moves";

    KifuBranchNode* result = lastNode;
    reset();

    emit sessionCommitted(result);
    return result;
}

void LiveGameSession::discard()
{
    qDebug() << "[LiveGameSession] Session discarded";
    reset();
    emit sessionDiscarded();
}

int LiveGameSession::totalPly() const
{
    return anchorPly() + static_cast<int>(m_moves.size());
}

QString LiveGameSession::currentSfen() const
{
    if (m_sfens.isEmpty()) {
        return QString();
    }
    return m_sfens.last();
}

bool LiveGameSession::willCreateBranch() const
{
    // 分岐起点がある場合、または既に子がある場合は分岐を作成
    if (m_branchPoint != nullptr) {
        return true;
    }

    // ルートからでも、既に本譜がある場合は分岐を作成
    if (m_tree != nullptr && m_tree->root() != nullptr) {
        return m_tree->root()->childCount() > 0;
    }

    return false;
}

QString LiveGameSession::newLineName() const
{
    if (!willCreateBranch()) {
        return QStringLiteral("本譜");
    }

    if (m_tree == nullptr) {
        return QStringLiteral("分岐1");
    }

    // 既存のライン数から新しい分岐名を決定
    int lineCount = m_tree->lineCount();
    return QStringLiteral("分岐%1").arg(lineCount);
}

void LiveGameSession::reset()
{
    m_active = false;
    m_hasTerminal = false;
    m_branchPoint = nullptr;
    m_moves.clear();
    m_gameMoves.clear();
    m_sfens.clear();
}
