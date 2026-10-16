/// @file clicommands_basic.cpp
/// @brief shogiboardq-cli の即時応答コマンド（version / list-engines / validate-sfen / convert-kifu / render-board）

#include "clicommands.h"

#include "applicationfonts.h"
#include "applicationtranslations.h"
#include "appsettings.h"
#include "boardimagerenderer.h"
#include "cliargs.h"
#include "clioutput.h"
#include "enginecatalog.h"
#include "kifuconversionservice.h"
#include "kifusavefile.h"
#include "sfenvalidationservice.h"

#include <QCommandLineParser>
#include <QLatin1StringView>
#include <QCoreApplication>
#include <QJsonArray>

namespace {

constexpr QLatin1StringView kInvalidArgument("invalid_argument");

void configureParser(QCommandLineParser& parser, const QString& command)
{
    parser.setApplicationDescription(QStringLiteral("shogiboardq-cli ") + command);
    parser.setSingleDashWordOptionMode(QCommandLineParser::ParseAsLongOptions);
}

/// パース失敗時は使い方の誤りとして終了コードを返す
bool parseOrFail(QCommandLineParser& parser, const QStringList& args, int& exitCode)
{
    if (!parser.parse(QStringList{QStringLiteral("shogiboardq-cli")} + args)) {
        exitCode = CliOutput::fail(kInvalidArgument, parser.errorText());
        return false;
    }
    return true;
}

QJsonArray gameInfoToJson(const QList<KifGameInfoItem>& items)
{
    QJsonArray array;
    for (const auto& item : items) {
        QJsonObject obj;
        obj[QStringLiteral("key")] = item.key;
        obj[QStringLiteral("value")] = item.value;
        array.append(obj);
    }
    return array;
}

/// 本譜の各行（指し手と終局）。指し手の行には USI の指し手を添える
QJsonArray movesToJson(const RecordLine& line)
{
    QJsonArray array;
    for (qsizetype i = 0; i < line.entries.size(); ++i) {
        const RecordEntry& entry = line.entries.at(i);
        QJsonObject obj;
        obj[QStringLiteral("ply")] = line.startPly + static_cast<int>(i);
        obj[QStringLiteral("text")] = entry.displayText;
        if (!entry.isTerminal()) obj[QStringLiteral("usi")] = entry.move.move.toString();
        if (const QString time = entry.time.toText(); !time.isEmpty()) obj[QStringLiteral("time")] = time;
        if (!entry.comment.isEmpty()) obj[QStringLiteral("comment")] = entry.comment;
        if (!entry.bookmark.isEmpty()) obj[QStringLiteral("bookmark")] = entry.bookmark;
        array.append(obj);
    }
    return array;
}

} // namespace

int CliCommands::version(const QStringList& args)
{
    Q_UNUSED(args)
    QJsonObject obj;
    obj[QStringLiteral("ok")] = true;
    obj[QStringLiteral("version")] = QCoreApplication::applicationVersion();
    obj[QStringLiteral("qt")] = QString::fromLatin1(qVersion());
    CliOutput::printJson(obj);
    return 0;
}

int CliCommands::listEngines(const QStringList& args)
{
    Q_UNUSED(args)
    QJsonObject obj;
    obj[QStringLiteral("ok")] = true;
    obj[QStringLiteral("engines")] = EngineCatalog::toJson();
    CliOutput::printJson(obj);
    return 0;
}

int CliCommands::validateSfen(const QStringList& args)
{
    QCommandLineParser parser;
    configureParser(parser, QStringLiteral("validate-sfen"));
    parser.addOption({QStringLiteral("sfen"), QStringLiteral("SFEN to validate."), QStringLiteral("sfen")});
    int exitCode = 0;
    if (!parseOrFail(parser, args, exitCode)) return exitCode;
    if (!parser.isSet(QStringLiteral("sfen"))) return CliOutput::fail(kInvalidArgument, QStringLiteral("--sfen is required"));

    QJsonObject obj = SfenValidationService::validate(parser.value(QStringLiteral("sfen"))).toJson();
    obj[QStringLiteral("ok")] = true;
    CliOutput::printJson(obj);
    return 0;
}

