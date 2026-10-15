/// @file gameinfopanecontroller.cpp
/// @brief 対局情報ペインコントローラクラスの実装

#include "gameinfopanecontroller.h"
#include "dialogutils.h"
#include "tablestyles.h"
#include "gamesettings.h"
#include "gameinfokeys.h"

#include <QTableWidget>
#include <QVBoxLayout>
#include <QHeaderView>
#include <QApplication>
#include <QLineEdit>
#include <QShortcut>
#include <QMessageBox>
#include <QClipboard>
#include <QAbstractItemDelegate>
#include <algorithm>
#include <QSignalBlocker>
#include "logcategories.h"
#include "buttonstyles.h"
#include "flowlayout.h"
#include "kifupresentation.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QMimeData>
#include <QPushButton>
#include <QStyledItemDelegate>
#include <QToolButton>

GameInfoPaneController::GameInfoPaneController(QObject* parent)
    : QObject(parent)
    , m_fontSize(GameSettings::gameInfoFontSize())
{
    // 設定値の範囲チェック
    if (m_fontSize < 8)  m_fontSize = 10;
    if (m_fontSize > 24) m_fontSize = 24;

    m_keyColumnWidth = qBound(0, GameSettings::gameInfoKeyColumnWidth(), 600);
    buildUi();
}

GameInfoPaneController::~GameInfoPaneController()
{
    // m_container は親ウィジェットに所有されるため、ここでは削除しない
}

void GameInfoPaneController::buildUi()
{
    // コンテナ作成
    m_container = new QWidget;
    QVBoxLayout* mainLayout = new QVBoxLayout(m_container);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // ツールバー作成
    buildToolbar();
    mainLayout->addWidget(m_toolbar);

    // テーブル作成
    m_table = new QTableWidget(m_container);
    m_table->setObjectName(QStringLiteral("gameInfoTable"));
    m_table->setColumnCount(2);
    m_table->setHorizontalHeaderLabels({tr("項目"), tr("内容")});
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_table->horizontalHeader()->setMinimumSectionSize(60);
    m_table->verticalHeader()->setVisible(false);
    m_table->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_table->setSelectionBehavior(QAbstractItemView::SelectItems);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setWordWrap(true);
    m_table->setShowGrid(false);
    m_table->setAlternatingRowColors(true);
    m_table->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_table->setAccessibleName(tr("対局情報"));
    installValueDelegate();

    // フォントサイズ適用
    applyFontSize();

    mainLayout->addWidget(m_table, 1);

    // セル変更時の接続
    QObject::connect(m_table, &QTableWidget::cellChanged,
                     this, &GameInfoPaneController::onCellChanged);

    const auto addShortcut = [this](QKeySequence::StandardKey key,
                                    void (GameInfoPaneController::*slot)()) {
        auto* shortcut = new QShortcut(key, m_container);
        shortcut->setContext(Qt::WidgetWithChildrenShortcut);
        connect(shortcut, &QShortcut::activated, this, slot);
    };
    addShortcut(QKeySequence::Undo, &GameInfoPaneController::undo);
    addShortcut(QKeySequence::Redo, &GameInfoPaneController::redo);
    addShortcut(QKeySequence::Cut, &GameInfoPaneController::cut);
    addShortcut(QKeySequence::Copy, &GameInfoPaneController::copy);
    addShortcut(QKeySequence::Paste, &GameInfoPaneController::paste);
    for (const auto key : {Qt::Key_Return, Qt::Key_Enter}) {
        auto* shortcut = new QShortcut(QKeySequence(Qt::CTRL | key), m_container);
        shortcut->setContext(Qt::WidgetWithChildrenShortcut);
        connect(shortcut, &QShortcut::activated, this, &GameInfoPaneController::applyChanges);
    }
    connect(m_table, &QTableWidget::itemSelectionChanged,
            this, &GameInfoPaneController::updateEditingIndicator);
    connect(m_table->horizontalHeader(), &QHeaderView::sectionResized,
            this, &GameInfoPaneController::onColumnResized);
    connect(qApp, &QApplication::focusChanged, this, &GameInfoPaneController::observeEditor);
    connect(QApplication::clipboard(), &QClipboard::dataChanged,
            this, &GameInfoPaneController::updateEditingIndicator);
    connect(m_table->itemDelegate(), &QAbstractItemDelegate::closeEditor,
            this, &GameInfoPaneController::updateEditingIndicator, Qt::QueuedConnection);
    resetHistory();
    updateTablePresentation();
    updateEditingIndicator();
}

