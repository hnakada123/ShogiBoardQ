/// @file automationcommands_widget.cpp
/// @brief 検討タブなどの通常UI操作を自動化APIへ公開する

#include "automationcommands.h"
#include "automationparams.h"
#include "automationwidgets.h"

#include <QAbstractButton>
#include <QApplication>
#include <QComboBox>
#include <QDockWidget>
#include <QMouseEvent>
#include <QPersistentModelIndex>
#include <QSpinBox>
#include <QTableView>
#include <QTabWidget>

namespace {

QWidget* targetWidget(const AutomationContext& context, const QJsonObject& params, bool requireVisible = true)
{
    const QString target = AutomationParams::optionalString(params, QStringLiteral("target"), QStringLiteral("main"));
    QWidget* window = AutomationWidgets::requireWindow(context, target);
    const QString name = AutomationParams::requireString(params, QStringLiteral("widget"));
    QList<QWidget*> matches;
    if (window) {
        for (auto* widget : window->findChildren<QWidget*>(name)) {
            if (widget->window() == window || (!requireVisible && qobject_cast<QDockWidget*>(widget)))
                matches.append(widget);
        }
    }
    if (matches.size() != 1) {
        throw AutomationError(AutomationErrorCode::NotFound, QStringLiteral("Expected one widget named \"%1\"").arg(name));
    }
    if (requireVisible) AutomationWidgets::requireInteractive(matches.first());
    return matches.first();
}

QJsonObject queued(QWidget* widget)
{
    return {{QStringLiteral("queued"), true}, {QStringLiteral("object_name"), widget->objectName()}};
}

void clickCell(QTableView* table, const QPersistentModelIndex& index)
{
    AutomationWidgets::requireInteractive(table);
    if (!index.isValid() || !(index.flags() & Qt::ItemIsEnabled)
        || table->isRowHidden(index.row()) || table->isColumnHidden(index.column())) return;
    table->scrollTo(index);
    const QRect rect = table->visualRect(index);
    if (rect.isEmpty()) return;
    QWidget* viewport = table->viewport();
    const QPointF pos(rect.center());
    const QPointF global(viewport->mapToGlobal(pos.toPoint()));
    QMouseEvent press(QEvent::MouseButtonPress, pos, global, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QMouseEvent release(QEvent::MouseButtonRelease, pos, global, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(viewport, &press);
    QApplication::sendEvent(viewport, &release);
}

} // namespace

void AutomationCommands::registerWidgetCommands(AutomationDispatcher& dispatcher, const AutomationContext& context)
{
    dispatcher.registerMethod(QStringLiteral("widget.showDock"), [context](const QJsonObject& params) {
        auto* dock = qobject_cast<QDockWidget*>(targetWidget(context, params, false));
        if (!dock) throw AutomationError(AutomationErrorCode::InvalidParams, QStringLiteral("Widget must be a dock"));
        AutomationWidgets::requireInteractive(dock->parentWidget());
        AutomationDeferredCall::schedule([dock]() {
            try {
                AutomationWidgets::requireInteractive(dock->parentWidget());
                dock->show();
                dock->raise();
            } catch (const AutomationError&) { }
        }, dock);
        return queued(dock);
    });

    dispatcher.registerMethod(QStringLiteral("widget.click"), [context](const QJsonObject& params) {
        auto* button = qobject_cast<QAbstractButton*>(targetWidget(context, params));
        if (!button) throw AutomationError(AutomationErrorCode::InvalidParams, QStringLiteral("Widget must be a button"));
        AutomationDeferredCall::schedule([button]() {
            try { AutomationWidgets::requireInteractive(button); button->click(); }
            catch (const AutomationError&) { }
        }, button);
        return queued(button);
    });

    dispatcher.registerMethod(QStringLiteral("widget.setValue"), [context](const QJsonObject& params) {
        QWidget* widget = targetWidget(context, params);
        const QJsonValue value = params.value(QStringLiteral("value"));
        if (value.isNull() || value.isUndefined()) {
            throw AutomationError(AutomationErrorCode::InvalidParams, QStringLiteral("value is required and must not be null"));
        }
        std::function<void()> apply;
        if (auto* combo = qobject_cast<QComboBox*>(widget)) {
            const int index = value.isString() ? combo->findText(value.toString(), Qt::MatchExactly)
                : AutomationParams::requireInt(params, QStringLiteral("value"), 0, combo->count() - 1);
            if (index < 0) throw AutomationError(AutomationErrorCode::InvalidParams, QStringLiteral("No matching combo item"));
            apply = [combo, index]() { combo->setCurrentIndex(index); };
        } else if (auto* spin = qobject_cast<QSpinBox*>(widget)) {
            if (spin->isReadOnly()) throw AutomationError(AutomationErrorCode::InvalidState, QStringLiteral("Spin box is read-only"));
            const int number = AutomationParams::requireInt(params, QStringLiteral("value"), spin->minimum(), spin->maximum());
            apply = [spin, number]() { spin->setValue(number); };
        } else if (auto* button = qobject_cast<QAbstractButton*>(widget); button && button->isCheckable()) {
            if (!value.isBool()) throw AutomationError(AutomationErrorCode::InvalidParams, QStringLiteral("value must be boolean"));
            const bool checked = value.toBool();
            if (!checked && button->autoExclusive()) {
                throw AutomationError(AutomationErrorCode::InvalidParams, QStringLiteral("Select another radio button instead"));
            }
            apply = [button, checked]() { if (button->isChecked() != checked) button->click(); };
        } else if (auto* tabs = qobject_cast<QTabWidget*>(widget)) {
            const int index = AutomationParams::requireInt(params, QStringLiteral("value"), 0, tabs->count() - 1);
            if (!tabs->isTabEnabled(index) || !tabs->isTabVisible(index)) {
                throw AutomationError(AutomationErrorCode::InvalidState, QStringLiteral("Tab is hidden or disabled"));
            }
            apply = [tabs, index]() { tabs->setCurrentIndex(index); };
        } else {
            throw AutomationError(AutomationErrorCode::InvalidParams, QStringLiteral("Unsupported widget type"));
        }
        AutomationDeferredCall::schedule([widget, apply]() {
            try { AutomationWidgets::requireInteractive(widget); apply(); }
            catch (const AutomationError&) { }
        }, widget);
        return queued(widget);
    });

    dispatcher.registerMethod(QStringLiteral("widget.clickCell"), [context](const QJsonObject& params) {
        auto* table = qobject_cast<QTableView*>(targetWidget(context, params));
        if (!table || !table->model()) {
            throw AutomationError(AutomationErrorCode::InvalidParams, QStringLiteral("Widget must be a table with a model"));
        }
        const int row = AutomationParams::requireInt(params, QStringLiteral("row"), 0, table->model()->rowCount() - 1);
        const int column = AutomationParams::requireInt(params, QStringLiteral("column"), 0, table->model()->columnCount() - 1);
        const QPersistentModelIndex index(table->model()->index(row, column));
        if (table->isRowHidden(row) || table->isColumnHidden(column) || !(index.flags() & Qt::ItemIsEnabled)) {
            throw AutomationError(AutomationErrorCode::InvalidState, QStringLiteral("Cell is hidden or disabled"));
        }
        AutomationDeferredCall::schedule([table, index]() {
            try { clickCell(table, index); }
            catch (const AutomationError&) { }
        }, table);
        return queued(table);
    });
}
