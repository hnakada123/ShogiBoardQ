/// @file recordpaneappearancemanager.cpp
/// @brief 棋譜欄の外観管理クラスの実装

#include "recordpaneappearancemanager.h"
#include "gamesettings.h"
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
        const int rowHeight = kifu->fontMetrics().height() + 4;
        kifu->verticalHeader()->setDefaultSectionSize(rowHeight);
    }

    if (branch) {
        branch->setFont(font);
        branch->setStyleSheet(branchTableStyleSheet(m_fontSize));
        const int rowHeight = branch->fontMetrics().height() + 4;
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

    // col 0: 常に ResizeToContents
    hh->setSectionResizeMode(0, QHeaderView::ResizeToContents);

    const bool col1Visible = !kifu->isColumnHidden(1);
    const bool col2Visible = !kifu->isColumnHidden(2);
    const bool col3Visible = !kifu->isColumnHidden(3);

    // 最右の表示列が Stretch を受け持つ
    if (col3Visible) {
        if (col1Visible) {
            hh->setSectionResizeMode(1, QHeaderView::Fixed);
            hh->resizeSection(1, 130);
        }
        if (col2Visible)
            hh->setSectionResizeMode(2, QHeaderView::ResizeToContents);
        hh->setSectionResizeMode(3, QHeaderView::Stretch);
    } else if (col2Visible) {
        if (col1Visible) {
            hh->setSectionResizeMode(1, QHeaderView::Fixed);
            hh->resizeSection(1, 130);
        }
        hh->setSectionResizeMode(2, QHeaderView::Stretch);
    } else if (col1Visible) {
        hh->setSectionResizeMode(1, QHeaderView::Stretch);
    } else {
        hh->setSectionResizeMode(0, QHeaderView::Stretch);
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
    const QColor kSelYellow(255, 255, 0);
    pal.setColor(QPalette::Active,   QPalette::Highlight,       kSelYellow);
    pal.setColor(QPalette::Inactive, QPalette::Highlight,       kSelYellow);
    pal.setColor(QPalette::Active,   QPalette::HighlightedText, Qt::black);
    pal.setColor(QPalette::Inactive, QPalette::HighlightedText, Qt::black);
    view->setPalette(pal);
}

// --------------------------------------------------------
// Stylesheet generation
// --------------------------------------------------------

QString RecordPaneAppearanceManager::kifuTableStyleSheet(int fontSize)
{
    return QStringLiteral(
        "QTableView {"
        "  background-color: #ffffff;"
        "}"
        "QTableView::item:selected:active {"
        "  background-color: #ffff00;"
        "  color: black;"
        "}"
        "QTableView::item:selected:!active {"
        "  background-color: #ffff00;"
        "  color: black;"
        "}"
        "QHeaderView::section {"
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        "    stop:0 #40acff, stop:1 #209cee);"
        "  color: white;"
        "  font-weight: normal;"
        "  font-size: %1pt;"
        "  padding: 2px 6px;"
        "  border: none;"
        "  border-bottom: 1px solid #209cee;"
        "}").arg(fontSize);
}

QString RecordPaneAppearanceManager::branchTableStyleSheet(int fontSize)
{
    return QStringLiteral(
        "QTableView {"
        "  background-color: #ffffff;"
        "  selection-background-color: #ffff00;"
        "  selection-color: black;"
        "}"
        "QTableView::item {"
        "  background-color: #ffffff;"
        "}"
        "QTableView::item:selected:active {"
        "  background-color: #ffff00;"
        "  color: black;"
        "}"
        "QTableView::item:selected:!active {"
        "  background-color: #ffff00;"
        "  color: black;"
        "}"
        "QHeaderView::section {"
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        "    stop:0 #40acff, stop:1 #209cee);"
        "  color: white;"
        "  font-weight: normal;"
        "  font-size: %1pt;"
        "  padding: 2px 6px;"
        "  border: none;"
        "  border-bottom: 1px solid #209cee;"
        "}").arg(fontSize);
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
