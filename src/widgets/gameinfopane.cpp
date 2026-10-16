/// @file gameinfopane.cpp
/// @brief 対局情報ドックの中身の実装

#include "gameinfopane.h"
#include "editactions.h"
#include "fontsizecontrol.h"

#include "buttonstyles.h"
#include "flowlayout.h"
#include "gameinfoeditmodel.h"
#include "gameinfokeys.h"
#include "gamesettings.h"
#include "kifupresentation.h"
#include "logcategories.h"
#include "tableheaderview.h"
#include "tablestyles.h"

#include <QAbstractItemDelegate>
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QMimeData>
#include <QPushButton>
#include <QShortcut>
#include <QStyledItemDelegate>
#include <QTableView>
#include <QToolButton>
#include <QVBoxLayout>

namespace {
// 項目名・内容は表記の設定に合わせて描き、空の内容には案内文を薄く出す。
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
            input->setPlaceholderText(index.data(GameInfoEditModel::PlaceholderRole).toString());
        return editor;
    }

protected:
    void initStyleOption(QStyleOptionViewItem* option, const QModelIndex& index) const override
    {
        QStyledItemDelegate::initStyleOption(option, index);
        if (!option->text.isEmpty()) {
            option->text = index.column() == GameInfoEditModel::KeyColumn ? KifuPresentation::infoKey(option->text)
                : KifuPresentation::infoValue(index.siblingAtColumn(GameInfoEditModel::KeyColumn).data().toString(),
                                              option->text);
            return;
        }
        option->text = index.data(GameInfoEditModel::PlaceholderRole).toString();
        option->features |= QStyleOptionViewItem::HasDisplay;
        option->palette.setColor(QPalette::Text, option->palette.color(QPalette::PlaceholderText));
    }
};
} // namespace

GameInfoPane::GameInfoPane(QWidget* parent)
    : QWidget(parent)
    , m_model(new GameInfoEditModel(this))
    , m_fontSize(GameSettings::gameInfoFontSize, 8, 24)
    , m_keyColumnWidth(qBound(0, GameSettings::gameInfoKeyColumnWidth(), 600))
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    buildToolbar();
    mainLayout->addWidget(m_toolbar);

    m_table = new QTableView(this);
    m_table->setObjectName(QStringLiteral("gameInfoTable"));
    auto* header = TableHeaderView::install(m_table);
    m_table->setModel(m_model);
    header->setStretchLastSection(true);
    header->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    header->setMinimumSectionSize(60);
    m_table->verticalHeader()->setVisible(false);
    m_table->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_table->setSelectionBehavior(QAbstractItemView::SelectItems);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setWordWrap(true);
    m_table->setShowGrid(false);
    m_table->setAlternatingRowColors(true);
    m_table->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_table->setAccessibleName(tr("対局情報"));
    m_table->setItemDelegate(new GameInfoValueDelegate(m_table));
    mainLayout->addWidget(m_table, 1);

    const auto addShortcut = [this](const QKeySequence& key, void (GameInfoPane::*slot)()) {
        auto* shortcut = new QShortcut(key, this);
        shortcut->setContext(Qt::WidgetWithChildrenShortcut);
        connect(shortcut, &QShortcut::activated, this, slot);
    };
    for (const auto key : {Qt::Key_Return, Qt::Key_Enter})
        addShortcut(QKeySequence(Qt::CTRL | key), &GameInfoPane::applyChanges);

    // 内容が変わったら、項目名の列幅と行の高さを合わせ、操作ボタンの状態を更新する
    connect(m_model, &QAbstractItemModel::modelReset, this, &GameInfoPane::updateTablePresentation);
    connect(m_model, &QAbstractItemModel::rowsInserted, this, &GameInfoPane::updateTablePresentation);
    connect(m_model, &QAbstractItemModel::rowsRemoved, this, &GameInfoPane::updateTablePresentation);
    connect(m_model, &QAbstractItemModel::dataChanged, this, &GameInfoPane::updateTablePresentation);
    connect(m_model, &GameInfoEditModel::editStateChanged, this, &GameInfoPane::updateEditingIndicator);
    // 表の中身の変更（編集・行の追加と削除・元に戻す）を文書へ知らせる
    connect(m_model, &QAbstractItemModel::dataChanged, this, &GameInfoPane::notifyEdited);
    connect(m_model, &QAbstractItemModel::rowsInserted, this, &GameInfoPane::notifyEdited);
    connect(m_model, &QAbstractItemModel::rowsRemoved, this, &GameInfoPane::notifyEdited);
    connect(m_model, &QAbstractItemModel::modelReset, this, &GameInfoPane::notifyEdited);
    connect(m_table->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &GameInfoPane::updateEditingIndicator);
    connect(m_table->selectionModel(), &QItemSelectionModel::currentChanged,
            this, &GameInfoPane::updateEditingIndicator);
    // 内容の列の幅が変わると折り返しが変わるので行の高さを合わせ、利用者が決めた項目名の列幅は保存する
    connect(header, &QHeaderView::sectionResized, m_table, &QTableView::resizeRowsToContents);
    connect(header, &TableHeaderView::sectionResizedByUser, this, &GameInfoPane::onSectionResizedByUser);
    connect(qApp, &QApplication::focusChanged, this, &GameInfoPane::observeEditor);
    connect(QApplication::clipboard(), &QClipboard::dataChanged, this, &GameInfoPane::updateEditingIndicator);
    connect(m_table->itemDelegate(), &QAbstractItemDelegate::closeEditor,
            this, &GameInfoPane::updateEditingIndicator, Qt::QueuedConnection);
    connect(&m_fontSize, &FontSizeHelper::fontSizeChanged, this, &GameInfoPane::applyFontSize);

    applyFontSize();
}

