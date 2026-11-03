/// @file kifuexport.cpp
/// @brief 棋譜の書き出し（形式ごとの書き出し関数の表）の実装

#include "kifuexport.h"

#include "csaexporter.h"
#include "jkfexporter.h"
#include "ki2exporter.h"
#include "kifexporter.h"
#include "kifubranchnode.h"
#include "kifubranchtree.h"
#include "kifudocument.h"
#include "terminalcodes.h"
#include "usenexporter.h"
#include "usiexporter.h"

namespace {

using Exporter = QStringList (*)(const KifuBranchTree&, const KifuExportMetadata&);

Exporter exporterFor(KifuFormat format)
{
    switch (format) {
    case KifuFormat::Ki2: return &Ki2Exporter::exportLines;
    case KifuFormat::Csa: return &CsaExporter::exportLines;
    case KifuFormat::Jkf: return &JkfExporter::exportLines;
    case KifuFormat::Usen: return &UsenExporter::exportLines;
    case KifuFormat::Usi: return &UsiExporter::exportLines;
    case KifuFormat::Kif:
    case KifuFormat::Sfen:
    case KifuFormat::Bod:
        break;
    }
    return &KifExporter::exportLines;
}

/// node と、その下のすべてのノード（変化を含む）
void collectSubtree(KifuBranchNode* node, QList<KifuBranchNode*>& out)
{
    if (!node) return;
    out.append(node);
    const QList<KifuBranchNode*> children = node->children();
    for (KifuBranchNode* child : children) collectSubtree(child, out);
}

} // namespace

namespace KifuExport {

KifuExportMetadata metadataOf(const KifuDocument& document)
{
    // 画面で入力中の内容も書き出す
    document.commitPendingEdits();
    KifuExportMetadata metadata;
    metadata.gameInfoItems = document.header();
    metadata.startSfen = document.startSfen();
    return metadata;
}

QStringList lines(const KifuBranchTree& tree, const KifuExportMetadata& metadata, KifuFormat format)
{
    return exporterFor(format)(tree, metadata);
}

QStringList lines(const KifuDocument& document, KifuFormat format, KifuEncoding encoding)
{
    KifuExportMetadata metadata = metadataOf(document);
    metadata.encoding = encoding;
    return lines(*document.tree(), metadata, format);
}

Capabilities capabilities(KifuFormat format)
{
    //                 変化   消費時間 コメント しおり
    switch (format) {
    case KifuFormat::Kif: return {true, true, true, true};
    case KifuFormat::Ki2: return {true, false, true, true};
    case KifuFormat::Csa: return {false, true, true, false};
    case KifuFormat::Jkf: return {true, true, true, false};
    case KifuFormat::Usen: return {true, false, false, false};
    case KifuFormat::Usi:
    case KifuFormat::Sfen:
    case KifuFormat::Bod:
        break;
    }
    return {};
}

bool writesVariations(KifuFormat format)
{
    return capabilities(format).variations;
}

bool writesTerminal(KifuFormat format, TerminalType type)
{
    switch (format) {
    case KifuFormat::Kif:
    case KifuFormat::Ki2:
    case KifuFormat::Jkf:
        return true;
    case KifuFormat::Csa:
        // 反則勝ちは相手の反則の符号で書く（記録した側によらず書ける）
        return !TerminalCodes::csaSpecial(type, Turn::Black, false).isEmpty();
    case KifuFormat::Usi:
        return !TerminalCodes::usi(type).isEmpty();
    case KifuFormat::Usen:
        return !TerminalCodes::usen(type).isEmpty();
    case KifuFormat::Sfen:
    case KifuFormat::Bod:
        break;
    }
    return false;
}

Loss lossIn(const KifuBranchTree& tree, KifuFormat format)
{
    const Capabilities can = capabilities(format);
    Loss loss;
    loss.variations = !can.variations && tree.lineCount() > 1;

    QList<KifuBranchNode*> nodes;
    if (can.variations) collectSubtree(tree.root(), nodes);
    else nodes = tree.mainLine();
    for (const KifuBranchNode* node : std::as_const(nodes)) {
        if (!can.times && node->time().isRecorded()) loss.times = true;
        if (!can.comments && !node->comment().trimmed().isEmpty()) loss.comments = true;
        if (!can.bookmarks && !node->bookmark().trimmed().isEmpty()) loss.bookmarks = true;
        if (node->isTerminal() && !writesTerminal(format, node->terminalType())
            && !loss.terminals.contains(node->terminalType())) {
            loss.terminals.append(node->terminalType());
        }
    }
    return loss;
}

} // namespace KifuExport
