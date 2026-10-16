/// @file kifuconversionservice.cpp
/// @brief 棋譜変換サービスの実装

#include "kifuconversionservice.h"

#include "gameinfokeys.h"
#include "gamerecordmodel.h"
#include "kifubranchtree.h"
#include "kifubranchtreebuilder.h"
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
        loaded.record.mainline.baseSfen = loaded.positionOnly;
    }
    result.gameInfo = loaded.gameInfo;
    result.hasBranches = !loaded.record.variations.isEmpty();

    // 一時的な分岐ツリーと棋譜モデルを組み立てて既存のエクスポータで出力する
    KifuBranchTree tree;
    if (!KifuBranchTreeBuilder::buildFromKifParseResult(&tree, loaded.record, loaded.initialSfen)) {
        result.error = QStringLiteral("Invalid variation parent or starting ply");
        return result;
    }
    GameRecordModel model;
    model.setBranchTree(&tree);

    result.initialSfen = model.initialSfenForExport(loaded.initialSfen);
    result.sfens = tree.sfenListForLine(0);
    result.moves = model.collectMainlineForExport();
    result.usiMoves = model.collectMainlineUsiForExport();

    GameRecordModel::ExportContext ctx;
    ctx.gameInfoItems = loaded.gameInfo;
    ctx.startSfen = result.initialSfen;
    for (const auto& item : std::as_const(loaded.gameInfo)) {
        if (item.key == GameInfoKeys::kBlackPlayer || item.key == GameInfoKeys::kShitatePlayer)
            ctx.players.blackHumanName = item.value;
        if (item.key == GameInfoKeys::kWhitePlayer || item.key == GameInfoKeys::kUwatePlayer)
            ctx.players.whiteHumanName = item.value;
    }

    switch (request.outputFormat) {
    case KifuFormat::Kif: result.lines = model.toKifLines(ctx); break;
    case KifuFormat::Ki2: result.lines = model.toKi2Lines(ctx); break;
    case KifuFormat::Csa: result.lines = model.toCsaLines(ctx); break;
    case KifuFormat::Jkf: result.lines = model.toJkfLines(ctx); break;
    case KifuFormat::Usi: result.lines = model.toUsiLines(ctx); break;
    case KifuFormat::Usen: result.lines = model.toUsenLines(ctx); break;
    case KifuFormat::Sfen: result.lines = result.sfens; break;
    case KifuFormat::Bod: break;
    }
    result.ok = true;
    return result;
}