int CliCommands::convertKifu(const QStringList& args)
{
    QCommandLineParser parser;
    configureParser(parser, QStringLiteral("convert-kifu"));
    parser.addOption({QStringLiteral("input"), QStringLiteral("Input kifu file."), QStringLiteral("path")});
    parser.addOption({QStringLiteral("text"), QStringLiteral("Input kifu text."), QStringLiteral("text")});
    parser.addOption({QStringLiteral("input-format"), QStringLiteral("auto|kif|ki2|csa|jkf|usi|usen"),
                      QStringLiteral("format"), QStringLiteral("auto")});
    parser.addOption({QStringLiteral("output-format"), QStringLiteral("kif|ki2|csa|jkf|usi|usen|sfen"),
                      QStringLiteral("format")});
    parser.addOption({QStringLiteral("output"), QStringLiteral("Write the result to this file."), QStringLiteral("path")});
    parser.addOption({QStringLiteral("overwrite"), QStringLiteral("Allow replacing an existing output file.")});
    int exitCode = 0;
    if (!parseOrFail(parser, args, exitCode)) return exitCode;

    KifuConversionService::Request request;
    request.inputPath = parser.value(QStringLiteral("input"));
    request.text = parser.value(QStringLiteral("text"));
    const QString inputName = parser.value(QStringLiteral("input-format"));
    if (inputName.compare(QLatin1String("auto"), Qt::CaseInsensitive) != 0) {
        request.inputFormat = KifuFormats::fromName(inputName);
        if (!request.inputFormat) return CliOutput::fail(kInvalidArgument, QStringLiteral("Unknown --input-format"));
    }
    const auto outputFormat = KifuFormats::fromName(parser.value(QStringLiteral("output-format")));
    if (!outputFormat || *outputFormat == KifuFormat::Bod) {
        return CliOutput::fail(kInvalidArgument, QStringLiteral("--output-format must be one of kif, ki2, csa, jkf, usi, usen, sfen"));
    }
    request.outputFormat = *outputFormat;

    const QString outputPath = parser.value(QStringLiteral("output"));
    QString error;
    if (!outputPath.isEmpty() && !CliArgs::checkOutputPath(outputPath, parser.isSet(QStringLiteral("overwrite")), &error)) {
        return CliOutput::fail(QStringLiteral("io_error"), error);
    }

    const KifuConversionService::Result result = KifuConversionService::convert(request);
    if (!result.ok) return CliOutput::fail(QStringLiteral("parse_error"), result.error);

    QJsonObject obj;
    obj[QStringLiteral("ok")] = true;
    obj[QStringLiteral("input_format")] = KifuFormats::name(result.inputFormat);
    obj[QStringLiteral("output_format")] = KifuFormats::name(request.outputFormat);
    obj[QStringLiteral("initial_sfen")] = result.initialSfen;
    obj[QStringLiteral("ply_count")] = static_cast<int>(result.usiMoves.size());
    obj[QStringLiteral("usi_moves")] = QJsonArray::fromStringList(result.usiMoves);
    obj[QStringLiteral("sfens")] = QJsonArray::fromStringList(result.sfens);
    obj[QStringLiteral("moves")] = movesToJson(result.mainline);
    obj[QStringLiteral("game_info")] = gameInfoToJson(result.gameInfo);
    obj[QStringLiteral("has_branches")] = result.hasBranches;
    obj[QStringLiteral("warnings")] = QJsonArray::fromStringList(result.warnings);
    obj[QStringLiteral("line_count")] = static_cast<int>(result.lines.size());

    if (outputPath.isEmpty()) {
        obj[QStringLiteral("text")] = result.lines.join(QLatin1Char('\n')) + QLatin1Char('\n');
    } else {
        if (!KifuSaveFile::write(outputPath, result.lines, &error)) {
            return CliOutput::fail(QStringLiteral("io_error"), error);
        }
        obj[QStringLiteral("output_path")] = outputPath;
    }
    CliOutput::printJson(obj);
    return 0;
}

int CliCommands::renderBoard(const QStringList& args)
{
    QCommandLineParser parser;
    configureParser(parser, QStringLiteral("render-board"));
    parser.addOption({QStringLiteral("sfen"), QStringLiteral("Position to draw (SFEN or startpos)."), QStringLiteral("sfen"),
                      QStringLiteral("startpos")});
    parser.addOption({QStringLiteral("output"), QStringLiteral("Output image path (.png recommended)."), QStringLiteral("path")});
    parser.addOption({QStringLiteral("square-size"), QStringLiteral("Square width in pixels (20-150)."), QStringLiteral("px"),
                      QStringLiteral("50")});
    parser.addOption({QStringLiteral("flip"), QStringLiteral("View from the white (gote) side.")});
    parser.addOption({QStringLiteral("last-move"), QStringLiteral("USI move to highlight."), QStringLiteral("move")});
    parser.addOption({QStringLiteral("black-name"), QStringLiteral("Name shown for black (sente)."), QStringLiteral("name")});
    parser.addOption({QStringLiteral("white-name"), QStringLiteral("Name shown for white (gote)."), QStringLiteral("name")});
    parser.addOption({QStringLiteral("overwrite"), QStringLiteral("Allow replacing an existing output file.")});
    int exitCode = 0;
    if (!parseOrFail(parser, args, exitCode)) return exitCode;

    QString error;
    const QString outputPath = parser.value(QStringLiteral("output"));
    if (!CliArgs::checkOutputPath(outputPath, parser.isSet(QStringLiteral("overwrite")), &error)) {
        return CliOutput::fail(QStringLiteral("io_error"), error);
    }
    const auto squareSize = CliArgs::intOption(parser, QStringLiteral("square-size"), 50, 20, 150, &error);
    if (!squareSize) return CliOutput::fail(kInvalidArgument, error);

    BoardImageRenderer::Options options;
    options.squareSize = *squareSize;
    options.flip = parser.isSet(QStringLiteral("flip"));
    options.lastMoveUsi = parser.value(QStringLiteral("last-move"));
    options.blackName = parser.value(QStringLiteral("black-name"));
    options.whiteName = parser.value(QStringLiteral("white-name"));

    // 「手番」などの表示・段の表記（一〜九／a〜i）・書体を、GUI と同じ表示言語と棋譜表記の設定に合わせる。
    // ほかのコマンドの出力（JSON・棋譜の形式）は言語に依存させないため、盤面描画のときだけ適用する。
    const ApplicationTranslations translations;
    ApplicationFonts::initialize(AppSettings::uiFontFamily(), translations.language());

    QSize size;
    if (!BoardImageRenderer::renderToFile(parser.value(QStringLiteral("sfen")), outputPath, options, &error, &size)) {
        return CliOutput::fail(QStringLiteral("render_error"), error);
    }
    QJsonObject obj;
    obj[QStringLiteral("ok")] = true;
    obj[QStringLiteral("output_path")] = outputPath;
    obj[QStringLiteral("width")] = size.width();
    obj[QStringLiteral("height")] = size.height();
    CliOutput::printJson(obj);
    return 0;
}
