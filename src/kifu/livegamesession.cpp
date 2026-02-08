/// @file livegamesession.cpp
/// @brief ライブ対局セッション管理クラスの実装

#include "livegamesession.h"
#include "kifubranchtree.h"

#include <QDebug>
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
    qDebug().noquote() << "[LGS] startFromNode ENTER branchPoint="
                       << (branchPoint ? "yes" : "null")
                       << "branchPointPly=" << (branchPoint ? branchPoint->ply() : -1)
                       << "branchPointSfen=" << (branchPoint ? branchPoint->sfen().left(60) : "(null)");

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
        qDebug().noquote() << "[LGS] startFromNode: recorded anchor sfen from branchPoint";
    } else if (m_tree != nullptr && m_tree->root() != nullptr) {
        m_sfens.append(m_tree->root()->sfen());
        qDebug().noquote() << "[LGS] startFromNode: recorded anchor sfen from root";
    }

    qDebug().noquote() << "[LGS] startFromNode: anchorSfen=" << anchorSfen().left(60);

    emit sessionStarted(m_branchPoint);

    qDebug().noquote() << "[LGS] startFromNode LEAVE";
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
    qDebug().noquote() << "[LGS] addMove ENTER displayText=" << displayText
                       << "sfen=" << sfen.left(60);

    if (!canAddMove()) {
        qWarning() << "[LiveGameSession] Cannot add move - session not active or already terminated";
        return;
    }

    const int ply = anchorPly() + static_cast<int>(m_moves.size()) + 1;
    qDebug().noquote() << "[LGS] addMove: ply=" << ply
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

        qDebug().noquote() << "[LGS] addMove: parent node ply="
                           << (parent ? parent->ply() : -1)
                           << "branchPoint ply="
                           << (m_branchPoint ? m_branchPoint->ply() : -1);

        if (parent != nullptr) {
            // addMoveQuiet() を使用: nodeAdded のみ発火、treeChanged は発火しない
            m_liveParent = m_tree->addMoveQuiet(parent, move, displayText, sfen, elapsed);
            qDebug().noquote() << "[LGS] addMove: added to tree, m_liveParent ply="
                               << (m_liveParent ? m_liveParent->ply() : -1)
                               << "sfen stored in node="
                               << (m_liveParent ? m_liveParent->sfen().left(60) : "(null)");
        }
    }

    KifDisplayItem item;
    item.prettyMove = displayText;
    item.timeText = elapsed;
    item.ply = ply;
    m_moves.append(item);

    m_gameMoves.append(move);
    m_sfens.append(sfen);

    emit moveAdded(ply, displayText);

    // 分岐マークを計算して通知
    computeAndEmitBranchMarks();

    // 棋譜欄モデルの更新が必要
    emit recordModelUpdateRequired();

    qDebug().noquote() << "[LGS] addMove LEAVE";
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

    const int lineIndex = m_tree->findLineIndexForNode(node);
    if (lineIndex >= 0) {
        return lineIndex;
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
        QVector<BranchLine> lines = m_tree->allLines();
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
