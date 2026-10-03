/// @file recordpaneappearancemanager.cpp
/// @brief 棋譜欄の外観管理クラスの実装

#include "recordpaneappearancemanager.h"
#include "kifumovedelegate.h"
#include "gamesettings.h"
#include "tablestyles.h"
#include "logcategories.h"

#include <QTableView>
#include <QHeaderView>
#include <QPalette>
#include <QFont>
#include <QAbstractItemView>
#include <QItemSelectionModel>

RecordPaneAppearanceManager::RecordPaneAppearanceManager(int initialFontSize)
    : m_fontSize(initialFontSize)
{
}

// --------------------------------------------------------
// Font management
// --------------------------------------------------------

bool RecordPaneAppearanceManager::tryIncreaseFontSize()
{
    if (m_fontSize < 24) {
        m_fontSize += 1;
        GameSettings::setKifuPaneFontSize(m_fontSize);
        return true;
    }
    return false;
}

bool RecordPaneAppearanceManager::tryDecreaseFontSize()
{
    if (m_fontSize > 8) {
        m_fontSize -= 1;
        GameSettings::setKifuPaneFontSize(m_fontSize);
        return true;
    }
    return false;
}

void RecordPaneAppearanceManager::applyFontToViews(QTableView* kifu, QTableView* branch)
{
    QFont font;
    font.setPointSize(m_fontSize);

    if (kifu) {
        kifu->setFont(font);
        kifu->setStyleSheet(kifuTableStyleSheet(m_fontSize));
        const int rowHeight = KifuMoveDelegate::textHeight(kifu->font()) + 4;
        kifu->verticalHeader()->setDefaultSectionSize(rowHeight);
    }

    if (branch) {
        branch->setFont(font);
        branch->setStyleSheet(branchTableStyleSheet(m_fontSize));
        const int rowHeight = KifuMoveDelegate::textHeight(branch->font()) + 4;
        branch->verticalHeader()->setDefaultSectionSize(rowHeight);
    }
}

// --------------------------------------------------------
// Column management
// --------------------------------------------------------

void RecordPaneAppearanceManager::toggleTimeColumn(QTableView* kifu, bool visible)
{
    if (kifu) {
        kifu->setColumnHidden(1, !visible);
        updateColumnResizeModes(kifu);
    }
    GameSettings::setKifuTimeColumnVisible(visible);
}

void RecordPaneAppearanceManager::toggleBookmarkColumn(QTableView* kifu, bool visible)
{
    if (kifu) {
        kifu->setColumnHidden(2, !visible);
        updateColumnResizeModes(kifu);
    }
    GameSettings::setKifuBookmarkColumnVisible(visible);
}

void RecordPaneAppearanceManager::toggleCommentColumn(QTableView* kifu, bool visible)
{
    if (kifu) {
        kifu->setColumnHidden(3, !visible);
        updateColumnResizeModes(kifu);
    }
    GameSettings::setKifuCommentColumnVisible(visible);
}

void RecordPaneAppearanceManager::applyColumnVisibility(QTableView* kifu)
{
    if (!kifu) return;

    const bool timeVisible = GameSettings::kifuTimeColumnVisible();
    const bool bookmarkVisible = GameSettings::kifuBookmarkColumnVisible();
    const bool commentVisible = GameSettings::kifuCommentColumnVisible();

    kifu->setColumnHidden(1, !timeVisible);
    kifu->setColumnHidden(2, !bookmarkVisible);
    kifu->setColumnHidden(3, !commentVisible);

    updateColumnResizeModes(kifu);
}

void RecordPaneAppearanceManager::updateColumnResizeModes(QTableView* kifu)
{
    if (!kifu) return;
    auto* hh = kifu->horizontalHeader();
    if (!hh) return;

    // 指し手列は内容に合わせ、表全体の幅は RecordPane 側で調整する
    hh->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    hh->setSectionResizeMode(1, QHeaderView::ResizeToContents);

    const bool col2Visible = !kifu->isColumnHidden(2);
    const bool col3Visible = !kifu->isColumnHidden(3);

    // しおり・コメントの最右の表示列だけを Stretch にする
    if (col3Visible) {
        if (col2Visible)
            hh->setSectionResizeMode(2, QHeaderView::ResizeToContents);
        hh->setSectionResizeMode(3, QHeaderView::Stretch);
    } else if (col2Visible) {
        hh->setSectionResizeMode(2, QHeaderView::Stretch);
    }
}

// --------------------------------------------------------
// Selection appearance
// --------------------------------------------------------

void RecordPaneAppearanceManager::setupSelectionPalette(QTableView* view)
{
    if (!view) return;

    view->setSelectionBehavior(QAbstractItemView::SelectRows);
    view->setSelectionMode(QAbstractItemView::SingleSelection);

    QPalette pal = view->palette();
    const QColor selectionBg = TableStyles::selectionBackground();
    pal.setColor(QPalette::Active,   QPalette::Highlight,       selectionBg);
    pal.setColor(QPalette::Inactive, QPalette::Highlight,       selectionBg);
    pal.setColor(QPalette::Active,   QPalette::HighlightedText, TableStyles::selectionText());
    pal.setColor(QPalette::Inactive, QPalette::HighlightedText, TableStyles::selectionText());
    view->setPalette(pal);
}

// --------------------------------------------------------
// Stylesheet generation
// --------------------------------------------------------

QString RecordPaneAppearanceManager::kifuTableStyleSheet(int fontSize)
{
    return TableStyles::header(fontSize) + TableStyles::selection() + QStringLiteral(
        "QTableView { background-color: #ffffff; color: #303841; }");
}

QString RecordPaneAppearanceManager::branchTableStyleSheet(int fontSize)
{
    return kifuTableStyleSheet(fontSize);
}

// --------------------------------------------------------
// Kifu view enable/disable
// --------------------------------------------------------

void RecordPaneAppearanceManager::setKifuViewEnabled(QTableView* kifu, bool on, bool& navigationDisabled)
{
    if (!kifu) return;

    const QString kBaseStyleSheet = kifuTableStyleSheet(m_fontSize);

    qCDebug(lcUi) << "setKifuViewEnabled called, on=" << on;

    navigationDisabled = !on;

    kifu->viewport()->setAttribute(Qt::WA_TransparentForMouseEvents, !on);

    if (!on) {
        kifu->setFocusPolicy(Qt::NoFocus);
        kifu->clearFocus();

        kifu->setStyleSheet(kBaseStyleSheet + QStringLiteral(
            "QTableView::item:focus { background-color: transparent; }"
            "QTableView::item:selected:focus { background-color: transparent; }"
        ));
        qCDebug(lcUi) << "setKifuViewEnabled: disabled stylesheet applied";

        if (QItemSelectionModel* sel = kifu->selectionModel()) {
            const bool wasBlocked = sel->blockSignals(true);
            sel->clearSelection();
            sel->setCurrentIndex(QModelIndex(), QItemSelectionModel::Clear);
            sel->blockSignals(wasBlocked);
        }

        kifu->viewport()->update();
    } else {
        kifu->setFocusPolicy(Qt::StrongFocus);
        kifu->setStyleSheet(kBaseStyleSheet);
        qCDebug(lcUi) << "setKifuViewEnabled: base stylesheet restored (enabled)";
    }
}
