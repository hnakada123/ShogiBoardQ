/// @file ki2exporter.cpp
/// @brief KI2形式棋譜エクスポータクラスの実装

#include "ki2exporter.h"
#include "ki2movenotation.h"
#include "kifexporter.h"
#include "kifunodereplay.h"
#include "kifubranchnode.h"
#include "kifubranchtree.h"
#include "logcategories.h"
#include "kifuexportcommon.h"

#include <QRegularExpression>

// ========================================
// ヘルパ関数
// ========================================

using KifuExportCommon::appendKifBookmarks;
using KifuExportCommon::appendKifComments;
using KifuExportCommon::buildEndingLine;

// ヘルパ関数: 手順のノードを KI2 の指し手の文字列にする
//
// 局面は開始局面からノードの指し手（USI）を順に盤へ適用して追い、修飾語・打・不成を局面から決める。
// fromPly 手目より前の手は局面を進めるだけで、文字列は空にする。終局ノードと開始局面も空。
// 指し手を持たない・局面に合わないノードは、表示の文字列から移動元を除いて書き、局面はノードから取り直す。
static QStringList ki2MoveTexts(const QList<KifuBranchNode*>& nodes, const QString& startSfen, int fromPly)
{
    QStringList out;
    out.reserve(nodes.size());
    KifuNodeReplay replay(startSfen, nodes.isEmpty() ? QString() : nodes.first()->sfen());
    for (const KifuBranchNode* node : nodes) {
        if (!node->isActualMove()) {
            out << QString();
            continue;
        }
        const bool needsText = node->ply() >= fromPly;
        QString text;
        if (needsText && replay.canWrite(node))
            text = Ki2MoveNotation::format(replay.board(), replay.mover(), node->move().move, replay.previousTo());
        if (!replay.advance(node) && needsText) {
            static const QRegularExpression fromPosPattern(QStringLiteral("\\([0-9０-９]+[0-9０-９]+\\)$"));
            text = node->displayText().trimmed().remove(fromPosPattern);
        }
        out << text;
    }
    return out;
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
            out << buildEndingLine(node->ply() - 1, node->terminalType(), node->terminalSide(), handicapNames);
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
        appendKi2Moves(mainNodes, ki2MoveTexts(mainNodes, ctx.startSfen, 1), 1, false, handicapNames, out);
    const KifuBranchNode* terminal = written.terminal;
    out << buildEndingLine(written.lastMovePly, terminal ? terminal->terminalType() : TerminalType::None,
                           terminal ? terminal->terminalSide() : Turn::Black, handicapNames);
    if (terminal) {
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
            appendKi2Moves(line.nodes, ki2MoveTexts(line.nodes, ctx.startSfen, line.branchPly), line.branchPly,
                           true, handicapNames, out);
        }
    }

    qCDebug(lcKifu).noquote() << "toKi2Lines: generated" << out.size() << "lines," << written.lastMovePly
                              << "moves";
    return out;
}
