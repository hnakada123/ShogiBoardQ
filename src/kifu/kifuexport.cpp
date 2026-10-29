/// @file kifuexport.cpp
/// @brief 棋譜の書き出し（形式ごとの書き出し関数の表）の実装

#include "kifuexport.h"

#include "csaexporter.h"
#include "jkfexporter.h"
#include "ki2exporter.h"
#include "kifexporter.h"
#include "kifudocument.h"
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

} // namespace

namespace KifuExport {

KifuExportMetadata metadataOf(const KifuDocument& document)
{
    // 画面で入力中の内容も書き出す
    document.commitPendingEdits();
    KifuExportMetadata metadata;
    metadata.gameInfoItems = document.header();
    metadata.startSfen = document.startSfen();
    metadata.players = document.players();
    return metadata;
}

QStringList lines(const KifuBranchTree& tree, const KifuExportMetadata& metadata, KifuFormat format)
{
    return exporterFor(format)(tree, metadata);
}

QStringList lines(const KifuDocument& document, KifuFormat format)
{
    return lines(*document.tree(), metadataOf(document), format);
}

} // namespace KifuExport
