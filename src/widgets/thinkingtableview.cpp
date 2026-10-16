/// @file thinkingtableview.cpp
/// @brief 思考の表の実装

#include "thinkingtableview.h"

#include "kifumovedelegate.h"
#include "numericrightaligncommadelegate.h"
#include "pvtextdelegate.h"
#include "shogienginethinkingmodel.h"
#include "tablebuttondelegate.h"
#include "tablemodelutils.h"
#include "tableheaderview.h"
#include "tablestyles.h"

#include <QFontMetrics>
#include <QHeaderView>
#include <QItemSelectionModel>

using Column = ShogiEngineThinkingModel::Column;

ThinkingTableView::ThinkingTableView(AnalysisSettings::ThinkingTable table, QWidget* parent)
    : IncrementalTableView(parent)
    , m_table(table)
{
    setStyleSheet(TableStyles::thinking());
    setShowGrid(false);
    setMouseTracking(true);
    setWordWrap(false);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    // 思考を始めるたびに表を消すので、縦のスクロールバーが毎回消えては現れ、そのたびに
    // 表示領域と最後の列の幅が変わって、表と見出しの全体を描き直していた。常に出しておく。
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);

    auto* numeric = new NumericRightAlignCommaDelegate(this);
    for (const int column : {Column::TimeColumn, Column::DepthColumn, Column::NodesColumn, Column::ScoreColumn})
        setItemDelegateForColumn(column, numeric);
    setItemDelegateForColumn(Column::BoardColumn, new TableButtonDelegate(this));
    setItemDelegateForColumn(Column::PvColumn, new PvTextDelegate(this));

    auto* header = static_cast<TableHeaderView*>(horizontalHeader());
    header->setDefaultSectionSize(100);
    header->setMinimumSectionSize(24);
    header->setStretchLastSection(true);
    verticalHeader()->setVisible(false);
    verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    verticalHeader()->setDefaultSectionSize(KifuMoveDelegate::textHeight(font()) + 4);

    connect(header, &TableHeaderView::sectionResizedByUser, this, &ThinkingTableView::onSectionResizedByUser);
    connect(this, &QTableView::clicked, this, &ThinkingTableView::onClicked);
}

void ThinkingTableView::setThinkingModel(ShogiEngineThinkingModel* model)
{
    m_model = model;
    TableModelUtils::setModel(this, model);
    applyColumnWidths();
}

void ThinkingTableView::setFontPointSize(int pointSize)
{
    QFont font;
    font.setPointSize(pointSize);
    setFont(font);
    setStyleSheet(TableStyles::thinking(pointSize));
    verticalHeader()->setDefaultSectionSize(KifuMoveDelegate::textHeight(this->font()) + 4);
    applyColumnWidths();
}

void ThinkingTableView::clearRowSelection()
{
    if (auto* selection = selectionModel()) {
        selection->clearSelection();
        selection->setCurrentIndex(QModelIndex(), QItemSelectionModel::NoUpdate);
    }
}

void ThinkingTableView::onClicked(const QModelIndex& index)
{
    if (m_model && index.isValid() && index.column() == Column::BoardColumn) emit pvRowClicked(m_model, index.row());
}

void ThinkingTableView::onSectionResizedByUser(int logicalIndex)
{
    // 読み筋の列は表示領域に合わせて伸び縮みするので、その幅だけが変わったときは保存しない
    if (!m_model || logicalIndex == Column::PvColumn) return;
    QList<int> widths;
    for (int column = 0; column < Column::ColumnCount; ++column) widths.append(columnWidth(column));
    AnalysisSettings::setThinkingViewColumnWidths(m_table, widths);
}

void ThinkingTableView::applyColumnWidths()
{
    if (!m_model) return;
    const QFontMetrics metrics(font());
    const QList<int> saved = AnalysisSettings::thinkingViewColumnWidths(m_table);
    // 数値の見本（3桁区切り）と「盤面」ボタンの文字が収まる幅を下限にする
    const QString samples[] = {QStringLiteral("99,999"), QStringLiteral("99"), QStringLiteral("99,999,999"),
                               QStringLiteral("-9,999"), ShogiEngineThinkingModel::boardButtonText()};
    for (int column = Column::TimeColumn; column < Column::PvColumn; ++column) {
        const QString title = m_model->headerData(column, Qt::Horizontal).toString();
        const int minimum = qMax(metrics.horizontalAdvance(title), metrics.horizontalAdvance(samples[column])) + 20;
        const int width = saved.size() == Column::ColumnCount ? qMax(minimum, saved.at(column)) : minimum;
        setColumnWidth(column, width);
    }
    // 評価値列は、訳語の長い詰みの表示（英語では "Mate in 99 plies"）も省略せずに表示する。
    const int mateWidth = metrics.horizontalAdvance(ShogiEngineThinkingModel::mateScoreText(-99)) + 20;
    setColumnWidth(Column::ScoreColumn, qMax(columnWidth(Column::ScoreColumn), mateWidth));
    // 検討の読み筋は、狭いドックでも横スクロールで読めるよう幅を確保する（残りの幅があれば伸ばす）
    if (m_table == AnalysisSettings::ThinkingTable::Consideration)
        setColumnWidth(Column::PvColumn, metrics.horizontalAdvance(QStringLiteral("M")) * 24);
}
