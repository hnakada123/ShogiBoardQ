#include "engineinfowidget.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QTableWidget>
#include <QHeaderView>
#include <QToolButton>
#include <QFont>
#include <QResizeEvent>
#include <QShowEvent>
#include "usicommlogmodel.h"

EngineInfoWidget::EngineInfoWidget(QWidget* parent, bool showFontButtons)
    : QWidget(parent), m_showFontButtons(showFontButtons) 
{
    m_table = new QTableWidget(1, COL_COUNT, this);
    
    // ヘッダー設定
    QStringList headers;
    headers << tr("エンジン") << tr("予想手") << tr("探索手") 
            << tr("深さ") << tr("ノード数") << tr("探索局面数") << tr("ハッシュ使用率");
    m_table->setHorizontalHeaderLabels(headers);
    
    // 行ヘッダーを非表示
    m_table->verticalHeader()->setVisible(false);
    
    // ★ 列幅設定（USIプロトコルのパラメータ内容に基づいた適切な幅）
    // エンジン名: 可変長（長い名前もある）→ 残りスペースで伸縮
    // 予想手: 例 "△8六歩(85)" → 100ピクセル程度
    // 探索手: 例 "△同飛(82)" → 100ピクセル程度
    // 深さ: 例 "25/35" → 55ピクセル程度
    // ノード数: 例 "47,864,025" → 85ピクセル程度
    // 探索局面数(NPS): 例 "8,184,682" → 85ピクセル程度
    // ハッシュ使用率: ヘッダー文字「ハッシュ使用率」が見えるように120ピクセル
    
    // ★ StretchLastSectionを無効にしてResizeToContentsで自動調整しない
    m_table->horizontalHeader()->setStretchLastSection(false);
    
    // ★ 全ての列をInteractive（ユーザーがリサイズ可能）に設定
    for (int col = 0; col < COL_COUNT; ++col) {
        m_table->horizontalHeader()->setSectionResizeMode(col, QHeaderView::Interactive);
    }
    
    // ★ 列幅を設定（setStretchLastSectionの後に設定）
    m_table->setColumnWidth(COL_ENGINE_NAME, 150);
    m_table->setColumnWidth(COL_PRED, 100);
    m_table->setColumnWidth(COL_SEARCHED, 100);
    m_table->setColumnWidth(COL_DEPTH, 55);
    m_table->setColumnWidth(COL_NODES, 85);
    m_table->setColumnWidth(COL_NPS, 85);
    m_table->setColumnWidth(COL_HASH, 120);
    
    // ★ 列幅変更時のシグナルを接続
    connect(m_table->horizontalHeader(), &QHeaderView::sectionResized,
            this, &EngineInfoWidget::onSectionResized);
    
    // 編集不可
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    
    // 選択不可
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->setFocusPolicy(Qt::NoFocus);
    
    // 行の高さを文字サイズに合わせて固定（棋譜欄と同様の余白）
    QFontMetrics fm(m_table->font());
    m_table->verticalHeader()->setDefaultSectionSize(fm.height() + 4);
    
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
    
    // レイアウト（縦配置：ボタン行 → テーブル）
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // ★ フォントサイズボタンを追加（showFontButtons=trueの場合のみ）
    // ヘッダーの上に表示するため、テーブルの上に配置
    m_buttonRowHeight = 0;
    if (m_showFontButtons) {
        QWidget* buttonRow = new QWidget(this);
        QHBoxLayout* buttonLayout = new QHBoxLayout(buttonRow);
        buttonLayout->setContentsMargins(0, 0, 0, 2);
        buttonLayout->setSpacing(4);

        m_btnFontDecrease = new QToolButton(buttonRow);
        m_btnFontDecrease->setText(QStringLiteral("A-"));
        m_btnFontDecrease->setToolTip(tr("フォントサイズを小さくする"));
        m_btnFontDecrease->setFixedSize(28, 20);
        connect(m_btnFontDecrease, &QToolButton::clicked,
                this, &EngineInfoWidget::fontSizeDecreaseRequested);

        m_btnFontIncrease = new QToolButton(buttonRow);
        m_btnFontIncrease->setText(QStringLiteral("A+"));
        m_btnFontIncrease->setToolTip(tr("フォントサイズを大きくする"));
        m_btnFontIncrease->setFixedSize(28, 20);
        connect(m_btnFontIncrease, &QToolButton::clicked,
                this, &EngineInfoWidget::fontSizeIncreaseRequested);

        buttonLayout->addWidget(m_btnFontDecrease);
        buttonLayout->addWidget(m_btnFontIncrease);
        buttonLayout->addStretch();  // 右側に余白を追加

        buttonRow->setLayout(buttonLayout);
        mainLayout->addWidget(buttonRow);
        m_buttonRowHeight = 22;  // ボタン行の高さ
    }

    mainLayout->addWidget(m_table);

    // 高さ固定（ボタン行 + ヘッダー + 1行）
    int headerHeight = m_table->horizontalHeader()->height();
    int rowHeight = m_table->verticalHeader()->defaultSectionSize();
    setFixedHeight(m_buttonRowHeight + headerHeight + rowHeight + 2);  // +2 for border
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

    // 行の高さを調整（棋譜欄と同様の余白）
    QFontMetrics fm(font);
    int rowHeight = fm.height() + 4;
    m_table->verticalHeader()->setDefaultSectionSize(rowHeight);

    // ウィジェット全体の高さを再計算（ボタン行 + ヘッダー + 1行）
    int headerHeight = m_table->horizontalHeader()->height();
    setFixedHeight(m_buttonRowHeight + headerHeight + rowHeight + 2);
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

// ★ 追加: 列幅の取得
QList<int> EngineInfoWidget::columnWidths() const
{
    QList<int> widths;
    if (!m_table) return widths;
    
    for (int col = 0; col < COL_COUNT; ++col) {
        widths.append(m_table->columnWidth(col));
    }
    return widths;
}

// ★ 追加: 列幅の設定
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
    
    // ★ 設定ファイルから読み込まれたことを記録
    m_columnWidthsLoaded = true;
}