QWidget* GameInfoPaneController::containerWidget() const
{
    return m_container;
}

QTableWidget* GameInfoPaneController::tableWidget() const
{
    return m_table;
}

void GameInfoPaneController::setGameInfo(const QList<KifGameInfoItem>& items, bool beforeMatchStart)
{
    if (!m_table) return;
    m_beforeMatchStart = beforeMatchStart;

    m_table->blockSignals(true);

    m_table->clearContents();
    m_table->setRowCount(static_cast<int>(items.size()));

    for (qsizetype row = 0; row < items.size(); ++row) {
        const KifGameInfoItem& item = items.at(row);
        auto* keyItem   = new QTableWidgetItem(item.key);
        auto* valueItem = new QTableWidgetItem(item.value);
        // 項目名は編集不可
        keyItem->setFlags(keyItem->flags() & ~Qt::ItemIsEditable);
        m_table->setItem(static_cast<int>(row), 0, keyItem);
        m_table->setItem(static_cast<int>(row), 1, valueItem);
    }

    m_table->blockSignals(false);

    // 元データを保存
    m_originalItems = items;
    m_dirty = false;
    resetHistory();
    updateTablePresentation();
    updateEditingIndicator();
}

void GameInfoPaneController::resetGameInfo()
{
    // 対局者は名前が決まるまで、将棋盤の名前欄と同じく「先手」「後手」と呼ぶ
    setGameInfo({{GameInfoKeys::kGameDate, {}},
                 {GameInfoKeys::kStartDateTime, {}},
                 {GameInfoKeys::kBlackPlayer, tr("先手")},
                 {GameInfoKeys::kWhitePlayer, tr("後手")},
                 {GameInfoKeys::kHandicap, QStringLiteral("平手")},
                 {GameInfoKeys::kTimeControl, {}},
                 {GameInfoKeys::kEvent, {}},
                 {GameInfoKeys::kSite, {}},
                 {GameInfoKeys::kNote, {}}}, true);
}

void GameInfoPaneController::setGameInfoForMatch(const QList<KifGameInfoItem>& automaticItems)
{
    commitPendingEditor();
    QList<KifGameInfoItem> items = automaticItems;
    // 対局者の見出しは平手（先手・後手）と駒落ち（下手・上手）で変わるため、前の対局の行を残さない。
    QSet<QString> replacedKeys = {GameInfoKeys::kEndDateTime, QStringLiteral("消費時間"),
                                 QStringLiteral("結果"),
                                 GameInfoKeys::kBlackPlayer, GameInfoKeys::kWhitePlayer,
                                 GameInfoKeys::kShitatePlayer, GameInfoKeys::kUwatePlayer};
    for (const auto& item : automaticItems) replacedKeys.insert(item.key);
    QSet<QString> retainedKeys;
    const QList<KifGameInfoItem> currentItems = gameInfo();
    for (const auto& item : currentItems) {
        const QString key = item.key.trimmed();
        if (key.isEmpty() || replacedKeys.contains(key)) continue;
        items.append(item);
        retainedKeys.insert(key);
    }
    for (const auto& key : {GameInfoKeys::kEvent, GameInfoKeys::kSite, GameInfoKeys::kNote}) {
        if (!retainedKeys.contains(key)) items.append({key, {}});
    }
    setGameInfo(items);
}

QList<KifGameInfoItem> GameInfoPaneController::gameInfo() const
{
    QList<KifGameInfoItem> items;
    if (!m_table) return items;

    const int rowCount = m_table->rowCount();
    items.reserve(rowCount);

    for (int r = 0; r < rowCount; ++r) {
        QTableWidgetItem* keyItem = m_table->item(r, 0);
        QTableWidgetItem* valueItem = m_table->item(r, 1);

        KifGameInfoItem item;
        item.key = keyItem ? keyItem->text() : QString();
        item.value = valueItem ? valueItem->text() : QString();
        items.append(item);
    }

    return items;
}

