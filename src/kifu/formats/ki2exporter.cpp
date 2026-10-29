/// @file ki2exporter.cpp
/// @brief KI2形式棋譜エクスポータクラスの実装

#include "ki2exporter.h"
#include "ki2movenotation.h"
#include "kifexporter.h"
#include "kifmovetext.h"
#include "kifunodereplay.h"
#include "kifubranchnode.h"
#include "kifubranchtree.h"
#include "logcategories.h"
#include "kifuexportcommon.h"


// ========================================
// ヘルパ関数
// ========================================

using KifuExportCommon::appendKifBookmarks;
using KifuExportCommon::appendKifComments;
using KifuExportCommon::buildEndingLine;

// ヘルパ関数: 手順のノードを KI2 の指し手の文字列にする
//
// 局面は replay（nodes の最初の手を指す前の局面）からノードの指し手（USI）を順に盤へ適用して追い、
// 修飾語・打・不成を局面から決める。fromPly 手目より前の手は局面を進めるだけで、文字列は空にする。
// 終局ノードと開始局面も空。
// 指し手を持たない・局面に合わないノードは、表示の文字列から移動元を除いて書き、局面はノードから取り直す。
static QStringList ki2MoveTexts(const QList<KifuBranchNode*>& nodes, KifuNodeReplay replay, int fromPly)
{
    QStringList out;
    out.reserve(nodes.size());
    for (const KifuBranchNode* node : nodes) {
        if (!node->isActualMove()) {
            out << QString();
            continue;
        }
        const bool needsText = node->ply() >= fromPly;
        const BoardSquare previousTo = replay.previousTo();
        QString text;
        if (needsText && replay.canWrite(node))
            text = Ki2MoveNotation::format(replay.board(), replay.mover(), node->move().move, previousTo);
        if (!replay.advance(node) && needsText) {
            // 局面の上で指せない手（反則の後など）は修飾語を決められないので、移動元を書かない KIF の表記にする
            const ShogiMove move = node->move();
            KifMoveText::Notes notes;
            notes.withoutSource = true;
            notes.declinedPromotion = KifMoveText::couldPromote(move.movingPiece, move.move);
            text = move.movingPiece == Piece::None
                ? node->displayText().trimmed()
                : KifMoveText::format(move.mover(), move.movingPiece, move.move, previousTo, notes);
        }
        out << text;
    }
    return out;
}

// 本譜の開始局面（読めなければ根のノードの局面）からたどる
static KifuNodeReplay replayFrom(const QList<KifuBranchNode*>& nodes, const QString& startSfen)
{
    return KifuNodeReplay(startSfen, nodes.isEmpty() ? QString() : nodes.first()->sfen());
}

// 変化の分岐した手を指す前の局面。分岐元の手の後の局面から始め、手順ごとに開始局面から指し直さない
// （分岐元の SFEN が読めないときだけ、開始局面から分岐元まで指して進める）
static KifuNodeReplay replayAtBranch(const BranchLine& line, const QString& startSfen)
{
    if (line.branchPoint != nullptr) {
        KifuNodeReplay replay = KifuNodeReplay::after(line.branchPoint);
        if (replay.synced()) return replay;
    }
    const QList<KifuBranchNode*> path = line.nodes();
    KifuNodeReplay replay = replayFrom(path, startSfen);
    for (const KifuBranchNode* node : path) {
        if (node->ply() >= line.branchPly) break;
        replay.advance(node);
    }
    return replay;
}

// ========================================
// 指し手の行（本譜と変化で共通）
// ========================================

namespace {
struct Ki2Written {
    int lastMovePly = 0;                     ///< 書いた最後の指し手の手数
    const KifuBranchNode* terminal = nullptr; ///< 手順の終局のノード
};
} // namespace

