/// @file pvtableview.cpp
/// @brief 読み筋の表の実装

#include "pvtableview.h"

#include <QHeaderView>
#include <QItemSelectionModel>
#include <QMenu>
#include <QStyle>

#include "kifumovedelegate.h"
#include "numericrightaligncommadelegate.h"
#include "pvtextdelegate.h"
#include "tablebuttondelegate.h"
#include "tablestyles.h"

PvTableView::PvTableView(const PvTableView::Columns& columns, int rowPadding, QWidget* parent)
    : IncrementalTableView(parent)
    , m_columns(columns)
    , m_rowPadding(rowPadding)
{
    setStyleSheet(TableStyles::thinking());
    setShowGrid(false);
    setMouseTracking(true);
    setWordWrap(false);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);

    auto* numeric = new NumericRightAlignCommaDelegate(this);
    for (const int column : std::as_const(m_columns.numeric)) setItemDelegateForColumn(column, numeric);
    if (m_columns.board >= 0) setItemDelegateForColumn(m_columns.board, new TableButtonDelegate(this));
    if (m_columns.pv >= 0) setItemDelegateForColumn(m_columns.pv, new PvTextDelegate(this));

    verticalHeader()->setVisible(false);
    verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    updateRowHeight();

    // 「盤面」はクリックでも Enter（activated）でも開く
    connect(this, &QTableView::clicked, this, &PvTableView::onClicked);
    connect(this, &QTableView::activated, this, &PvTableView::onActivated);
    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &QWidget::customContextMenuRequested, this, &PvTableView::onCustomContextMenuRequested);
}

void PvTableView::setFontPointSize(int pointSize)
{
    QFont font = this->font();
    font.setPointSize(pointSize);
    setFont(font);
    setStyleSheet(TableStyles::thinking(pointSize));
    updateRowHeight();
    fitColumns();
}

void PvTableView::clearRowSelection()
{
    if (auto* selection = selectionModel()) {
        selection->clearSelection();
        selection->setCurrentIndex(QModelIndex(), QItemSelectionModel::NoUpdate);
    }
}

void PvTableView::onClicked(const QModelIndex& index)
{
    if (index.isValid() && index.column() == m_columns.board) emit boardRequested(index.row());
}

void PvTableView::onActivated(const QModelIndex& index)
{
    // 1回のクリックで activated も出す環境では、clicked で開いたものを開き直さない
    if (style()->styleHint(QStyle::SH_ItemView_ActivateItemOnSingleClick, nullptr, this)) return;
    onClicked(index);
}

void PvTableView::onCustomContextMenuRequested(const QPoint& pos)
{
    const QModelIndex index = indexAt(pos);
    if (!index.isValid()) return;
    // 行は選び直さない（棋譜解析の結果の表では、行を選ぶと棋譜がその手へ移る）
    QMenu menu(this);
    menu.setToolTipsVisible(true);
    emit rowContextMenuAboutToShow(index.row(), &menu);
    if (!menu.isEmpty()) menu.exec(viewport()->mapToGlobal(pos));
}

void PvTableView::updateRowHeight()
{
    // 読み筋は日本語棋譜の書体で書くことがあるので、高い方の書体に合わせる
    verticalHeader()->setDefaultSectionSize(KifuMoveDelegate::textHeight(font()) + m_rowPadding);
}
