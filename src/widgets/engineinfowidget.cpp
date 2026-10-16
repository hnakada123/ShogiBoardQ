/// @file engineinfowidget.cpp
/// @brief エンジン情報表示ウィジェットクラスの実装

#include "engineinfowidget.h"
#include "tableheaderview.h"
#include "itemtextdelegate.h"
#include "kifumovedelegate.h"
#include "tablestyles.h"
#include "logcategories.h"
#include <QHBoxLayout>
#include <QLocale>
#include <QTableWidget>
#include <QHeaderView>
#include <QFont>
#include <QPalette>
#include <QResizeEvent>
#include <QShowEvent>
#include <QScrollBar>
#include "usicommlogmodel.h"

EngineInfoWidget::EngineInfoWidget(AnalysisSettings::ThinkingTable table, QWidget* parent,
                                   bool showPredictedMove)
    : QWidget(parent)
    , m_table(new QTableWidget(1, COL_COUNT, this))
    , m_thinkingTable(table)
    , m_showPredictedMove(showPredictedMove)
{
    setupTable();
    initializeCells();
    buildLayout();
    // 保存した列幅があれば使う
    setColumnWidths(AnalysisSettings::engineInfoColumnWidths(m_thinkingTable));
}

void EngineInfoWidget::setupTable()
{
    // 表の幅が変わるたびの見出しの文字の測り直しなどを省く見出しにする（QTableView の既定と同じ設定）
    auto* header = TableHeaderView::install(m_table);

    // ヘッダー設定
    QStringList headers;
    headers << tr("エンジン") << tr("予想手") << tr("探索手")
            << tr("深さ") << tr("ノード数") << tr("探索局面数/秒") << tr("ハッシュ使用率");
    m_table->setHorizontalHeaderLabels(headers);
    // 値は思考情報が届くたびに描き直すので、文字は配置を覚えて描く（見た目は Qt と同じ）
    m_table->setItemDelegate(new ItemTextDelegate(m_table));
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

    // 利用者が列幅を変えたら保存する
    connect(header, &TableHeaderView::sectionResizedByUser, this, &EngineInfoWidget::onSectionResizedByUser);

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
    mainLayout->addWidget(m_table);
    mainLayout->addStretch();
    updateTableGeometry();
}

void EngineInfoWidget::setCellValue(int col, const QString& value) {
    QTableWidgetItem* item = m_table->item(0, col);
    // 思考情報は1秒に何度も届く。値が同じなら表も幅も変えない
    if (item && item->text() != value) {
        item->setText(value);
        item->setToolTip(value);
        scheduleTableGeometry();
    }
}

void EngineInfoWidget::scheduleTableGeometry()
{
    if (m_tableGeometryPending) return;
    m_tableGeometryPending = true;
    // 描画（優先度の低い更新要求）より先に処理されるので、古い幅で描かれることはない
    QMetaObject::invokeMethod(this, &EngineInfoWidget::updateTableGeometry, Qt::QueuedConnection);
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

    connect(m_model, &UsiCommLogModel::engineNameChanged, this, &EngineInfoWidget::onNameChanged);
    connect(m_model, &UsiCommLogModel::statusChanged, this, &EngineInfoWidget::onStatusChanged);
    onNameChanged();
    onStatusChanged();
}

void EngineInfoWidget::onStatusChanged()
{
    // ノード数・NPS は英語の3桁区切り、深さは「深さ/選択的探索の深さ」、ハッシュは百分率
    static const QLocale numbers(QLocale::English);
    const EngineSearchStatus& status = m_model->status();
    QString depth;
    if (status.depth >= 0) {
        depth = QString::number(status.depth);
        if (status.seldepth >= 0) depth += QLatin1Char('/') + QString::number(status.seldepth);
    }
    setCellValue(COL_PRED, m_model->predictedMove());
    setCellValue(COL_SEARCHED, status.searchedMove);
    setCellValue(COL_DEPTH, depth);
    setCellValue(COL_NODES, status.nodes >= 0 ? numbers.toString(status.nodes) : QString());
    setCellValue(COL_NPS, status.nps >= 0 ? numbers.toString(status.nps) : QString());
    setCellValue(COL_HASH, status.hashfull >= 0 ? QString::number(status.hashfull / 10) + QLatin1Char('%') : QString());
}

