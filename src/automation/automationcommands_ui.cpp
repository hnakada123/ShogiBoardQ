/// @file automationcommands_ui.cpp
/// @brief 自動化 API の ui.* メソッド（予約した UI 操作の結果）の実装

#include "automationcommands.h"
#include "automationdispatcher.h"
#include "automationinteractions.h"

void AutomationCommands::registerUiCommands(AutomationDispatcher& dispatcher, const AutomationContext& /*context*/)
{
    dispatcher.registerMethod(QStringLiteral("ui.interaction"), [](const QJsonObject& params) {
        return AutomationInteractions::status(params.value(QStringLiteral("id")).toInt());
    });
}
