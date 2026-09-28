/// @file automationcommands_widget.cpp
/// @brief 検討タブなどの通常UI操作を自動化APIへ公開する

#include "automationcommands.h"
#include "automationparams.h"
#include "automationwidgets.h"

#include <QAbstractButton>
#include <QApplication>
#include <QComboBox>
#include <QCompleter>
#include <QColorDialog>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QListView>
#include <QSlider>
#include <QClipboard>
#include <QDockWidget>
#include <QImage>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPersistentModelIndex>
#include <QPointer>
#include <QPlainTextEdit>
#include <QSpinBox>
#include <QTableView>
#include <QTabWidget>
#include <QTextEdit>
#include <cmath>

namespace {

QWidget* targetWidget(const AutomationContext& context, const QJsonObject& params, bool requireVisible = true)
{
    return AutomationWidgets::requireWidget(context, params, requireVisible);
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
    dispatcher.registerMethod(QStringLiteral("widget.editCell"), [context](const QJsonObject& params) {
        auto* table = qobject_cast<QTableView*>(targetWidget(context, params));
        if (!table || !table->model())
            throw AutomationError(AutomationErrorCode::InvalidParams, QStringLiteral("Widget must be a table with a model"));
        const int row = AutomationParams::requireInt(params, QStringLiteral("row"), 0, table->model()->rowCount() - 1);
        const int column = AutomationParams::requireInt(params, QStringLiteral("column"), 0, table->model()->columnCount() - 1);
        const QString text = AutomationParams::optionalString(params, QStringLiteral("text"));
        const bool commit = AutomationParams::optionalBool(params, QStringLiteral("commit"), true);
        const QPersistentModelIndex index(table->model()->index(row, column));
        if (!(index.flags() & Qt::ItemIsEditable) || !(index.flags() & Qt::ItemIsEnabled)
            || table->isRowHidden(row) || table->isColumnHidden(column))
            throw AutomationError(AutomationErrorCode::InvalidState, QStringLiteral("Cell is read-only, hidden or disabled"));
        AutomationDeferredCall::schedule([table, index, text, commit]() {
            try {
                AutomationWidgets::requireInteractive(table);
                if (!index.isValid() || !(index.flags() & Qt::ItemIsEditable)
                    || !(index.flags() & Qt::ItemIsEnabled)
                    || table->isRowHidden(index.row()) || table->isColumnHidden(index.column())) return;
                table->scrollTo(index);
                table->setCurrentIndex(index);
                table->setFocus();
                table->edit(index);
                auto* editor = qobject_cast<QLineEdit*>(QApplication::focusWidget());
                // ダイアログを閉じた直後など、ウィンドウが非アクティブだと
                // 生成済みのdelegate入力欄にもフォーカスが移らないことがある。
                if (!editor || !table->isAncestorOf(editor)) {
                    editor = nullptr;
                    for (auto* candidate : table->findChildren<QLineEdit*>()) {
                        if (candidate->isVisibleTo(table)) { editor = candidate; break; }
                    }
                }
                if (!editor || !table->isAncestorOf(editor) || editor->isReadOnly()
                    || editor->echoMode() != QLineEdit::Normal) return;
                editor->setText(text);
                if (commit) {
                    QKeyEvent enter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
                    QApplication::sendEvent(editor, &enter);
                }
            } catch (const AutomationError&) { }
        }, table);
        return queued(table);
    });

    dispatcher.registerMethod(QStringLiteral("clipboard.get"), [](const QJsonObject& params) {
        const int maxChars = AutomationParams::optionalInt(params, QStringLiteral("max_chars"), 30000, 1, 1000000);
        const auto* clipboard = QApplication::clipboard();
        const QString text = clipboard->text();
        const QImage image = clipboard->image();
        return QJsonObject{{QStringLiteral("text"), text.left(maxChars)},
                           {QStringLiteral("truncated"), text.size() > maxChars},
                           {QStringLiteral("has_image"), !image.isNull()},
                           {QStringLiteral("image_width"), image.width()},
                           {QStringLiteral("image_height"), image.height()}};
    });

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
        const bool submit = AutomationParams::optionalBool(params, QStringLiteral("submit"), false);
        if (submit && !qobject_cast<QLineEdit*>(widget))
            throw AutomationError(AutomationErrorCode::InvalidParams, QStringLiteral("submit requires a line edit"));
        if (value.isNull() || value.isUndefined()) {
            throw AutomationError(AutomationErrorCode::InvalidParams, QStringLiteral("value is required and must not be null"));
        }
        std::function<void()> apply;
        if (auto* picker = qobject_cast<QColorDialog*>(widget)) {
            const QColor color(value.toString());
            if (!value.isString() || !color.isValid())
                throw AutomationError(AutomationErrorCode::InvalidParams, QStringLiteral("value must be a valid color"));
            apply = [picker, color]() { picker->setCurrentColor(color); };
        } else if (auto* combo = qobject_cast<QComboBox*>(widget)) {
            const int index = value.isString() ? combo->findText(value.toString(), Qt::MatchExactly)
                : AutomationParams::requireInt(params, QStringLiteral("value"), 0, combo->count() - 1);
            if (index < 0) throw AutomationError(AutomationErrorCode::InvalidParams, QStringLiteral("No matching combo item"));
            apply = [combo, index]() {
                QPointer<QComboBox> guard(combo);
                const QString text = combo->itemText(index);
                combo->setCurrentIndex(index);
                if (!guard) return;
                Q_EMIT combo->activated(index);
                if (guard) Q_EMIT combo->textActivated(text);
            };
        } else if (auto* lineEdit = qobject_cast<QLineEdit*>(widget)) {
            if (lineEdit->isReadOnly() || lineEdit->echoMode() != QLineEdit::Normal)
                throw AutomationError(AutomationErrorCode::InvalidState, QStringLiteral("Text field is read-only or protected"));
            if (!value.isString()) throw AutomationError(AutomationErrorCode::InvalidParams, QStringLiteral("value must be text"));
            const QString text = value.toString();
            if (text.size() > lineEdit->maxLength())
                throw AutomationError(AutomationErrorCode::InvalidParams, QStringLiteral("Text exceeds the field limit"));
            apply = [lineEdit, text, submit]() {
                if (lineEdit->isReadOnly() || lineEdit->echoMode() != QLineEdit::Normal) return;
                QPointer<QLineEdit> guard(lineEdit);
                lineEdit->selectAll();
                lineEdit->insert(text); // ユーザー編集と同じtextEditedも発行する（色入力等）。
                // 全文を一度に指定する操作では、補完候補が次のボタン操作を遮らないよう閉じる。
                if (guard && lineEdit->completer() && lineEdit->completer()->popup())
                    lineEdit->completer()->popup()->hide();
                if (submit && guard) {
                    QKeyEvent enter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
                    QApplication::sendEvent(lineEdit, &enter);
                }
            };
        } else if (auto* plainEdit = qobject_cast<QPlainTextEdit*>(widget)) {
            if (plainEdit->isReadOnly()) throw AutomationError(AutomationErrorCode::InvalidState, QStringLiteral("Text field is read-only"));
            if (!value.isString()) throw AutomationError(AutomationErrorCode::InvalidParams, QStringLiteral("value must be text"));
            apply = [plainEdit, value]() { plainEdit->setPlainText(value.toString()); };
        } else if (auto* richEdit = qobject_cast<QTextEdit*>(widget)) {
            if (richEdit->isReadOnly()) throw AutomationError(AutomationErrorCode::InvalidState, QStringLiteral("Text field is read-only"));
            if (!value.isString()) throw AutomationError(AutomationErrorCode::InvalidParams, QStringLiteral("value must be text"));
            apply = [richEdit, value]() { richEdit->setPlainText(value.toString()); };
        } else if (auto* spin = qobject_cast<QSpinBox*>(widget)) {
            if (spin->isReadOnly()) throw AutomationError(AutomationErrorCode::InvalidState, QStringLiteral("Spin box is read-only"));
            const int number = AutomationParams::requireInt(params, QStringLiteral("value"), spin->minimum(), spin->maximum());
            apply = [spin, number]() { spin->setValue(number); };
        } else if (auto* doubleSpin = qobject_cast<QDoubleSpinBox*>(widget)) {
            const double number = value.toDouble();
            if (doubleSpin->isReadOnly()) throw AutomationError(AutomationErrorCode::InvalidState, QStringLiteral("Spin box is read-only"));
            if (!value.isDouble() || !std::isfinite(number) || number < doubleSpin->minimum() || number > doubleSpin->maximum())
                throw AutomationError(AutomationErrorCode::InvalidParams, QStringLiteral("value is outside the spin box range"));
            apply = [doubleSpin, number]() { doubleSpin->setValue(number); };
        } else if (auto* slider = qobject_cast<QSlider*>(widget)) {
            const int number = AutomationParams::requireInt(params, QStringLiteral("value"), slider->minimum(), slider->maximum());
            apply = [slider, number]() { slider->setValue(number); };
        } else if (auto* group = qobject_cast<QGroupBox*>(widget); group && group->isCheckable()) {
            if (!value.isBool()) throw AutomationError(AutomationErrorCode::InvalidParams, QStringLiteral("value must be boolean"));
            apply = [group, value]() { group->setChecked(value.toBool()); };
        } else if (auto* list = qobject_cast<QListView*>(widget); list && list->model()) {
            const int row = AutomationParams::requireInt(params, QStringLiteral("value"), 0, list->model()->rowCount() - 1);
            const QPersistentModelIndex index(list->model()->index(row, 0));
            if (!(index.flags() & Qt::ItemIsEnabled) || !(index.flags() & Qt::ItemIsSelectable) || list->isRowHidden(row))
                throw AutomationError(AutomationErrorCode::InvalidState, QStringLiteral("List item is hidden or disabled"));
            apply = [list, index]() {
                if (!index.isValid() || !(index.flags() & Qt::ItemIsEnabled) || list->isRowHidden(index.row())) return;
                list->setCurrentIndex(index);
                list->selectionModel()->select(index, QItemSelectionModel::ClearAndSelect);
                list->scrollTo(index);
            };
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
