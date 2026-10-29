/// @file automationcommands_dialog.cpp
/// @brief 自動化 API の dialog.* メソッド（開いている画面の一覧・ボタンのクリック・閉じる）の実装

#include "automationcommands.h"
#include "automationdispatcher.h"
#include "automationinteractions.h"
#include "automationwidgets.h"

#include <QAbstractButton>
#include <QDialog>
#include <QMainWindow>

namespace {

using AutomationWidgets::requireInteractive;
using AutomationWidgets::requireWindow;

} // namespace

void AutomationCommands::registerDialogCommands(AutomationDispatcher& dispatcher, const AutomationContext& context)
{
    dispatcher.registerMethod(QStringLiteral("dialog.list"), [context](const QJsonObject&) {
        QJsonArray windows;
        const QList<QWidget*> visible = AutomationWidgets::visibleWindows();
        for (QWidget* w : visible) {
            windows.append(AutomationWidgets::describeWindow(w, w == context.mainWindow));
        }
        QJsonObject result;
        result[QStringLiteral("windows")] = windows;
        return result;
    });

    dispatcher.registerMethod(QStringLiteral("dialog.clickButton"), [context](const QJsonObject& params) {
        const QString target = params.value(QStringLiteral("target")).toString();
        QWidget* window = requireWindow(context, target);
        if (!qobject_cast<QDialog*>(window)) {
            throw AutomationError(ErrorKind::InvalidArgument, QStringLiteral("Target must be an open dialog"));
        }
        const QString widget = params.value(QStringLiteral("widget")).toString();
        const QString text = params.value(QStringLiteral("text")).toString();
        if (widget.isEmpty() == text.isEmpty()) {
            throw AutomationError(ErrorKind::InvalidArgument, QStringLiteral("Specify exactly one of widget or text"));
        }
        const int index = params.value(QStringLiteral("index")).toInt();
        QList<QAbstractButton*> matches;
        const auto buttons = window->findChildren<QAbstractButton*>();
        for (auto* button : buttons) {
            if (button->window() == window && button->isVisible()
                && ((!widget.isEmpty() && (button->objectName() == widget || AutomationWidgets::selector(button) == widget))
                    || (!text.isEmpty() && button->text() == text)))
                matches.append(button);
        }
        if (index >= matches.size()) throw AutomationError(ErrorKind::NotFound, QStringLiteral("No matching visible button"));
        auto* button = matches[index];
        return AutomationInteractions::queue(
            {button, [button]() { requireInteractive(button); }, [button]() { button->click(); }},
            QJsonObject{{QStringLiteral("object_name"), button->objectName()}, {QStringLiteral("text"), button->text()}});
    });

    dispatcher.registerMethod(QStringLiteral("dialog.close"), [context](const QJsonObject& params) {
        const QString target = params.value(QStringLiteral("target")).toString();
        QWidget* window = AutomationWidgets::findWindow(target);
        if (!window || window == context.mainWindow) {
            throw AutomationError(ErrorKind::NotFound, QStringLiteral("No open dialog matches \"%1\"").arg(target));
        }
        // モーダルダイアログの exec() を抜けさせるため、応答後に閉じる
        return AutomationInteractions::queue(
            {window, [window]() { requireInteractive(window); },
             [window]() {
                 if (auto* dialog = qobject_cast<QDialog*>(window)) dialog->reject();
                 else window->close();
             }},
            QJsonObject{{QStringLiteral("closed"), true}, {QStringLiteral("object_name"), window->objectName()},
                        {QStringLiteral("title"), window->windowTitle()}});
    });
}
