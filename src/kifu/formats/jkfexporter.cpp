/// @file jkfexporter.cpp
/// @brief JKF形式棋譜エクスポータクラスの実装

#include "jkfexporter.h"
#include "jkfformatter.h"
#include "terminalcodes.h"
#include "jkfmoveparser.h"
#include "kifuexportmetadata.h"
#include "kifparsetypes.h"
#include "kifubranchnode.h"
#include "kifubranchtree.h"
#include "sfenutils.h"
#include "shogiposition.h"
#include "logcategories.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

// ========================================
// JKF構築用の内部関数
// ========================================

// コメントを行ごとの配列にする（空なら空の配列）
static QJsonArray commentLines(const QString& comment)
{
    QJsonArray lines;
    if (comment.isEmpty()) return lines;
    static const QRegularExpression newlineRe(QStringLiteral("\r?\n"));
    const QStringList parts = comment.split(newlineRe);
    for (const QString& line : parts) {
        lines.append(line);
    }
    return lines;
}

// JKF形式のヘッダ部分を構築
static QJsonObject buildJkfHeader(const KifuExportMetadata& ctx)
{
    QJsonObject header;

    const QList<KifGameInfoItem> items = KifuExportMetadataBuilder::collect(ctx);
    for (const auto& item : std::as_const(items)) {
        const QString key = item.key.trimmed();
        const QString val = item.value.trimmed();
        if (!key.isEmpty()) {
            header[key] = val;
        }
    }

    return header;
}

// JKF形式の初期局面部分を構築
static QJsonObject buildJkfInitial(const QString& startSfen)
{
    QJsonObject initial;

    const QString preset = JkfMoveParser::presetFromSfen(startSfen);
    if (!preset.isEmpty()) {
        initial[QStringLiteral("preset")] = preset;
    } else {
        initial[QStringLiteral("preset")] = QStringLiteral("OTHER");

        const QJsonObject data = JkfFormatter::sfenToJkfData(startSfen);
        if (!data.isEmpty()) {
            initial[QStringLiteral("data")] = data;
        }
    }

    return initial;
}

// 盤の局面の SFEN（変化の手順を別の盤で続けるため。手数は使わない）
static QString positionSfen(Position& board, Turn mover)
{
    return board.toSfen(mover, 1);
}

/**
 * @brief first から最初の子をたどった手順を JKF の moves の要素の列にする
 * @param board first を指す前の局面。指し手を適用して進める
 * @param synced board がノードの局面と合っているか
 * @param mover first を指す側
 * @param previousTo 直前の手の移動先（same を決める）
 *
 * 指し手は、ノードの指し手（USI）と局面から作る。分岐点では、親の2番目以降の子からの手順を、
 * 代わられる手（先頭の子）の要素の forks に入れる。局面に合わない指し手のノードは書かず、
 * 局面をノードから取り直して続ける。
 */
static QJsonArray buildJkfMoves(const KifuBranchNode* first, Position& board, bool synced, Turn mover,
                                BoardSquare previousTo)
{
    QJsonArray moves;
    for (const KifuBranchNode* node = first; node != nullptr; node = node->childAt(0)) {
        // この手に代わる手順。指す前の局面から別の盤で続ける
        QJsonArray forks;
        const KifuBranchNode* parent = node->parent();
        if (parent != nullptr && parent->childAt(0) == node && parent->hasBranch()) {
            const QString before = synced ? positionSfen(board, mover) : parent->sfen();
            for (int i = 1; i < parent->childCount(); ++i) {
                Position forkBoard;
                const bool forkSynced = forkBoard.assignSfen(before);
                const QJsonArray fork = buildJkfMoves(parent->childAt(i), forkBoard, forkSynced,
                                                      forkSynced ? forkBoard.sideToMove() : mover, previousTo);
                if (!fork.isEmpty()) forks.append(fork);
            }
        }

        QJsonObject entry;
        if (node->isTerminal()) {
            const QString special = TerminalCodes::csaSpecial(node->terminalType(), node->terminalSide(), true);
            if (!special.isEmpty()) entry[QStringLiteral("special")] = special;
        } else {
            const UsiMove move = node->move().move;
            const bool applicable = synced && move.isValid();
            if (applicable) entry[QStringLiteral("move")] = JkfFormatter::moveToJkf(board, mover, move, previousTo);
            if (applicable && board.applyAs(move, mover)) {
                mover = (mover == Turn::Black) ? Turn::White : Turn::Black;
            } else {
                qCWarning(lcKifu).noquote() << "toJkfLines: skipped a move that does not fit the position: ply="
                                            << node->ply() << "usi=" << move.toString();
                entry = QJsonObject();
                synced = board.assignSfen(node->sfen());
                mover = synced ? board.sideToMove() : ((mover == Turn::Black) ? Turn::White : Turn::Black);
            }
            previousTo = move.to;
        }

        if (entry.isEmpty()) continue;
        const QJsonObject time = JkfFormatter::timeToJkf(node->time());
        if (!time.isEmpty()) entry[QStringLiteral("time")] = time;
        const QJsonArray comments = commentLines(node->comment());
        if (!comments.isEmpty()) entry[QStringLiteral("comments")] = comments;
        if (!forks.isEmpty()) entry[QStringLiteral("forks")] = forks;
        moves.append(entry);
    }
    return moves;
}

// ========================================
// JkfExporter 本体
// ========================================

QStringList JkfExporter::exportLines(const KifuBranchTree& recordTree, const KifuExportMetadata& ctx)
{
    QStringList out;

    QJsonObject root;

    // 1) header
    root[QStringLiteral("header")] = buildJkfHeader(ctx);

    // 2) initial
    const QString startSfen = ctx.startSfen.trimmed().isEmpty() ? SfenUtils::hirateSfen()
                                                                : SfenUtils::normalizeStart(ctx.startSfen);
    root[QStringLiteral("initial")] = buildJkfInitial(startSfen);

    // 3) moves（先頭は開始局面。本譜の手順と、分岐点ごとの forks）
    QJsonObject opening;
    QJsonArray movesArray;
    const KifuBranchTree* tree = &recordTree;
    if (tree != nullptr && !tree->isEmpty()) {
        const KifuBranchNode* rootNode = tree->root();
        const QJsonArray comments = commentLines(rootNode->comment());
        if (!comments.isEmpty()) opening[QStringLiteral("comments")] = comments;

        Position board;
        const bool synced = board.assignSfen(startSfen) || board.assignSfen(rootNode->sfen());
        movesArray = buildJkfMoves(rootNode->childAt(0), board, synced, board.sideToMove(), BoardSquare{});
    }
    movesArray.prepend(opening);

    root[QStringLiteral("moves")] = movesArray;

    const QJsonDocument doc(root);
    out << QString::fromUtf8(doc.toJson(QJsonDocument::Compact));

    qCDebug(lcKifu).noquote() << "toJkfLines: generated JKF with"
                              << movesArray.size() << "moves";

    return out;
}
