/// @file analysisresultspresenter.cpp
/// @brief 解析結果表示プレゼンタクラスの実装

#include "analysisresultspresenter.h"
#include "tablemodelutils.h"
#include "kifumovedelegate.h"
#include "logcategories.h"
#include "buttonstyles.h"
#include <QDockWidget>
#include <QTableView>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTimer>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>
#include "elidelabel.h"
#include "tablestyles.h"
#include "pvboardbuttondelegate.h"
#include "analysissettings.h"
#include "numericrightaligncommadelegate.h"
#include "kifuanalysislistmodel.h"

AnalysisResultsPresenter::AnalysisResultsPresenter(QObject* parent)
    : QObject(parent)
    , m_reflowTimer(new QTimer(this))
{
    m_reflowTimer->setSingleShot(true);
    m_reflowTimer->setInterval(0);
    connect(m_reflowTimer, &QTimer::timeout, this, &AnalysisResultsPresenter::reflowNow);
}

void AnalysisResultsPresenter::setDockWidget(QDockWidget* dock)
{
    m_dock = dock;
}

QWidget* AnalysisResultsPresenter::containerWidget()
{
    if (!m_container) {
        m_container = new QWidget;
    }

    if (!m_uiBuilt) {
        buildUi(nullptr);
        m_uiBuilt = true;
    }

    return m_container;
}

void AnalysisResultsPresenter::showWithModel(KifuAnalysisListModel* model)
{
    if (!m_uiBuilt) {
        buildUi(nullptr);
        m_uiBuilt = true;
    }

    // モデルを設定（既存の接続を切断してから新しいモデルを設定）
    if (m_view) {
        // 古いモデルの接続を切断
        QAbstractItemModel* oldModel = m_view->model();
        if (oldModel) {
            disconnect(oldModel, nullptr, this, nullptr);
        }
        // 古いselectionModelの接続も切断
        if (m_view->selectionModel()) {
            disconnect(m_view->selectionModel(), nullptr, this, nullptr);
        }

        // 新しいモデルを設定
        if (model) {
            TableModelUtils::setModel(m_view, model);
            connectModelSignals(model);
            setupHeaderConfiguration();

            // selectionModelの接続（モデル設定後に行う必要がある）
            connect(m_view->selectionModel(), &QItemSelectionModel::currentRowChanged,
                    this, &AnalysisResultsPresenter::onTableSelectionChanged);
        }
    }

    if (m_dock) {
        m_dock->setVisible(true);
        m_dock->raise();
    }

    m_reflowTimer->start();
}