void GameInfoPaneController::setOriginalGameInfo(const QList<KifGameInfoItem>& items)
{
    m_beforeMatchStart = false;
    m_originalItems = items;
    m_dirty = false;
    resetHistory();
    updateTablePresentation();
    updateEditingIndicator();
}

void GameInfoPaneController::updatePlayerNames(const QString& blackName, const QString& whiteName)
{
    if (!m_table) return;
    commitPendingEditor();

    // 駒落ちの対局情報では、先手・後手の代わりに下手・上手の行がある
    const auto isBlackKey = [](const QString& key) {
        return key == GameInfoKeys::kBlackPlayer || key == GameInfoKeys::kShitatePlayer;
    };
    const auto isWhiteKey = [](const QString& key) {
        return key == GameInfoKeys::kWhitePlayer || key == GameInfoKeys::kUwatePlayer;
    };

    m_table->blockSignals(true);

    // 先手（下手）の行を検索して更新
    for (int row = 0; row < m_table->rowCount(); ++row) {
        QTableWidgetItem* keyItem = m_table->item(row, 0);
        if (keyItem && isBlackKey(keyItem->text())) {
            QTableWidgetItem* valItem = m_table->item(row, 1);
            if (valItem) {
                valItem->setText(blackName);
            }
            break;
        }
    }

    // 後手（上手）の行を検索して更新
    for (int row = 0; row < m_table->rowCount(); ++row) {
        QTableWidgetItem* keyItem = m_table->item(row, 0);
        if (keyItem && isWhiteKey(keyItem->text())) {
            QTableWidgetItem* valItem = m_table->item(row, 1);
            if (valItem) {
                valItem->setText(whiteName);
            }
            break;
        }
    }

    m_table->blockSignals(false);

    // 元データも更新
    for (qsizetype i = 0; i < m_originalItems.size(); ++i) {
        if (isBlackKey(m_originalItems[i].key)) {
            m_originalItems[i].value = blackName;
        } else if (isWhiteKey(m_originalItems[i].key)) {
            m_originalItems[i].value = whiteName;
        }
    }
    // 自動同期された名前は、他のセルのUndo/Redoで古い名前に戻さない。
    for (auto& state : m_history) {
        for (auto& cell : state.cells) {
            if (isBlackKey(cell.first)) cell.second = blackName;
            else if (isWhiteKey(cell.first)) cell.second = whiteName;
        }
    }
    m_dirty = checkDirty();
    updateTablePresentation();
    updateEditingIndicator();
}

bool GameInfoPaneController::isDirty() const
{
    auto* editor = activeEditor();
    if (editor && m_table->currentItem()
        && editor->text() != m_table->currentItem()->text()) return true;
    return m_dirty;
}

bool GameInfoPaneController::confirmDiscardUnsaved()
{
    commitPendingEditor();
    if (!m_dirty) {
        return true;
    }

    const bool discard = DialogUtils::confirmAction(
        m_container,
        tr("未保存の対局情報"),
        tr("対局情報が編集されていますが、まだ更新されていません。\n"
           "変更を破棄して続行しますか？"),
        tr("破棄して続行"), QMessageBox::Warning);

    if (discard) {
        setGameInfo(m_originalItems, m_beforeMatchStart);
        return true;
    }
    return false;
}

int GameInfoPaneController::fontSize() const
{
    return m_fontSize;
}

void GameInfoPaneController::setFontSize(int size)
{
    if (size < 8)  size = 8;
    if (size > 24) size = 24;

    if (m_fontSize != size) {
        m_fontSize = size;
        applyFontSize();
        GameSettings::gameInfoFontSize.set(m_fontSize);
    }
}

void GameInfoPaneController::increaseFontSize()
{
    setFontSize(m_fontSize + 1);
}

void GameInfoPaneController::decreaseFontSize()
{
    setFontSize(m_fontSize - 1);
}

