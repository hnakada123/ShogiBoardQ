#include "analysisresultspresenter.h"
#include <QDialog>
#include <QTableView>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QTimer>
#include <QScrollBar>
#include "numeric_right_align_comma_delegate.h"
#include "kifuanalysislistmodel.h"

AnalysisResultsPresenter::AnalysisResultsPresenter(QObject* parent)
    : QObject(parent)
    , m_reflowTimer(new QTimer(this))
{
    m_reflowTimer->setSingleShot(true);
    m_reflowTimer->setInterval(0);
    connect(m_reflowTimer, &QTimer::timeout, this, &AnalysisResultsPresenter::reflowNow);
}

void AnalysisResultsPresenter::showWithModel(KifuAnalysisListModel* model)
{
    buildUi(model);
    connectModelSignals(model);

    if (m_dlg) {
        m_dlg->resize(1000, 600);
        m_dlg->setModal(false);
        m_dlg->setWindowModality(Qt::NonModal);
        m_dlg->show();
    }

    // 初回レイアウト
    m_reflowTimer->start();
}

void AnalysisResultsPresenter::buildUi(KifuAnalysisListModel* model)
{
    if (m_dlg) {
        m_dlg->deleteLater();
        m_dlg = nullptr;
    }

    m_dlg = new QDialog;
    m_dlg->setAttribute(Qt::WA_DeleteOnClose, true);
    m_dlg->setWindowTitle(tr("棋譜解析結果"));

    m_view = new QTableView(m_dlg);
    m_view->setModel(model);
    m_view->setAlternatingRowColors(true);
    m_view->setWordWrap(false);
    m_view->verticalHeader()->setVisible(false);
    m_view->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_view->setSelectionMode(QAbstractItemView::SingleSelection);
    m_view->setTextElideMode(Qt::ElideRight);

    auto* numDelegate = new NumericRightAlignCommaDelegate(m_view);
    m_view->setItemDelegateForColumn(1, numDelegate); // Evaluation Value
    m_view->setItemDelegateForColumn(2, numDelegate); // Difference

    m_header = m_view->horizontalHeader();
    m_header->setMinimumSectionSize(60);
    m_header->setSectionResizeMode(0, QHeaderView::ResizeToContents); // Move
    m_header->setSectionResizeMode(1, QHeaderView::ResizeToContents); // Evaluation
    m_header->setSectionResizeMode(2, QHeaderView::ResizeToContents); // Difference
    m_header->setSectionResizeMode(3, QHeaderView::Stretch);          // PV
    m_header->setStretchLastSection(true);

    QVBoxLayout* lay = new QVBoxLayout(m_dlg);
    lay->setContentsMargins(8,8,8,8);
    lay->addWidget(m_view);

    // スクロールバーの出現/消失でも幅が変わる
    connect(m_view->verticalScrollBar(), &QAbstractSlider::rangeChanged,
            this, &AnalysisResultsPresenter::onScrollRangeChanged);
}

void AnalysisResultsPresenter::connectModelSignals(KifuAnalysisListModel* model)
{
    if (!model) return;
    connect(model, &QAbstractItemModel::modelReset,   this, &AnalysisResultsPresenter::onModelReset);
    connect(model, &QAbstractItemModel::rowsInserted, this, &AnalysisResultsPresenter::onRowsInserted);
    connect(model, &QAbstractItemModel::dataChanged,  this, &AnalysisResultsPresenter::onDataChanged);
    connect(model, &QAbstractItemModel::layoutChanged,this, &AnalysisResultsPresenter::onLayoutChanged);
}

void AnalysisResultsPresenter::reflowNow()
{
    if (!m_view || !m_header) return;

    // 0..2 を内容幅に
    m_header->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_header->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_header->setSectionResizeMode(2, QHeaderView::ResizeToContents);

    m_view->resizeColumnsToContents();

    // PV を一時的に Interactive に変更して幅を与える
    m_header->setSectionResizeMode(3, QHeaderView::Interactive);

    const int w0 = m_header->sectionSize(0);
    const int w1 = m_header->sectionSize(1);
    const int w2 = m_header->sectionSize(2);

    const int viewportW = m_view->viewport()->width();
    const int vScrollW  = m_view->verticalScrollBar()->isVisible()
                             ? m_view->verticalScrollBar()->width() : 0;
    const int margins   = 4;
    const int remain    = viewportW - (w0 + w1 + w2) - vScrollW - margins;
    const int pvWidth   = qMax(160, remain);

    m_header->resizeSection(3, pvWidth);

    // Stretch に戻す（ウィンドウ幅追従）
    m_header->setSectionResizeMode(3, QHeaderView::Stretch);
    m_header->setStretchLastSection(true);
}

void AnalysisResultsPresenter::onModelReset() { m_reflowTimer->start(); }
void AnalysisResultsPresenter::onRowsInserted(const QModelIndex&, int, int) { m_reflowTimer->start(); }
void AnalysisResultsPresenter::onDataChanged(const QModelIndex&, const QModelIndex&, const QList<int>&) { m_reflowTimer->start(); }
void AnalysisResultsPresenter::onLayoutChanged() { m_reflowTimer->start(); }
void AnalysisResultsPresenter::onScrollRangeChanged(int, int) { m_reflowTimer->start(); }
