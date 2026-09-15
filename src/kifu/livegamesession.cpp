/// @file livegamesession.cpp
/// @brief ライブ対局セッション管理クラスの実装

#include "livegamesession.h"
#include "kifubranchtree.h"

#include "logcategories.h"
#include <QSet>

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
        m_sfens.append(m_branchPoint->sfen());
        qCDebug(lcKifu).noquote() << "startFromNode: recorded anchor sfen from branchPoint";
    } else if (m_tree != nullptr && m_tree->root() != nullptr) {
        m_sfens.append(m_tree->root()->sfen());
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
    qCDebug(lcKifu).noquote() << "addMove ENTER displayText=" << displayText
                       << "sfen=" << sfen.left(60);

    if (!canAddMove()) {
        qCWarning(lcKifu) << "Cannot add move - session not active or already terminated";
        return;
    }

    const int ply = anchorPly() + static_cast<int>(m_moves.size()) + 1;
    qCDebug(lcKifu).noquote() << "addMove: ply=" << ply
                       << "anchorPly=" << anchorPly()
                       << "movesSize=" << m_moves.size();

    // リアルタイムでツリーに追加（treeChanged を発火しない quiet 版を使用）
    // treeChanged が発火すると KifuDisplayCoordinator::onTreeChanged() が呼ばれ、
    // 棋譜モデルが再構築されて HvE 対局に干渉する
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
            KifuBranchNode* existing = m_tree->findMatchingChild(parent, move, displayText, sfen);
            if (existing != nullptr) {
                m_liveParent = existing;
                qCDebug(lcKifu).noquote() << "addMove: reused existing node ply=" << existing->ply()
                                   << "displayText=" << existing->displayText();
            } else {
                // addMoveQuiet() を使用: treeChanged は発火しない
                m_liveParent = m_tree->addMoveQuiet(parent, move, displayText, sfen, elapsed);
                qCDebug(lcKifu).noquote() << "addMove: added to tree, m_liveParent ply="
                                   << (m_liveParent ? m_liveParent->ply() : -1)
                                   << "sfen stored in node="
                                   << (m_liveParent ? m_liveParent->sfen().left(60) : "(null)");
            }
        }
    }

    KifDisplayItem item;
    item.prettyMove = displayText;
    item.timeText = elapsed;
    item.ply = ply;
    m_moves.append(item);

    m_gameMoves.append(move);
    m_sfens.append(sfen);

    // 終局手を追加したらセッションは終了状態（これ以上の追加は canAddMove() で拒否）
    if (detectTerminalType(displayText) != TerminalType::None) {
        m_hasTerminal = true;
    }

    emit moveAdded(ply, displayText);

    // 分岐マークを計算して通知
    computeAndEmitBranchMarks();

    // 棋譜欄モデルの更新が必要
    emit recordModelUpdateRequired();

    qCDebug(lcKifu).noquote() << "addMove LEAVE";
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
    if (m_moves.isEmpty()) {
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
    return anchorPly() + static_cast<int>(m_moves.size());
}

QString LiveGameSession::currentSfen() const
{
    if (m_sfens.isEmpty()) {
        return QString();
    }
    return m_sfens.last();
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
    m_liveParent = nullptr;
    m_moves.clear();
    m_gameMoves.clear();
    m_sfens.clear();
}

void LiveGameSession::computeAndEmitBranchMarks()
{
    QSet<int> branchPlys;

    if (m_tree == nullptr || m_tree->isEmpty()) {
        emit branchMarksUpdated(branchPlys);
        return;
    }

    // 分岐起点から始まるラインを取得
    // 親ラインの分岐マークを取得
    KifuBranchNode* parentNode = m_branchPoint;
    if (parentNode == nullptr) {
        parentNode = m_tree->root();
    }

    // 分岐起点から親をたどって分岐マークを収集
    if (parentNode != nullptr) {
        // 親ラインを含むラインを探す
        QList<BranchLine> lines = m_tree->allLines();
        for (const BranchLine& line : std::as_const(lines)) {
            for (KifuBranchNode* node : std::as_const(line.nodes)) {
                // 分岐起点以下の手で、親に複数の子がある場合は分岐マーク
                if (node->ply() <= anchorPly() && node->parent() != nullptr &&
                    node->parent()->childCount() > 1) {
                    branchPlys.insert(node->ply());
                }
                // 自分自身への分岐マーク（最初の1手目）
                if (node == parentNode && !m_moves.isEmpty()) {
                    // 最初の手が追加された位置は分岐点になる可能性がある
                    int firstMovePly = anchorPly() + 1;
                    // 親に複数の子がある場合は分岐（子が1つだけなら分岐ではない）
                    if (parentNode->childCount() > 1) {
                        branchPlys.insert(firstMovePly);
                    }
                }
            }
        }
    }

    emit branchMarksUpdated(branchPlys);
}
