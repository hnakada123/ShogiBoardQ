/// @file kifuconversionservice.cpp
/// @brief 棋譜変換サービスの実装

#include "kifuconversionservice.h"

#include "csatosfenconverter.h"
#include "gamerecordmodel.h"
#include "jkftosfenconverter.h"
#include "ki2tosfenconverter.h"
#include "kifreader.h"
#include "kiftosfenconverter.h"
#include "kifubranchtree.h"
#include "kifubranchtreebuilder.h"
#include "kifufilereader.h"
#include "sfenpositiontracer.h"
#include "sfenutils.h"
#include "usentosfenconverter.h"
#include "usitosfenconverter.h"

#include <QFileInfo>
#include <QTemporaryFile>

using Format = KifuConversionService::Format;

std::optional<Format> KifuConversionService::parseFormat(const QString& name)
{
    const QString n = name.trimmed().toLower();
    if (n == QLatin1String("auto")) return Format::Auto;
    if (n == QLatin1String("kif") || n == QLatin1String("kifu")) return Format::Kif;
    if (n == QLatin1String("ki2")) return Format::Ki2;
    if (n == QLatin1String("csa")) return Format::Csa;
    if (n == QLatin1String("jkf")) return Format::Jkf;
    if (n == QLatin1String("usi")) return Format::Usi;
    if (n == QLatin1String("usen")) return Format::Usen;
    if (n == QLatin1String("sfen")) return Format::Sfen;
    return std::nullopt;
}

QString KifuConversionService::formatName(Format format)
{
    switch (format) {
    case Format::Auto: return QStringLiteral("auto");
    case Format::Kif: return QStringLiteral("kif");
    case Format::Ki2: return QStringLiteral("ki2");
    case Format::Csa: return QStringLiteral("csa");
    case Format::Jkf: return QStringLiteral("jkf");
    case Format::Usi: return QStringLiteral("usi");
    case Format::Usen: return QStringLiteral("usen");
    case Format::Sfen: return QStringLiteral("sfen");
    }
    return QStringLiteral("auto");
}

Format KifuConversionService::formatFromPath(const QString& path)
{
    const QString ext = QFileInfo(path).suffix().toLower();
    if (ext == QLatin1String("kif") || ext == QLatin1String("kifu")) return Format::Kif;
    if (ext == QLatin1String("ki2") || ext == QLatin1String("ki2u")) return Format::Ki2;
    if (ext == QLatin1String("csa")) return Format::Csa;
    if (ext == QLatin1String("jkf")) return Format::Jkf;
    if (ext == QLatin1String("usi") || ext == QLatin1String("sfen")) return Format::Usi;
    if (ext == QLatin1String("usen")) return Format::Usen;
    return Format::Auto;
}

Format KifuConversionService::detectFormatFromContent(const QString& content)
{
    switch (KifuFileReader::detectFormat(content)) {
    case KifuFileReader::KifuFormat::KIF: return Format::Kif;
    case KifuFileReader::KifuFormat::KI2: return Format::Ki2;
    case KifuFileReader::KifuFormat::CSA: return Format::Csa;
    case KifuFileReader::KifuFormat::JKF: return Format::Jkf;
    case KifuFileReader::KifuFormat::USEN: return Format::Usen;
    case KifuFileReader::KifuFormat::USI:
    case KifuFileReader::KifuFormat::SFEN:
        return Format::Usi;
    case KifuFileReader::KifuFormat::BOD:
    case KifuFileReader::KifuFormat::Unknown:
        break;
    }
    return Format::Kif;
}

void KifuConversionService::completeLine(KifLine& line, const QString& baseSfen)
{
    const QString base = line.baseSfen.isEmpty() ? baseSfen : line.baseSfen;
    if (line.baseSfen.isEmpty()) line.baseSfen = base;
    if (line.usiMoves.isEmpty()) return;
    if (line.sfenList.isEmpty()) {
        line.sfenList = SfenPositionTracer::buildSfenRecord(base, line.usiMoves, line.endsWithTerminal);
    }
    if (line.gameMoves.isEmpty()) {
        line.gameMoves = SfenPositionTracer::buildGameMoves(base, line.usiMoves);
    }
}