void EngineInfoWidget::setDisplayNameFallback(const QString& name) {
    qCDebug(lcUi).noquote() << "[EngineInfoWidget::setDisplayNameFallback] name=" << name
                       << "this=" << this
                       << "table=" << static_cast<int>(m_thinkingTable)
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
    for (int col = 0; col < COL_COUNT; ++col) {
        if (widths.at(col) > 0) m_table->setColumnWidth(col, widths.at(col));
    }
    m_fixedColumnWidths = true;
    updateTableGeometry();
}

void EngineInfoWidget::onSectionResizedByUser()
{
    m_fixedColumnWidths = true;
    AnalysisSettings::setEngineInfoColumnWidths(m_thinkingTable, columnWidths());
    updateTableGeometry();
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

bool EngineInfoWidget::HeaderHeightKey::sameAs(const HeaderHeightKey& other) const
{
    return font == other.font && styleSheet == other.styleSheet && headings == other.headings
        && hiddenColumns == other.hiddenColumns;
}

// 見出しの高さ。QHeaderView は列幅が変わるたびに全見出しの大きさを測り直すので、
// 高さが変わりうる条件（書体・スタイルシート・見出し・表示する列）が同じなら使い回す。
// 思考情報が届くたびに呼ばれるので、書体は文字列の形（QFont::key）にせず、そのまま比べる
int EngineInfoWidget::headerHeight()
{
    QHeaderView* header = m_table->horizontalHeader();
    HeaderHeightKey key{m_table->font(), m_table->styleSheet(), {}, 0};
    key.headings.reserve(COL_COUNT);
    for (int col = 0; col < COL_COUNT; ++col) {
        const auto* heading = m_table->horizontalHeaderItem(col);
        key.headings.append(heading ? heading->text() : QString());
        if (header->isSectionHidden(col)) key.hiddenColumns |= 1u << col;
    }
    if (!m_headerHeightMeasured || !key.sameAs(m_headerHeightKey)) {
        m_headerHeightKey = key;
        m_headerHeightMeasured = true;
        m_headerHeight = header->sizeHint().height();
    }
    return m_headerHeight;
}

void EngineInfoWidget::updateTableGeometry()
{
    m_tableGeometryPending = false;
    if (!m_fixedColumnWidths) {
        const QFont headingFont = m_table->font();
        const bool japanese = KifuPresentation::options().notation == KifuPresentation::Notation::Japanese;
        const QFont moveFont = japanese ? ApplicationFonts::japaneseFont(headingFont) : headingFont;
        if (headingFont != m_measuredHeadingFont || moveFont != m_measuredMoveFont
            || m_cellWidths.size() != COL_COUNT) {
            m_measuredHeadingFont = headingFont;
            m_measuredMoveFont = moveFont;
            m_cellWidths = QList<MeasuredText>(COL_COUNT);
            m_headingWidths = QList<MeasuredText>(COL_COUNT);
        }
        const QFontMetrics headingMetrics(headingFont);
        const QFontMetrics moveMetrics(moveFont);
        const auto measured = [](MeasuredText& cache, const QString& text, const QFontMetrics& metrics) {
            if (cache.width < 0 || cache.text != text) {
                cache.text = text;
                cache.width = metrics.horizontalAdvance(text);
            }
            return cache.width;
        };
        for (int col = 0; col < COL_COUNT; ++col) {
            const auto* item = m_table->item(0, col);
            const auto* heading = m_table->horizontalHeaderItem(col);
            const QFontMetrics& metrics = (col == COL_PRED || col == COL_SEARCHED) ? moveMetrics : headingMetrics;
            int contentWidth = qMax(measured(m_cellWidths[col], item ? item->text() : QString(), metrics),
                                    measured(m_headingWidths[col], heading ? heading->text() : QString(),
                                             headingMetrics)) + 20;
            if (col == COL_ENGINE_NAME) contentWidth = qBound(120, contentWidth, 260);
            m_table->setColumnWidth(col, contentWidth);
        }
    }
    const int contentWidth = m_table->horizontalHeader()->length();
    const int availableWidth = qMax(1, width());
    const int frame = 2 * m_table->frameWidth();
    const int tableWidth = qMin(contentWidth + frame, availableWidth);
    m_table->setFixedWidth(tableWidth);
    const int scrollHeight = contentWidth + frame > tableWidth
        ? m_table->horizontalScrollBar()->sizeHint().height() : 0;
    const int tableHeight = headerHeight()
        + m_table->verticalHeader()->defaultSectionSize() + frame + scrollHeight;
    m_table->setFixedHeight(tableHeight);
    setFixedHeight(tableHeight);
}
