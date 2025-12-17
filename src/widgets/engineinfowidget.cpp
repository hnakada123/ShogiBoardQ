#include "engineinfowidget.h"
#include <QHBoxLayout>
#include <QTableWidget>
#include <QHeaderView>
#include <QToolButton>
#include <QFont>
#include "usicommlogmodel.h"

EngineInfoWidget::EngineInfoWidget(QWidget* parent, bool showFontButtons)
    : QWidget(parent), m_showFontButtons(showFontButtons) 
{
    m_table = new QTableWidget(1, COL_COUNT, this);
    
    // ヘッダー設定
    QStringList headers;
    headers << tr("エンジン名") << tr("予想手") << tr("探索手") 
            << tr("深さ") << tr("ノード数") << tr("探索局面数") << tr("ハッシュ使用率");
    m_table->setHorizontalHeaderLabels(headers);
    
    // 行ヘッダーを非表示
    m_table->verticalHeader()->setVisible(false);
    
    // 列幅設定
    m_table->setColumnWidth(COL_ENGINE_NAME, 200);
    m_table->setColumnWidth(COL_PRED, 120);
    m_table->setColumnWidth(COL_SEARCHED, 120);
    m_table->setColumnWidth(COL_DEPTH, 50);
    m_table->setColumnWidth(COL_NODES, 100);
    m_table->setColumnWidth(COL_NPS, 100);
    m_table->setColumnWidth(COL_HASH, 60);
    
    // 最後の列を伸縮
    m_table->horizontalHeader()->setStretchLastSection(true);
    
    // 編集不可
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    
    // 選択不可
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->setFocusPolicy(Qt::NoFocus);
    
    // 行の高さを固定
    m_table->verticalHeader()->setDefaultSectionSize(22);
    
    // スクロールバー非表示
    m_table->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_table->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    
    // 初期セルを作成
    for (int col = 0; col < COL_COUNT; ++col) {
        QTableWidgetItem* item = new QTableWidgetItem();
        if (col == COL_DEPTH || col == COL_NODES || col == COL_NPS || col == COL_HASH) {
            item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        } else {
            item->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        }
        m_table->setItem(0, col, item);
    }
    
    // レイアウト
    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);
    
    // ★ フォントサイズボタンを追加（showFontButtons=trueの場合のみ）
    if (m_showFontButtons) {
        m_btnFontDecrease = new QToolButton(this);
        m_btnFontDecrease->setText(QStringLiteral("A-"));
        m_btnFontDecrease->setToolTip(tr("フォントサイズを小さくする"));
        m_btnFontDecrease->setFixedSize(28, 24);
        connect(m_btnFontDecrease, &QToolButton::clicked,
                this, &EngineInfoWidget::fontSizeDecreaseRequested);
        
        m_btnFontIncrease = new QToolButton(this);
        m_btnFontIncrease->setText(QStringLiteral("A+"));
        m_btnFontIncrease->setToolTip(tr("フォントサイズを大きくする"));
        m_btnFontIncrease->setFixedSize(28, 24);
        connect(m_btnFontIncrease, &QToolButton::clicked,
                this, &EngineInfoWidget::fontSizeIncreaseRequested);
        
        layout->addWidget(m_btnFontDecrease);
        layout->addWidget(m_btnFontIncrease);
    }
    
    layout->addWidget(m_table);
    
    // 高さ固定（ヘッダー + 1行）
    int headerHeight = m_table->horizontalHeader()->height();
    int rowHeight = m_table->verticalHeader()->defaultSectionSize();
    setFixedHeight(headerHeight + rowHeight + 2);  // +2 for border
}

void EngineInfoWidget::setCellValue(int col, const QString& value) {
    QTableWidgetItem* item = m_table->item(0, col);
    if (item) {
        item->setText(value);
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
    
    // 行の高さを調整
    QFontMetrics fm(font);
    int rowHeight = fm.height() + 6;
    m_table->verticalHeader()->setDefaultSectionSize(rowHeight);
    
    // ウィジェット全体の高さを再計算
    int headerHeight = m_table->horizontalHeader()->height();
    setFixedHeight(headerHeight + rowHeight + 2);
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
    m_fallbackName = name;
    // モデル未設定 or まだ空ならフォールバックを表示
    if (!m_model || m_model->engineName().isEmpty()) {
        setCellValue(COL_ENGINE_NAME, m_fallbackName);
    }
}

void EngineInfoWidget::onNameChanged() {
    const QString n = m_model ? m_model->engineName() : QString();
    setCellValue(COL_ENGINE_NAME, n.isEmpty() ? m_fallbackName : n);
}

