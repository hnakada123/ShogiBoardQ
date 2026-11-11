/// @file clicommands_p2p.cpp
/// @brief shogiboardq-cli の P2P対局のコマンド（verify-match）
///
/// コマンドの表（CliCommandSpecs）はビルドの構成によらず同じにし（describe の出力と MCP の specs.json を
/// 1つに保つ）、P2P対局を含まないビルド（ENABLE_P2P_PLAY が OFF）では使えないことを失敗で返す。

#include "clicommands.h"
#include "clioutput.h"

#ifdef SBQ_ENABLE_P2P
#include "matchverificationservice.h"
#endif

int CliCommands::verifyMatch(const QJsonObject& params)
{
#ifdef SBQ_ENABLE_P2P
    const auto outcome = MatchVerificationService::verifyFile(params.value(QStringLiteral("input_path")).toString());
    if (const auto* failure = std::get_if<Failure>(&outcome)) return CliOutput::fail(*failure);
    CliOutput::printJson(std::get<QJsonObject>(outcome));
    return 0;
#else
    Q_UNUSED(params);
    return CliOutput::fail(ErrorKind::NotAllowed,
                           QStringLiteral("This build does not include P2P play (built with ENABLE_P2P_PLAY=OFF)."));
#endif
}
