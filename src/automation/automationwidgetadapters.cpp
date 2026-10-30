/// @file automationwidgetadapters.cpp
/// @brief 自動化 API のウィジェットの読み書き（型ごとのアダプタの表）の実装

#include "automationwidgetadapters.h"

#include "automationdispatcher.h"
#include "automationparams.h"
#include "automationwidgets.h"
#include "branchtreeitemroles.h"
#include "shogiboard.h"
#include "shogiview.h"
#include "shogiviewhighlighting.h"

#include <QAbstractButton>
#include <QAbstractItemModel>
#include <QApplication>
#include <QColorDialog>
#include <QComboBox>
#include <QCompleter>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGroupBox>
#include <QJsonArray>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QPersistentModelIndex>
#include <QPlainTextEdit>
#include <QPointer>
#include <QProgressBar>
#include <QSlider>
#include <QSpinBox>
#include <QTabWidget>
#include <QTableView>
#include <QTextEdit>
#include <QTreeWidget>
#include <cmath>

namespace AutomationWidgetAdapters {

namespace {

using Write = std::function<void()>;

[[noreturn]] void fail(ErrorKind kind, const QString& message)
{
    throw AutomationError(kind, message);
}

QJsonValue valueOf(const QJsonObject& params)
{
    return params.value(QStringLiteral("value"));
}

QString requireText(const QJsonObject& params)
{
    const QJsonValue value = valueOf(params);
    if (!value.isString()) fail(ErrorKind::InvalidArgument, QStringLiteral("value must be text"));
    return value.toString();
}

bool requireBool(const QJsonObject& params)
{
    const QJsonValue value = valueOf(params);
    if (!value.isBool()) fail(ErrorKind::InvalidArgument, QStringLiteral("value must be boolean"));
    return value.toBool();
}

int requireIntValue(const QJsonObject& params, int minimum, int maximum)
{
    return AutomationParams::requireInt(params, QStringLiteral("value"), minimum, maximum);
}

QJsonArray tableRows(const QAbstractItemModel* model, int maxRows)
{
    QJsonArray rows;
    if (!model) return rows;
    const int columns = model->columnCount();
    QJsonArray head;
    for (int c = 0; c < columns; ++c) head.append(model->headerData(c, Qt::Horizontal).toString());
    rows.append(head);
    const int count = qMin(model->rowCount(), maxRows);
    for (int r = 0; r < count; ++r) {
        QJsonArray row;
        for (int c = 0; c < columns; ++c) row.append(model->index(r, c).data(Qt::DisplayRole).toString());
        rows.append(row);
    }
    return rows;
}

// ---- 読み（読めなければ false） ----

bool readColorDialog(QWidget* w, int, QJsonObject& obj)
{
    obj[QStringLiteral("color")] = static_cast<QColorDialog*>(w)->currentColor().name(QColor::HexArgb);
    return true;
}

bool readMenu(QWidget* w, int, QJsonObject& obj)
{
    obj[QStringLiteral("text")] = static_cast<QMenu*>(w)->title();
    return true;
}

bool readGraphicsView(QWidget* w, int maxRows, QJsonObject& obj)
{
    auto* graphics = static_cast<QGraphicsView*>(w);
    QJsonArray nodes;
    if (graphics->scene()) {
        const auto items = graphics->scene()->items(Qt::AscendingOrder);
        for (auto* item : items) {
            const QVariant id = item->data(BranchTreeItemRoles::NodeId);
            if (!id.isValid() || nodes.size() >= maxRows) continue;
            nodes.append(QJsonObject{{QStringLiteral("id"), id.toInt()},
                                     {QStringLiteral("row"), item->data(BranchTreeItemRoles::Row).toInt()},
                                     {QStringLiteral("ply"), item->data(BranchTreeItemRoles::Ply).toInt()}});
        }
    }
    obj[QStringLiteral("nodes")] = nodes;
    return true;
}

bool readDock(QWidget* w, int, QJsonObject& obj)
{
    auto* dock = static_cast<QDockWidget*>(w);
    obj[QStringLiteral("text")] = dock->windowTitle();
    obj[QStringLiteral("floating")] = dock->isFloating();
    return true;
}

bool readBoard(QWidget* w, int, QJsonObject& obj)
{
    auto* board = static_cast<ShogiView*>(w);
    obj[QStringLiteral("flipped")] = board->flipMode();
    obj[QStringLiteral("square_size")] = board->squareSize();
    if (board->board()) obj[QStringLiteral("board_sfen")] = board->board()->position().boardSfen();
    QJsonArray arrows;
    for (const auto& arrow : board->highlighting()->arrows()) {
        arrows.append(QJsonObject{{QStringLiteral("from_file"), arrow.fromFile},
                                  {QStringLiteral("from_rank"), arrow.fromRank},
                                  {QStringLiteral("to_file"), arrow.toFile},
                                  {QStringLiteral("to_rank"), arrow.toRank},
                                  {QStringLiteral("priority"), arrow.priority},
                                  {QStringLiteral("drop_piece"), QString(pieceLetter(arrow.dropPiece))}});
    }
    obj[QStringLiteral("arrows")] = arrows;
    return true;
}

bool readTable(QWidget* w, int maxRows, QJsonObject& obj)
{
    auto* view = static_cast<QTableView*>(w);
    obj[QStringLiteral("rows")] = tableRows(view->model(), maxRows);
    obj[QStringLiteral("row_count")] = view->model() ? view->model()->rowCount() : 0;
    return true;
}

bool readListWidget(QWidget* w, int maxRows, QJsonObject& obj)
{
    auto* list = static_cast<QListWidget*>(w);
    QJsonArray items;
    for (int i = 0; i < qMin(list->count(), maxRows); ++i) items.append(list->item(i)->text());
    obj[QStringLiteral("items")] = items;
    obj[QStringLiteral("row_count")] = list->count();
    obj[QStringLiteral("current_index")] = list->currentRow();
    return true;
}

bool readTreeWidget(QWidget* w, int maxRows, QJsonObject& obj)
{
    auto* tree = static_cast<QTreeWidget*>(w);
    QJsonArray items;
    for (int i = 0; i < qMin(tree->topLevelItemCount(), maxRows); ++i) {
        QStringList cells;
        for (int c = 0; c < tree->columnCount(); ++c) cells.append(tree->topLevelItem(i)->text(c));
        items.append(cells.join(QStringLiteral(" | ")));
    }
    obj[QStringLiteral("items")] = items;
    return true;
}

bool readCombo(QWidget* w, int maxRows, QJsonObject& obj)
{
    auto* combo = static_cast<QComboBox*>(w);
    QJsonArray items;
    for (int i = 0; i < qMin(combo->count(), maxRows); ++i) items.append(combo->itemText(i));
    obj[QStringLiteral("text")] = combo->currentText();
    obj[QStringLiteral("current_index")] = combo->currentIndex();
    obj[QStringLiteral("items")] = items;
    return true;
}

bool readTabs(QWidget* w, int, QJsonObject& obj)
{
    auto* tabs = static_cast<QTabWidget*>(w);
    QJsonArray items;
    for (int i = 0; i < tabs->count(); ++i) items.append(tabs->tabText(i));
    obj[QStringLiteral("text")] = tabs->tabText(tabs->currentIndex());
    obj[QStringLiteral("current_index")] = tabs->currentIndex();
    obj[QStringLiteral("items")] = items;
    return true;
}

bool readLabel(QWidget* w, int, QJsonObject& obj)
{
    auto* label = static_cast<QLabel*>(w);
    const QVariant fullText = label->property("fullText");
    const QString text = fullText.isValid() ? fullText.toString() : label->text();
    if (text.isEmpty()) return false;
    obj[QStringLiteral("text")] = text;
    return true;
}

bool readLineEdit(QWidget* w, int, QJsonObject& obj)
{
    auto* edit = static_cast<QLineEdit*>(w);
    obj[QStringLiteral("text")] = !AutomationWidgets::isSensitive(edit) ? edit->text() : QStringLiteral("*****");
    obj[QStringLiteral("placeholder")] = edit->placeholderText();
    return true;
}

bool readTextEdit(QWidget* w, int, QJsonObject& obj)
{
    obj[QStringLiteral("text")] = static_cast<QTextEdit*>(w)->toPlainText();
    return true;
}

bool readPlainTextEdit(QWidget* w, int, QJsonObject& obj)
{
    obj[QStringLiteral("text")] = static_cast<QPlainTextEdit*>(w)->toPlainText();
    return true;
}

bool readSpin(QWidget* w, int, QJsonObject& obj)
{
    auto* spin = static_cast<QSpinBox*>(w);
    obj[QStringLiteral("value")] = spin->value();
    obj[QStringLiteral("text")] = spin->text();
    return true;
}

bool readDoubleSpin(QWidget* w, int, QJsonObject& obj)
{
    auto* spin = static_cast<QDoubleSpinBox*>(w);
    obj[QStringLiteral("value")] = spin->value();
    obj[QStringLiteral("text")] = spin->text();
    return true;
}

bool readSlider(QWidget* w, int, QJsonObject& obj)
{
    auto* slider = static_cast<QSlider*>(w);
    obj[QStringLiteral("value")] = slider->value();
    obj[QStringLiteral("minimum")] = slider->minimum();
    obj[QStringLiteral("maximum")] = slider->maximum();
    return true;
}

bool readProgress(QWidget* w, int, QJsonObject& obj)
{
    auto* progress = static_cast<QProgressBar*>(w);
    obj[QStringLiteral("value")] = progress->value();
    obj[QStringLiteral("text")] = progress->text();
    return true;
}

bool readGroup(QWidget* w, int, QJsonObject& obj)
{
    auto* group = static_cast<QGroupBox*>(w);
    if (group->title().isEmpty()) return false;
    obj[QStringLiteral("text")] = group->title();
    if (group->isCheckable()) obj[QStringLiteral("checked")] = group->isChecked();
    return true;
}

bool readButton(QWidget* w, int, QJsonObject& obj)
{
    auto* button = static_cast<QAbstractButton*>(w);
    obj[QStringLiteral("text")] = button->text();
    if (button->isCheckable()) obj[QStringLiteral("checked")] = button->isChecked();
    return true;
}

// ---- 書き（受付の検査をして、応答の後に行う操作を返す） ----

Write writeColorDialog(QWidget* w, const QJsonObject& params)
{
    auto* picker = static_cast<QColorDialog*>(w);
    const QJsonValue value = valueOf(params);
    const QColor color(value.toString());
    if (!value.isString() || !color.isValid())
        fail(ErrorKind::InvalidArgument, QStringLiteral("value must be a valid color"));
    return [picker, color]() { picker->setCurrentColor(color); };
}

Write writeCombo(QWidget* w, const QJsonObject& params)
{
    auto* combo = static_cast<QComboBox*>(w);
    const QJsonValue value = valueOf(params);
    const int index = value.isString() ? combo->findText(value.toString(), Qt::MatchExactly)
                                       : requireIntValue(params, 0, combo->count() - 1);
    if (index < 0) fail(ErrorKind::InvalidArgument, QStringLiteral("No matching combo item"));
    return [combo, index]() {
        if (index >= combo->count()) fail(ErrorKind::InvalidState, QStringLiteral("The combo item was removed"));
        QPointer<QComboBox> guard(combo);
        const QString text = combo->itemText(index);
        combo->setCurrentIndex(index);
        if (!guard) return;
        Q_EMIT combo->activated(index);
        if (guard) Q_EMIT combo->textActivated(text);
    };
}

Write writeLineEdit(QWidget* w, const QJsonObject& params)
{
    auto* lineEdit = static_cast<QLineEdit*>(w);
    if (lineEdit->isReadOnly()) fail(ErrorKind::InvalidState, QStringLiteral("Text field is read-only or protected"));
    const QString text = requireText(params);
    if (text.size() > lineEdit->maxLength()) fail(ErrorKind::InvalidArgument, QStringLiteral("Text exceeds the field limit"));
    const bool submit = params.value(QStringLiteral("submit")).toBool();
    return [lineEdit, text, submit]() {
        if (lineEdit->isReadOnly()) fail(ErrorKind::InvalidState, QStringLiteral("Text field became read-only"));
        QPointer<QLineEdit> guard(lineEdit);
        lineEdit->selectAll();
        lineEdit->insert(text); // ユーザー編集と同じ textEdited も発行する（色入力等）。
        // 全文を一度に指定する操作では、補完候補が次のボタン操作を遮らないよう閉じる。
        if (guard && lineEdit->completer() && lineEdit->completer()->popup())
            lineEdit->completer()->popup()->hide();
        if (submit && guard) {
            QKeyEvent enter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
            QApplication::sendEvent(lineEdit, &enter);
        }
    };
}

Write writePlainTextEdit(QWidget* w, const QJsonObject& params)
{
    auto* edit = static_cast<QPlainTextEdit*>(w);
    if (edit->isReadOnly()) fail(ErrorKind::InvalidState, QStringLiteral("Text field is read-only"));
    const QString text = requireText(params);
    return [edit, text]() { edit->setPlainText(text); };
}

Write writeTextEdit(QWidget* w, const QJsonObject& params)
{
    auto* edit = static_cast<QTextEdit*>(w);
    if (edit->isReadOnly()) fail(ErrorKind::InvalidState, QStringLiteral("Text field is read-only"));
    const QString text = requireText(params);
    return [edit, text]() { edit->setPlainText(text); };
}

Write writeSpin(QWidget* w, const QJsonObject& params)
{
    auto* spin = static_cast<QSpinBox*>(w);
    if (spin->isReadOnly()) fail(ErrorKind::InvalidState, QStringLiteral("Spin box is read-only"));
    const int number = requireIntValue(params, spin->minimum(), spin->maximum());
    return [spin, number]() { spin->setValue(number); };
}

Write writeDoubleSpin(QWidget* w, const QJsonObject& params)
{
    auto* spin = static_cast<QDoubleSpinBox*>(w);
    if (spin->isReadOnly()) fail(ErrorKind::InvalidState, QStringLiteral("Spin box is read-only"));
    const QJsonValue value = valueOf(params);
    const double number = value.toDouble();
    if (!value.isDouble() || !std::isfinite(number) || number < spin->minimum() || number > spin->maximum())
        fail(ErrorKind::InvalidArgument, QStringLiteral("value is outside the spin box range"));
    return [spin, number]() { spin->setValue(number); };
}

Write writeSlider(QWidget* w, const QJsonObject& params)
{
    auto* slider = static_cast<QSlider*>(w);
    const int number = requireIntValue(params, slider->minimum(), slider->maximum());
    return [slider, number]() { slider->setValue(number); };
}

Write writeGroup(QWidget* w, const QJsonObject& params)
{
    auto* group = static_cast<QGroupBox*>(w);
    if (!group->isCheckable()) fail(ErrorKind::InvalidArgument, QStringLiteral("Unsupported widget type"));
    const bool checked = requireBool(params);
    return [group, checked]() { group->setChecked(checked); };
}

Write writeListView(QWidget* w, const QJsonObject& params)
{
    auto* list = static_cast<QListView*>(w);
    if (!list->model()) fail(ErrorKind::InvalidArgument, QStringLiteral("Unsupported widget type"));
    const int row = requireIntValue(params, 0, list->model()->rowCount() - 1);
    const QPersistentModelIndex index(list->model()->index(row, 0));
    const auto usable = [list](const QPersistentModelIndex& item) {
        return item.isValid() && (item.flags() & Qt::ItemIsEnabled) && (item.flags() & Qt::ItemIsSelectable)
            && !list->isRowHidden(item.row());
    };
    if (!usable(index)) fail(ErrorKind::InvalidState, QStringLiteral("List item is hidden or disabled"));
    return [list, index, usable]() {
        if (!usable(index)) fail(ErrorKind::InvalidState, QStringLiteral("List item became hidden or disabled"));
        list->setCurrentIndex(index);
        list->selectionModel()->select(index, QItemSelectionModel::ClearAndSelect);
        list->scrollTo(index);
    };
}

Write writeButton(QWidget* w, const QJsonObject& params)
{
    auto* button = static_cast<QAbstractButton*>(w);
    if (!button->isCheckable()) fail(ErrorKind::InvalidArgument, QStringLiteral("Unsupported widget type"));
    const bool checked = requireBool(params);
    if (!checked && button->autoExclusive())
        fail(ErrorKind::InvalidArgument, QStringLiteral("Select another radio button instead"));
    return [button, checked]() { if (button->isChecked() != checked) button->click(); };
}

Write writeTabs(QWidget* w, const QJsonObject& params)
{
    auto* tabs = static_cast<QTabWidget*>(w);
    const int index = requireIntValue(params, 0, tabs->count() - 1);
    if (!tabs->isTabEnabled(index) || !tabs->isTabVisible(index))
        fail(ErrorKind::InvalidState, QStringLiteral("Tab is hidden or disabled"));
    return [tabs, index]() {
        if (index >= tabs->count() || !tabs->isTabEnabled(index) || !tabs->isTabVisible(index))
            fail(ErrorKind::InvalidState, QStringLiteral("Tab became hidden or disabled"));
        tabs->setCurrentIndex(index);
    };
}

// ---- 表 ----

struct Adapter {
    bool (*matches)(QWidget*);
    bool (*read)(QWidget*, int maxRows, QJsonObject&);   ///< nullptr なら読めない
    Write (*write)(QWidget*, const QJsonObject& params); ///< nullptr なら書けない
};

template <typename T>
bool is(QWidget* widget)
{
    return qobject_cast<T*>(widget) != nullptr;
}

/// 上から順に当てはめる（QListWidget は QListView より先）
constexpr Adapter kAdapters[] = {
    {&is<QColorDialog>, &readColorDialog, &writeColorDialog},
    {&is<QMenu>, &readMenu, nullptr},
    {&is<QGraphicsView>, &readGraphicsView, nullptr},
    {&is<QDockWidget>, &readDock, nullptr},
    {&is<ShogiView>, &readBoard, nullptr},
    {&is<QTableView>, &readTable, nullptr},
    {&is<QListWidget>, &readListWidget, &writeListView},
    {&is<QListView>, nullptr, &writeListView},
    {&is<QTreeWidget>, &readTreeWidget, nullptr},
    {&is<QComboBox>, &readCombo, &writeCombo},
    {&is<QTabWidget>, &readTabs, &writeTabs},
    {&is<QLabel>, &readLabel, nullptr},
    {&is<QLineEdit>, &readLineEdit, &writeLineEdit},
    {&is<QTextEdit>, &readTextEdit, &writeTextEdit},
    {&is<QPlainTextEdit>, &readPlainTextEdit, &writePlainTextEdit},
    {&is<QSpinBox>, &readSpin, &writeSpin},
    {&is<QDoubleSpinBox>, &readDoubleSpin, &writeDoubleSpin},
    {&is<QSlider>, &readSlider, &writeSlider},
    {&is<QProgressBar>, &readProgress, nullptr},
    {&is<QGroupBox>, &readGroup, &writeGroup},
    {&is<QAbstractButton>, &readButton, &writeButton},
};

const Adapter* adapterFor(QWidget* widget)
{
    for (const Adapter& adapter : kAdapters)
        if (adapter.matches(widget)) return &adapter;
    return nullptr;
}

} // namespace

bool describe(QWidget* widget, int maxRows, QJsonObject& obj)
{
    const Adapter* adapter = adapterFor(widget);
    return adapter && adapter->read && adapter->read(widget, maxRows, obj);
}

std::function<void()> writer(QWidget* widget, const QJsonObject& params)
{
    const Adapter* adapter = adapterFor(widget);
    if (!adapter || !adapter->write) fail(ErrorKind::InvalidArgument, QStringLiteral("Unsupported widget type"));
    return adapter->write(widget, params);
}

} // namespace AutomationWidgetAdapters