GameInfoPane::~GameInfoPane() = default;

// ============================================================
// 対局情報の設定と取得
// ============================================================

void GameInfoPane::setGameInfo(const QList<KifGameInfoItem>& items, bool beforeMatchStart)
{
    m_model->setItems(items, beforeMatchStart);
}

QList<KifGameInfoItem> GameInfoPane::gameInfo() const
{
    return m_model->items();
}

void GameInfoPane::notifyEdited()
{
    emit gameInfoEdited(m_model->items());
}

void GameInfoPane::updateGameInfoValue(const QString& key, const QString& value)
{
    commitPendingEditor();
    m_model->setAutomaticValue(key, value);
}

// ============================================================
// 未反映の変更
// ============================================================

bool GameInfoPane::isDirty() const
{
    const auto* editor = activeEditor();
    if (editor && editor->text() != m_table->currentIndex().data(Qt::EditRole).toString()) return true;
    return m_model->isDirty();
}

void GameInfoPane::commitPendingEditor()
{
    if (auto* editor = activeEditor()) {
        // Enterキーではdelegateの確定が次のイベントループまで遅延する。
        // 保存処理が直後に表を読むため、通常の確定シグナルを同期的に送る。
        auto* delegate = m_table->itemDelegateForIndex(m_table->currentIndex());
        emit delegate->commitData(editor);
        emit delegate->closeEditor(editor, QAbstractItemDelegate::NoHint);
    }
}

void GameInfoPane::applyChanges()
{
    commitPendingEditor();
    if (!m_model->isDirty()) return;
    m_model->markApplied();
    const QList<KifGameInfoItem> items = gameInfo();
    emit gameInfoUpdated(items);
    qCDebug(lcUi).noquote() << "[GameInfoPane] applyChanges: Game info updated, items=" << items.size();
}

// ============================================================
// 編集の履歴とクリップボード操作
// ============================================================

void GameInfoPane::undo()
{
    commitPendingEditor();
    const QModelIndex current = m_table->currentIndex();
    m_model->undo();
    restoreCurrentCell(current.row(), current.column());
}

void GameInfoPane::redo()
{
    commitPendingEditor();
    const QModelIndex current = m_table->currentIndex();
    m_model->redo();
    restoreCurrentCell(current.row(), current.column());
}

void GameInfoPane::restoreCurrentCell(int row, int column)
{
    if (m_model->rowCount() == 0) return;
    m_table->setCurrentIndex(m_model->index(qBound(0, row, m_model->rowCount() - 1), qMax(0, column)));
}

void GameInfoPane::cut()
{
    if (auto* editor = focusedEditor()) {
        editor->cut();
        return;
    }
    const QModelIndex cell = selectedCell();
    if (cell.flags() & Qt::ItemIsEditable) {
        QApplication::clipboard()->setText(cell.data(Qt::EditRole).toString());
        m_model->setData(cell, QString());
    }
}

void GameInfoPane::copy()
{
    if (auto* editor = focusedEditor()) {
        editor->copy();
        return;
    }
    const QModelIndex cell = selectedCell();
    if (cell.isValid()) QApplication::clipboard()->setText(cell.data(Qt::EditRole).toString());
}

