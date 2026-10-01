/// @file gameinfopanecontroller_edit.cpp
/// @brief 対局情報の編集履歴とクリップボード操作

#include "gameinfopanecontroller.h"
#include <algorithm>
#include <QApplication>
#include <QClipboard>
#include <QAbstractItemDelegate>
#include <QLineEdit>
#include <QSignalBlocker>
#include <QTableWidget>
#include "logcategories.h"

void GameInfoPaneController::undo()
{
    commitPendingEditor();
    if (m_historyIndex > 0) restoreEditState(m_history[--m_historyIndex]);
}

void GameInfoPaneController::redo()
{
    commitPendingEditor();
    if (m_historyIndex + 1 < m_history.size()) restoreEditState(m_history[++m_historyIndex]);
}

void GameInfoPaneController::cut()
{
    if (!m_table) return;
    if (auto* editor = qobject_cast<QLineEdit*>(QApplication::focusWidget());
        editor && m_table->isAncestorOf(editor)) { editor->cut(); return; }

    QTableWidgetItem* item = m_table->currentItem();
    if (item && (item->flags() & Qt::ItemIsEditable)) {
        QApplication::clipboard()->setText(item->text());
        item->setText(QString());
    }
}

void GameInfoPaneController::copy()
{
    if (!m_table) return;
    if (auto* editor = qobject_cast<QLineEdit*>(QApplication::focusWidget());
        editor && m_table->isAncestorOf(editor)) { editor->copy(); return; }

    QTableWidgetItem* item = m_table->currentItem();
    if (item) {
        QApplication::clipboard()->setText(item->text());
    }
}

void GameInfoPaneController::paste()
{
    if (!m_table) return;
    if (auto* editor = qobject_cast<QLineEdit*>(QApplication::focusWidget());
        editor && m_table->isAncestorOf(editor)) { editor->paste(); return; }

    QTableWidgetItem* item = m_table->currentItem();
    if (item && (item->flags() & Qt::ItemIsEditable)) {
        QString text = QApplication::clipboard()->text();
        item->setText(text);
    }
}

void GameInfoPaneController::addRow()
{
    if (!m_table) return;
    commitPendingEditor();

    const int newRow = m_table->rowCount();
    m_table->blockSignals(true);
    m_table->setRowCount(newRow + 1);

    // 両列とも編集可能（ユーザー定義の項目のため）
    auto* keyItem = new QTableWidgetItem();
    auto* valueItem = new QTableWidgetItem();
    m_table->setItem(newRow, 0, keyItem);
    m_table->setItem(newRow, 1, valueItem);
    m_table->blockSignals(false);

    // 新しい行の項目名セルを選択して編集開始
    m_table->setCurrentCell(newRow, 0);
    m_table->editItem(keyItem);

    // dirty状態を更新
    onCellChanged(newRow, 0);
}

void GameInfoPaneController::applyChanges()
{
    if (!m_table) return;
    commitPendingEditor();
    if (!m_dirty) return;

    // 現在のテーブル内容を取得
    QList<KifGameInfoItem> currentItems = gameInfo();

    // 元データを更新
    m_originalItems = currentItems;
    m_dirty = false;
    updateEditingIndicator();

    emit gameInfoUpdated(currentItems);

    qCDebug(lcUi).noquote() << "[GameInfoPane] applyChanges: Game info updated, items=" << currentItems.size();
}

void GameInfoPaneController::removeRow()
{
    if (!m_table || !m_table->currentItem() || !m_table->currentItem()->isSelected()) return;
    commitPendingEditor();
    const int row = m_table->currentRow();
    {
        const QSignalBlocker blocker(m_table);
        m_table->removeRow(row);
        if (m_table->rowCount() > 0)
            m_table->setCurrentCell(qMin(row, m_table->rowCount() - 1), 1);
    }
    onCellChanged(row, 0);
}