bool KifuConversionService::parseFile(const QString& path, Format format, Parsed& out, QString* error)
{
    QString warn;
    QString label;
    bool ok = false;
    switch (format) {
    case Format::Kif:
        out.initialSfen = KifToSfenConverter::detectInitialSfenFromFile(path, &label);
        ok = KifToSfenConverter::parseWithVariations(path, out.result, &warn);
        out.gameInfo = KifToSfenConverter::extractGameInfo(path);
        break;
    case Format::Ki2:
        out.initialSfen = Ki2ToSfenConverter::detectInitialSfenFromFile(path, &label);
        ok = Ki2ToSfenConverter::parseWithVariations(path, out.result, &warn);
        out.gameInfo = Ki2ToSfenConverter::extractGameInfo(path);
        break;
    case Format::Csa:
        ok = CsaToSfenConverter::parse(path, out.result, &warn);
        out.initialSfen = out.result.mainline.baseSfen;
        out.gameInfo = CsaToSfenConverter::extractGameInfo(path);
        break;
    case Format::Jkf:
        out.initialSfen = JkfToSfenConverter::detectInitialSfenFromFile(path, &label);
        ok = JkfToSfenConverter::parseWithVariations(path, out.result, &warn);
        out.gameInfo = JkfToSfenConverter::extractGameInfo(path);
        break;
    case Format::Usen:
        out.initialSfen = UsenToSfenConverter::detectInitialSfenFromFile(path, &label);
        ok = UsenToSfenConverter::parseWithVariations(path, out.result, &warn);
        out.gameInfo = UsenToSfenConverter::extractGameInfo(path);
        break;
    case Format::Usi:
    case Format::Auto:
    case Format::Sfen:
        out.initialSfen = UsiToSfenConverter::detectInitialSfenFromFile(path, &label);
        ok = UsiToSfenConverter::parseWithVariations(path, out.result, &warn);
        out.gameInfo = UsiToSfenConverter::extractGameInfo(path);
        break;
    }
    if (!warn.trimmed().isEmpty()) out.warnings.append(warn.trimmed());
    if (!ok) {
        if (error) *error = warn.trimmed().isEmpty() ? QStringLiteral("Failed to parse the kifu") : warn.trimmed();
        return false;
    }
    if (out.initialSfen.isEmpty()) {
        out.initialSfen = out.result.mainline.baseSfen.isEmpty() ? SfenUtils::hirateSfen()
                                                                  : out.result.mainline.baseSfen;
    }
    return true;
}