void GameInfoPane::paste()
{
    if (auto* editor = focusedEditor()) {
        editor->paste();
        return;
    }
    const QModelIndex cell = selectedCell();
    if (cell.flags() & Qt::ItemIsEditable) m_model->setData(cell, QApplication::clipboard()->text());
}

void GameInfoPane::addRow()
{
    commitPendingEditor();
    const int row = m_model->appendRow();
    // 新しい行の項目名セルを選択して編集を始める
    const QModelIndex keyCell = m_model->index(row, GameInfoEditModel::KeyColumn);
    m_table->setCurrentIndex(keyCell);
    m_table->edit(keyCell);
}

void GameInfoPane::removeRow()
{
    const QModelIndex cell = selectedCell();
    if (!cell.isValid()) return;
    commitPendingEditor();
    const int row = cell.row();
    m_model->removeRowAt(row);
    if (m_model->rowCount() > 0)
        m_table->setCurrentIndex(m_model->index(qMin(row, m_model->rowCount() - 1), GameInfoEditModel::ValueColumn));
}

QModelIndex GameInfoPane::selectedCell() const
{
    const QModelIndex current = m_table->currentIndex();
    return current.isValid() && m_table->selectionModel()->isSelected(current) ? current : QModelIndex();
}

QLineEdit* GameInfoPane::focusedEditor() const
{
    auto* editor = qobject_cast<QLineEdit*>(QApplication::focusWidget());
    return editor && m_table->isAncestorOf(editor) ? editor : nullptr;
}

QLineEdit* GameInfoPane::activeEditor() const
{
    // 終局ダイアログなどへフォーカスが移った後にも、入力欄が残ることがある。
    const auto editors = m_table->findChildren<QLineEdit*>();
    for (auto* editor : editors) {
        if (editor->isVisibleTo(m_table)) return editor;
    }
    return nullptr;
}

// ============================================================
// 表の見た目と操作ボタンの状態
// ============================================================

void GameInfoPane::buildToolbar()
{
    m_toolbar = new QWidget(this);
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
    const auto button = [](QHBoxLayout* layout, const QString& name, const QString& text,
                           const QString& tip, const QString& icon) {
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
        layout->addWidget(result);
        return result;
    };
    auto* fonts = group();
    m_btnFontDecrease = button(fonts, QStringLiteral("gameInfoFontDecrease"), {}, {}, {});
    m_btnFontIncrease = button(fonts, QStringLiteral("gameInfoFontIncrease"), {}, {}, {});
    new FontSizeControl(m_btnFontDecrease, m_btnFontIncrease, &m_fontSize);
    // 元に戻す〜貼り付けは標準ショートカット付きの QAction（コメント欄と同じ部品）
    m_editActions = new EditActions(this);
    const auto editButton = [this](QHBoxLayout* layout, EditActions::Kind kind, const QString& name) {
        QToolButton* result = m_editActions->createButton(kind, layout->parentWidget());
        result->setObjectName(name);
        result->setStyleSheet(ButtonStyles::panelToolButton());
        result->setMinimumSize(32, 28);
        result->setIconSize(QSize(18, 18));
        layout->addWidget(result);
    };
    auto* history = group();
    editButton(history, EditActions::Undo, QStringLiteral("gameInfoUndo"));
    editButton(history, EditActions::Redo, QStringLiteral("gameInfoRedo"));
    auto* clipboard = group();
    editButton(clipboard, EditActions::Cut, QStringLiteral("gameInfoCut"));
    editButton(clipboard, EditActions::Copy, QStringLiteral("gameInfoCopy"));
    editButton(clipboard, EditActions::Paste, QStringLiteral("gameInfoPaste"));
    auto* rows = group();
    m_btnAddRow = button(rows, QStringLiteral("gameInfoAddRow"), tr("行を追加"), tr("新しい行を追加する"), {});
    m_btnRemoveRow = button(rows, QStringLiteral("gameInfoRemoveRow"), tr("行を削除"),
        tr("選択行を削除する（元に戻す操作で復元できます）"), {});
    connect(m_editActions->action(EditActions::Undo), &QAction::triggered, this, &GameInfoPane::undo);
    connect(m_editActions->action(EditActions::Redo), &QAction::triggered, this, &GameInfoPane::redo);
    connect(m_editActions->action(EditActions::Cut), &QAction::triggered, this, &GameInfoPane::cut);
    connect(m_editActions->action(EditActions::Copy), &QAction::triggered, this, &GameInfoPane::copy);
    connect(m_editActions->action(EditActions::Paste), &QAction::triggered, this, &GameInfoPane::paste);
    connect(m_btnAddRow, &QToolButton::clicked, this, &GameInfoPane::addRow);
    connect(m_btnRemoveRow, &QToolButton::clicked, this, &GameInfoPane::removeRow);

    auto* apply = group();
    m_btnUpdate = new QPushButton(tr("対局情報更新"), m_toolbar);
    m_btnUpdate->setObjectName(QStringLiteral("gameInfoApply"));
    m_btnUpdate->setToolTip(tr("編集した対局情報を棋譜に反映する (Ctrl+Enter)"));
    m_btnUpdate->setMinimumHeight(28);
    m_btnUpdate->setStyleSheet(ButtonStyles::primaryAction());
    connect(m_btnUpdate, &QPushButton::clicked, this, &GameInfoPane::applyChanges);
    apply->addWidget(m_btnUpdate);

    m_editingLabel = new QLabel(m_toolbar);
    m_editingLabel->setObjectName(QStringLiteral("gameInfoEditing"));
    // FlowLayout は sizeHint の幅で並べるため、折り返し可能なラベルは次の行に回ったとき
    // 余白があっても短く折り返される。案内文は短いので1行で表示する。
    m_editingLabel->setWordWrap(false);
    flow->addWidget(m_editingLabel);
}

