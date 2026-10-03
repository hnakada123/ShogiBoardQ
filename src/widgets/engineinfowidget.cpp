/// @file engineinfowidget.cpp
/// @brief エンジン情報表示ウィジェットクラスの実装

#include "engineinfowidget.h"
#include "kifumovedelegate.h"
#include "buttonstyles.h"
#include "tablestyles.h"
#include "logcategories.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QTableWidget>
#include <QHeaderView>
#include <QToolButton>
#include <QFont>
#include <QPalette>
#include <QResizeEvent>
#include <QShowEvent>
#include <QScrollBar>
#include <QSignalBlocker>
#include "usicommlogmodel.h"

EngineInfoWidget::EngineInfoWidget(QWidget* parent, bool showFontButtons, bool showPredictedMove)
    : QWidget(parent)
    , m_table(new QTableWidget(1, COL_COUNT, this))
    , m_showFontButtons(showFontButtons)
    , m_showPredictedMove(showPredictedMove)
{
    setupTable();
    initializeCells();
    buildLayout();
}

void EngineInfoWidget::setupTable()
{
    // ヘッダー設定
    QStringList headers;
    headers << tr("エンジン") << tr("予想手") << tr("探索手")
            << tr("深さ") << tr("ノード数") << tr("探索局面数/秒") << tr("ハッシュ使用率");
    m_table->setHorizontalHeaderLabels(headers);
    auto* moveDelegate = new KifuMoveDelegate(m_table);
    m_table->setItemDelegateForColumn(COL_PRED, moveDelegate);
    m_table->setItemDelegateForColumn(COL_SEARCHED, moveDelegate);
    applyHeaderStyle();

    m_table->setObjectName(QStringLiteral("engineInfoTable"));
    m_table->setShowGrid(false);

    // 行ヘッダーを非表示
    m_table->verticalHeader()->setVisible(false);

    // StretchLastSectionを無効にしてResizeToContentsで自動調整しない
    m_table->horizontalHeader()->setStretchLastSection(false);

    // 全ての列をInteractive（ユーザーがリサイズ可能）に設定
    for (int col = 0; col < COL_COUNT; ++col) {
        m_table->horizontalHeader()->setSectionResizeMode(col, QHeaderView::Interactive);
    }

    // 列幅を設定
    m_table->setColumnWidth(COL_ENGINE_NAME, 150);
    m_table->setColumnWidth(COL_PRED, 100);
    m_table->setColumnWidth(COL_SEARCHED, 100);
    m_table->setColumnWidth(COL_DEPTH, 55);
    m_table->setColumnWidth(COL_NODES, 85);
    m_table->setColumnWidth(COL_NPS, 85);
    m_table->setColumnWidth(COL_HASH, 120);

    // 予想手列を非表示にする場合
    if (!m_showPredictedMove) {
        m_table->setColumnHidden(COL_PRED, true);
    }

    // 列幅変更時のシグナルを接続
    connect(m_table->horizontalHeader(), &QHeaderView::sectionResized,
            this, &EngineInfoWidget::onSectionResized);

    // 編集不可・選択不可
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->setFocusPolicy(Qt::NoFocus);

    // 行の高さを文字サイズに合わせて固定（棋譜欄と同様の余白）
    m_table->verticalHeader()->setDefaultSectionSize(KifuMoveDelegate::textHeight(m_table->font()) + 4);

    // スクロールバー非表示
    m_table->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_table->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
}

void EngineInfoWidget::initializeCells()
{
    for (int col = 0; col < COL_COUNT; ++col) {
        QTableWidgetItem* item = new QTableWidgetItem();
        if (col == COL_DEPTH || col == COL_NODES || col == COL_NPS || col == COL_HASH) {
            item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        } else {
            item->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        }
        m_table->setItem(0, col, item);
    }
}

