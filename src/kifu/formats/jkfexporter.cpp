/// @file jkfexporter.cpp
/// @brief JKF形式棋譜エクスポータクラスの実装

#include "jkfexporter.h"
#include "jkfformatter.h"
#include "terminalcodes.h"
#include "jkfcodec.h"
#include "kifuexportmetadata.h"
#include "kifparsetypes.h"
#include "kifubranchnode.h"
#include "kifubranchtree.h"
#include "kifunodereplay.h"
#include "sfenutils.h"
#include "usiposition.h"
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

    for (const auto& item : std::as_const(ctx.gameInfoItems)) {
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

    const QString preset = JkfCodec::presetFromSfen(startSfen);
    // 古い読み込み実装にも伝わる基本の手合だけを preset で書く。
    // 後から追加された駒落ちは、標準の OTHER と盤・持駒・手番で正確に表す。
    static const QStringList portablePresets = {
        QStringLiteral("HIRATE"), QStringLiteral("KY"), QStringLiteral("KY_R"),
        QStringLiteral("KA"), QStringLiteral("HI"), QStringLiteral("HIKY"),
        QStringLiteral("2"), QStringLiteral("4"), QStringLiteral("6"),
        QStringLiteral("8"), QStringLiteral("10")};
    if (portablePresets.contains(preset)) {
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

// 書き出す変化の入れ子の深さの上限。forks は入れ子ごとに JSON を3段深くし、Qt の JSON の読み込みは
// 1024 段までなので、これより深い変化は読み直せない。再帰とスタックの深さもこれで抑える（セキュリティ監査 S5）
static constexpr int kMaxForkDepth = 300;

/**
 * @brief first から最初の子をたどった手順を JKF の moves の要素の列にする
 * @param replay first を指す前の局面。指し手を適用して進める
 * @param depth first の手順の変化の入れ子の深さ（本譜は 0）
 *
 * 指し手は、ノードの指し手（USI）と局面から作る。分岐点では、親の2番目以降の子からの手順を、
 * 代わられる手（先頭の子）の要素の forks に入れる。局面に合わない指し手のノードは書かず、
 * 局面をノードから取り直して続ける。入れ子が kMaxForkDepth を超える変化は書かない。
 */
static QJsonArray buildJkfMoves(const KifuBranchNode* first, KifuNodeReplay replay, int depth = 0)
{
    QJsonArray moves;
    for (const KifuBranchNode* node = first; node != nullptr; node = node->childAt(0)) {
        // この手に代わる手順。指す前の局面から別にたどる
        QJsonArray forks;
        const KifuBranchNode* parent = node->parent();
        if (parent != nullptr && parent->childAt(0) == node && parent->hasBranch()) {
            if (depth >= kMaxForkDepth) {
                qCWarning(lcKifu).noquote() << "toJkfLines: skipped variations nested deeper than" << kMaxForkDepth
                                            << "at ply" << node->ply();
            } else {
                for (int i = 1; i < parent->childCount(); ++i) {
                    const QJsonArray fork = buildJkfMoves(parent->childAt(i), replay.branchFrom(parent), depth + 1);
                    if (!fork.isEmpty()) forks.append(fork);
                }
            }
        }

        QJsonObject entry;
        if (node->isTerminal()) {
            QString special = TerminalCodes::csaSpecial(node->terminalType(), node->terminalSide(), true);
            if (special.isEmpty()) special = node->record().terminalLabel();
            if (!special.isEmpty()) entry[QStringLiteral("special")] = special;
        } else {
            const UsiMove move = node->move().move;
            if (replay.canWrite(node))
                entry[QStringLiteral("move")] =
                    JkfFormatter::moveToJkf(replay.board(), replay.mover(), move, replay.previousTo());
            if (!replay.advance(node)) {
                qCWarning(lcKifu).noquote() << "toJkfLines: skipped a move that does not fit the position: ply="
                                            << node->ply() << "usi=" << move.toString();
                entry = QJsonObject();
            }
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
                                                                : UsiPosition::sfenOf(ctx.startSfen);
    root[QStringLiteral("initial")] = buildJkfInitial(startSfen);

    // 3) moves（先頭は開始局面。本譜の手順と、分岐点ごとの forks）
    QJsonObject opening;
    QJsonArray movesArray;
    const KifuBranchTree* tree = &recordTree;
    if (tree != nullptr && !tree->isEmpty()) {
        const KifuBranchNode* rootNode = tree->root();
        const QJsonArray comments = commentLines(rootNode->comment());
        if (!comments.isEmpty()) opening[QStringLiteral("comments")] = comments;

        movesArray = buildJkfMoves(rootNode->childAt(0), KifuNodeReplay(startSfen, rootNode->sfen()));
    }
    movesArray.prepend(opening);

    root[QStringLiteral("moves")] = movesArray;

    const QJsonDocument doc(root);
    out << QString::fromUtf8(doc.toJson(QJsonDocument::Compact));

    qCDebug(lcKifu).noquote() << "toJkfLines: generated JKF with"
                              << movesArray.size() << "moves";

    return out;
}
