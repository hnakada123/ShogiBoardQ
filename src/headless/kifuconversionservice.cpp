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
        result.failure = {ErrorKind::InvalidArgument, QStringLiteral("Specify exactly one of input path or text")};
        return result;
    }
    if (request.outputFormat == KifuFormat::Bod) {
        result.failure = {ErrorKind::InvalidArgument, QStringLiteral("Output format must be a kifu format or sfen")};
        return result;
    }

    // 読み込み（GUI の読み込みと同じ。指し手のない USI・SFEN と BOD は局面だけ）
    KifuLoadResult loaded;
    if (!request.text.isEmpty()) {
        loaded = request.inputFormat ? KifuLoadParser::parseContent(request.text, *request.inputFormat)
                                     : KifuLoadParser::parseText(request.text);
    } else {
        if (!QFileInfo::exists(request.inputPath)) {
            result.failure = {ErrorKind::NotFound, QStringLiteral("Input file does not exist: %1").arg(request.inputPath)};
            return result;
        }
        loaded = KifuLoadParser::parseFile(request.inputPath, request.inputFormat);
    }
    result.inputFormat = loaded.format;
    if (!loaded.warning.trimmed().isEmpty()) result.warnings.append(loaded.warning.trimmed());
    if (!loaded.success) {
        result.failure = {ErrorKind::InvalidContent,
                          loaded.error.isEmpty() ? QStringLiteral("Failed to parse the kifu") : loaded.error};
        return result;
    }
    result.gameInfo = loaded.gameInfo;
    result.hasBranches = !loaded.record.variations.isEmpty();

    // 一時的な分岐ツリーを組み立てて、GUI と同じ書き出しの表で出力する
    KifuBranchTree tree;
    if (!KifuBranchTreeBuilder::buildFromKifParseResult(&tree, loaded.record)) {
        result.failure = {ErrorKind::InvalidContent, QStringLiteral("Invalid variation parent or starting ply")};
        return result;
    }

    result.initialSfen = tree.root() ? tree.root()->sfen() : loaded.startSfen();
    result.sfens = tree.sfenListForLine(0);
    result.mainline = tree.exportLine(0);
    result.usiMoves = tree.isEmpty() ? QStringList() : tree.mainRecordLine().usiMoves();

    if (request.outputFormat == KifuFormat::Sfen) {
        result.lines = result.sfens;
    } else {
        // GUI の保存と同じく、読み込んだ対局情報だけを書く（変換した時刻や仮の対局者名を作らない）
        KifuExportMetadata metadata;
        metadata.gameInfoItems = loaded.gameInfo;
        metadata.startSfen = result.initialSfen;
        metadata.encoding = request.outputEncoding;
        result.lines = KifuExport::lines(tree, metadata, request.outputFormat);
    }
    result.ok = true;
    return result;
}
