/// @file cliargs.cpp
/// @brief shogiboardq-cli のコマンド共通の引数解釈の実装

#include "cliargs.h"

#include "clicommandspecs.h"
#include "clioutput.h"
#include "enginecatalog.h"
#include "enginemovevalidator.h"
#include "sfenvalidationservice.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>

std::optional<UsiPosition> CliArgs::resolvePosition(const QJsonObject& params, QString* error)
{
    const QString sfenArg = params.value(QStringLiteral("sfen")).toString().trimmed();
    const SfenValidation validation = SfenValidationService::validate(sfenArg.isEmpty() ? QStringLiteral("startpos") : sfenArg);
    if (!validation.valid) {
        if (error) *error = QStringLiteral("Invalid SFEN: %1").arg(validation.errors.join(QStringLiteral("; ")));
        return std::nullopt;
    }

    // 合法手だけを1手ずつ指す（盤の局面は1回だけ進める）
    UsiPosition usi(*Position::fromSfen(validation.normalizedSfen));
    const QJsonArray moveArray = params.value(QStringLiteral("moves")).toArray();
    for (const QJsonValue& value : moveArray) {
        const QString text = value.toString();
        const std::optional<UsiMove> move = UsiMove::parse(text);
        std::optional<UsiPosition> next;
        if (move && EngineMoveValidator::isLegal(EngineMoveValidator::load(usi.position()), *move))
            next = usi.withMove(*move);
        if (!next) {
            if (error) *error = QStringLiteral("Illegal or malformed USI move: %1").arg(text);
            return std::nullopt;
        }
        usi = *next;
    }
    return usi;
}

std::optional<EngineEntry> CliArgs::resolveEngine(const QJsonObject& params, QString* error)
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

