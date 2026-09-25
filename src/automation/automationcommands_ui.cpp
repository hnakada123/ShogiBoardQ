/// @file automationcommands_ui.cpp
/// @brief 自動化 API の action.* / screenshot.* / dialog.* / widget.* メソッドの実装

#include "automationactionpolicy.h"
#include "automationcommands.h"
#include "automationdispatcher.h"
#include "automationparams.h"
#include "automationwidgets.h"
#include "screenshotservice.h"

#include <QAction>
#include <QDialog>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QMainWindow>

namespace {

QAction* findAction(const AutomationContext& context, const QString& name)
{
    return context.mainWindow ? context.mainWindow->findChild<QAction*>(name) : nullptr;
}

QWidget* requireWindow(const AutomationContext& context, const QString& target)
{
    if (target.isEmpty() || target == QLatin1String("main")) return context.mainWindow;
    QWidget* window = AutomationWidgets::findWindow(target);
    if (!window) {
        throw AutomationError(AutomationErrorCode::NotFound, QStringLiteral("No open window matches \"%1\"").arg(target),
                              QStringLiteral("Use dialog.list to see the open windows"));
    }
    return window;
}

} // namespace

void AutomationCommands::registerUiCommands(AutomationDispatcher& dispatcher, const AutomationContext& context)
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
        const QString name = AutomationParams::requireString(params, QStringLiteral("name"));
        if (!AutomationActionPolicy::isAllowed(name)) {
            throw AutomationError(AutomationErrorCode::NotAllowed, QStringLiteral("Action \"%1\" is not allowed").arg(name),
                                  QStringLiteral("Use action.list for the allowed actions"));
        }
        QAction* action = findAction(context, name);
        if (!action) {
            throw AutomationError(AutomationErrorCode::NotFound, QStringLiteral("Action \"%1\" does not exist").arg(name));
        }
        if (!action->isEnabled()) {
            throw AutomationError(AutomationErrorCode::InvalidState,
                                  QStringLiteral("Action \"%1\" is disabled in the current state").arg(name));
        }
        // モーダルダイアログを開く動作でも応答が返るように、次のイベントループ反復で実行する
        AutomationDeferredCall::schedule([action]() { action->trigger(); }, action);
        QJsonObject result;
        result[QStringLiteral("name")] = name;
        result[QStringLiteral("triggered")] = true;
        return result;
    });

    dispatcher.registerMethod(QStringLiteral("screenshot.capture"), [context](const QJsonObject& params) {
        const QString target = AutomationParams::optionalString(params, QStringLiteral("target"), QStringLiteral("main"));
        const QString outputDir = AutomationParams::optionalString(params, QStringLiteral("output_dir"));
        QWidget* window = requireWindow(context, target);
        ScreenshotService service(context.mainWindow);
        if (!outputDir.isEmpty()) {
            if (!QFileInfo(outputDir).isAbsolute()) {
                throw AutomationError(AutomationErrorCode::FileError, QStringLiteral("output_dir must be absolute"));
            }
            service.setOutputDirectory(outputDir);
        }
        const QString label = window == context.mainWindow ? QStringLiteral("main")
                                                            : (window->objectName().isEmpty() ? window->windowTitle() : window->objectName());
        const QString path = window == context.mainWindow ? service.captureMainWindow(label)
                                                           : service.captureWidget(window, label);
        if (path.isEmpty()) {
            throw AutomationError(AutomationErrorCode::InternalError, QStringLiteral("Capturing the screenshot failed"));
        }
        const QImage image(path);
        QJsonObject result;
        result[QStringLiteral("path")] = path;
        result[QStringLiteral("width")] = image.width();
        result[QStringLiteral("height")] = image.height();
        result[QStringLiteral("target")] = label;
        return result;
    });

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

    dispatcher.registerMethod(QStringLiteral("dialog.close"), [context](const QJsonObject& params) {
        const QString target = AutomationParams::requireString(params, QStringLiteral("dialog"));
        QWidget* window = AutomationWidgets::findWindow(target);
        if (!window || window == context.mainWindow) {
            throw AutomationError(AutomationErrorCode::NotFound, QStringLiteral("No open dialog matches \"%1\"").arg(target));
        }
        // モーダルダイアログの exec() を抜けさせるため、応答後に閉じる
        AutomationDeferredCall::schedule([window]() {
            if (auto* dialog = qobject_cast<QDialog*>(window)) dialog->reject();
            else window->close();
        }, window);
        QJsonObject result;
        result[QStringLiteral("closed")] = true;
        result[QStringLiteral("object_name")] = window->objectName();
        result[QStringLiteral("title")] = window->windowTitle();
        return result;
    });

    dispatcher.registerMethod(QStringLiteral("widget.text"), [context](const QJsonObject& params) {
        const QString target = AutomationParams::optionalString(params, QStringLiteral("dialog"));
        const QString widget = AutomationParams::optionalString(params, QStringLiteral("widget"));
        const int maxRows = AutomationParams::optionalInt(params, QStringLiteral("max_rows"), 50, 1, 1000);
        QWidget* window = requireWindow(context, target);
        QJsonObject result;
        result[QStringLiteral("window")] = AutomationWidgets::describeWindow(window, window == context.mainWindow);
        result[QStringLiteral("widgets")] = AutomationWidgets::describeWidgets(window, widget, maxRows);
        if (!widget.isEmpty() && result.value(QStringLiteral("widgets")).toArray().isEmpty()) {
            throw AutomationError(AutomationErrorCode::NotFound, QStringLiteral("No readable widget named \"%1\"").arg(widget));
        }
        return result;
    });
}
