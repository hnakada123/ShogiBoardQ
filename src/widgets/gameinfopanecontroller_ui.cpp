/// @file gameinfopanecontroller_ui.cpp
/// @brief 対局情報の表示と操作ボタンの状態管理

#include "gameinfopanecontroller.h"
#include "buttonstyles.h"
#include "flowlayout.h"
#include "gamesettings.h"
#include "gameinfokeys.h"
#include "kifupresentation.h"

#include <QApplication>
#include <QClipboard>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMimeData>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStyledItemDelegate>
#include <QTableWidget>
#include <QToolButton>

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
    if (key == GameInfoKeys::kBlackPlayer || key == GameInfoKeys::kWhitePlayer)
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
    m_editingLabel->setWordWrap(true);
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
    m_editingLabel->setStyleSheet(dirty
        ? QStringLiteral("QLabel { color: #8a5800; font-weight: bold; }")
        : QStringLiteral("QLabel { color: #596773; }"));
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
        GameSettings::setGameInfoKeyColumnWidth(newSize);
    }
    m_table->resizeRowsToContents();
}
