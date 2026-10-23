#ifndef CLIARGS_H
#define CLIARGS_H

/// @file cliargs.h
/// @brief shogiboardq-cli のコマンド共通の引数解釈（局面・エンジン・数値）

#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <optional>

#include "registeredengines.h"
#include "errorkinds.h"
#include "usiposition.h"

namespace CliArgs {

/// 引数の "sfen"・"moves"（CliCommandSpecs::parse で読んだ値）から、エンジンに探索させる局面を解決する
/// （開始局面は正規化した SFEN、手は合法手だけ）。失敗時は nullopt と error
std::optional<UsiPosition> resolvePosition(const QJsonObject& params, QString* error);
/// 引数の "engine" の登録エンジンを解決する。失敗時は nullopt と error
std::optional<EngineEntry> resolveEngine(const QJsonObject& params, QString* error);

} // namespace CliArgs

#endif // CLIARGS_H