void GameInfoPaneController::applyFontSize()
{
    if (m_table) {
        QFont font = m_table->font();
        font.setPointSize(m_fontSize);
        m_table->setFont(font);
        m_table->setStyleSheet(TableStyles::thinking(m_fontSize) + QStringLiteral(
            "QTableView { alternate-background-color: #f7f9fb; }"
            "QTableView::item { padding: 4px 8px; }"));
        m_table->verticalHeader()->setMinimumSectionSize(m_table->fontMetrics().height() + 10);
        updateTablePresentation();
        updateEditingIndicator();
    }
}

bool GameInfoPaneController::checkDirty() const
{
    if (!m_table) return false;

    const int rowCount = m_table->rowCount();
    if (rowCount != m_originalItems.size()) {
        return true;
    }

    for (int r = 0; r < rowCount; ++r) {
        QTableWidgetItem* keyItem = m_table->item(r, 0);
        QTableWidgetItem* valueItem = m_table->item(r, 1);

        QString currentKey = keyItem ? keyItem->text() : QString();
        QString currentValue = valueItem ? valueItem->text() : QString();

        if (r < m_originalItems.size()) {
            if (currentKey != m_originalItems[r].key ||
                currentValue != m_originalItems[r].value) {
                return true;
            }
        }
    }

    return false;
}

// ============================================================
// 対局情報の編集履歴とクリップボード操作
// ============================================================

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
    const auto editors = m_table->findChildren<QLineEdit*>();
    for (auto* editor : editors) {
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

// ============================================================
// 対局情報の表示と操作ボタンの状態管理
// ============================================================

namespace {
constexpr int kPlaceholderRole = Qt::UserRole + 1;

// 案内文は描画時だけ表示し、編集値・コピー・棋譜出力には含めない。
class GameInfoValueDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option,
                          const QModelIndex& index) const override
    {
        auto* editor = QStyledItemDelegate::createEditor(parent, option, index);
        if (auto* input = qobject_cast<QLineEdit*>(editor))
            input->setPlaceholderText(index.data(kPlaceholderRole).toString());
        return editor;
    }

protected:
    void initStyleOption(QStyleOptionViewItem* option, const QModelIndex& index) const override
    {
        QStyledItemDelegate::initStyleOption(option, index);
        if (!option->text.isEmpty()) {
            option->text = index.column() == 0 ? KifuPresentation::infoKey(option->text)
                : KifuPresentation::infoValue(index.siblingAtColumn(0).data().toString(), option->text);
            return;
        }
        option->text = index.data(kPlaceholderRole).toString();
        option->features |= QStyleOptionViewItem::HasDisplay;
        option->palette.setColor(QPalette::Text, option->palette.color(QPalette::PlaceholderText));
    }
};
} // namespace

void GameInfoPaneController::installValueDelegate()
{
    m_table->setItemDelegate(new GameInfoValueDelegate(m_table));
}

QString GameInfoPaneController::placeholderForKey(const QString& key) const
{
    if (key == GameInfoKeys::kGameDate || key == GameInfoKeys::kStartDateTime)
        return m_beforeMatchStart ? tr("未開始（対局開始時に設定）") : tr("未設定");
    if (key == GameInfoKeys::kBlackPlayer || key == GameInfoKeys::kWhitePlayer
        || key == GameInfoKeys::kShitatePlayer || key == GameInfoKeys::kUwatePlayer)
        return tr("未設定（名前を入力できます）");
    if (key == GameInfoKeys::kTimeControl)
        return m_beforeMatchStart ? tr("未設定（対局設定から反映）") : tr("未設定");
    if (key == GameInfoKeys::kEvent || key == GameInfoKeys::kSite || key == GameInfoKeys::kNote)
        return tr("任意入力");
    return {};
}

