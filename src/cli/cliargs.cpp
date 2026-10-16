/// @file cliargs.cpp
/// @brief shogiboardq-cli のコマンド共通の引数解釈の実装

#include "cliargs.h"

#include "clicommandspecs.h"
#include "clioutput.h"
#include "enginecatalog.h"
#include "sfenvalidationservice.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>

std::optional<QJsonObject> CliArgs::parse(const QString& command, const QStringList& args, bool events, int* exitCode)
{
    const OperationSpec* spec = CliCommandSpecs::find(command);
    Q_ASSERT(spec);
    QString error;
    std::optional<QJsonObject> params = CliCommandSpecs::parse(*spec, args, &error);
    if (!params) {
        const int code = events ? CliOutput::failEvent(CliOutput::kInvalidArgument, error)
                                : CliOutput::fail(CliOutput::kInvalidArgument, error);
        if (exitCode) *exitCode = code;
    }
    return params;
}

std::optional<CliPosition> CliArgs::resolvePosition(const QJsonObject& params, QString* error)
{
    CliPosition position;
    const QString sfenArg = params.value(QStringLiteral("sfen")).toString().trimmed();
    const SfenValidation validation = SfenValidationService::validate(sfenArg.isEmpty() ? QStringLiteral("startpos") : sfenArg);
    if (!validation.valid) {
        if (error) *error = QStringLiteral("Invalid SFEN: %1").arg(validation.errors.join(QStringLiteral("; ")));
        return std::nullopt;
    }
    position.startSfen = validation.normalizedSfen;

    const QJsonArray moveArray = params.value(QStringLiteral("moves")).toArray();
    for (const QJsonValue& move : moveArray) position.moves << move.toString();
    QString applyError;
    position.finalSfen = SfenValidationService::applyUsiMoves(position.startSfen, position.moves, &applyError);
    if (position.finalSfen.isEmpty()) {
        if (error) *error = applyError;
        return std::nullopt;
    }

    QList<UsiMove> moves;
    for (const QString& text : std::as_const(position.moves)) {
        const std::optional<UsiMove> move = UsiMove::parse(text);
        if (!move) {
            if (error) *error = QStringLiteral("Invalid USI move: %1").arg(text);
            return std::nullopt;
        }
        moves << *move;
    }
    const std::optional<UsiPosition> usi = UsiPosition::fromMoves(position.startSfen, moves);
    if (!usi) {
        if (error) *error = QStringLiteral("Moves cannot be applied to the start position");
        return std::nullopt;
    }
    position.position = *usi;
    return position;
}

std::optional<EngineListSettings::EngineEntry> CliArgs::resolveEngine(const QJsonObject& params, QString* error)
{
    const QString name = params.value(QStringLiteral("engine")).toString().trimmed();
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
