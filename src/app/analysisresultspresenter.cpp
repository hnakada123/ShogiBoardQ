#include "analysisresultspresenter.h"
#include <QDialog>
#include <QTableView>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTimer>
#include <QScrollBar>
#include <QPushButton>
#include <QMessageBox>
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
    
    // テーブル全体の水平スクロールは無効化（ヘッダーを常に表示するため）
    m_view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    
    // ダブルクリックで読み筋表示ウィンドウを開く
    connect(m_view, &QTableView::doubleClicked, this, &AnalysisResultsPresenter::onTableDoubleClicked);

    auto* numDelegate = new NumericRightAlignCommaDelegate(m_view);
    m_view->setItemDelegateForColumn(1, numDelegate); // 評価値
    m_view->setItemDelegateForColumn(2, numDelegate); // 差

    m_header = m_view->horizontalHeader();
    m_header->setMinimumSectionSize(60);
    m_header->setSectionResizeMode(0, QHeaderView::ResizeToContents); // 指し手
    m_header->setSectionResizeMode(1, QHeaderView::ResizeToContents); // 評価値
    m_header->setSectionResizeMode(2, QHeaderView::ResizeToContents); // 差
    m_header->setSectionResizeMode(3, QHeaderView::Stretch);          // 読み筋（残り幅を使用）
    m_header->setStretchLastSection(true);  // 最後の列を引き伸ばす

    // 棋譜解析中止ボタン
    m_stopButton = new QPushButton(tr("棋譜解析中止"), m_dlg);
    connect(m_stopButton, &QPushButton::clicked, this, &AnalysisResultsPresenter::stopRequested);

    // 閉じるボタン
    QPushButton* closeButton = new QPushButton(tr("閉じる"), m_dlg);
    connect(closeButton, &QPushButton::clicked, m_dlg, &QDialog::accept);

    // ボタン用の水平レイアウト（右寄せ）
    QHBoxLayout* buttonLayout = new QHBoxLayout;
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_stopButton);
    buttonLayout->addWidget(closeButton);

    QVBoxLayout* lay = new QVBoxLayout(m_dlg);
    lay->setContentsMargins(8,8,8,8);
    lay->addWidget(m_view);
    lay->addLayout(buttonLayout);

    // ウィンドウリサイズ時に再レイアウト
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

    // 指し手・評価値・差を内容幅に合わせる
    m_header->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_header->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_header->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    
    m_view->resizeColumnToContents(0);
    m_view->resizeColumnToContents(1);
    m_view->resizeColumnToContents(2);
    
    // 読み筋列は残りの幅を使用（Stretchモード）
    m_header->setSectionResizeMode(3, QHeaderView::Stretch);
    m_header->setStretchLastSection(true);
}

void AnalysisResultsPresenter::onModelReset() { m_reflowTimer->start(); }
void AnalysisResultsPresenter::onRowsInserted(const QModelIndex&, int, int) { m_reflowTimer->start(); }
void AnalysisResultsPresenter::onDataChanged(const QModelIndex&, const QModelIndex&, const QList<int>&) { m_reflowTimer->start(); }
void AnalysisResultsPresenter::onLayoutChanged() { m_reflowTimer->start(); }
void AnalysisResultsPresenter::onScrollRangeChanged(int, int) { m_reflowTimer->start(); }

void AnalysisResultsPresenter::setStopButtonEnabled(bool enabled)
{
    if (m_stopButton) {
        m_stopButton->setEnabled(enabled);
    }
}

void AnalysisResultsPresenter::onTableDoubleClicked(const QModelIndex& index)
{
    if (!index.isValid()) return;
    
    qDebug().noquote() << "[AnalysisResultsPresenter::onTableDoubleClicked] row=" << index.row();
    Q_EMIT rowDoubleClicked(index.row());
}

void AnalysisResultsPresenter::showAnalysisComplete(int totalMoves)
{
    if (!m_dlg) return;
    
    // 中止ボタンを無効化
    setStopButtonEnabled(false);
    
    // 完了メッセージを表示
    QMessageBox::information(
        m_dlg,
        tr("棋譜解析完了"),
        tr("棋譜解析が完了しました。\n\n解析手数: %1 手").arg(totalMoves),
        QMessageBox::Ok
    );
}