void GameInfoPaneController::onCellChanged(int row, int column)
{
    Q_UNUSED(row);
    Q_UNUSED(column);

    const auto state = editState();
    if (m_history.isEmpty() || !(state == m_history[m_historyIndex])) {
        while (m_history.size() > m_historyIndex + 1) m_history.removeLast();
        m_history.append(state);
        m_historyIndex = static_cast<int>(m_history.size()) - 1;
    }

    m_dirty = checkDirty();
    updateTablePresentation();
    updateEditingIndicator();
}

void GameInfoPaneController::commitPendingEditor()
{
    if (auto* editor = activeEditor()) {
        // Enterキーではdelegateの確定が次のイベントループまで遅延する。
        // 保存処理が直後にテーブルを読むため、通常の確定シグナルを同期的に送る。
        auto* delegate = m_table->itemDelegateForIndex(m_table->currentIndex());
        emit delegate->commitData(editor);
        emit delegate->closeEditor(editor, QAbstractItemDelegate::NoHint);
    }
}

QLineEdit* GameInfoPaneController::activeEditor() const
{
    if (!m_table) return nullptr;
    // 終局ダイアログなどへフォーカスが移った後にも、入力欄が残ることがある。
    for (auto* editor : m_table->findChildren<QLineEdit*>()) {
        if (editor->isVisibleTo(m_table)) return editor;
    }
    return nullptr;
}

GameInfoPaneController::EditState GameInfoPaneController::editState() const
{
    EditState state;
    for (int row = 0; row < m_table->rowCount(); ++row) {
        const auto* key = m_table->item(row, 0);
        const auto* value = m_table->item(row, 1);
        state.cells.append({key ? key->text() : QString(), value ? value->text() : QString()});
        if (key && (key->flags() & Qt::ItemIsEditable)) state.editableKeys.insert(row);
    }
    return state;
}

void GameInfoPaneController::restoreEditState(const EditState& state)
{
    const QSignalBlocker blocker(m_table);
    const int currentRow = m_table->currentRow();
    const int currentColumn = m_table->currentColumn();
    m_table->setRowCount(static_cast<int>(state.cells.size()));
    for (int row = 0; row < state.cells.size(); ++row) {
        auto* key = new QTableWidgetItem(state.cells[row].first);
        if (!state.editableKeys.contains(row)) key->setFlags(key->flags() & ~Qt::ItemIsEditable);
        m_table->setItem(row, 0, key);
        m_table->setItem(row, 1, new QTableWidgetItem(state.cells[row].second));
    }
    if (!state.cells.isEmpty()) m_table->setCurrentCell(qBound(0, currentRow, m_table->rowCount() - 1), qMax(0, currentColumn));
    m_dirty = checkDirty();
    updateTablePresentation();
    updateEditingIndicator();
}

void GameInfoPaneController::resetHistory()
{
    m_history = {editState()};
    m_historyIndex = 0;
}

void GameInfoPaneController::updateGameInfoValue(const QString& key, const QString& value)
{
    commitPendingEditor();
    const QSignalBlocker blocker(m_table);
    int row = 0;
    for (; row < m_table->rowCount(); ++row) {
        if (m_table->item(row, 0) && m_table->item(row, 0)->text() == key) break;
    }
    if (row == m_table->rowCount()) {
        m_table->insertRow(row);
        auto* keyItem = new QTableWidgetItem(key);
        keyItem->setFlags(keyItem->flags() & ~Qt::ItemIsEditable);
        m_table->setItem(row, 0, keyItem);
    }
    m_table->setItem(row, 1, new QTableWidgetItem(value));

    auto original = std::find_if(m_originalItems.begin(), m_originalItems.end(),
                                 [&key](const auto& item) { return item.key == key; });
    if (original == m_originalItems.end()) m_originalItems.append({key, value});
    else original->value = value;
    for (auto& state : m_history) {
        auto cell = std::find_if(state.cells.begin(), state.cells.end(),
                                 [&key](const auto& item) { return item.first == key; });
        if (cell == state.cells.end()) state.cells.append({key, value});
        else cell->second = value;
    }
    m_dirty = checkDirty();
    updateTablePresentation();
    updateEditingIndicator();
}
