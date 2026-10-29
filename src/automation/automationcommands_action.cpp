/// @file automationcommands_action.cpp
/// @brief 自動化 API の action.* メソッド（許可したメニュー項目の一覧と実行）の実装

#include "automationactionpolicy.h"
#include "automationcommands.h"
#include "automationdispatcher.h"
#include "automationinteractions.h"
#include "automationwidgets.h"

#include <QAction>
#include <QJsonArray>
#include <QMainWindow>

namespace {

QAction* findAction(const AutomationContext& context, const QString& name)
{
    return context.mainWindow ? context.mainWindow->findChild<QAction*>(name) : nullptr;
}

using AutomationWidgets::requireInteractive;

} // namespace

void AutomationCommands::registerActionCommands(AutomationDispatcher& dispatcher, const AutomationContext& context)
{
    dispatcher.registerMethod(QStringLiteral("action.list"), [context](const QJsonObject&) {
        QJsonArray actions;
        const QStringList& allowed = AutomationActionPolicy::allowedActions();
        for (const QString& name : allowed) {
            QAction* action = findAction(context, name);
            if (!action) continue;
            QJsonObject obj;
            obj[QStringLiteral("name")] = name;
            obj[QStringLiteral("text")] = action->text().remove(QLatin1Char('&'));
            obj[QStringLiteral("enabled")] = action->isEnabled();
            obj[QStringLiteral("checkable")] = action->isCheckable();
            if (action->isCheckable()) obj[QStringLiteral("checked")] = action->isChecked();
            actions.append(obj);
        }
        QJsonObject result;
        result[QStringLiteral("actions")] = actions;
        return result;
    });

    dispatcher.registerMethod(QStringLiteral("action.trigger"), [context](const QJsonObject& params) {
        const QString name = params.value(QStringLiteral("name")).toString();
        if (!AutomationActionPolicy::isAllowed(name)) {
            throw AutomationError(ErrorKind::NotAllowed, QStringLiteral("Action \"%1\" is not allowed").arg(name),
                                  QStringLiteral("Use action.list for the allowed actions"));
        }
        QAction* action = findAction(context, name);
        if (!action) {
            throw AutomationError(ErrorKind::NotFound, QStringLiteral("Action \"%1\" does not exist").arg(name));
        }
        // モーダルダイアログを開く動作でも応答が返るように、次のイベントループ反復で実行する
        QWidget* mainWindow = context.mainWindow;
        return AutomationInteractions::queue(
            {action,
             [action, mainWindow, name]() {
                 requireInteractive(mainWindow);
                 if (!action->isEnabled())
                     throw AutomationError(ErrorKind::InvalidState,
                                           QStringLiteral("Action \"%1\" is disabled in the current state").arg(name));
             },
             [action]() { action->trigger(); }},
            QJsonObject{{QStringLiteral("name"), name}, {QStringLiteral("triggered"), true}});
    });
}
