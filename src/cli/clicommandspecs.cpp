/// @file clicommandspecs.cpp
/// @brief shogiboardq-cli のコマンドごとの引数の表と変換の実装

#include "clicommandspecs.h"

#include <QCommandLineParser>
#include <QJsonArray>
#include <QRegularExpression>

using namespace ParamSpecs;
using Type = ParamSpec::Type;

namespace {
ParamSpec engine()
{
    return string(QStringLiteral("engine"), QStringLiteral("Name of a USI engine registered in ShogiBoardQ."))
        .atLeast(1).require().value(QStringLiteral("name"));
}
ParamSpec sfen(bool required)
{
    ParamSpec spec = string(QStringLiteral("sfen"), QStringLiteral("Position as SFEN, or 'startpos'."))
                         .atLeast(1).value(QStringLiteral("sfen"));
    return required ? spec.require() : spec.withDefault(QStringLiteral("startpos"));
}
ParamSpec moves()
{
    return stringArray(QStringLiteral("moves"), QStringLiteral("USI moves applied after the start position, space separated."))
        .atMost(1000).withDefault(QJsonArray()).value(QStringLiteral("moves"));
}
ParamSpec stdinControl()
{
    return boolean(QStringLiteral("stdin_control"), QStringLiteral("Stop when a line 'stop' arrives on stdin or stdin is closed."));
}
ParamSpec overwrite()
{
    return boolean(QStringLiteral("overwrite"), QStringLiteral("Allow replacing an existing output file."));
}
ParamSpec path(const QString& name, const QString& description)
{
    return string(name, description).atLeast(1).value(QStringLiteral("path"));
}
ParamSpec seconds(const QString& description, qint64 defaultValue)
{
    return integer(QStringLiteral("seconds"), description, 1, 600).withDefault(defaultValue);
}
ParamSpec noFinalAlternatives(const QString& description)
{
    return boolean(QStringLiteral("no_final_alternatives"), description);
}

QStringList kifuFormats()
{
    return {QStringLiteral("kif"), QStringLiteral("ki2"), QStringLiteral("csa"),
            QStringLiteral("jkf"), QStringLiteral("usi"), QStringLiteral("usen")};
}

/// 使い方に出す1つのオプション（任意のものは [] で囲む）
QString optionUsage(const ParamSpec& spec)
{
    QString text = QStringLiteral("--") + CliCommandSpecs::optionName(spec.name);
    if (spec.type != Type::Boolean) text += QLatin1Char(' ') + (spec.valueName.isEmpty() ? QStringLiteral("value") : spec.valueName);
    return spec.required ? text : QLatin1Char('[') + text + QLatin1Char(']');
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
         {path(QStringLiteral("input"), QStringLiteral("Input kifu file.")),
          string(QStringLiteral("text"), QStringLiteral("Input kifu text.")).atLeast(1),
          choice(QStringLiteral("input_format"), QStringLiteral("Input format."), QStringList{QStringLiteral("auto")} + kifuFormats())
              .withDefault(QStringLiteral("auto")),
          choice(QStringLiteral("output_format"), QStringLiteral("Output format."), kifuFormats() + QStringList{QStringLiteral("sfen")})
              .require(),
          path(QStringLiteral("output"), QStringLiteral("Write the result to this file.")), overwrite()}},
        {QStringLiteral("render-board"), QStringLiteral("Render a position as a PNG image"),
         {sfen(false), path(QStringLiteral("output"), QStringLiteral("Output image path (.png recommended).")).require(),
          integer(QStringLiteral("square_size"), QStringLiteral("Square width in pixels."), 20, 150).withDefault(50).value(QStringLiteral("px")),
          boolean(QStringLiteral("flip"), QStringLiteral("View from the white (gote) side.")),
          string(QStringLiteral("last_move"), QStringLiteral("USI move to highlight.")).value(QStringLiteral("move")),
          string(QStringLiteral("black_name"), QStringLiteral("Name shown for black (sente).")).length(0, 64).value(QStringLiteral("name")),
          string(QStringLiteral("white_name"), QStringLiteral("Name shown for white (gote).")).length(0, 64).value(QStringLiteral("name")),
          overwrite()}},
        {QStringLiteral("analyze"), QStringLiteral("Analyse a position with a registered engine (JSON Lines)"),
         {engine(), sfen(false), moves(), stdinControl(), seconds(QStringLiteral("Thinking time in seconds."), 10),
          integer(QStringLiteral("multipv"), QStringLiteral("Number of candidate lines."), 1, 10).withDefault(1)}},
        {QStringLiteral("mate"), QStringLiteral("Search for a mate with an engine that supports go mate (JSON Lines)"),
         {engine(), sfen(true), moves(), stdinControl(), seconds(QStringLiteral("Search time in seconds."), 10)}},
        {QStringLiteral("generate-tsume"), QStringLiteral("Generate tsume problems (JSON Lines)"),
         {engine(), stdinControl(),
          integer(QStringLiteral("target_moves"), QStringLiteral("Mate length in plies (odd)."), 1, 19).withDefault(3),
          integer(QStringLiteral("max_positions"), QStringLiteral("Stop after this many positions."), 1, 100).withDefault(5),
          integer(QStringLiteral("timeout_ms"), QStringLiteral("Engine time per candidate, per trimming step and per verification."),
                  500, 60000).withDefault(5000).value(QStringLiteral("ms")),
          integer(QStringLiteral("max_attack"), QStringLiteral("Maximum attacking pieces on board and in hand."), 1, 10).withDefault(4),
          integer(QStringLiteral("max_defend"), QStringLiteral("Maximum defending pieces other than the king."), 0, 10).withDefault(1),
          integer(QStringLiteral("attack_range"), QStringLiteral("Placement range around the king."), 1, 8).withDefault(3),
          boolean(QStringLiteral("no_remaining_to_hand"), QStringLiteral("Do not give unused pieces to the defender's hand.")),
          noFinalAlternatives(QStringLiteral("Reject positions whose final mating move has alternatives."))}},
        {QStringLiteral("verify-tsume"), QStringLiteral("Check a tsume problem for a unique solution"),
         {engine(), sfen(true),
          integer(QStringLiteral("target_moves"), QStringLiteral("Expected mate length in plies (odd)."), 1, 39).require(),
          integer(QStringLiteral("timeout_ms"), QStringLiteral("Total engine time budget."), 1000, 120000)
              .withDefault(15000).value(QStringLiteral("ms")),
          noFinalAlternatives(QStringLiteral("Treat alternatives on the final mating move as a cook."))}},
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
    for (const ParamSpec& spec : command.params) {
        if (spec.type == Type::Boolean) {
            parser.addOption({optionName(spec.name), optionHelp(spec)});
        } else {
            parser.addOption({optionName(spec.name), optionHelp(spec),
                              spec.valueName.isEmpty() ? QStringLiteral("value") : spec.valueName});
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
    static const QRegularExpression whitespace(QStringLiteral("\\s+"));
    QJsonObject params;
    for (const ParamSpec& spec : command.params) {
        const QString option = optionName(spec.name);
        if (!parser.isSet(option)) continue;
        const QString text = parser.value(option);
        switch (spec.type) {
        case Type::Boolean:
            params[spec.name] = true;
            break;
        case Type::Integer: {
            bool ok = false;
            const qint64 number = text.toLongLong(&ok);
            if (!ok) {
                if (error) *error = QStringLiteral("--%1 must be an integer").arg(option);
                return std::nullopt;
            }
            params[spec.name] = static_cast<double>(number);
            break;
        }
        case Type::StringArray:
            params[spec.name] = QJsonArray::fromStringList(text.split(whitespace, Qt::SkipEmptyParts));
            break;
        default:
            params[spec.name] = text;
            break;
        }
    }
    QString validationError;
    const std::optional<QJsonObject> validated = ParamSpecs::validate(command, params, &validationError);
    if (!validated && error) {
        // 引数名をオプション名で示す（"max_positions" → --max-positions）
        *error = validationError;
        for (const ParamSpec& spec : command.params)
            error->replace(QLatin1Char('"') + spec.name + QLatin1Char('"'), QStringLiteral("--") + optionName(spec.name));
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
        "Long-running commands accept --stdin-control: a line 'stop' on stdin (or EOF) stops them.\n"
        "Engines are referenced by the name registered in ShogiBoardQ; arbitrary paths are not accepted.\n"
        "Run 'shogiboardq-cli describe' for the ranges and defaults of every option as JSON Schema.\n");
    return text;
}
