/// @file cliargs.cpp
/// @brief shogiboardq-cli のコマンド共通の引数解釈の実装

#include "cliargs.h"

#include "enginecatalog.h"
#include "sfenutils.h"
#include "sfenvalidationservice.h"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>

void CliArgs::addPositionOptions(QCommandLineParser& parser)
{
    parser.addOption({QStringLiteral("sfen"),
                      QStringLiteral("Start position as SFEN, or 'startpos'."), QStringLiteral("sfen"),
                      QStringLiteral("startpos")});
    parser.addOption({QStringLiteral("moves"),
                      QStringLiteral("USI moves applied after the start position, space separated."),
                      QStringLiteral("moves")});
}

void CliArgs::addEngineOption(QCommandLineParser& parser)
{
    parser.addOption({QStringLiteral("engine"),
                      QStringLiteral("Name of a USI engine registered in ShogiBoardQ."), QStringLiteral("name")});
}

void CliArgs::addStdinControlOption(QCommandLineParser& parser)
{
    parser.addOption({QStringLiteral("stdin-control"),
                      QStringLiteral("Stop when a line 'stop' arrives on stdin or stdin is closed.")});
}

std::optional<CliPosition> CliArgs::resolvePosition(const QCommandLineParser& parser, QString* error)
{
    CliPosition position;
    const QString sfenArg = parser.value(QStringLiteral("sfen")).trimmed();
    const SfenValidation validation = SfenValidationService::validate(sfenArg.isEmpty() ? QStringLiteral("startpos") : sfenArg);
    if (!validation.valid) {
        if (error) *error = QStringLiteral("Invalid SFEN: %1").arg(validation.errors.join(QStringLiteral("; ")));
        return std::nullopt;
    }
    position.startSfen = validation.normalizedSfen;

    static const QRegularExpression ws(QStringLiteral("\\s+"));
    position.moves = parser.value(QStringLiteral("moves")).split(ws, Qt::SkipEmptyParts);
    QString applyError;
    position.finalSfen = SfenValidationService::applyUsiMoves(position.startSfen, position.moves, &applyError);
    if (position.finalSfen.isEmpty()) {
        if (error) *error = applyError;
        return std::nullopt;
    }

    const bool hirate = SfenUtils::isHirateStart(position.startSfen)
                        && position.startSfen.endsWith(QStringLiteral(" 1"));
    position.positionCommand = hirate ? QStringLiteral("position startpos")
                                      : QStringLiteral("position sfen ") + position.startSfen;
    if (!position.moves.isEmpty()) {
        position.positionCommand += QStringLiteral(" moves ") + position.moves.join(QLatin1Char(' '));
    }
    return position;
}

std::optional<EngineListSettings::EngineEntry> CliArgs::resolveEngine(const QCommandLineParser& parser, QString* error)
{
    const QString name = parser.value(QStringLiteral("engine")).trimmed();
    if (name.isEmpty()) {
        if (error) *error = QStringLiteral("--engine is required");
        return std::nullopt;
    }
    auto entry = EngineCatalog::findByName(name, error);
    if (!entry) return std::nullopt;
    if (!QFileInfo::exists(entry->path)) {
        if (error) *error = QStringLiteral("Engine executable not found: %1").arg(entry->path);
        return std::nullopt;
    }
    return entry;
}

std::optional<int> CliArgs::intOption(const QCommandLineParser& parser, const QString& name, int defaultValue,
                                      int minimum, int maximum, QString* error)
{
    if (!parser.isSet(name)) return defaultValue;
    bool ok = false;
    const int value = parser.value(name).toInt(&ok);
    if (!ok || value < minimum || value > maximum) {
        if (error) *error = QStringLiteral("--%1 must be an integer between %2 and %3").arg(name).arg(minimum).arg(maximum);
        return std::nullopt;
    }
    return value;
}

bool CliArgs::checkOutputPath(const QString& path, bool overwrite, QString* error)
{
    const QFileInfo info(path);
    if (path.isEmpty() || !info.isAbsolute()) {
        if (error) *error = QStringLiteral("Output path must be absolute");
        return false;
    }
    if (info.exists() && !overwrite) {
        if (error) *error = QStringLiteral("Output file already exists (pass --overwrite to replace it): %1").arg(path);
        return false;
    }
    if (info.exists() && info.isDir()) {
        if (error) *error = QStringLiteral("Output path is a directory: %1").arg(path);
        return false;
    }
    if (!info.dir().exists()) {
        if (error) *error = QStringLiteral("Output directory does not exist: %1").arg(info.dir().path());
        return false;
    }
    return true;
}
