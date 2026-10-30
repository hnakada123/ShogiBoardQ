/// @file clicommandspecs.cpp
/// @brief shogiboardq-cli のコマンドごとの引数の表と変換の実装

#include "clicommandspecs.h"
#include "boardsquaresize.h"
#include "kifuformat.h"

#include <QCommandLineParser>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>

#include <cmath>

using namespace ParamSpecs;
using Type = ParamSpec::Type;

namespace {
/// 全パラメータを JSON で渡すオプション（引数名は JSON の名前。コマンドラインのオプションが優先する）
QString paramsJsonOption()
{
    return QStringLiteral("params-json");
}

ParamSpec engine()
{
    return string(QStringLiteral("engine"),
                  QStringLiteral("Name of a USI engine registered in ShogiBoardQ, exactly as listed by list-engines."))
        .atLeast(1).require().value(QStringLiteral("name"));
}
ParamSpec sfen(bool required)
{
    ParamSpec spec = string(QStringLiteral("sfen"),
                            QStringLiteral("Position in SFEN (board, side to move b/w, hands, move number), or 'startpos' "
                                           "for the initial position. A leading 'position sfen ' is accepted."))
                         .atLeast(1).value(QStringLiteral("sfen"));
    return required ? spec.require() : spec.withDefault(QStringLiteral("startpos"));
}
ParamSpec moves()
{
    return stringArray(QStringLiteral("moves"),
                       QStringLiteral("USI moves played after the start position, e.g. [\"7g7f\", \"3c3d\"] "
                                      "(space separated on the command line). Drops use 'P*5e'."))
        .atMost(1000).withDefault(QJsonArray()).value(QStringLiteral("moves")).matching(usiMovePattern());
}
ParamSpec stdinControl()
{
    return boolean(QStringLiteral("stdin_control"), QStringLiteral("Stop when a line 'stop' arrives on stdin or stdin is closed."));
}
ParamSpec overwrite()
{
    return boolean(QStringLiteral("overwrite"),
                   QStringLiteral("Allow replacing an existing file. Without it an existing file is an error."));
}
ParamSpec path(const QString& name, const QString& description)
{
    return string(name, description).atLeast(1).value(QStringLiteral("path"));
}
ParamSpec seconds(const QString& description, qint64 defaultValue)
{
    return integer(QStringLiteral("seconds"), description, 1, 600).withDefault(defaultValue);
}
/// 最終手の別の詰め方を許すか（旧名 --no-final-alternatives は false）
ParamSpec allowFinalAlternatives(const QString& description)
{
    return boolean(QStringLiteral("allow_final_move_alternatives"), description, true)
        .aliasFlag(QStringLiteral("no-final-alternatives"), false);
}

/// 棋譜の形式の名前（保存できる形式。形式の表 KifuFormats から作る）
QStringList kifuFormats()
{
    return KifuFormats::savableNames();
}

/// 既定値が true の真偽値を false にするオプション（--no-<名前>）
QString negatedOption(const ParamSpec& spec)
{
    return QStringLiteral("no-") + CliCommandSpecs::optionName(spec.name);
}

bool hasNegatedOption(const ParamSpec& spec)
{
    return spec.type == Type::Boolean && spec.defaultValue.toBool();
}

QString valueNameOf(const ParamSpec& spec)
{
    return spec.valueName.isEmpty() ? QStringLiteral("value") : spec.valueName;
}

/// 使い方に出す1つのオプション（任意のものは [] で囲む。既定値が true の真偽値は --no-<名前>）
QString optionUsage(const ParamSpec& spec)
{
    QString text = QStringLiteral("--") + (hasNegatedOption(spec) ? negatedOption(spec) : CliCommandSpecs::optionName(spec.name));
    if (spec.type != Type::Boolean) text += QLatin1Char(' ') + valueNameOf(spec);
    return spec.required ? text : QLatin1Char('[') + text + QLatin1Char(']');
}

/// オプションの文字列を引数の型の値にする（scale は旧名の単位の換算。整数は切り上げ）
std::optional<QJsonValue> optionValue(const ParamSpec& spec, const QString& text, double scale, const QString& option,
                                      QString* error)
{
    static const QRegularExpression whitespace(QStringLiteral("\\s+"));
    switch (spec.type) {
    case Type::Integer: {
        bool ok = false;
        const qint64 number = text.toLongLong(&ok);
        if (!ok) {
            if (error) *error = QStringLiteral("--%1 must be an integer").arg(option);
            return std::nullopt;
        }
        return QJsonValue(scale == 1.0 ? static_cast<double>(number) : std::ceil(static_cast<double>(number) * scale));
    }
    case Type::StringArray:
        return QJsonValue(QJsonArray::fromStringList(text.split(whitespace, Qt::SkipEmptyParts)));
    default:
        return QJsonValue(text);
    }
}

/// --help に出す説明（範囲と既定値を添える）
QString optionHelp(const ParamSpec& spec)
{
    QString text = spec.description;
    QStringList notes;
    if (spec.type == Type::Integer && spec.minimum && spec.maximum)
        notes << QStringLiteral("%1-%2").arg(*spec.minimum).arg(*spec.maximum);
    if (spec.type != Type::Boolean && !spec.defaultValue.isUndefined())
        notes << QStringLiteral("default %1").arg(spec.defaultValue.toVariant().toString());
    if (spec.required) notes << QStringLiteral("required");
    if (!notes.isEmpty()) text += QStringLiteral(" (") + notes.join(QStringLiteral(", ")) + QLatin1Char(')');
    return text;
}
} // namespace