void EngineInfoWidget::buildLayout()
{
    auto* mainLayout = new QHBoxLayout(this);
    mainLayout->setSizeConstraint(QLayout::SetNoConstraint);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(8);
    if (m_showFontButtons) {
        m_fontControls = new QWidget(this);
        auto* buttonLayout = new QVBoxLayout(m_fontControls);
        buttonLayout->setContentsMargins(0, 0, 0, 0);
        buttonLayout->setSpacing(3);
        m_btnFontDecrease = new QToolButton(m_fontControls);
        m_btnFontDecrease->setText(QStringLiteral("A-"));
        m_btnFontDecrease->setToolTip(tr("フォントサイズを小さくする"));
        m_btnFontIncrease = new QToolButton(m_fontControls);
        m_btnFontIncrease->setText(QStringLiteral("A+"));
        m_btnFontIncrease->setToolTip(tr("フォントサイズを大きくする"));
        for (auto* button : {m_btnFontDecrease, m_btnFontIncrease}) {
            button->setFixedSize(36, 24);
            button->setStyleSheet(ButtonStyles::panelToolButton());
            buttonLayout->addWidget(button);
        }
        connect(m_btnFontDecrease, &QToolButton::clicked,
                this, &EngineInfoWidget::fontSizeDecreaseRequested);
        connect(m_btnFontIncrease, &QToolButton::clicked,
                this, &EngineInfoWidget::fontSizeIncreaseRequested);
        mainLayout->addWidget(m_fontControls, 0, Qt::AlignTop);
    }
    mainLayout->addWidget(m_table);
    mainLayout->addStretch();
    updateTableGeometry();
}

void EngineInfoWidget::setCellValue(int col, const QString& value) {
    QTableWidgetItem* item = m_table->item(0, col);
    if (item) {
        item->setText(value);
        item->setToolTip(value);
        updateTableGeometry();
    }
}

void EngineInfoWidget::setFontSize(int pointSize) {
    if (!m_table) return;

    m_fontSize = pointSize;

    QFont font = m_table->font();
    font.setPointSize(pointSize);
    m_table->setFont(font);

    // ヘッダーのフォントも変更
    m_table->horizontalHeader()->setFont(font);
    applyHeaderStyle();

    // 行の高さを調整（棋譜欄と同様の余白）
    int rowHeight = KifuMoveDelegate::textHeight(font) + 4;
    m_table->verticalHeader()->setDefaultSectionSize(rowHeight);

    updateTableGeometry();
}

void EngineInfoWidget::setModel(UsiCommLogModel* m) {
    if (m_model == m) return;
    if (m_model) disconnect(m_model, nullptr, this, nullptr);
    m_model = m;
    if (!m_model) {
        // モデルが無いときは見た目を空に
        for (int col = 0; col < COL_COUNT; ++col) {
            setCellValue(col, QString());
        }
        return;
    }

    connect(m_model, &UsiCommLogModel::engineNameChanged,     this, &EngineInfoWidget::onNameChanged);
    connect(m_model, &UsiCommLogModel::predictiveMoveChanged, this, &EngineInfoWidget::onPredChanged);
    connect(m_model, &UsiCommLogModel::searchedMoveChanged,   this, &EngineInfoWidget::onSearchedChanged);
    connect(m_model, &UsiCommLogModel::searchDepthChanged,    this, &EngineInfoWidget::onDepthChanged);
    connect(m_model, &UsiCommLogModel::nodeCountChanged,      this, &EngineInfoWidget::onNodesChanged);
    connect(m_model, &UsiCommLogModel::nodesPerSecondChanged, this, &EngineInfoWidget::onNpsChanged);
    connect(m_model, &UsiCommLogModel::hashUsageChanged,      this, &EngineInfoWidget::onHashChanged);
    onNameChanged(); onPredChanged(); onSearchedChanged(); onDepthChanged(); onNodesChanged(); onNpsChanged(); onHashChanged();
}

void EngineInfoWidget::onPredChanged()    { setCellValue(COL_PRED, m_model->predictiveMove()); }
void EngineInfoWidget::onSearchedChanged(){ setCellValue(COL_SEARCHED, m_model->searchedMove()); }
void EngineInfoWidget::onDepthChanged()   { setCellValue(COL_DEPTH, m_model->searchDepth()); }
void EngineInfoWidget::onNodesChanged()   { setCellValue(COL_NODES, m_model->nodeCount()); }
void EngineInfoWidget::onNpsChanged()     { setCellValue(COL_NPS, m_model->nodesPerSecond()); }
void EngineInfoWidget::onHashChanged()    { setCellValue(COL_HASH, m_model->hashUsage()); }

