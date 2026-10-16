/// @file thinkingtableview.cpp
/// @brief 思考の表の実装

#include "thinkingtableview.h"

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

    connect(header, &TableHeaderView::sectionResizedByUser, this, &ThinkingTableView::onSectionResizedByUser);
    connect(this, &PvTableView::boardRequested, this, &ThinkingTableView::onBoardRequested);
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

void ThinkingTableView::onSectionResizedByUser(int logicalIndex)
{
    // 読み筋の列は表示領域に合わせて伸び縮みするので、その幅だけが変わったときは保存しない
    if (!m_model || logicalIndex == Column::PvColumn) return;
    QList<int> widths;
    for (int column = 0; column < Column::ColumnCount; ++column) widths.append(columnWidth(column));
    AnalysisSettings::setThinkingViewColumnWidths(m_table, widths);
}

void ThinkingTableView::fitColumns()
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