void GameInfoPaneController::buildToolbar()
{
    m_toolbar = new QWidget(m_container);
    m_toolbar->setObjectName(QStringLiteral("gameInfoToolbar"));
    m_toolbar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    auto* flow = new FlowLayout(m_toolbar, 6, 12, 6);
    const auto group = [this, flow]() {
        auto* widget = new QWidget(m_toolbar);
        auto* layout = new QHBoxLayout(widget);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(4);
        flow->addWidget(widget);
        return layout;
    };
    const auto button = [this](QHBoxLayout* layout, const QString& name,
                               const QString& text, const QString& tip,
                               const QString& icon, void (GameInfoPaneController::*slot)()) {
        auto* result = new QToolButton(layout->parentWidget());
        result->setObjectName(name);
        result->setText(text);
        result->setToolTip(tip);
        result->setAccessibleName(tip);
        result->setFocusPolicy(Qt::NoFocus); // セルの編集中の選択範囲を保つ
        result->setStyleSheet(ButtonStyles::panelToolButton());
        result->setMinimumSize(32, 28);
        if (!icon.isEmpty()) {
            result->setIcon(QIcon(QStringLiteral(":/images/actions/") + icon + QStringLiteral(".svg")));
            result->setIconSize(QSize(18, 18));
        }
        if (icon.isEmpty() && text.size() > 2)
            result->setMinimumWidth(result->fontMetrics().horizontalAdvance(text) + 20);
        connect(result, &QToolButton::clicked, this, slot);
        layout->addWidget(result);
        return result;
    };
    auto* fonts = group();
    m_btnFontDecrease = button(fonts, QStringLiteral("gameInfoFontDecrease"), QStringLiteral("A-"),
        tr("フォントサイズを小さくする"), {}, &GameInfoPaneController::decreaseFontSize);
    m_btnFontIncrease = button(fonts, QStringLiteral("gameInfoFontIncrease"), QStringLiteral("A+"),
        tr("フォントサイズを大きくする"), {}, &GameInfoPaneController::increaseFontSize);
    auto* history = group();
    m_btnUndo = button(history, QStringLiteral("gameInfoUndo"), {}, tr("元に戻す (Ctrl+Z)"),
        QStringLiteral("editUndo"), &GameInfoPaneController::undo);
    m_btnRedo = button(history, QStringLiteral("gameInfoRedo"), {}, tr("やり直す (Ctrl+Y)"),
        QStringLiteral("editRedo"), &GameInfoPaneController::redo);
    auto* clipboard = group();
    m_btnCut = button(clipboard, QStringLiteral("gameInfoCut"), {}, tr("切り取り (Ctrl+X)"),
        QStringLiteral("editCut"), &GameInfoPaneController::cut);
    m_btnCopy = button(clipboard, QStringLiteral("gameInfoCopy"), {}, tr("コピー (Ctrl+C)"),
        QStringLiteral("editCopy"), &GameInfoPaneController::copy);
    m_btnPaste = button(clipboard, QStringLiteral("gameInfoPaste"), {}, tr("貼り付け (Ctrl+V)"),
        QStringLiteral("editPaste"), &GameInfoPaneController::paste);
    auto* rows = group();
    m_btnAddRow = button(rows, QStringLiteral("gameInfoAddRow"), tr("行を追加"),
        tr("新しい行を追加する"), {}, &GameInfoPaneController::addRow);
    m_btnRemoveRow = button(rows, QStringLiteral("gameInfoRemoveRow"), tr("行を削除"),
        tr("選択行を削除する（元に戻す操作で復元できます）"), {}, &GameInfoPaneController::removeRow);

    auto* apply = group();
    m_btnUpdate = new QPushButton(tr("対局情報更新"), m_toolbar);
    m_btnUpdate->setObjectName(QStringLiteral("gameInfoApply"));
    m_btnUpdate->setToolTip(tr("編集した対局情報を棋譜に反映する (Ctrl+Enter)"));
    m_btnUpdate->setMinimumHeight(28);
    m_btnUpdate->setStyleSheet(ButtonStyles::primaryAction());
    connect(m_btnUpdate, &QPushButton::clicked, this, &GameInfoPaneController::applyChanges);
    apply->addWidget(m_btnUpdate);

    m_editingLabel = new QLabel(m_toolbar);
    m_editingLabel->setObjectName(QStringLiteral("gameInfoEditing"));
    // FlowLayout は sizeHint の幅で並べるため、折り返し可能なラベルは次の行に回ったとき
    // 余白があっても短く折り返される。案内文は短いので1行で表示する。
    m_editingLabel->setWordWrap(false);
    flow->addWidget(m_editingLabel);
}

