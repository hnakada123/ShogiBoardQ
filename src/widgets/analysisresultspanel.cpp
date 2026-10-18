/// @file analysisresultspanel.cpp
/// @brief 棋譜解析の結果ドックの中身の実装

#include "analysisresultspanel.h"
#include "fontsizecontrol.h"
#include "tableheaderview.h"
#include "tablemodelutils.h"
#include "logcategories.h"
#include "buttonstyles.h"
#include <QTableView>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTimer>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>
#include <QSignalBlocker>
#include "elidelabel.h"
#include "analysissettings.h"
#include "kifuanalysislistmodel.h"

namespace {
/// 幅を内容に合わせ、利用者が変えた幅を保存する列（読み筋より前の列）
constexpr int kSizedColumns = KifuAnalysisListModel::PvColumn;
} // namespace

AnalysisResultsPanel::AnalysisResultsPanel(QWidget* parent)
    : QWidget(parent)
    , m_reflowTimer(new QTimer(this))
    // 未保存なら 10pt から始める
    , m_fontSize({AnalysisSettings::kifuAnalysisFontSize.valueOr(10), 8, 24, 1,
                  [](int size) { AnalysisSettings::kifuAnalysisFontSize.set(size); }})
{
    connect(&m_fontSize, &FontSizeHelper::fontSizeChanged, this, &AnalysisResultsPanel::applyFontSize);
    m_reflowTimer->setSingleShot(true);
    m_reflowTimer->setInterval(0);
    connect(m_reflowTimer, &QTimer::timeout, this, &AnalysisResultsPanel::reflowNow);

    // 読み筋の表（思考の表と共通）。評価値と差は数値の列
    PvTableView::Columns columns;
    columns.numeric = {KifuAnalysisListModel::EvaluationColumn, KifuAnalysisListModel::DifferenceColumn};
    columns.board = KifuAnalysisListModel::BoardColumn;
    columns.pv = KifuAnalysisListModel::PvColumn;
    m_view = new PvTableView(columns, 10, this);
    m_view->setObjectName(QStringLiteral("analysisResultsTable"));

    // ヘッダー表示用に空のモデルを作成（起動時からヘッダーを表示するため）
    auto* emptyModel = new KifuAnalysisListModel(m_view);
    TableModelUtils::setModel(m_view, emptyModel);

    m_view->setAlternatingRowColors(true);
    m_view->setTextElideMode(Qt::ElideRight);
    m_view->setAccessibleName(tr("棋譜解析結果"));
    // 盤面列のクリック・Enter で読み筋表示ウィンドウを開く
    connect(m_view, &PvTableView::boardRequested, this, &AnalysisResultsPanel::onBoardRequested);

    auto* header = static_cast<TableHeaderView*>(m_view->horizontalHeader());
    m_header = header;
    m_header->setMinimumSectionSize(30);
    m_header->setDefaultSectionSize(30);

    // 空のモデルが設定されているのでヘッダー設定を適用
    setupHeaderConfiguration();

    m_columnWidths = AnalysisSettings::kifuAnalysisColumnWidths();
    if (m_columnWidths.size() != kSizedColumns) m_columnWidths = QList<int>(kSizedColumns, 0);
    // 利用者が見出しの境目をドラッグしたときだけ保存する（内容に合わせた自動の幅は保存しない）
    connect(header, &TableHeaderView::sectionResizedByUser, this, &AnalysisResultsPanel::saveColumnWidth);

    m_fontDecrease = new QPushButton(this);
    m_fontIncrease = new QPushButton(this);
    m_fontDecrease->setObjectName(QStringLiteral("analysisFontDecrease"));
    m_fontIncrease->setObjectName(QStringLiteral("analysisFontIncrease"));
    new FontSizeControl(m_fontDecrease, m_fontIncrease, &m_fontSize);
    for (auto* button : {m_fontDecrease, m_fontIncrease}) {
        button->setFixedWidth(40);
        button->setMinimumHeight(28);
    }

    m_stopButton = new QPushButton(tr("解析中止"), this);
    m_stopButton->setObjectName(QStringLiteral("analysisStopButton"));
    m_stopButton->setStyleSheet(ButtonStyles::dangerStop());
    m_stopButton->setEnabled(false);
    connect(m_stopButton, &QPushButton::clicked, this, &AnalysisResultsPanel::stopRequested);

    m_engineLabel = new ElideLabel(this);
    m_engineLabel->setObjectName(QStringLiteral("analysisEngineLabel"));
    m_engineLabel->setTextFormat(Qt::PlainText);
    auto* buttonLayout = new QHBoxLayout;
    buttonLayout->addWidget(m_stopButton);
    buttonLayout->addSpacing(8);
    buttonLayout->addWidget(m_engineLabel, 1);
    buttonLayout->addWidget(m_fontDecrease);
    buttonLayout->addWidget(m_fontIncrease);

    m_statusLabel = new QLabel(tr("解析条件を設定して、棋譜解析を開始してください。"), this);
    m_statusLabel->setObjectName(QStringLiteral("analysisStatusLabel"));
    m_statusLabel->setWordWrap(true);
    m_progressBar = new QProgressBar(this);
    m_progressBar->setObjectName(QStringLiteral("analysisProgressBar"));
    m_progressBar->setAccessibleName(tr("解析の進捗"));
    m_progressBar->setRange(0, 1);
    m_progressBar->setValue(0);
    m_progressBar->setFixedWidth(160);
    m_progressBar->setVisible(false);
    auto* progressLayout = new QHBoxLayout;
    progressLayout->addWidget(m_statusLabel, 1);
    progressLayout->addWidget(m_progressBar);

    auto* hint = new QLabel(tr("評価値は先手視点（＋: 先手有利／−: 後手有利）。行を選択すると局面へ移動し、「表示」で読み筋を確認できます。"), this);
    hint->setWordWrap(true);
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(8, 8, 8, 8);
    lay->setSpacing(8);
    lay->addLayout(buttonLayout);
    lay->addLayout(progressLayout);
    lay->addWidget(m_view, 1);
    lay->addWidget(hint);

    // 保存されたフォントサイズを復元
    applyFontSize(m_fontSize.fontSize());
}