KifuConversionService::Result KifuConversionService::convert(const Request& request)
{
    Result result;
    if (request.inputPath.isEmpty() == request.text.isEmpty()) {
        result.error = QStringLiteral("Specify exactly one of input path or text");
        return result;
    }
    if (request.outputFormat == Format::Auto) {
        result.error = QStringLiteral("Output format must be explicit");
        return result;
    }

    // 入力形式の決定と、文字列入力の一時ファイル化
    Format inputFormat = request.inputFormat;
    QString path = request.inputPath;
    std::unique_ptr<QTemporaryFile> temp;
    if (!request.text.isEmpty()) {
        QString content = request.text;
        if (KifuFileReader::detectFormat(content) == KifuFileReader::KifuFormat::BOD) {
            QString sfen;
            QString warn;
            if (!KifToSfenConverter::buildInitialSfenFromBod(content.split(QLatin1Char('\n')), sfen, nullptr, &warn)) {
                result.error = QStringLiteral("Failed to parse BOD text: %1").arg(warn);
                return result;
            }
            content = QStringLiteral("position sfen ") + sfen;
            inputFormat = Format::Usi;
        }
        if (inputFormat == Format::Auto) inputFormat = detectFormatFromContent(content);
        const auto fmt = inputFormat == Format::Csa ? KifuFileReader::KifuFormat::CSA
                       : inputFormat == Format::Ki2 ? KifuFileReader::KifuFormat::KI2
                       : inputFormat == Format::Jkf ? KifuFileReader::KifuFormat::JKF
                       : inputFormat == Format::Usen ? KifuFileReader::KifuFormat::USEN
                       : inputFormat == Format::Usi ? KifuFileReader::KifuFormat::USI
                                                    : KifuFileReader::KifuFormat::KIF;
        temp = KifuFileReader::createTempFile(fmt, content);
        if (!temp) {
            result.error = QStringLiteral("Failed to create a temporary file");
            return result;
        }
        path = temp->fileName();
    } else {
        if (!QFileInfo::exists(path)) {
            result.error = QStringLiteral("Input file does not exist: %1").arg(path);
            return result;
        }
        if (inputFormat == Format::Auto) inputFormat = formatFromPath(path);
        if (inputFormat == Format::Auto) {
            QStringList lines;
            QString warn;
            if (!KifReader::readAllLinesAuto(path, lines, nullptr, &warn)) {
                result.error = QStringLiteral("Failed to read %1: %2").arg(path, warn);
                return result;
            }
            inputFormat = detectFormatFromContent(lines.join(QLatin1Char('\n')));
        }
    }
    result.inputFormat = inputFormat;

    Parsed parsed;
    QString error;
    if (!parseFile(path, inputFormat, parsed, &error)) {
        result.error = error;
        result.warnings = parsed.warnings;
        return result;
    }
    result.warnings = parsed.warnings;
    result.gameInfo = parsed.gameInfo;
    result.hasBranches = !parsed.result.variations.isEmpty();

    // CSA など一部の形式は各手後の SFEN と指し手データを埋めない。ツリー構築前に補完する
    completeLine(parsed.result.mainline, parsed.initialSfen);
    for (KifVariation& variation : parsed.result.variations) {
        completeLine(variation.line, variation.line.baseSfen);
    }

    // 一時的な分岐ツリーと棋譜モデルを組み立てて既存のエクスポータで出力する
    KifuBranchTree tree;
    KifuBranchTreeBuilder::buildFromKifParseResult(&tree, parsed.result, parsed.initialSfen);
    GameRecordModel model;
    model.setBranchTree(&tree);

    result.initialSfen = model.initialSfenForExport(parsed.initialSfen);
    result.sfens = tree.sfenListForLine(0);
    result.moves = model.collectMainlineForExport();
    result.usiMoves = model.collectMainlineUsiForExport();

    GameRecordModel::ExportContext ctx;
    ctx.gameInfoItems = parsed.gameInfo;
    ctx.startSfen = result.initialSfen;
    ctx.playMode = PlayMode::HumanVsHuman;
    for (const auto& item : std::as_const(parsed.gameInfo)) {
        if (item.key == QStringLiteral("先手") || item.key == QStringLiteral("下手")) ctx.human1 = item.value;
        if (item.key == QStringLiteral("後手") || item.key == QStringLiteral("上手")) ctx.human2 = item.value;
    }

    switch (request.outputFormat) {
    case Format::Kif: result.lines = model.toKifLines(ctx); break;
    case Format::Ki2: result.lines = model.toKi2Lines(ctx); break;
    case Format::Csa: result.lines = model.toCsaLines(ctx, result.usiMoves); break;
    case Format::Jkf: result.lines = model.toJkfLines(ctx); break;
    case Format::Usi: result.lines = model.toUsiLines(ctx, result.usiMoves); break;
    case Format::Usen: result.lines = model.toUsenLines(ctx, result.usiMoves); break;
    case Format::Sfen: result.lines = result.sfens; break;
    case Format::Auto: break;
    }
    result.ok = true;
    return result;
}
