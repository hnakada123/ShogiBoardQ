/// @file kifubranchtree_search.cpp
/// @brief 分岐ツリーデータモデル - 局面SFEN・指し手によるノード検索

#include "kifubranchtree.h"
#include "kifubranchnode.h"

#include <QMap>
#include <QStringList>

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
