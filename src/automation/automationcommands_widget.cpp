/// @file automationcommands_widget.cpp
/// @brief 自動化 API の widget.* メソッド（部品の読み取り・クリック・値の設定・表のセル・ドックの表示）の実装

#include "automationcommands.h"
#include "automationinput.h"
#include "automationinteractions.h"
#include "automationparams.h"
#include "automationwidgetadapters.h"
#include "automationwidgets.h"

#include <QAbstractButton>
#include <QApplication>
#include <QClipboard>
#include <QDockWidget>
#include <QImage>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMainWindow>
#include <QPersistentModelIndex>
#include <QTableView>

namespace {

QWidget* targetWidget(const AutomationContext& context, const QJsonObject& params, bool requireVisible = true)
{
    return AutomationWidgets::requireWidget(context, params, requireVisible);
}

QJsonObject named(QWidget* widget)
{
    return {{QStringLiteral("object_name"), widget->objectName()}};
}

/// セルが表示されていて有効か（editable なら編集もできるか）。そうでなければ AutomationError
void requireCell(QTableView* table, const QPersistentModelIndex& index, bool editable)
{
    AutomationWidgets::requireInteractive(table);
    if (!index.isValid() || !(index.flags() & Qt::ItemIsEnabled) || (editable && !(index.flags() & Qt::ItemIsEditable))
        || table->isRowHidden(index.row()) || table->isColumnHidden(index.column()))
        throw AutomationError(ErrorKind::InvalidState, editable ? QStringLiteral("Cell is read-only, hidden or disabled")
                                                                : QStringLiteral("Cell is hidden or disabled"));
}

/// table のセルの入力欄を開いて text を入れる（commit なら Enter で確定する）
void editCell(QTableView* table, const QPersistentModelIndex& index, const QString& text, bool commit)
{
    table->scrollTo(index);
    table->setCurrentIndex(index);
    table->setFocus();
    table->edit(index);
    auto* editor = qobject_cast<QLineEdit*>(QApplication::focusWidget());
    // ダイアログを閉じた直後など、ウィンドウが非アクティブだと
    // 生成済みのdelegate入力欄にもフォーカスが移らないことがある。
    if (!editor || !table->isAncestorOf(editor)) {
        editor = nullptr;
        const auto edits = table->findChildren<QLineEdit*>();
        for (auto* candidate : edits) {
            if (candidate->isVisibleTo(table)) { editor = candidate; break; }
        }
    }
    if (!editor || !table->isAncestorOf(editor) || editor->isReadOnly())
        throw AutomationError(ErrorKind::InvalidState, QStringLiteral("The cell did not open a text editor"));
    if (AutomationWidgets::isSensitive(editor))
        throw AutomationError(ErrorKind::NotAllowed, QStringLiteral("Sensitive controls are not accessible through automation"));
    editor->setText(text);
    if (commit) {
        QKeyEvent enter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
        QApplication::sendEvent(editor, &enter);
    }
}

/// table のセルを通常のマウス入力でクリックする
void clickCell(QTableView* table, const QPersistentModelIndex& index)
{
    table->scrollTo(index);
    const QRect rect = table->visualRect(index);
    if (rect.isEmpty()) throw AutomationError(ErrorKind::InvalidState, QStringLiteral("Cell is not displayed"));
    AutomationInput::sendClick(table->viewport(), rect.center());
}

QTableView* requireTable(const AutomationContext& context, const QJsonObject& params)
{
    auto* table = qobject_cast<QTableView*>(targetWidget(context, params));
    if (!table || !table->model())
        throw AutomationError(ErrorKind::InvalidArgument, QStringLiteral("Widget must be a table with a model"));
    return table;
}

QPersistentModelIndex requireIndex(QTableView* table, const QJsonObject& params)
{
    const int row = AutomationParams::requireInt(params, QStringLiteral("row"), 0, table->model()->rowCount() - 1);
    const int column = AutomationParams::requireInt(params, QStringLiteral("column"), 0, table->model()->columnCount() - 1);
    return QPersistentModelIndex(table->model()->index(row, column));
}

} // namespace

