/// @file clicommands_basic.cpp
/// @brief shogiboardq-cli の即時応答コマンド（version / list-engines / validate-sfen / convert-kifu / render-board）

#include "clicommands.h"

#include "applicationfonts.h"
#include "applicationtranslations.h"
#include "appsettings.h"
#include "automationmethodspecs.h"
#include "automationserver.h"
#include "settingscommon.h"
#include "clicommandspecs.h"
#include "boardimagerenderer.h"
#include "cliargs.h"
#include "clioutput.h"
#include "enginecatalog.h"
#include "kifuconversionservice.h"
#include "kifusavefile.h"
#include "outputpath.h"
#include "sfenvalidationservice.h"

#include <QCoreApplication>
#include <QJsonArray>

namespace {


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
    int exitCode = 0;
    if (!CliArgs::parse(QStringLiteral("version"), args, false, &exitCode)) return exitCode;
    QJsonObject obj;
    obj[QStringLiteral("ok")] = true;
    obj[QStringLiteral("version")] = QCoreApplication::applicationVersion();
    obj[QStringLiteral("qt")] = QString::fromLatin1(qVersion());
    CliOutput::printJson(obj);
    return 0;
}

int CliCommands::describe(const QStringList& args)
{
    int exitCode = 0;
    if (!CliArgs::parse(QStringLiteral("describe"), args, false, &exitCode)) return exitCode;
    QJsonArray commands;
    for (const OperationSpec& command : CliCommandSpecs::all()) commands.append(ParamSpecs::toJson(command));
    QJsonArray methods;
    for (const OperationSpec& method : AutomationMethodSpecs::all()) methods.append(ParamSpecs::toJson(method));
    QJsonObject obj;
    obj[QStringLiteral("ok")] = true;
    obj[QStringLiteral("commands")] = commands;
    obj[QStringLiteral("methods")] = methods;
    CliOutput::printJson(obj);
    return 0;
}

int CliCommands::environment(const QStringList& args)
{
    int exitCode = 0;
    if (!CliArgs::parse(QStringLiteral("environment"), args, false, &exitCode)) return exitCode;
    // MCP サーバーが ShogiBoardQ の値を写さずに済むよう、アプリと同じ計算で場所と上限を出す
    QJsonObject obj;
    obj[QStringLiteral("ok")] = true;
    obj[QStringLiteral("version")] = QCoreApplication::applicationVersion();
    obj[QStringLiteral("socket_path")] = AutomationServer::defaultSocketPath();
    obj[QStringLiteral("endpoint_file")] = AutomationServer::endpointFilePath();
    obj[QStringLiteral("settings_file")] = SettingsCommon::settingsFilePath();
    obj[QStringLiteral("max_message_bytes")] = static_cast<double>(AutomationServer::kMaxMessageBytes);
    obj[QStringLiteral("error_kinds")] = ErrorKinds::toJson();
    CliOutput::printJson(obj);
    return 0;
}

int CliCommands::listEngines(const QStringList& args)
{
    int exitCode = 0;
    if (!CliArgs::parse(QStringLiteral("list-engines"), args, false, &exitCode)) return exitCode;
    QJsonObject obj;
    obj[QStringLiteral("ok")] = true;
    obj[QStringLiteral("engines")] = EngineCatalog::toJson();
    CliOutput::printJson(obj);
    return 0;
}

int CliCommands::validateSfen(const QStringList& args)
{
    int exitCode = 0;
    const auto params = CliArgs::parse(QStringLiteral("validate-sfen"), args, false, &exitCode);
    if (!params) return exitCode;

    QJsonObject obj = SfenValidationService::validate(params->value(QStringLiteral("sfen")).toString()).toJson();
    obj[QStringLiteral("ok")] = true;
    CliOutput::printJson(obj);
    return 0;
}

int CliCommands::convertKifu(const QStringList& args)
{
    int exitCode = 0;
    const auto params = CliArgs::parse(QStringLiteral("convert-kifu"), args, false, &exitCode);
    if (!params) return exitCode;

    // 形式の名前は表の選択肢で検査済み
    KifuConversionService::Request request;
    request.inputPath = params->value(QStringLiteral("input")).toString();
    request.text = params->value(QStringLiteral("text")).toString();
    const QString inputName = params->value(QStringLiteral("input_format")).toString();
    if (inputName != QLatin1String("auto")) request.inputFormat = KifuFormats::fromName(inputName);
    request.outputFormat = *KifuFormats::fromName(params->value(QStringLiteral("output_format")).toString());

    const QString outputPath = params->value(QStringLiteral("output")).toString();
    if (const std::optional<Failure> problem = outputPath.isEmpty() ? std::nullopt
            : OutputPath::check(outputPath, params->value(QStringLiteral("overwrite")).toBool(), QStringLiteral("--overwrite"))) {
        return CliOutput::fail(*problem);
    }

    const KifuConversionService::Result result = KifuConversionService::convert(request);
    if (!result.ok) return CliOutput::fail(result.failure);

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
        QString error;
        if (!KifuSaveFile::write(outputPath, result.lines, &error)) {
            return CliOutput::fail(ErrorKind::FileError, error);
        }
        obj[QStringLiteral("output_path")] = outputPath;
    }
    CliOutput::printJson(obj);
    return 0;
}

int CliCommands::renderBoard(const QStringList& args)
{
    int exitCode = 0;
    const auto params = CliArgs::parse(QStringLiteral("render-board"), args, false, &exitCode);
    if (!params) return exitCode;

    const QString outputPath = params->value(QStringLiteral("output")).toString();
    if (const std::optional<Failure> problem =
            OutputPath::check(outputPath, params->value(QStringLiteral("overwrite")).toBool(), QStringLiteral("--overwrite"))) {
        return CliOutput::fail(*problem);
    }

    BoardImageRenderer::Options options;
    options.squareSize = params->value(QStringLiteral("square_size")).toInt();
    options.flip = params->value(QStringLiteral("flip")).toBool();
    options.lastMoveUsi = params->value(QStringLiteral("last_move")).toString();
    options.blackName = params->value(QStringLiteral("black_name")).toString();
    options.whiteName = params->value(QStringLiteral("white_name")).toString();

    // 「手番」などの表示・段の表記（一〜九／a〜i）・書体を、GUI と同じ表示言語と棋譜表記の設定に合わせる。
    // ほかのコマンドの出力（JSON・棋譜の形式）は言語に依存させないため、盤面描画のときだけ適用する。
    const ApplicationTranslations translations;
    ApplicationFonts::initialize(AppSettings::uiFontFamily(), translations.language());

    QSize size;
    Failure failure;
    if (!BoardImageRenderer::renderToFile(params->value(QStringLiteral("sfen")).toString(), outputPath, options, &failure, &size)) {
        return CliOutput::fail(failure);
    }
    QJsonObject obj;
    obj[QStringLiteral("ok")] = true;
    obj[QStringLiteral("output_path")] = outputPath;
    obj[QStringLiteral("width")] = size.width();
    obj[QStringLiteral("height")] = size.height();
    CliOutput::printJson(obj);
    return 0;
}
