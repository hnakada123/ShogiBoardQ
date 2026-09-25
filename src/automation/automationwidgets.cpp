/// @file automationwidgets.cpp
/// @brief 自動化 API のウィンドウ列挙・ウィジェット内容取得ヘルパの実装

#include "automationwidgets.h"

#include <QAbstractButton>
#include <QAbstractItemModel>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMainWindow>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QSpinBox>
#include <QTabWidget>
#include <QTableView>
#include <QTableWidget>
#include <QTextEdit>
#include <QTreeWidget>

namespace {

QJsonArray tableRows(const QAbstractItemModel* model, const QHeaderView* header, int maxRows)
{
    QJsonArray rows;
    if (!model) return rows;
    const int columns = model->columnCount();
    QJsonArray head;
    for (int c = 0; c < columns; ++c) {
        head.append(model->headerData(c, Qt::Horizontal).toString());
    }
    Q_UNUSED(header)
    rows.append(head);
    const int count = qMin(model->rowCount(), maxRows);
    for (int r = 0; r < count; ++r) {
        QJsonArray row;
        for (int c = 0; c < columns; ++c) {
            row.append(model->index(r, c).data(Qt::DisplayRole).toString());
        }
        rows.append(row);
    }
    return rows;
}

bool describeOne(QWidget* w, int maxRows, QJsonObject& obj)
{
    if (auto* table = qobject_cast<QTableWidget*>(w)) {
        obj[QStringLiteral("rows")] = tableRows(table->model(), table->horizontalHeader(), maxRows);
        obj[QStringLiteral("row_count")] = table->rowCount();
        return true;
    }
    if (auto* view = qobject_cast<QTableView*>(w)) {
        obj[QStringLiteral("rows")] = tableRows(view->model(), view->horizontalHeader(), maxRows);
        obj[QStringLiteral("row_count")] = view->model() ? view->model()->rowCount() : 0;
        return true;
    }
    if (auto* list = qobject_cast<QListWidget*>(w)) {
        QJsonArray items;
        for (int i = 0; i < qMin(list->count(), maxRows); ++i) items.append(list->item(i)->text());
        obj[QStringLiteral("items")] = items;
        obj[QStringLiteral("row_count")] = list->count();
        return true;
    }
    if (auto* tree = qobject_cast<QTreeWidget*>(w)) {
        QJsonArray items;
        for (int i = 0; i < qMin(tree->topLevelItemCount(), maxRows); ++i) {
            QStringList cells;
            for (int c = 0; c < tree->columnCount(); ++c) cells.append(tree->topLevelItem(i)->text(c));
            items.append(cells.join(QStringLiteral(" | ")));
        }
        obj[QStringLiteral("items")] = items;
        return true;
    }
    if (auto* combo = qobject_cast<QComboBox*>(w)) {
        QJsonArray items;
        for (int i = 0; i < qMin(combo->count(), maxRows); ++i) items.append(combo->itemText(i));
        obj[QStringLiteral("text")] = combo->currentText();
        obj[QStringLiteral("items")] = items;
        return true;
    }
    if (auto* tabs = qobject_cast<QTabWidget*>(w)) {
        QJsonArray items;
        for (int i = 0; i < tabs->count(); ++i) items.append(tabs->tabText(i));
        obj[QStringLiteral("text")] = tabs->tabText(tabs->currentIndex());
        obj[QStringLiteral("items")] = items;
        return true;
    }
    if (auto* label = qobject_cast<QLabel*>(w)) {
        if (label->text().isEmpty()) return false;
        obj[QStringLiteral("text")] = label->text();
        return true;
    }
    if (auto* edit = qobject_cast<QLineEdit*>(w)) {
        obj[QStringLiteral("text")] = edit->text();
        obj[QStringLiteral("placeholder")] = edit->placeholderText();
        return true;
    }
    if (auto* edit = qobject_cast<QTextEdit*>(w)) {
        obj[QStringLiteral("text")] = edit->toPlainText();
        return true;
    }
    if (auto* edit = qobject_cast<QPlainTextEdit*>(w)) {
        obj[QStringLiteral("text")] = edit->toPlainText();
        return true;
    }
    if (auto* spin = qobject_cast<QSpinBox*>(w)) {
        obj[QStringLiteral("value")] = spin->value();
        obj[QStringLiteral("text")] = spin->text();
        return true;
    }
    if (auto* spin = qobject_cast<QDoubleSpinBox*>(w)) {
        obj[QStringLiteral("value")] = spin->value();
        obj[QStringLiteral("text")] = spin->text();
        return true;
    }
    if (auto* progress = qobject_cast<QProgressBar*>(w)) {
        obj[QStringLiteral("value")] = progress->value();
        obj[QStringLiteral("text")] = progress->text();
        return true;
    }
    if (auto* group = qobject_cast<QGroupBox*>(w)) {
        if (group->title().isEmpty()) return false;
        obj[QStringLiteral("text")] = group->title();
        if (group->isCheckable()) obj[QStringLiteral("checked")] = group->isChecked();
        return true;
    }
    if (auto* button = qobject_cast<QAbstractButton*>(w)) {
        if (button->text().isEmpty()) return false;
        obj[QStringLiteral("text")] = button->text();
        if (button->isCheckable()) obj[QStringLiteral("checked")] = button->isChecked();
        return true;
    }
    return false;
}

} // namespace