void AnalysisResultsPresenter::buildUi(KifuAnalysisListModel* /*model*/)
{
    if (!m_container) {
        m_container = new QWidget;
    }

    m_view = new QTableView(m_container);
    m_view->setObjectName(QStringLiteral("analysisResultsTable"));

    // ヘッダー表示用に空のモデルを作成（起動時からヘッダーを表示するため）
    auto* emptyModel = new KifuAnalysisListModel(m_view);
    TableModelUtils::setModel(m_view, emptyModel);

    m_view->setAlternatingRowColors(true);
    m_view->setWordWrap(false);
    m_view->verticalHeader()->setVisible(false);
    m_view->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_view->setSelectionMode(QAbstractItemView::SingleSelection);
    m_view->setTextElideMode(Qt::ElideRight);

    m_view->setStyleSheet(TableStyles::thinking());
    m_view->setShowGrid(false);
    m_view->setMouseTracking(true);
    m_view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_view->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_view->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_view->setAccessibleName(tr("棋譜解析結果"));

    // 盤面列（列6）のクリックで読み筋表示ウィンドウを開く
    connect(m_view, &QTableView::clicked, this, &AnalysisResultsPresenter::onTableClicked);

    // Enterでも盤面列の読み筋を開ける。
    connect(m_view, &QTableView::activated, this, &AnalysisResultsPresenter::onTableClicked);

    // 数値デリゲートを評価値と差の列に設定
    auto* numDelegate = new NumericRightAlignCommaDelegate(m_view);
    m_view->setItemDelegateForColumn(3, numDelegate); // 評価値
    m_view->setItemDelegateForColumn(5, numDelegate); // 差

    // 思考タブと共通の盤面表示ボタン
    auto* boardBtnDelegate = new PvBoardButtonDelegate(m_view);
    m_view->setItemDelegateForColumn(6, boardBtnDelegate); // 盤面

    m_header = m_view->horizontalHeader();
    m_header->setMinimumSectionSize(30);
    m_header->setDefaultSectionSize(30);

    // 空のモデルが設定されているのでヘッダー設定を適用
    setupHeaderConfiguration();

    m_columnWidths = AnalysisSettings::kifuAnalysisColumnWidths();
    if (m_columnWidths.size() != 7) m_columnWidths = QList<int>(7, 0);
    connect(m_header, &QHeaderView::sectionResized, this, &AnalysisResultsPresenter::saveColumnWidth);

    m_fontDecrease = new QPushButton(tr("A-"), m_container);
    m_fontIncrease = new QPushButton(tr("A+"), m_container);
    m_fontDecrease->setObjectName(QStringLiteral("analysisFontDecrease"));
    m_fontIncrease->setObjectName(QStringLiteral("analysisFontIncrease"));
    m_fontDecrease->setToolTip(tr("文字サイズを縮小"));
    m_fontIncrease->setToolTip(tr("文字サイズを拡大"));
    for (auto* button : {m_fontDecrease.data(), m_fontIncrease.data()}) {
        button->setFixedWidth(40);
        button->setMinimumHeight(28);
        button->setStyleSheet(ButtonStyles::panelToolButton());
        button->setAccessibleName(button->toolTip());
    }
    connect(m_fontDecrease, &QPushButton::clicked, this, &AnalysisResultsPresenter::decreaseFontSize);
    connect(m_fontIncrease, &QPushButton::clicked, this, &AnalysisResultsPresenter::increaseFontSize);

    m_stopButton = new QPushButton(tr("解析中止"), m_container);
    m_stopButton->setObjectName(QStringLiteral("analysisStopButton"));
    m_stopButton->setStyleSheet(ButtonStyles::dangerStop());
    m_stopButton->setEnabled(false);
    connect(m_stopButton, &QPushButton::clicked, this, &AnalysisResultsPresenter::stopRequested);

    m_engineLabel = new ElideLabel(m_container);
    m_engineLabel->setObjectName(QStringLiteral("analysisEngineLabel"));
    m_engineLabel->setTextFormat(Qt::PlainText);
    auto* buttonLayout = new QHBoxLayout;
    buttonLayout->addWidget(m_stopButton);
    buttonLayout->addSpacing(8);
    buttonLayout->addWidget(m_engineLabel, 1);
    buttonLayout->addWidget(m_fontDecrease);
    buttonLayout->addWidget(m_fontIncrease);

    m_statusLabel = new QLabel(tr("解析条件を設定して、棋譜解析を開始してください。"), m_container);
    m_statusLabel->setObjectName(QStringLiteral("analysisStatusLabel"));
    m_statusLabel->setWordWrap(true);
    m_progressBar = new QProgressBar(m_container);
    m_progressBar->setObjectName(QStringLiteral("analysisProgressBar"));
    m_progressBar->setAccessibleName(tr("解析の進捗"));
    m_progressBar->setRange(0, 1);
    m_progressBar->setValue(0);
    m_progressBar->setFixedWidth(160);
    m_progressBar->setVisible(false);
    auto* progressLayout = new QHBoxLayout;
    progressLayout->addWidget(m_statusLabel, 1);
    progressLayout->addWidget(m_progressBar);

    auto* hint = new QLabel(tr("評価値は先手視点（＋: 先手有利／−: 後手有利）。行を選択すると局面へ移動し、「表示」で読み筋を確認できます。"), m_container);
    hint->setWordWrap(true);
    auto* lay = new QVBoxLayout(m_container);
    lay->setContentsMargins(8, 8, 8, 8);
    lay->setSpacing(8);
    lay->addLayout(buttonLayout);
    lay->addLayout(progressLayout);
    lay->addWidget(m_view, 1);
    lay->addWidget(hint);

    // 保存されたフォントサイズを復元
    restoreFontSize();
}