const QList<OperationSpec>& CliCommandSpecs::all()
{
    static const QList<OperationSpec> table = {
        {QStringLiteral("version"), QStringLiteral("Print the application version"), {}},
        {QStringLiteral("describe"), QStringLiteral("Print the parameters of every command and automation method as JSON Schema"), {}},
        {QStringLiteral("environment"),
         QStringLiteral("Print the automation socket, endpoint file, message limit and error kinds as JSON"), {}},
        {QStringLiteral("list-engines"), QStringLiteral("List USI engines registered in ShogiBoardQ"), {}},
        {QStringLiteral("validate-sfen"), QStringLiteral("Validate a SFEN and report turn/hands/check/legal moves"), {sfen(true)}},
        {QStringLiteral("convert-kifu"), QStringLiteral("Convert a kifu file or text to another format"),
         {path(QStringLiteral("input_path"), QStringLiteral("Input kifu file (absolute path). Either input_path or text is required."))
              .alias(QStringLiteral("input")),
          string(QStringLiteral("text"), QStringLiteral("Input kifu text. Either input_path or text is required."))
              .length(1, 2000000),
          choice(QStringLiteral("input_format"), QStringLiteral("Input format ('auto' detects it from the content)."),
                 QStringList{QStringLiteral("auto")} + kifuFormats())
              .withDefault(QStringLiteral("auto")),
          choice(QStringLiteral("output_format"), QStringLiteral("Output format ('sfen' lists the position after each move)."),
                 kifuFormats() + QStringList{QStringLiteral("sfen")})
              .require(),
          path(QStringLiteral("output_path"), QStringLiteral("Write the result to this file (absolute path) instead of returning it."))
              .alias(QStringLiteral("output")),
          overwrite()}},
        {QStringLiteral("render-board"), QStringLiteral("Render a position as a PNG image"),
         {sfen(false),
          path(QStringLiteral("output_path"), QStringLiteral("Output image path (absolute, .png recommended)."))
              .require().alias(QStringLiteral("output")),
          integer(QStringLiteral("square_size"), QStringLiteral("Square width in pixels."), BoardSquareSize::kMin,
                  BoardSquareSize::kMax).withDefault(BoardSquareSize::kDefault).value(QStringLiteral("px")),
          boolean(QStringLiteral("flip"), QStringLiteral("View from the white (gote) side.")),
          string(QStringLiteral("last_move"), QStringLiteral("USI move to highlight.")).value(QStringLiteral("move"))
              .matching(usiMovePattern()),
          string(QStringLiteral("black_name"), QStringLiteral("Name shown for black (sente).")).length(0, 64).value(QStringLiteral("name")),
          string(QStringLiteral("white_name"), QStringLiteral("Name shown for white (gote).")).length(0, 64).value(QStringLiteral("name")),
          overwrite()}},
        {QStringLiteral("analyze"), QStringLiteral("Analyse a position with a registered engine (JSON Lines)"),
         {engine(), sfen(false), moves(), stdinControl(), seconds(QStringLiteral("Thinking time in seconds."), 10),
          integer(QStringLiteral("multipv"), QStringLiteral("Number of candidate lines."), 1, 10).withDefault(1)}},
        {QStringLiteral("analyze-kifu"),
         QStringLiteral("Analyse every main-line position of a record with one engine process (JSON Lines)"),
         {engine(),
          path(QStringLiteral("input_path"), QStringLiteral("Kifu file (absolute path). Either input_path or text is required."))
              .alias(QStringLiteral("input")),
          string(QStringLiteral("text"), QStringLiteral("Kifu text. Either input_path or text is required.")).length(1, 2000000),
          choice(QStringLiteral("input_format"), QStringLiteral("Input format ('auto' detects it from the content)."),
                 QStringList{QStringLiteral("auto")} + kifuFormats())
              .withDefault(QStringLiteral("auto")),
          integer(QStringLiteral("from_ply"), QStringLiteral("First position (0 = the start position, N = after N moves)."), 0, 10000)
              .withDefault(0),
          integer(QStringLiteral("to_ply"), QStringLiteral("Last position (inclusive; default: the end of the main line)."), 0, 10000),
          integer(QStringLiteral("seconds_per_position"), QStringLiteral("Thinking time per position in seconds."), 1, 600)
              .withDefault(1),
          integer(QStringLiteral("multipv"), QStringLiteral("Number of candidate lines."), 1, 10).withDefault(1),
          stdinControl()}},
        {QStringLiteral("mate"), QStringLiteral("Search for a mate with an engine that supports go mate (JSON Lines)"),
         {engine(), sfen(true), moves(), stdinControl(), seconds(QStringLiteral("Search time in seconds."), 10)}},
        {QStringLiteral("generate-tsume"), QStringLiteral("Generate tsume problems (JSON Lines)"),
         {engine(), stdinControl(),
          integer(QStringLiteral("target_moves"), QStringLiteral("Mate length in plies (odd)."), 1, 19).withDefault(3),
          integer(QStringLiteral("max_positions"), QStringLiteral("Stop after this many positions."), 1, 100).withDefault(5),
          integer(QStringLiteral("timeout_ms"), QStringLiteral("Engine time per candidate, per trimming step and per verification."),
                  500, 60000).withDefault(5000).value(QStringLiteral("ms")),
          integer(QStringLiteral("max_attack_pieces"), QStringLiteral("Maximum attacking pieces on board and in hand."), 1, 10)
              .withDefault(4).alias(QStringLiteral("max-attack")),
          integer(QStringLiteral("max_defend_pieces"), QStringLiteral("Maximum defending pieces other than the king."), 0, 10)
              .withDefault(1).alias(QStringLiteral("max-defend")),
          integer(QStringLiteral("attack_range"), QStringLiteral("Placement range around the king."), 1, 8).withDefault(3),
          boolean(QStringLiteral("add_remaining_to_defender_hand"),
                  QStringLiteral("Give the pieces not on the board to the defender's hand."), true)
              .aliasFlag(QStringLiteral("no-remaining-to-hand"), false),
          allowFinalAlternatives(QStringLiteral("Accept positions whose final mating move has alternatives."))}},
        {QStringLiteral("verify-tsume"), QStringLiteral("Check a tsume problem for a unique solution"),
         {engine(), sfen(true),
          integer(QStringLiteral("target_moves"), QStringLiteral("Expected mate length in plies (odd)."), 1, 39).require(),
          integer(QStringLiteral("timeout_seconds"), QStringLiteral("Total engine time budget in seconds."), 1, 120)
              .withDefault(15).value(QStringLiteral("seconds")).alias(QStringLiteral("timeout-ms"), 0.001),
          allowFinalAlternatives(QStringLiteral("Accept alternatives on the final mating move (otherwise they are a cook)."))}},
    };
    return table;
}