QList<QWidget*> AutomationWidgets::visibleWindows()
{
    QList<QWidget*> result;
    const QWidgetList windows = QApplication::topLevelWidgets();
    for (QWidget* w : windows) {
        if (!w->isVisible() || w->isHidden()) continue;
        if (qobject_cast<QMainWindow*>(w) || qobject_cast<QDialog*>(w) || w->isWindow()) {
            // ツールチップやメニューのポップアップは除外する
            if (w->windowType() == Qt::ToolTip || w->windowType() == Qt::Popup) continue;
            result.append(w);
        }
    }
    return result;
}

QWidget* AutomationWidgets::findWindow(const QString& nameOrTitle)
{
    const QList<QWidget*> windows = visibleWindows();
    for (QWidget* w : windows) {
        if (w->objectName() == nameOrTitle) return w;
    }
    for (QWidget* w : windows) {
        if (!nameOrTitle.isEmpty() && w->windowTitle().contains(nameOrTitle, Qt::CaseInsensitive)) return w;
    }
    return nullptr;
}

QJsonObject AutomationWidgets::describeWindow(const QWidget* window, bool isMain)
{
    QJsonObject obj;
    obj[QStringLiteral("object_name")] = window->objectName();
    obj[QStringLiteral("class")] = QString::fromLatin1(window->metaObject()->className());
    obj[QStringLiteral("title")] = window->windowTitle();
    obj[QStringLiteral("visible")] = window->isVisible();
    obj[QStringLiteral("modal")] = window->isModal();
    obj[QStringLiteral("active")] = QApplication::activeWindow() == window;
    obj[QStringLiteral("main")] = isMain;
    obj[QStringLiteral("width")] = window->width();
    obj[QStringLiteral("height")] = window->height();
    return obj;
}

QJsonArray AutomationWidgets::describeWidgets(QWidget* root, const QString& objectName, int maxRows, int maxWidgets)
{
    QJsonArray array;
    if (!root) return array;
    QList<QWidget*> targets;
    if (!objectName.isEmpty()) {
        if (root->objectName() == objectName) targets.append(root);
        else if (auto* child = root->findChild<QWidget*>(objectName)) targets.append(child);
    } else {
        targets = root->findChildren<QWidget*>();
    }
    for (QWidget* w : std::as_const(targets)) {
        if (array.size() >= maxWidgets) break;
        if (objectName.isEmpty() && !w->isVisible()) continue;
        QJsonObject obj;
        if (!describeOne(w, maxRows, obj)) continue;
        obj[QStringLiteral("object_name")] = w->objectName();
        obj[QStringLiteral("class")] = QString::fromLatin1(w->metaObject()->className());
        obj[QStringLiteral("enabled")] = w->isEnabled();
        if (!w->isVisible()) obj[QStringLiteral("visible")] = false;
        array.append(obj);
    }
    return array;
}