void EngineInfoWidget::setDisplayNameFallback(const QString& name) {
    qCDebug(lcUi).noquote() << "[EngineInfoWidget::setDisplayNameFallback] name=" << name
                       << "this=" << this
                       << "m_widgetIndex=" << m_widgetIndex
                       << "m_model=" << m_model
                       << "modelEngineName=" << (m_model ? m_model->engineName() : "<no model>");
    m_fallbackName = name;
    // モデル未設定 or まだ空ならフォールバックを表示
    if (!m_model || m_model->engineName().isEmpty()) {
        qCDebug(lcUi).noquote() << "[EngineInfoWidget::setDisplayNameFallback] Setting cell value to:" << m_fallbackName;
        setCellValue(COL_ENGINE_NAME, m_fallbackName);
    } else {
        qCDebug(lcUi).noquote() << "[EngineInfoWidget::setDisplayNameFallback] NOT setting cell (model has name):" << m_model->engineName();
    }
}

void EngineInfoWidget::onNameChanged() {
    const QString n = m_model ? m_model->engineName() : QString();
    setCellValue(COL_ENGINE_NAME, n.isEmpty() ? m_fallbackName : n);
}

// 列幅の取得
QList<int> EngineInfoWidget::columnWidths() const
{
    QList<int> widths;
    if (!m_table) return widths;
    
    for (int col = 0; col < COL_COUNT; ++col) {
        widths.append(m_table->columnWidth(col));
    }
    return widths;
}

void EngineInfoWidget::applyHeaderStyle()
{
    if (!m_table) return;
    m_table->setStyleSheet(TableStyles::header(m_fontSize) + QStringLiteral(
        "QTableView { background: #ffffff; color: #303841; border: 1px solid #d8dee5; }"));
}

// 列幅の設定
void EngineInfoWidget::setColumnWidths(const QList<int>& widths)
{
    if (!m_table || widths.size() != COL_COUNT) return;
    
    // シグナルを一時的にブロック（設定中にシグナルが発火しないように）
    m_table->horizontalHeader()->blockSignals(true);
    
    for (int col = 0; col < COL_COUNT; ++col) {
        if (widths.at(col) > 0) {
            m_table->setColumnWidth(col, widths.at(col));
        }
    }
    
    m_table->horizontalHeader()->blockSignals(false);
    
    // 設定ファイルから読み込まれたことを記録
    m_columnWidthsLoaded = true;
    updateTableGeometry();
}

// 列幅変更時のスロット
void EngineInfoWidget::onSectionResized(int logicalIndex, int oldSize, int newSize)
{
    Q_UNUSED(logicalIndex)
    Q_UNUSED(oldSize)
    Q_UNUSED(newSize)
    
    m_columnWidthsLoaded = true;
    updateTableGeometry();
    emit columnWidthChanged();
}

void EngineInfoWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    updateTableGeometry();
}

void EngineInfoWidget::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    updateTableGeometry();
}

void EngineInfoWidget::updateTableGeometry()
{
    const QSignalBlocker blocker(m_table->horizontalHeader());
    if (!m_columnWidthsLoaded) {
        for (int col = 0; col < COL_COUNT; ++col) {
            const auto* item = m_table->item(0, col);
            const auto* heading = m_table->horizontalHeaderItem(col);
            const QFontMetrics headingMetrics(m_table->font());
            const bool japaneseMove = (col == COL_PRED || col == COL_SEARCHED)
                && KifuPresentation::options().notation == KifuPresentation::Notation::Japanese;
            const QFontMetrics metrics(japaneseMove ? ApplicationFonts::japaneseFont(m_table->font()) : m_table->font());
            int contentWidth = qMax(metrics.horizontalAdvance(item ? item->text() : QString()),
                                    headingMetrics.horizontalAdvance(heading ? heading->text() : QString())) + 20;
            if (col == COL_ENGINE_NAME) contentWidth = qBound(120, contentWidth, 260);
            m_table->setColumnWidth(col, contentWidth);
        }
    }
    const int contentWidth = m_table->horizontalHeader()->length();
    const int controlsWidth = m_fontControls ? m_fontControls->sizeHint().width() + 8 : 0;
    const int availableWidth = qMax(1, width() - controlsWidth);
    const int frame = 2 * m_table->frameWidth();
    const int tableWidth = qMin(contentWidth + frame, availableWidth);
    m_table->setFixedWidth(tableWidth);
    const int scrollHeight = contentWidth + frame > tableWidth
        ? m_table->horizontalScrollBar()->sizeHint().height() : 0;
    const int tableHeight = m_table->horizontalHeader()->sizeHint().height()
        + m_table->verticalHeader()->defaultSectionSize() + frame + scrollHeight;
    m_table->setFixedHeight(tableHeight);
    setFixedHeight(qMax(tableHeight, m_fontControls ? m_fontControls->sizeHint().height() : 0));
}
