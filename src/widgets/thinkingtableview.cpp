/// @file thinkingtableview.cpp
/// @brief 思考の表の実装

#include "thinkingtableview.h"
#include "columnwidthmemory.h"
#include "scoretext.h"

#include "shogienginethinkingmodel.h"
#include "tablemodelutils.h"
#include "tableheaderview.h"

#include <QFontMetrics>
#include <QHeaderView>

using Column = ShogiEngineThinkingModel::Column;

namespace {
PvTableView::Columns thinkingColumns()
{
    PvTableView::Columns columns;
    columns.numeric = {Column::TimeColumn, Column::DepthColumn, Column::NodesColumn, Column::ScoreColumn};
    columns.board = Column::BoardColumn;
    columns.pv = Column::PvColumn;
    return columns;
}
} // namespace

ThinkingTableView::ThinkingTableView(AnalysisSettings::ThinkingTable table, QWidget* parent)
    : PvTableView(thinkingColumns(), 4, parent)
    , m_table(table)
{
    // 思考を始めるたびに表を消すので、縦のスクロールバーが毎回消えては現れ、そのたびに
    // 表示領域と最後の列の幅が変わって、表と見出しの全体を描き直していた。常に出しておく。
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);

    auto* header = static_cast<TableHeaderView*>(horizontalHeader());
    header->setDefaultSectionSize(100);
    header->setMinimumSectionSize(24);
    header->setStretchLastSection(true);

    // 上下限の列を足す前の保存（列が1つ少ない）は、盤面・読み筋の位置をずらして引き継ぐ
    const ListSetting<int> widths = AnalysisSettings::thinkingViewColumnWidths(table);
    if (QList<int> saved = widths(); saved.size() == Column::ColumnCount - 1) {
        saved.insert(Column::BoundColumn, 0);
        widths.set(saved);
    }
    // 読み筋の列（最後の列）は表示領域に合わせて伸び縮みするので、ColumnWidthMemory は覚えない
    m_columnWidths = new ColumnWidthMemory(header, widths, Column::ColumnCount);
    connect(this, &PvTableView::boardRequested, this, &ThinkingTableView::onBoardRequested);
    connect(this, &PvTableView::rowContextMenuAboutToShow, this, &ThinkingTableView::onRowContextMenuAboutToShow);
}

void ThinkingTableView::setThinkingModel(ShogiEngineThinkingModel* model)
{
    m_model = model;
    TableModelUtils::setModel(this, model);
    fitColumns();
}

void ThinkingTableView::onBoardRequested(int row)
{
    if (m_model) emit pvRowClicked(m_model, row);
}

void ThinkingTableView::onRowContextMenuAboutToShow(int row, QMenu* menu)
{
    if (m_model) emit thinkingRowContextMenuAboutToShow(m_model, row, menu);
}

void ThinkingTableView::fitColumns()
{
    if (!m_model) return;
    const QFontMetrics metrics(font());
    // 利用者が決めていない列は、見出し・数値の見本（3桁区切り）・上下限の記号・「盤面」ボタンの文字が収まる幅
    const QString samples[] = {QStringLiteral("99,999"), QStringLiteral("99"), QStringLiteral("99,999,999"),
                               QStringLiteral("-9,999"), QStringLiteral("++"), ShogiEngineThinkingModel::boardButtonText()};
    for (int column = Column::TimeColumn; column < Column::PvColumn; ++column) {
        const QString title = m_model->headerData(column, Qt::Horizontal).toString();
        int sampleWidth = metrics.horizontalAdvance(samples[column]);
        if (column == Column::BoundColumn) sampleWidth = qMax(sampleWidth, metrics.horizontalAdvance(QStringLiteral("--")));
        // 評価値列は、訳語の長い詰みの表示（英語では "Mate in 99 plies"）も省略せずに表示する
        if (column == Column::ScoreColumn) sampleWidth = qMax(sampleWidth, metrics.horizontalAdvance(ScoreText::mateIn(-99)));
        const int automatic = qMax(metrics.horizontalAdvance(title), sampleWidth) + 20;
        setColumnWidth(column, m_columnWidths->widthOr(column, automatic));
    }
    // 検討の読み筋は、狭いドックでも横スクロールで読めるよう幅を確保する（残りの幅があれば伸ばす）
    if (m_table == AnalysisSettings::ThinkingTable::Consideration)
        setColumnWidth(Column::PvColumn, metrics.horizontalAdvance(QStringLiteral("M")) * 24);
}