void AutomationCommands::registerWidgetCommands(AutomationDispatcher& dispatcher, const AutomationContext& context)
{
    dispatcher.registerMethod(QStringLiteral("widget.text"), [context](const QJsonObject& params) {
        const QString target = params.value(QStringLiteral("target")).toString();
        const QString widget = params.value(QStringLiteral("widget")).toString();
        const int maxRows = params.value(QStringLiteral("max_rows")).toInt();
        const int maxWidgets = params.value(QStringLiteral("max_widgets")).toInt();
        const bool children = params.value(QStringLiteral("include_children")).toBool();
        QWidget* window = AutomationWidgets::requireWindow(context, target);
        QJsonObject result;
        result[QStringLiteral("window")] = AutomationWidgets::describeWindow(window, window == context.mainWindow);
        result[QStringLiteral("widgets")] = AutomationWidgets::describeWidgets(window, widget, maxRows, maxWidgets, children);
        if (!widget.isEmpty() && result.value(QStringLiteral("widgets")).toArray().isEmpty()) {
            throw AutomationError(ErrorKind::NotFound, QStringLiteral("No readable widget named \"%1\"").arg(widget));
        }
        return result;
    });

    dispatcher.registerMethod(QStringLiteral("widget.editCell"), [context](const QJsonObject& params) {
        auto* table = requireTable(context, params);
        const QPersistentModelIndex index = requireIndex(table, params);
        const QString text = params.value(QStringLiteral("text")).toString();
        const bool commit = params.value(QStringLiteral("commit")).toBool();
        return AutomationInteractions::queue(
            {table, [table, index]() { requireCell(table, index, true); },
             [table, index, text, commit]() { editCell(table, index, text, commit); }},
            named(table));
    });

    dispatcher.registerMethod(QStringLiteral("widget.showDock"), [context](const QJsonObject& params) {
        auto* dock = qobject_cast<QDockWidget*>(targetWidget(context, params, false));
        if (!dock) throw AutomationError(ErrorKind::InvalidArgument, QStringLiteral("Widget must be a dock"));
        return AutomationInteractions::queue(
            {dock, [dock]() { AutomationWidgets::requireInteractive(dock->parentWidget()); },
             [dock]() {
                 dock->show();
                 dock->raise();
             }},
            named(dock));
    });

    dispatcher.registerMethod(QStringLiteral("widget.click"), [context](const QJsonObject& params) {
        auto* button = qobject_cast<QAbstractButton*>(targetWidget(context, params));
        if (!button) throw AutomationError(ErrorKind::InvalidArgument, QStringLiteral("Widget must be a button"));
        return AutomationInteractions::queue(
            {button, [button]() { AutomationWidgets::requireInteractive(button); }, [button]() { button->click(); }},
            named(button));
    });

    dispatcher.registerMethod(QStringLiteral("widget.setValue"), [context](const QJsonObject& params) {
        QWidget* widget = targetWidget(context, params, false);
        if (widget->window() != AutomationWidgets::requireWindow(context,
                params.value(QStringLiteral("target")).toString()))
            throw AutomationError(ErrorKind::NotAllowed, QStringLiteral("Widget belongs to another window"));
        if (params.value(QStringLiteral("submit")).toBool() && !qobject_cast<QLineEdit*>(widget))
            throw AutomationError(ErrorKind::InvalidArgument, QStringLiteral("submit requires a line edit"));
        // 型ごとの検査と書き方は AutomationWidgetAdapters の表
        const std::function<void()> write = AutomationWidgetAdapters::writer(widget, params);
        return AutomationInteractions::queue(
            {widget, [widget]() { AutomationWidgets::requireInteractive(widget, false, true); }, write}, named(widget));
    });

    dispatcher.registerMethod(QStringLiteral("widget.clickCell"), [context](const QJsonObject& params) {
        auto* table = requireTable(context, params);
        const QPersistentModelIndex index = requireIndex(table, params);
        return AutomationInteractions::queue(
            {table, [table, index]() { requireCell(table, index, false); },
             [table, index]() { clickCell(table, index); }},
            named(table));
    });
}