void AnalysisResultsPresenter::connectModelSignals(KifuAnalysisListModel* model)
{
    if (!model) return;
    connect(model, &QAbstractItemModel::modelReset,   this, &AnalysisResultsPresenter::onModelReset);
    connect(model, &QAbstractItemModel::rowsInserted, this, &AnalysisResultsPresenter::onRowsInserted);
    connect(model, &QAbstractItemModel::dataChanged,  this, &AnalysisResultsPresenter::onDataChanged);
    connect(model, &QAbstractItemModel::layoutChanged,this, &AnalysisResultsPresenter::onLayoutChanged);
}

void AnalysisResultsPresenter::setupHeaderConfiguration()
{
    if (!m_header) return;

    m_header->setSectionResizeMode(QHeaderView::Interactive);
    m_header->setStretchLastSection(true);
    m_header->resizeSection(7, 320);
}

void AnalysisResultsPresenter::reflowNow()
{
    if (!m_view || !m_header) return;
    m_resizingColumns = true;
    // ユーザーが調整した列幅を保ち、自動列だけを内容に合わせて広げる。
    static const int minWidths[] = {140, 120, 50, 85, 110, 85, 60};
    for (int col = 0; col < 7; ++col) {
        int width = m_columnWidths.value(col);
        if (width <= 0) {
            const int previousWidth = m_header->sectionSize(col);
            m_view->resizeColumnToContents(col);
            width = qMax(minWidths[col], m_header->sectionSize(col));
            if (m_view->model()->rowCount() > 0) width = qMax(width, previousWidth);
        }
        m_header->resizeSection(col, width);
    }
    m_resizingColumns = false;
}

void AnalysisResultsPresenter::saveColumnWidth(int column, int, int newSize)
{
    if (m_resizingColumns || column < 0 || column >= 7) return;
    m_columnWidths[column] = newSize;
    AnalysisSettings::setKifuAnalysisColumnWidths(m_columnWidths);
}

void AnalysisResultsPresenter::onModelReset()
{
    m_reflowTimer->start();
    updateProgress();
}

void AnalysisResultsPresenter::onRowsInserted(const QModelIndex&, int, int)
{
    m_reflowTimer->start();

    updateProgress();

    // 解析中は最後に確定した行を強調する。
    if (m_isAnalyzing && m_view) {
        // 自動選択中はシグナル発行をスキップするためフラグを立てる
        m_autoSelecting = true;

        // 最後に追加された行を選択
        QAbstractItemModel* model = m_view->model();
        if (model) {
            int lastRow = model->rowCount() - 1;
            if (lastRow >= 0) {
                // 一時的に選択モードをSingleSelectionに変更（setCurrentIndexを機能させるため）
                m_view->setSelectionMode(QAbstractItemView::SingleSelection);

                QModelIndex idx = model->index(lastRow, 0);
                m_view->setCurrentIndex(idx);
                m_view->scrollTo(idx);  // 最後の行が見えるようにスクロール

                // 選択モードをNoSelectionに戻す（ユーザークリックを無効化）
                m_view->setSelectionMode(QAbstractItemView::NoSelection);
            }
        }

        m_autoSelecting = false;
    }
}

void AnalysisResultsPresenter::onDataChanged(const QModelIndex&, const QModelIndex&, const QList<int>&) { m_reflowTimer->start(); }
void AnalysisResultsPresenter::onLayoutChanged() { m_reflowTimer->start(); }

void AnalysisResultsPresenter::setStopButtonEnabled(bool enabled)
{
    if (m_stopButton) {
        m_stopButton->setEnabled(enabled);
    }

    // 解析中フラグを更新（enabled=true: 解析中, enabled=false: 解析終了）
    const bool wasAnalyzing = m_isAnalyzing;
    m_isAnalyzing = enabled;
    if (!enabled && wasAnalyzing && m_statusLabel) {
        m_progressBar->setValue(qMin(m_view->model()->rowCount(), m_totalPositions));
        m_statusLabel->setText(tr("解析中止 · %1 / %2局面を解析済み")
            .arg(m_view->model()->rowCount()).arg(m_totalPositions));
    }

    // 解析中はテーブルのクリック操作を無効化
    if (m_view) {
        if (enabled) {
            // 解析中: 選択不可（ただし自動選択は可能）
            m_view->setSelectionMode(QAbstractItemView::NoSelection);
        } else {
            // 解析終了: 選択可能に戻す
            m_view->setSelectionMode(QAbstractItemView::SingleSelection);
        }
    }
}