void GameInfoPane::applyFontSize()
{
    QFont font = m_table->font();
    font.setPointSize(m_fontSize.fontSize());
    m_table->setFont(font);
    m_table->setStyleSheet(TableStyles::thinking(m_fontSize.fontSize()) + QStringLiteral(
        "QTableView { alternate-background-color: #f7f9fb; }"
        "QTableView::item { padding: 4px 8px; }"));
    m_table->verticalHeader()->setMinimumSectionSize(m_table->fontMetrics().height() + 10);
    updateTablePresentation();
    updateEditingIndicator();
}

void GameInfoPane::observeEditor()
{
    if (auto* editor = activeEditor()) {
        connect(editor, &QLineEdit::textChanged,
                this, &GameInfoPane::updateEditingIndicator, Qt::UniqueConnection);
        connect(editor, &QLineEdit::selectionChanged,
                this, &GameInfoPane::updateEditingIndicator, Qt::UniqueConnection);
    }
    updateEditingIndicator();
}

void GameInfoPane::updateEditingIndicator()
{
    const QModelIndex cell = selectedCell();
    const bool selected = cell.isValid();
    const bool editable = selected && (cell.flags() & Qt::ItemIsEditable);
    const auto* editor = activeEditor();
    const QString cellText = m_table->currentIndex().data(Qt::EditRole).toString();
    const bool pending = editor && m_table->currentIndex().isValid() && editor->text() != cellText;
    const bool hasText = editor ? editor->hasSelectedText()
                                : selected && !cell.data(Qt::EditRole).toString().isEmpty();
    m_editActions->setEnabled(EditActions::Undo, m_model->canUndo() || pending);
    m_editActions->setEnabled(EditActions::Redo, !pending && m_model->canRedo());
    m_editActions->setEnabled(EditActions::Copy, hasText);
    m_editActions->setEnabled(EditActions::Cut, editable && hasText);
    const auto* mime = QApplication::clipboard()->mimeData();
    m_editActions->setEnabled(EditActions::Paste, editable && mime && mime->hasText());
    m_btnRemoveRow->setEnabled(selected);
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

void GameInfoPane::updateTablePresentation()
{
    // 項目名の列は、利用者が幅を決めていなければ表記した項目名が収まる幅にする
    int keyWidth = 120;
    for (int row = 0; row < m_model->rowCount(); ++row) {
        const QString key = m_model->index(row, GameInfoEditModel::KeyColumn).data(Qt::AccessibleTextRole).toString();
        keyWidth = qMax(keyWidth, m_table->fontMetrics().horizontalAdvance(key) + 24);
    }
    m_table->setColumnWidth(GameInfoEditModel::KeyColumn, m_keyColumnWidth > 0 ? m_keyColumnWidth : qMin(keyWidth, 320));
    m_table->resizeRowsToContents();
}

void GameInfoPane::onSectionResizedByUser(int logicalIndex, int /*oldSize*/, int newSize)
{
    if (logicalIndex != GameInfoEditModel::KeyColumn) return;
    m_keyColumnWidth = newSize;
    GameSettings::gameInfoKeyColumnWidth.set(newSize);
}
