/// @file kifuconversionservice.cpp
/// @brief 棋譜変換サービスの実装

#include "kifuconversionservice.h"

#include "kifubranchnode.h"
#include "kifubranchtree.h"
#include "kifubranchtreebuilder.h"
#include "kifuexport.h"
#include "kifuloadparser.h"

#include <QFileInfo>

KifuConversionService::Result KifuConversionService::convert(const Request& request)
{
    Result result;
    if (request.inputPath.isEmpty() == request.text.isEmpty()) {
        result.error = QStringLiteral("Specify exactly one of input path or text");
        return result;
    }
    if (request.outputFormat == KifuFormat::Bod) {
        result.error = QStringLiteral("Output format must be a kifu format or sfen");
        return result;
    }

    // 読み込み（GUI の読み込みと同じ。指し手のない USI・SFEN と BOD は局面だけ）
    KifuLoadResult loaded;
    if (!request.text.isEmpty()) {
        loaded = request.inputFormat ? KifuLoadParser::parseContent(request.text, *request.inputFormat)
                                     : KifuLoadParser::parseText(request.text);
    } else {
        if (!QFileInfo::exists(request.inputPath)) {
            result.error = QStringLiteral("Input file does not exist: %1").arg(request.inputPath);
            return result;
        }
        loaded = KifuLoadParser::parseFile(request.inputPath, request.inputFormat);
    }
    result.inputFormat = loaded.format;
    if (!loaded.warning.trimmed().isEmpty()) result.warnings.append(loaded.warning.trimmed());
    if (!loaded.success) {
        result.error = loaded.error.isEmpty() ? QStringLiteral("Failed to parse the kifu") : loaded.error;
        return result;
    }
    if (!loaded.positionOnly.isEmpty()) {
        loaded.initialSfen = loaded.positionOnly;
        loaded.record.mainline.startSfen = loaded.positionOnly;
    }
    result.gameInfo = loaded.gameInfo;
    result.hasBranches = !loaded.record.variations.isEmpty();

    // 一時的な分岐ツリーを組み立てて、GUI と同じ書き出しの表で出力する
    KifuBranchTree tree;
    if (!KifuBranchTreeBuilder::buildFromKifParseResult(&tree, loaded.record, loaded.initialSfen)) {
        result.error = QStringLiteral("Invalid variation parent or starting ply");
        return result;
    }

    result.initialSfen = tree.root() ? tree.root()->sfen() : loaded.initialSfen;
    result.sfens = tree.sfenListForLine(0);
    result.mainline = tree.exportLine(0);
    result.usiMoves = tree.isEmpty() ? QStringList() : tree.mainRecordLine().usiMoves();

    if (request.outputFormat == KifuFormat::Sfen) {
        result.lines = result.sfens;
    } else {
        KifuExportMetadata metadata;
        metadata.gameInfoItems = loaded.gameInfo;
        metadata.startSfen = result.initialSfen;
        metadata.players.blackHumanName = KifuExportMetadataBuilder::playerName(loaded.gameInfo, Turn::Black);
        metadata.players.whiteHumanName = KifuExportMetadataBuilder::playerName(loaded.gameInfo, Turn::White);
        result.lines = KifuExport::lines(tree, metadata, request.outputFormat);
    }
    result.ok = true;
    return result;
}