// 手順の fromPly 手目以降を KI2 の行にする。しおり・コメントのない手は1行に並べ、ある手とその次の手は1行に1手。
// 終局のノードは、inlineEnding なら「まで…」の行（としおり・コメント）として書き、そうでなければ書かずに返す
static Ki2Written appendKi2Moves(const QList<KifuBranchNode*>& nodes, const QStringList& moveTexts, int fromPly,
                                 bool inlineEnding, bool handicapNames, QStringList& out)
{
    Ki2Written written;
    QStringList movesOnLine;
    bool lastMoveHadNote = false;
    const auto flush = [&movesOnLine, &out]() {
        if (movesOnLine.isEmpty()) return;
        out << movesOnLine.join(QStringLiteral("    "));
        movesOnLine.clear();
    };

    for (qsizetype i = 0; i < nodes.size(); ++i) {
        const KifuBranchNode* node = nodes.at(i);
        if (node->ply() < fromPly) continue;
        const bool isTerminal = node->isTerminal();
        const QString& ki2Move = moveTexts.at(i);
        // 書ける指し手のないノードは飛ばす
        if (!isTerminal && ki2Move.isEmpty()) continue;

        const QString comment = node->comment().trimmed();
        const QString bookmark = node->bookmark().trimmed();
        const bool hasNote = !comment.isEmpty() || !bookmark.isEmpty();

        if (isTerminal) {
            written.terminal = node;
            flush();
            if (!inlineEnding) continue;
            // 終局手は「まで…」の結果行で表す（KI2 では指し手として書かない）
            out << buildEndingLine(node->ply() - 1, node->terminalType(), node->terminalSide(), handicapNames,
                                   node->record().terminalText);
        } else {
            written.lastMovePly = node->ply();
            if (!hasNote && !lastMoveHadNote) {
                movesOnLine.append(ki2Move);
                continue;
            }
            flush();
            out << ki2Move;
        }
        appendKifBookmarks(bookmark, out);
        if (!comment.isEmpty()) appendKifComments(comment, out);
        lastMoveHadNote = hasNote;
    }
    flush();
    return written;
}

// ========================================
// エクスポート
// ========================================

QStringList Ki2Exporter::exportLines(const KifuBranchTree& recordTree, const KifuExportMetadata& ctx)
{
    QStringList out;

    // 0〜1) 文字コード宣言・対局情報（消費時間は KI2 では省く）・平手でない開始局面の盤面図
    KifExporter::HeaderOptions headerOptions;
    headerOptions.consumedTime = false;
    const bool handicapNames = KifExporter::appendHeaderLines(ctx, out, headerOptions);

    // 2) 本譜の手順（分岐ツリーの各ノードの最初の子をたどる）。開始局面のしおり・コメントを先に書く
    const QList<KifuBranchNode*> mainNodes = recordTree.mainLine();
    if (!mainNodes.isEmpty() && mainNodes.first()->ply() == 0) {
        appendKifBookmarks(mainNodes.first()->bookmark(), out);
        appendKifComments(mainNodes.first()->comment(), out);
    }

    // 3) 指し手（修飾語・打・不成は局面と指し手から決める）と終了行
    const Ki2Written written =
        appendKi2Moves(mainNodes, ki2MoveTexts(mainNodes, replayFrom(mainNodes, ctx.startSfen), 1), 1, false,
                       handicapNames, out);
    const KifuBranchNode* terminal = written.terminal;
    if (terminal) {
        // 未終局の「までN手」を未知の終局として読む実装があるため、結果は終局したときだけ書く。
        out << buildEndingLine(written.lastMovePly, terminal->terminalType(), terminal->terminalSide(), handicapNames,
                               terminal->record().terminalText);
        appendKifBookmarks(terminal->bookmark(), out);
        appendKifComments(terminal->comment(), out);
    }

    // 4) 変化（lineIndex = 0 は本譜なので 1 から）。分岐点より前の手は局面を進めるだけ
    if (!recordTree.isEmpty()) {
        const QList<BranchLine> lines = recordTree.allLines();
        for (qsizetype lineIdx = 1; lineIdx < lines.size(); ++lineIdx) {
            const BranchLine& line = lines.at(lineIdx);
            out << QString();
            out << QStringLiteral("変化：%1手").arg(line.branchPly);
            const QList<KifuBranchNode*> nodes = line.nodesFrom(line.branchPly);
            appendKi2Moves(nodes, ki2MoveTexts(nodes, replayAtBranch(line, ctx.startSfen), line.branchPly),
                           line.branchPly, true, handicapNames, out);
        }
    }

    qCDebug(lcKifu).noquote() << "toKi2Lines: generated" << out.size() << "lines," << written.lastMovePly
                              << "moves";
    return out;
}