void AnalysisResultsPresenter::onTableClicked(const QModelIndex& index)
{
    if (!index.isValid()) return;

    // 解析中はクリックを無視
    if (m_isAnalyzing) {
        return;
    }

    // 盤面列（列6）のみ反応
    if (index.column() != 6) {
        return;
    }

    qCDebug(lcAnalysis).noquote() << "onTableClicked: row=" << index.row();
    Q_EMIT rowDoubleClicked(index.row());
}

void AnalysisResultsPresenter::onTableSelectionChanged(const QModelIndex& current, const QModelIndex& /*previous*/)
{
    if (!current.isValid()) return;

    // 自動選択中（解析中の行追加時）はシグナル発行をスキップ
    // （解析中は既にanalysisProgressReportedで盤面更新済みのため）
    if (m_autoSelecting) {
        return;
    }

    // 解析中は選択変更を無視（NoSelectionモードなので通常来ないが念のため）
    if (m_isAnalyzing) {
        return;
    }

    int row = current.row();
    qCDebug(lcAnalysis).noquote() << "onTableSelectionChanged: row=" << row;
    Q_EMIT rowSelected(row);
}

void AnalysisResultsPresenter::beginAnalysis(int totalPositions, const QString& engineName)
{
    m_totalPositions = qMax(1, totalPositions);
    m_engineLabel->setFullText(engineName);
    m_progressBar->setRange(0, m_totalPositions);
    m_progressBar->setVisible(true);
    setStopButtonEnabled(true);
    updateProgress();
}

void AnalysisResultsPresenter::updateProgress()
{
    if (!m_view || !m_statusLabel) return;
    const int count = m_view->model() ? m_view->model()->rowCount() : 0;
    m_progressBar->setValue(qMin(count, m_totalPositions));
    if (m_isAnalyzing) {
        m_statusLabel->setText(tr("解析中 · %1 / %2局面を解析済み").arg(count).arg(m_totalPositions));
    } else if (count == 0) {
        m_statusLabel->setText(tr("解析条件を設定して、棋譜解析を開始してください。"));
        m_progressBar->setVisible(false);
        m_engineLabel->setFullText(QString());
    }
}

void AnalysisResultsPresenter::showAnalysisComplete(int totalMoves)
{
    setStopButtonEnabled(false);
    m_statusLabel->setText(tr("解析完了 · %1局面").arg(totalMoves));
    m_progressBar->setValue(m_progressBar->maximum());
}

void AnalysisResultsPresenter::applyFontSize(int size)
{
    if (!m_view) return;
    QFont font = m_view->font();
    font.setPointSize(qBound(8, size, 24));
    m_view->setFont(font);
    m_view->setStyleSheet(TableStyles::thinking(font.pointSize()));
    m_view->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    m_view->verticalHeader()->setDefaultSectionSize(KifuMoveDelegate::textHeight(m_view->font()) + 10);
    m_fontDecrease->setEnabled(font.pointSize() > 8);
    m_fontIncrease->setEnabled(font.pointSize() < 24);
    m_reflowTimer->start();
}

void AnalysisResultsPresenter::increaseFontSize()
{
    if (!m_view) return;
    applyFontSize(m_view->font().pointSize() + 1);
    AnalysisSettings::setKifuAnalysisFontSize(m_view->font().pointSize());
}

void AnalysisResultsPresenter::decreaseFontSize()
{
    if (!m_view) return;
    applyFontSize(m_view->font().pointSize() - 1);
    AnalysisSettings::setKifuAnalysisFontSize(m_view->font().pointSize());
}

void AnalysisResultsPresenter::restoreFontSize()
{
    int size = AnalysisSettings::kifuAnalysisFontSize();
    if (size < 8 || size > 24) size = qMax(10, m_view->font().pointSize());
    applyFontSize(size);
}

void AnalysisResultsPresenter::saveWindowSize()
{
    // ドックのサイズを保存
    if (m_dock) {
        AnalysisSettings::setKifuAnalysisResultsWindowSize(m_dock->size());
        // 閉じるボタンが押されたらドックを非表示にする
        m_dock->setVisible(false);
    }
}
