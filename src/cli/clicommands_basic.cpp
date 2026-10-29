/// @file clicommands_basic.cpp
/// @brief shogiboardq-cli の即時応答コマンド（version / list-engines / validate-sfen / convert-kifu / render-board）

#include "clicommands.h"

#include "recordjson.h"
#include "applicationfonts.h"
#include "applicationtranslations.h"
#include "appsettings.h"
#include "automationmethodspecs.h"
#include "automationendpoint.h"
#include "settingscommon.h"
#include "clicommandspecs.h"
#include "boardimagerenderer.h"
#include "cliargs.h"
#include "clioutput.h"
#include "enginecatalog.h"
#include "kifuconversionservice.h"
#include "kifusavefile.h"
#include "outputpath.h"
#include "sfenvalidationjson.h"
#include "sfenvalidator.h"

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

} // namespace

int CliCommands::version(const QJsonObject& /*params*/)
{
    QJsonObject obj;
    obj[QStringLiteral("ok")] = true;
    obj[QStringLiteral("version")] = QCoreApplication::applicationVersion();
    obj[QStringLiteral("qt")] = QString::fromLatin1(qVersion());
    CliOutput::printJson(obj);
    return 0;
}

int CliCommands::describe(const QJsonObject& /*params*/)
{
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

int CliCommands::environment(const QJsonObject& /*params*/)
{
    // MCP サーバーが ShogiBoardQ の値を写さずに済むよう、アプリと同じ計算で場所と上限を出す
    QJsonObject obj;
    obj[QStringLiteral("ok")] = true;
    obj[QStringLiteral("version")] = QCoreApplication::applicationVersion();
    obj[QStringLiteral("socket_path")] = AutomationEndpoint::defaultSocketPath();
    obj[QStringLiteral("endpoint_file")] = AutomationEndpoint::endpointFilePath();
    obj[QStringLiteral("settings_file")] = SettingsCommon::settingsFilePath();
    obj[QStringLiteral("max_message_bytes")] = static_cast<double>(AutomationEndpoint::kMaxMessageBytes);
    obj[QStringLiteral("error_kinds")] = ErrorKinds::toJson();
    // MCP サーバーが読み書きを許すファイルの拡張子
    obj[QStringLiteral("kifu_read_suffixes")] = QJsonArray::fromStringList(KifuFormats::readableSuffixes());
    obj[QStringLiteral("kifu_write_suffixes")] = QJsonArray::fromStringList(KifuFormats::writableSuffixes());
    obj[QStringLiteral("image_write_suffixes")] = QJsonArray::fromStringList(BoardImageRenderer::fileSuffixes());
    CliOutput::printJson(obj);
    return 0;
}

int CliCommands::listEngines(const QJsonObject& /*params*/)
{
    QJsonObject obj;
    obj[QStringLiteral("ok")] = true;
    obj[QStringLiteral("engines")] = EngineCatalog::toJson();
    CliOutput::printJson(obj);
    return 0;
}

int CliCommands::validateSfen(const QJsonObject& params)
{

    QJsonObject obj = SfenValidationJson::toJson(SfenValidator::validate(params.value(QStringLiteral("sfen")).toString()));
    obj[QStringLiteral("ok")] = true;
    CliOutput::printJson(obj);
    return 0;
}

int CliCommands::convertKifu(const QJsonObject& params)
{

    // 形式の名前は表の選択肢で検査済み
    KifuConversionService::Request request;
    request.inputPath = params.value(QStringLiteral("input_path")).toString();
    request.text = params.value(QStringLiteral("text")).toString();
    const QString inputName = params.value(QStringLiteral("input_format")).toString();
    if (inputName != QLatin1String("auto")) request.inputFormat = KifuFormats::fromName(inputName);
    request.outputFormat = *KifuFormats::fromName(params.value(QStringLiteral("output_format")).toString());

    const QString outputPath = params.value(QStringLiteral("output_path")).toString();
    if (const std::optional<Failure> problem = outputPath.isEmpty() ? std::nullopt
            : OutputPath::check(outputPath, params.value(QStringLiteral("overwrite")).toBool(), QStringLiteral("--overwrite"))) {
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
    obj[QStringLiteral("moves")] = RecordJson::moves(result.mainline);
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

int CliCommands::renderBoard(const QJsonObject& params)
{

    const QString outputPath = params.value(QStringLiteral("output_path")).toString();
    if (const std::optional<Failure> problem =
            OutputPath::check(outputPath, params.value(QStringLiteral("overwrite")).toBool(), QStringLiteral("--overwrite"))) {
        return CliOutput::fail(*problem);
    }

    BoardImageRenderer::Options options;
    options.squareSize = params.value(QStringLiteral("square_size")).toInt();
    options.flip = params.value(QStringLiteral("flip")).toBool();
    options.lastMoveUsi = params.value(QStringLiteral("last_move")).toString();
    options.blackName = params.value(QStringLiteral("black_name")).toString();
    options.whiteName = params.value(QStringLiteral("white_name")).toString();

    // 「手番」などの表示・段の表記（一〜九／a〜i）・書体を、GUI と同じ表示言語と棋譜表記の設定に合わせる。
    // ほかのコマンドの出力（JSON・棋譜の形式）は言語に依存させないため、盤面描画のときだけ適用する。
    const ApplicationTranslations translations;
    ApplicationFonts::initialize(AppSettings::uiFontFamily(), translations.language());

    QSize size;
    Failure failure;
    if (!BoardImageRenderer::renderToFile(params.value(QStringLiteral("sfen")).toString(), outputPath, options, &failure, &size)) {
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