const OperationSpec* CliCommandSpecs::find(const QString& name)
{
    for (const OperationSpec& spec : all())
        if (spec.name == name) return &spec;
    return nullptr;
}

QString CliCommandSpecs::optionName(const QString& paramName)
{
    return QString(paramName).replace(QLatin1Char('_'), QLatin1Char('-'));
}

std::optional<QJsonObject> CliCommandSpecs::parse(const OperationSpec& command, const QStringList& args, QString* error)
{
    QCommandLineParser parser;
    parser.setSingleDashWordOptionMode(QCommandLineParser::ParseAsLongOptions);
    parser.addOption({paramsJsonOption(), QStringLiteral("All parameters as a JSON object."), QStringLiteral("json")});
    for (const ParamSpec& spec : command.params) {
        const QString option = optionName(spec.name);
        if (spec.type == Type::Boolean) {
            parser.addOption({option, optionHelp(spec)});
            if (hasNegatedOption(spec)) parser.addOption({negatedOption(spec), optionHelp(spec)});
        } else {
            parser.addOption({option, optionHelp(spec), valueNameOf(spec)});
        }
        for (const ParamSpec::Alias& alias : spec.aliases) {
            if (alias.flagValue.isUndefined()) parser.addOption({alias.option, optionHelp(spec), valueNameOf(spec)});
            else parser.addOption({alias.option, optionHelp(spec)});
        }
    }
    if (!parser.parse(QStringList{QStringLiteral("shogiboardq-cli")} + args)) {
        if (error) *error = parser.errorText();
        return std::nullopt;
    }
    if (!parser.positionalArguments().isEmpty()) {
        if (error) *error = QStringLiteral("Unexpected argument: %1").arg(parser.positionalArguments().constFirst());
        return std::nullopt;
    }
    // --params-json の値に、コマンドラインのオプション（旧名・--no-<名前>・本来の名前の順で後が優先）を重ねる
    QJsonObject params;
    if (parser.isSet(paramsJsonOption())) {
        const QJsonDocument document = QJsonDocument::fromJson(parser.value(paramsJsonOption()).toUtf8());
        if (!document.isObject()) {
            if (error) *error = QStringLiteral("--%1 must be a JSON object").arg(paramsJsonOption());
            return std::nullopt;
        }
        params = document.object();
    }
    for (const ParamSpec& spec : command.params) {
        for (const ParamSpec::Alias& alias : spec.aliases) {
            if (!parser.isSet(alias.option)) continue;
            if (!alias.flagValue.isUndefined()) {
                params[spec.name] = alias.flagValue;
                continue;
            }
            const std::optional<QJsonValue> value = optionValue(spec, parser.value(alias.option), alias.scale, alias.option, error);
            if (!value) return std::nullopt;
            params[spec.name] = *value;
        }
        if (hasNegatedOption(spec) && parser.isSet(negatedOption(spec))) params[spec.name] = false;
        const QString option = optionName(spec.name);
        if (!parser.isSet(option)) continue;
        if (spec.type == Type::Boolean) {
            params[spec.name] = true;
            continue;
        }
        const std::optional<QJsonValue> value = optionValue(spec, parser.value(option), 1.0, option, error);
        if (!value) return std::nullopt;
        params[spec.name] = *value;
    }
    QString validationError;
    const std::optional<QJsonObject> validated = ParamSpecs::validate(command, params, &validationError);
    if (!validated && error) {
        *error = validationError;
        // オプションで指定したときは、引数名をオプション名で示す（"max_positions" → --max-positions）
        if (!parser.isSet(paramsJsonOption())) {
            for (const ParamSpec& spec : command.params)
                error->replace(QLatin1Char('"') + spec.name + QLatin1Char('"'), QStringLiteral("--") + optionName(spec.name));
        }
    }
    return validated;
}

QString CliCommandSpecs::usage()
{
    QString text = QStringLiteral(
        "Usage: shogiboardq-cli <command> [options]\n"
        "\n"
        "Commands (output is JSON on stdout; long-running commands emit JSON Lines):\n");
    for (const OperationSpec& command : all()) {
        text += QStringLiteral("  %1%2\n").arg(command.name.leftJustified(16), command.summary);
        if (command.params.isEmpty()) continue;
        QStringList options;
        for (const ParamSpec& spec : command.params) options << optionUsage(spec);
        text += QStringLiteral("    ") + options.join(QLatin1Char(' ')) + QLatin1Char('\n');
    }
    text += QStringLiteral(
        "\n"
        "Every command also accepts --params-json '<object>' with the parameters under their JSON names\n"
        "(options given on the command line take precedence).\n"
        "Long-running commands accept --stdin-control: a line 'stop' on stdin (or EOF) stops them.\n"
        "Engines are referenced by the name registered in ShogiBoardQ; arbitrary paths are not accepted.\n"
        "Run 'shogiboardq-cli describe' for the ranges and defaults of every option as JSON Schema.\n");
    return text;
}