void GameInfoPaneController::observeEditor()
{
    if (!m_table || !m_toolbar) return;
    if (auto* editor = activeEditor()) {
        connect(editor, &QLineEdit::textChanged,
                this, &GameInfoPaneController::updateEditingIndicator, Qt::UniqueConnection);
        connect(editor, &QLineEdit::selectionChanged,
                this, &GameInfoPaneController::updateEditingIndicator, Qt::UniqueConnection);
    }
    updateEditingIndicator();
}

void GameInfoPaneController::updateEditingIndicator()
{
    if (!m_table || !m_toolbar) return;
    const auto* item = m_table->currentItem();
    const bool selected = item && item->isSelected();
    const bool editable = selected && (item->flags() & Qt::ItemIsEditable);
    const auto* editor = activeEditor();
    const bool pending = editor && item && editor->text() != item->text();
    const bool hasText = editor ? editor->hasSelectedText() : selected && !item->text().isEmpty();
    m_btnUndo->setEnabled(m_historyIndex > 0 || pending);
    m_btnRedo->setEnabled(!pending && m_historyIndex + 1 < m_history.size());
    m_btnCopy->setEnabled(hasText);
    m_btnCut->setEnabled(editable && hasText);
    const auto* mime = QApplication::clipboard()->mimeData();
    m_btnPaste->setEnabled(editable && mime && mime->hasText());
    m_btnRemoveRow->setEnabled(selected);
    m_btnFontDecrease->setEnabled(m_fontSize > 8);
    m_btnFontIncrease->setEnabled(m_fontSize < 24);
    const bool dirty = isDirty();
    m_btnUpdate->setEnabled(dirty);
    m_editingLabel->setText(dirty ? tr("未反映の変更があります") : tr("内容をダブルクリックして編集"));
    // フォーカスの移動やクリップボードの変化のたびに呼ばれる。同じスタイルシートでも付け直すと
    // Qt はスタイルを適用し直して描き直すので、変わったときだけ付ける
    const QString style = dirty
        ? QStringLiteral("QLabel { color: #8a5800; font-weight: bold; }")
        : QStringLiteral("QLabel { color: #596773; }");
    if (m_editingLabel->styleSheet() != style) m_editingLabel->setStyleSheet(style);
}

void GameInfoPaneController::updateTablePresentation()
{
    if (!m_table) return;
    const QSignalBlocker blocker(m_table);
    int keyWidth = 120;
    for (int row = 0; row < m_table->rowCount(); ++row) {
        for (int column = 0; column < 2; ++column) {
            auto* item = m_table->item(row, column);
            if (!item) continue;
            const auto* keyItem = m_table->item(row, 0);
            const QString placeholder = column == 1 && keyItem
                ? placeholderForKey(keyItem->text().trimmed()) : QString();
            if (item->data(kPlaceholderRole).toString() != placeholder)
                item->setData(kPlaceholderRole, placeholder);
            const QString displayText = item->text().isEmpty() ? placeholder : column == 0
                ? KifuPresentation::infoKey(item->text())
                : KifuPresentation::infoValue(keyItem ? keyItem->text() : QString(), item->text());
            if (item->data(Qt::AccessibleTextRole).toString() != displayText)
                item->setData(Qt::AccessibleTextRole, displayText);
            // ツールチップでは棋譜中のHTMLらしい文字列もそのまま表示する。
            const QString tooltip = QStringLiteral("<qt>%1</qt>")
                .arg(displayText.toHtmlEscaped().replace(QLatin1Char('\n'), QStringLiteral("<br/>")));
            if (item->toolTip() != tooltip) item->setToolTip(tooltip);
            if (column == 0)
                keyWidth = qMax(keyWidth, m_table->fontMetrics().horizontalAdvance(displayText) + 24);
        }
    }
    m_table->setColumnWidth(0, m_keyColumnWidth > 0 ? m_keyColumnWidth : qMin(keyWidth, 320));
    m_table->resizeRowsToContents();
}

void GameInfoPaneController::onColumnResized(int column, int oldSize, int newSize)
{
    Q_UNUSED(oldSize);
    if (column == 0 && !m_table->signalsBlocked()) {
        m_keyColumnWidth = newSize;
        GameSettings::gameInfoKeyColumnWidth.set(newSize);
    }
    m_table->resizeRowsToContents();
}