void AnalysisResultsPanel::setModel(KifuAnalysisListModel* model)
{
    if (!model || m_view->model() == model) return;
    // 前のモデル（初めは見出しを出すための空のモデル）の通知を切ってから替える
    disconnect(m_view->model(), nullptr, this, nullptr);
    disconnect(m_view->selectionModel(), nullptr, this, nullptr);
    TableModelUtils::setModel(m_view, model);
    connectModelSignals(model);
    setupHeaderConfiguration();
    connect(m_view->selectionModel(), &QItemSelectionModel::currentRowChanged,
            this, &AnalysisResultsPanel::onTableSelectionChanged);
    m_reflowAllRows = true;
    m_reflowTimer->start();
}

void AnalysisResultsPanel::selectRow(int row)
{
    if (m_isAnalyzing) return;
    // 表示の同期を利用者の選択として通知しない。ビューには選択変更を伝えて再描画させる。
    const QSignalBlocker blocker(this);
    const QModelIndex index = m_view->model()->index(row, 0);
    if (!index.isValid()) {
        m_view->clearRowSelection();
        return;
    }
    if (m_view->currentIndex().row() != row) {
        m_view->selectionModel()->setCurrentIndex(index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    }
    m_view->scrollTo(index);
}

void AnalysisResultsPanel::connectModelSignals(KifuAnalysisListModel* model)
{
    if (!model) return;
    connect(model, &QAbstractItemModel::modelReset,   this, &AnalysisResultsPanel::onModelReset);
    connect(model, &QAbstractItemModel::rowsInserted, this, &AnalysisResultsPanel::onRowsInserted);
    connect(model, &QAbstractItemModel::dataChanged,  this, &AnalysisResultsPanel::onDataChanged);
    connect(model, &QAbstractItemModel::layoutChanged,this, &AnalysisResultsPanel::onLayoutChanged);
}

void AnalysisResultsPanel::setupHeaderConfiguration()
{
    m_header->setSectionResizeMode(QHeaderView::Interactive);
    m_header->setStretchLastSection(true);
    m_header->resizeSection(KifuAnalysisListModel::PvColumn, 320);
}

void AnalysisResultsPanel::reflowNow()
{
    // ユーザーが調整した列幅を保ち、自動列だけを内容に合わせて広げる。
    // 読み筋より前の列の最小幅（読み筋の列は残りの幅に伸ばす）
    static constexpr int minWidths[kSizedColumns] = {140, 120, 50, 85, 110, 85, 60};
    QAbstractItemModel* model = m_view->model();
    const int rowCount = model ? model->rowCount() : 0;
    // 列幅は広がる一方なので、足した・変わった行だけ測れば全行を測り直した結果と同じになる。
    // 解析中は1局面ごとに行が増え、全行を測り直すと1行あたりの処理が行数に比例して伸びていた。
    const bool measureAll = m_reflowAllRows || m_reflowFromRow < 0 || rowCount == 0;
    for (int col = 0; col < kSizedColumns; ++col) {
        int width = m_columnWidths.value(col);
        if (width <= 0) {
            const int previousWidth = m_header->sectionSize(col);
            if (measureAll) {
                m_view->resizeColumnToContents(col);
                width = qMax(minWidths[col], m_header->sectionSize(col));
            } else {
                int content = 0;
                for (int row = m_reflowFromRow; row < rowCount; ++row)
                    content = qMax(content, m_view->sizeHintForIndex(model->index(row, col)).width());
                if (m_view->showGrid()) content += 1;
                width = qMax(minWidths[col], qMax(content, m_header->sectionSizeHint(col)));
            }
            if (rowCount > 0) width = qMax(width, previousWidth);
        }
        m_header->resizeSection(col, width);
    }
    m_reflowAllRows = false;
    m_reflowFromRow = -1;
}

void AnalysisResultsPanel::saveColumnWidth(int column, int, int newSize)
{
    if (column < 0 || column >= kSizedColumns) return;
    m_columnWidths[column] = newSize;
    AnalysisSettings::setKifuAnalysisColumnWidths(m_columnWidths);
}

void AnalysisResultsPanel::onModelReset()
{
    m_reflowAllRows = true;
    m_reflowTimer->start();
    updateProgress();
}

void AnalysisResultsPanel::onRowsInserted(const QModelIndex&, int first, int)
{
    m_reflowFromRow = m_reflowFromRow < 0 ? first : qMin(m_reflowFromRow, first);
    m_reflowTimer->start();

    updateProgress();

    // 解析中は最後に確定した行を選んで見せる（解析中の選択の通知は onTableSelectionChanged が無視する）
    if (m_isAnalyzing) {
        const int lastRow = m_view->model()->rowCount() - 1;
        if (lastRow >= 0) {
            const QModelIndex index = m_view->model()->index(lastRow, 0);
            m_view->setCurrentIndex(index);
            m_view->scrollTo(index);
        }
    }
}

void AnalysisResultsPanel::onDataChanged(const QModelIndex& topLeft, const QModelIndex&, const QList<int>&)
{
    if (topLeft.isValid()) m_reflowFromRow = m_reflowFromRow < 0 ? topLeft.row() : qMin(m_reflowFromRow, topLeft.row());
    else m_reflowAllRows = true;
    m_reflowTimer->start();
}
void AnalysisResultsPanel::onLayoutChanged()
{
    m_reflowAllRows = true;
    m_reflowTimer->start();
}

void AnalysisResultsPanel::setStopButtonEnabled(bool enabled)
{
    m_stopButton->setEnabled(enabled);

    // 解析中フラグを更新（enabled=true: 解析中, enabled=false: 解析終了）
    const bool wasAnalyzing = m_isAnalyzing;
    m_isAnalyzing = enabled;
    if (!enabled && wasAnalyzing) {
        m_progressBar->setValue(qMin(m_view->model()->rowCount(), m_totalPositions));
        m_statusLabel->setText(tr("解析中止 · %1 / %2局面を解析済み")
            .arg(m_view->model()->rowCount()).arg(m_totalPositions));
    }
}

void AnalysisResultsPanel::onBoardRequested(int row)
{
    // 解析中は盤面を開かない
    if (m_isAnalyzing) return;
    qCDebug(lcAnalysis).noquote() << "onBoardRequested: row=" << row;
    Q_EMIT rowDoubleClicked(row);
}

void AnalysisResultsPanel::onTableSelectionChanged(const QModelIndex& current, const QModelIndex& /*previous*/)
{
    if (!current.isValid()) return;

    // 解析中の選択（最後に確定した行を選んだとき）は知らせない。棋譜は解析の進み具合で移している
    if (m_isAnalyzing) {
        return;
    }

    int row = current.row();
    qCDebug(lcAnalysis).noquote() << "onTableSelectionChanged: row=" << row;
    Q_EMIT rowSelected(row);
}

void AnalysisResultsPanel::beginAnalysis(int totalPositions, const QString& engineName)
{
    m_totalPositions = qMax(1, totalPositions);
    m_engineLabel->setFullText(engineName);
    m_progressBar->setRange(0, m_totalPositions);
    m_progressBar->setVisible(true);
    setStopButtonEnabled(true);
    updateProgress();
}

void AnalysisResultsPanel::updateProgress()
{
    const int count = m_view->model()->rowCount();
    m_progressBar->setValue(qMin(count, m_totalPositions));
    if (m_isAnalyzing) {
        m_statusLabel->setText(tr("解析中 · %1 / %2局面を解析済み").arg(count).arg(m_totalPositions));
    } else if (count == 0) {
        m_statusLabel->setText(tr("解析条件を設定して、棋譜解析を開始してください。"));
        m_progressBar->setVisible(false);
        m_engineLabel->setFullText(QString());
    }
}

void AnalysisResultsPanel::showAnalysisComplete(int totalMoves)
{
    setStopButtonEnabled(false);
    m_statusLabel->setText(tr("解析完了 · %1局面").arg(totalMoves));
    m_progressBar->setValue(m_progressBar->maximum());
}

void AnalysisResultsPanel::applyFontSize(int size)
{
    m_view->setFontPointSize(size);
    m_reflowAllRows = true;
    m_reflowTimer->start();
}