// ★ 追加: 列幅変更時のスロット
void EngineInfoWidget::onSectionResized(int logicalIndex, int oldSize, int newSize)
{
    Q_UNUSED(logicalIndex)
    Q_UNUSED(oldSize)
    Q_UNUSED(newSize)
    
    // ユーザーが列幅を手動で変更した場合は、エンジン名列の自動調整は行わない
    // （各列が独立してリサイズされるようにする）
    
    // 列幅が変更されたことを通知
    emit columnWidthChanged();
}

// ★ 追加: リサイズイベント
void EngineInfoWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    // 設定ファイルから列幅が読み込まれていない場合のみ、エンジン名列を調整
    // （読み込み済みの場合は、ユーザーの設定を維持する）
    if (!m_columnWidthsLoaded) {
        adjustEngineNameColumn();
    }
}

// ★ 追加: 表示イベント
void EngineInfoWidget::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    // 設定ファイルから列幅が読み込まれていない場合のみ、エンジン名列を調整
    if (!m_columnWidthsLoaded) {
        adjustEngineNameColumn();
    }
}

// ★ 追加: エンジン名列の幅を残りスペースに合わせて調整
void EngineInfoWidget::adjustEngineNameColumn()
{
    if (!m_table) return;
    
    // 他の列の合計幅を計算
    int otherColumnsWidth = 0;
    for (int col = COL_PRED; col < COL_COUNT; ++col) {
        otherColumnsWidth += m_table->columnWidth(col);
    }
    
    // テーブルの利用可能な幅を取得
    int availableWidth = m_table->viewport()->width();
    
    // エンジン名列の幅を計算（最小幅100を確保）
    int engineNameWidth = availableWidth - otherColumnsWidth;
    if (engineNameWidth < 100) {
        engineNameWidth = 100;
    }
    
    // シグナルをブロックして設定
    m_table->horizontalHeader()->blockSignals(true);
    m_table->setColumnWidth(COL_ENGINE_NAME, engineNameWidth);
    m_table->horizontalHeader()->blockSignals(false);
}

