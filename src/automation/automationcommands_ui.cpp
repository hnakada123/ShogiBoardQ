/// @file automationcommands_ui.cpp
/// @brief 自動化 API の action.* / screenshot.* / dialog.* / widget.* メソッドの実装

#include "automationactionpolicy.h"
#include "automationcommands.h"
#include "automationdispatcher.h"
#include "automationparams.h"
#include "automationwidgets.h"
#include "screenshotservice.h"
#include "shogiview.h"

#include <QAction>
#include <QAbstractButton>
#include <QApplication>
#include <QDialog>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QMainWindow>
#include <QLineEdit>
#include <QMouseEvent>

namespace {

QAction* findAction(const AutomationContext& context, const QString& name)
{
    return context.mainWindow ? context.mainWindow->findChild<QAction*>(name) : nullptr;
}

using AutomationWidgets::requireWindow;
using AutomationWidgets::requireInteractive;

QPoint squareClickPosition(ShogiView* view, const QPoint& square)
{
    // 入力変換を逆引きして、回転・盤サイズ・駒台の表示位置に追従する。
    const QSize cell = view->fieldSize();
    const int stepX = qMax(1, cell.width() / 4);
    const int stepY = qMax(1, cell.height() / 4);
    QRect area;
    for (int y = 0; y < view->height(); y += stepY) {
        for (int x = 0; x < view->width(); x += stepX) {
            if (view->clickedSquare({x, y}) == square) area |= QRect(x, y, 1, 1);
        }
    }
    if (area.isEmpty() || view->clickedSquare(area.center()) != square) {
        throw AutomationError(AutomationErrorCode::InvalidParams, QStringLiteral("Square is not displayed on this board"));
    }
    return area.center();
}

} // namespace

void AutomationCommands::registerUiCommands(AutomationDispatcher& dispatcher, const AutomationContext& context)
{
    dispatcher.registerMethod(QStringLiteral("board.click"), [context](const QJsonObject& params) {
        // 引数は AutomationMethodSpecs の表で検査済み（既定値も補ってある）
        const QString target = params.value(QStringLiteral("target")).toString();
        const int file = params.value(QStringLiteral("file")).toInt();
        const int rank = params.value(QStringLiteral("rank")).toInt();
        if ((file == 10 && rank > 8) || (file == 11 && rank < 2)) {
            throw AutomationError(AutomationErrorCode::InvalidParams, QStringLiteral("Invalid hand piece coordinate"));
        }
        const QString button = params.value(QStringLiteral("button")).toString();
        QWidget* window = requireWindow(context, target);
        ShogiView* view = nullptr;
        if (window) {
            const auto views = window->findChildren<ShogiView*>();
            for (auto* candidate : views) {
                if (candidate->window() == window && candidate->isVisible()) { view = candidate; break; }
            }
        }
        if (!view) throw AutomationError(AutomationErrorCode::NotFound, QStringLiteral("No visible board in the target window"));
        requireInteractive(view);
        const QPoint square(file, rank);
        squareClickPosition(view, square); // 不正な座標は応答前に拒否する。
        const auto mouseButton = button == QLatin1String("left") ? Qt::LeftButton : Qt::RightButton;
        AutomationDeferredCall::schedule([view, square, mouseButton]() {
            try {
                requireInteractive(view);
                const QPointF pos(squareClickPosition(view, square));
                const QPointF global(view->mapToGlobal(pos.toPoint()));
                QMouseEvent move(QEvent::MouseMove, pos, global, Qt::NoButton, Qt::NoButton, Qt::NoModifier);
                QMouseEvent press(QEvent::MouseButtonPress, pos, global, mouseButton, mouseButton, Qt::NoModifier);
                QMouseEvent release(QEvent::MouseButtonRelease, pos, global, mouseButton, Qt::NoButton, Qt::NoModifier);
                QApplication::sendEvent(view, &move);
                QApplication::sendEvent(view, &press);
                QApplication::sendEvent(view, &release);
            } catch (const AutomationError&) {
                // 応答後に対象が非表示になった場合は、背後の盤面へ入力しない。
            }
        }, view);
        return QJsonObject{{QStringLiteral("target"), target}, {QStringLiteral("file"), file},
                           {QStringLiteral("rank"), rank}, {QStringLiteral("queued"), true}};
    });

    dispatcher.registerMethod(QStringLiteral("dialog.clickButton"), [context](const QJsonObject& params) {
        const QString target = params.value(QStringLiteral("target")).toString();
        QWidget* window = requireWindow(context, target);
        if (!qobject_cast<QDialog*>(window)) {
            throw AutomationError(AutomationErrorCode::InvalidParams, QStringLiteral("Target must be an open dialog"));
        }
        const QString widget = params.value(QStringLiteral("widget")).toString();
        const QString text = params.value(QStringLiteral("text")).toString();
        if (widget.isEmpty() == text.isEmpty()) {
            throw AutomationError(AutomationErrorCode::InvalidParams, QStringLiteral("Specify exactly one of widget or text"));
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
        if (index >= matches.size()) throw AutomationError(AutomationErrorCode::NotFound, QStringLiteral("No matching visible button"));
        auto* button = matches[index];
        requireInteractive(button);
        AutomationDeferredCall::schedule([button]() {
            try { requireInteractive(button); button->click(); }
            catch (const AutomationError&) { }
        }, button);
        return QJsonObject{{QStringLiteral("queued"), true}, {QStringLiteral("object_name"), button->objectName()},
                           {QStringLiteral("text"), button->text()}};
    });

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
        requireInteractive(context.mainWindow);
        // モーダルダイアログを開く動作でも応答が返るように、次のイベントループ反復で実行する
        AutomationDeferredCall::schedule([action, context]() {
            try {
                requireInteractive(context.mainWindow);
                if (action->isEnabled()) action->trigger();
            } catch (const AutomationError&) { }
        }, action);
        QJsonObject result;
        result[QStringLiteral("name")] = name;
        result[QStringLiteral("triggered")] = true;
        return result;
    });

    dispatcher.registerMethod(QStringLiteral("screenshot.capture"), [context](const QJsonObject& params) {
        const QString target = params.value(QStringLiteral("target")).toString();
        const QString outputDir = params.value(QStringLiteral("output_dir")).toString();
        QWidget* window = requireWindow(context, target);
        // A revealed secret must not leak through a screenshot of this window or a child dialog.
        for (QWidget* visible : AutomationWidgets::visibleWindows()) {
            const auto edits = visible->findChildren<QLineEdit*>();
            for (auto* edit : edits) {
                if (edit->isVisible() && AutomationWidgets::isSensitive(edit)
                    && edit->echoMode() == QLineEdit::Normal && !edit->text().isEmpty())
                    throw AutomationError(AutomationErrorCode::NotAllowed,
                                          QStringLiteral("Hide sensitive text before taking a screenshot"));
            }
        }
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
        const QString target = params.value(QStringLiteral("target")).toString();
        QWidget* window = AutomationWidgets::findWindow(target);
        if (!window || window == context.mainWindow) {
            throw AutomationError(AutomationErrorCode::NotFound, QStringLiteral("No open dialog matches \"%1\"").arg(target));
        }
        requireInteractive(window);
        // モーダルダイアログの exec() を抜けさせるため、応答後に閉じる
        AutomationDeferredCall::schedule([window]() {
            try {
                requireInteractive(window);
                if (auto* dialog = qobject_cast<QDialog*>(window)) dialog->reject();
                else window->close();
            } catch (const AutomationError&) { }
        }, window);
        QJsonObject result;
        result[QStringLiteral("closed")] = true;
        result[QStringLiteral("object_name")] = window->objectName();
        result[QStringLiteral("title")] = window->windowTitle();
        return result;
    });

    dispatcher.registerMethod(QStringLiteral("widget.text"), [context](const QJsonObject& params) {
        const QString target = params.value(QStringLiteral("target")).toString();
        const QString widget = params.value(QStringLiteral("widget")).toString();
        const int maxRows = params.value(QStringLiteral("max_rows")).toInt();
        const int maxWidgets = params.value(QStringLiteral("max_widgets")).toInt();
        const bool children = params.value(QStringLiteral("include_children")).toBool();
        QWidget* window = requireWindow(context, target);
        QJsonObject result;
        result[QStringLiteral("window")] = AutomationWidgets::describeWindow(window, window == context.mainWindow);
        result[QStringLiteral("widgets")] = AutomationWidgets::describeWidgets(window, widget, maxRows, maxWidgets, children);
        if (!widget.isEmpty() && result.value(QStringLiteral("widgets")).toArray().isEmpty()) {
            throw AutomationError(AutomationErrorCode::NotFound, QStringLiteral("No readable widget named \"%1\"").arg(widget));
        }
        return result;
    });
}
