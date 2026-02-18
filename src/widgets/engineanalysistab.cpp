/// @file engineanalysistab.cpp
/// @brief エンジン解析タブクラスの実装

#include "engineanalysistab.h"
#include "commenteditorpanel.h"
#include "considerationtabmanager.h"
#include "logviewfontmanager.h"
#include "buttonstyles.h"

#include <QTabWidget>
#include <QTableView>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPlainTextEdit>
#include <QGraphicsView>
#include <QHeaderView>
#include <QFont>
#include <QToolButton>
#include <QPushButton>
#include <QLabel>
#include <QTimer>       // 列幅設定の遅延用
#include <QLineEdit>    // CSAコマンド入力用
#include <QComboBox>    // USIコマンド送信先選択用
#include <QTextCursor>      // ログ色付け用
#include <QTextCharFormat>  // ログ色付け用
#include <QItemSelectionModel>
#include <QSizePolicy>

#include "settingsservice.h"  // フォントサイズ保存用
#include "loggingcategory.h"
#include "numericrightaligncommadelegate.h"
#include "engineinfowidget.h"
#include "shogienginethinkingmodel.h"
#include "usicommlogmodel.h"

namespace {
// ツールバーの幅制約を緩和する（ドックウィンドウで縮小可能にする）
// ツールバー自体のサイズポリシーのみを変更し、子ウィジェットはそのまま維持
void relaxToolbarWidth(QWidget* toolbar)
{
    if (!toolbar) return;
    toolbar->setMinimumWidth(0);
    QSizePolicy pol = toolbar->sizePolicy();
    pol.setHorizontalPolicy(QSizePolicy::Ignored);
    toolbar->setSizePolicy(pol);
}
} // namespace

// ===================== コンストラクタ/UI =====================

EngineAnalysisTab::EngineAnalysisTab(QWidget* parent)
    : QWidget(parent)
{
}

EngineAnalysisTab::~EngineAnalysisTab()
{
}

void EngineAnalysisTab::buildUi()
{
    // --- QTabWidget 準備 ---
    if (!m_tab) {
        m_tab = new QTabWidget(this);  // 親をthisに指定（メモリリーク防止）
        m_tab->setObjectName(QStringLiteral("analysisTabs"));
        m_tab->setMinimumWidth(0);
        m_tab->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);

        // タブバーを青色、タブ内をクリーム色にスタイル設定
        m_tab->setStyleSheet(QStringLiteral(
            "QTabWidget::pane {"
            "  background-color: #fefcf6;"
            "  border: 1px solid #d4c9a8;"
            "  border-top: none;"
            "}"
            "QTabBar::tab {"
            "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
            "    stop:0 #40acff, stop:1 #209cee);"
            "  color: white;"
            "  font-weight: normal;"
            "  padding: 4px 12px;"
            "  border: 1px solid #209cee;"
            "  border-bottom: none;"
            "  border-top-left-radius: 4px;"
            "  border-top-right-radius: 4px;"
            "  margin-right: 2px;"
            "}"
            "QTabBar::tab:selected {"
            "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
            "    stop:0 #50bcff, stop:1 #30acfe);"
            "}"
            "QTabBar::tab:hover:!selected {"
            "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
            "    stop:0 #50bcff, stop:1 #30acfe);"
            "}"
        ));
    } else {
        m_tab->clear();
    }

    // --- 思考タブ ---
    QWidget* page = new QWidget(m_tab);
    auto* v = new QVBoxLayout(page);
    v->setContentsMargins(4,4,4,4);
    v->setSpacing(4);

    // 最初のEngineInfoWidgetにフォントサイズボタンを表示
    m_info1 = new EngineInfoWidget(page, true);  // showFontButtons=true
    m_info1->setWidgetIndex(0);  // インデックス設定
    m_view1 = new QTableView(page);
    m_info2 = new EngineInfoWidget(page, false); // showFontButtons=false
    m_info2->setWidgetIndex(1);  // インデックス設定
    m_view2 = new QTableView(page);
    
    // フォントサイズ変更シグナルを接続
    connect(m_info1, &EngineInfoWidget::fontSizeIncreaseRequested,
            this, &EngineAnalysisTab::onThinkingFontIncrease);
    connect(m_info1, &EngineInfoWidget::fontSizeDecreaseRequested,
            this, &EngineAnalysisTab::onThinkingFontDecrease);
    
    // 列幅変更シグナルを接続
    connect(m_info1, &EngineInfoWidget::columnWidthChanged,
            this, &EngineAnalysisTab::onEngineInfoColumnWidthChanged);
    connect(m_info2, &EngineInfoWidget::columnWidthChanged,
            this, &EngineAnalysisTab::onEngineInfoColumnWidthChanged);
    
    // 設定ファイルから列幅を読み込んで適用
    QList<int> widths0 = SettingsService::engineInfoColumnWidths(0);
    if (!widths0.isEmpty() && widths0.size() == m_info1->columnCount()) {
        m_info1->setColumnWidths(widths0);
    }
    QList<int> widths1 = SettingsService::engineInfoColumnWidths(1);
    if (!widths1.isEmpty() && widths1.size() == m_info2->columnCount()) {
        m_info2->setColumnWidths(widths1);
    }

    // ヘッダの基本設定のみ（列幅はsetModels後に適用）
    setupThinkingViewHeader(m_view1);
    setupThinkingViewHeader(m_view2);
    
    // 列幅変更シグナルを接続
    connect(m_view1->horizontalHeader(), &QHeaderView::sectionResized,
            this, &EngineAnalysisTab::onView1SectionResized);
    connect(m_view2->horizontalHeader(), &QHeaderView::sectionResized,
            this, &EngineAnalysisTab::onView2SectionResized);

    // 読み筋テーブルのクリックシグナルを接続
    connect(m_view1, &QTableView::clicked,
            this, &EngineAnalysisTab::onView1Clicked);
    connect(m_view2, &QTableView::clicked,
            this, &EngineAnalysisTab::onView2Clicked);

    v->addWidget(m_info1);
    v->addWidget(m_view1, 1);
    v->addWidget(m_info2);
    v->addWidget(m_view2, 1);

    m_tab->addTab(page, tr("思考"));

    // --- 検討タブ ---
    {
        QWidget* considerationPage = new QWidget(m_tab);
        auto* considerationLayout = new QVBoxLayout(considerationPage);
        considerationLayout->setContentsMargins(4, 4, 4, 4);
        considerationLayout->setSpacing(4);

        ensureConsiderationTabManager();
        m_considerationTabManager->buildConsiderationUi(considerationPage);

        m_considerationTabIndex = m_tab->addTab(considerationPage, tr("検討"));
    }

    // --- USI通信ログ ---
    // ツールバー付きコンテナに変更
    m_usiLogContainer = new QWidget(m_tab);
    QVBoxLayout* usiLogLayout = new QVBoxLayout(m_usiLogContainer);
    usiLogLayout->setContentsMargins(4, 4, 4, 4);
    usiLogLayout->setSpacing(2);

    // ツールバーを構築
    buildUsiLogToolbar();
    usiLogLayout->addWidget(m_usiLogToolbar);

    // コマンド入力バーを構築
    buildUsiCommandBar();
    usiLogLayout->addWidget(m_usiCommandBar);

    m_usiLog = new QPlainTextEdit(m_usiLogContainer);
    m_usiLog->setReadOnly(true);
    usiLogLayout->addWidget(m_usiLog);

    m_tab->addTab(m_usiLogContainer, tr("USI通信ログ"));

    // --- CSA通信ログ ---
    m_csaLogContainer = new QWidget(m_tab);
    QVBoxLayout* csaLogLayout = new QVBoxLayout(m_csaLogContainer);
    csaLogLayout->setContentsMargins(4, 4, 4, 4);
    csaLogLayout->setSpacing(2);

    // ツールバーを構築
    buildCsaLogToolbar();
    csaLogLayout->addWidget(m_csaLogToolbar);

    // コマンド入力バーを構築
    buildCsaCommandBar();
    csaLogLayout->addWidget(m_csaCommandBar);

    m_csaLog = new QPlainTextEdit(m_csaLogContainer);
    m_csaLog->setReadOnly(true);
    csaLogLayout->addWidget(m_csaLog);

    m_tab->addTab(m_csaLogContainer, tr("CSA通信ログ"));

    // --- 棋譜コメント ---
    ensureCommentEditor();
    QWidget* commentContainer = m_commentEditor->buildCommentUi(m_tab);
    m_tab->addTab(commentContainer, tr("棋譜コメント"));

    // --- 分岐ツリー ---
    {
        auto* branchView = new QGraphicsView(m_tab);
        branchView->setRenderHint(QPainter::Antialiasing, true);
        branchView->setRenderHint(QPainter::TextAntialiasing, true);
        branchView->setRenderHint(QPainter::SmoothPixmapTransform, true);
        branchView->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        branchView->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        branchView->setAlignment(Qt::AlignLeft | Qt::AlignTop);

        ensureBranchTreeManager();
        m_branchTreeManager->setView(branchView);
        // シグナル転送（BranchTreeManager → EngineAnalysisTab）
        connect(m_branchTreeManager, &BranchTreeManager::branchNodeActivated,
                this, &EngineAnalysisTab::branchNodeActivated);

        m_tab->addTab(branchView, tr("分岐ツリー"));
    }

    // 初回起動時（あるいは再構築時）にモデルが既にあるなら即時適用
    reapplyViewTuning(m_view1, m_model1);  // 右寄せ＋3桁カンマ＆列幅チューニング
    reapplyViewTuning(m_view2, m_model2);

    // フォントマネージャーの初期化
    initUsiLogFontManager();
    initCsaLogFontManager();
    initThinkingFontManager();

}

// 思考ページを独立したウィジェットとして作成
QWidget* EngineAnalysisTab::createThinkingPage(QWidget* parent)
{
    QWidget* page = new QWidget(parent);
    auto* v = new QVBoxLayout(page);
    v->setContentsMargins(4, 4, 4, 4);
    v->setSpacing(4);

    m_info1 = new EngineInfoWidget(page, true);  // showFontButtons=true
    m_info1->setWidgetIndex(0);
    m_view1 = new QTableView(page);
    m_info2 = new EngineInfoWidget(page, false);
    m_info2->setWidgetIndex(1);
    m_view2 = new QTableView(page);

    connect(m_info1, &EngineInfoWidget::fontSizeIncreaseRequested,
            this, &EngineAnalysisTab::onThinkingFontIncrease);
    connect(m_info1, &EngineInfoWidget::fontSizeDecreaseRequested,
            this, &EngineAnalysisTab::onThinkingFontDecrease);
    connect(m_info1, &EngineInfoWidget::columnWidthChanged,
            this, &EngineAnalysisTab::onEngineInfoColumnWidthChanged);
    connect(m_info2, &EngineInfoWidget::columnWidthChanged,
            this, &EngineAnalysisTab::onEngineInfoColumnWidthChanged);

    QList<int> widths0 = SettingsService::engineInfoColumnWidths(0);
    if (!widths0.isEmpty() && widths0.size() == m_info1->columnCount()) {
        m_info1->setColumnWidths(widths0);
    }
    QList<int> widths1 = SettingsService::engineInfoColumnWidths(1);
    if (!widths1.isEmpty() && widths1.size() == m_info2->columnCount()) {
        m_info2->setColumnWidths(widths1);
    }

    setupThinkingViewHeader(m_view1);
    setupThinkingViewHeader(m_view2);

    connect(m_view1->horizontalHeader(), &QHeaderView::sectionResized,
            this, &EngineAnalysisTab::onView1SectionResized);
    connect(m_view2->horizontalHeader(), &QHeaderView::sectionResized,
            this, &EngineAnalysisTab::onView2SectionResized);
    connect(m_view1, &QTableView::clicked,
            this, &EngineAnalysisTab::onView1Clicked);
    connect(m_view2, &QTableView::clicked,
            this, &EngineAnalysisTab::onView2Clicked);

    v->addWidget(m_info1);
    v->addWidget(m_view1, 1);
    v->addWidget(m_info2);
    v->addWidget(m_view2, 1);

    // 起動時はエンジン2を非表示
    m_info2->setVisible(false);
    m_view2->setVisible(false);

    // フォントマネージャーの初期化
    initThinkingFontManager();

    return page;
}

// 検討ページを独立したウィジェットとして作成
QWidget* EngineAnalysisTab::createConsiderationPage(QWidget* parent)
{
    QWidget* considerationPage = new QWidget(parent);
    auto* considerationLayout = new QVBoxLayout(considerationPage);
    considerationLayout->setContentsMargins(4, 4, 4, 4);
    considerationLayout->setSpacing(4);

    ensureConsiderationTabManager();
    m_considerationTabManager->buildConsiderationUi(considerationPage);

    return considerationPage;
}

// USI通信ログページを独立したウィジェットとして作成
QWidget* EngineAnalysisTab::createUsiLogPage(QWidget* parent)
{
    m_usiLogContainer = new QWidget(parent);
    QVBoxLayout* usiLogLayout = new QVBoxLayout(m_usiLogContainer);
    usiLogLayout->setContentsMargins(4, 4, 4, 4);
    usiLogLayout->setSpacing(2);

    buildUsiLogToolbar();
    usiLogLayout->addWidget(m_usiLogToolbar);

    buildUsiCommandBar();
    usiLogLayout->addWidget(m_usiCommandBar);

    m_usiLog = new QPlainTextEdit(m_usiLogContainer);
    m_usiLog->setReadOnly(true);
    usiLogLayout->addWidget(m_usiLog);

    initUsiLogFontManager();

    return m_usiLogContainer;
}

// CSA通信ログページを独立したウィジェットとして作成
QWidget* EngineAnalysisTab::createCsaLogPage(QWidget* parent)
{
    m_csaLogContainer = new QWidget(parent);
    QVBoxLayout* csaLogLayout = new QVBoxLayout(m_csaLogContainer);
    csaLogLayout->setContentsMargins(4, 4, 4, 4);
    csaLogLayout->setSpacing(2);

    buildCsaLogToolbar();
    csaLogLayout->addWidget(m_csaLogToolbar);

    buildCsaCommandBar();
    csaLogLayout->addWidget(m_csaCommandBar);

    m_csaLog = new QPlainTextEdit(m_csaLogContainer);
    m_csaLog->setReadOnly(true);
    csaLogLayout->addWidget(m_csaLog);

    initCsaLogFontManager();

    return m_csaLogContainer;
}

// 棋譜コメントページを独立したウィジェットとして作成
QWidget* EngineAnalysisTab::createCommentPage(QWidget* parent)
{
    ensureCommentEditor();
    return m_commentEditor->buildCommentUi(parent);
}

// 分岐ツリーページを独立したウィジェットとして作成
QWidget* EngineAnalysisTab::createBranchTreePage(QWidget* parent)
{
    auto* branchView = new QGraphicsView(parent);
    branchView->setRenderHint(QPainter::Antialiasing, true);
    branchView->setRenderHint(QPainter::TextAntialiasing, true);
    branchView->setRenderHint(QPainter::SmoothPixmapTransform, true);
    branchView->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    branchView->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    branchView->setAlignment(Qt::AlignLeft | Qt::AlignTop);

    ensureBranchTreeManager();
    m_branchTreeManager->setView(branchView);
    // シグナル転送（BranchTreeManager → EngineAnalysisTab）
    connect(m_branchTreeManager, &BranchTreeManager::branchNodeActivated,
            this, &EngineAnalysisTab::branchNodeActivated);

    return branchView;
}

void EngineAnalysisTab::reapplyViewTuning(QTableView* v, QAbstractItemModel* m)
{
    if (!v) return;
    // 数値列（時間/深さ/ノード数/評価値）の右寄せ＆3桁カンマ
    if (m) applyNumericFormattingTo(v, m);
}

void EngineAnalysisTab::onModel1Reset()
{
    reapplyViewTuning(m_view1, m_model1);
}

void EngineAnalysisTab::onModel2Reset()
{
    reapplyViewTuning(m_view2, m_model2);
}

void EngineAnalysisTab::onLog1Changed()
{
    if (m_usiLog && m_log1) {
        appendColoredUsiLog(m_log1->usiCommLog(), QColor(0x20, 0x60, 0xa0));  // E1: 青色
    }
}

void EngineAnalysisTab::onLog2Changed()
{
    if (m_usiLog && m_log2) {
        appendColoredUsiLog(m_log2->usiCommLog(), QColor(0xa0, 0x20, 0x60));  // E2: 赤色
    }
}

void EngineAnalysisTab::appendColoredUsiLog(const QString& logLine, const QColor& lineColor)
{
    if (!m_usiLog || logLine.isEmpty()) return;

    // 行全体を指定された色で表示
    QTextCursor cursor = m_usiLog->textCursor();
    cursor.movePosition(QTextCursor::End);

    QTextCharFormat coloredFormat;
    coloredFormat.setForeground(lineColor);
    cursor.insertText(logLine + QStringLiteral("\n"), coloredFormat);

    m_usiLog->setTextCursor(cursor);
    m_usiLog->ensureCursorVisible();
}

void EngineAnalysisTab::appendUsiLogStatus(const QString& message)
{
    if (!m_usiLog || message.isEmpty()) return;

    // ステータスメッセージをグレーで表示
    QTextCursor cursor = m_usiLog->textCursor();
    cursor.movePosition(QTextCursor::End);

    QTextCharFormat grayFormat;
    grayFormat.setForeground(QColor(0x80, 0x80, 0x80));  // グレー
    cursor.insertText(QStringLiteral("⚙ ") + message + QStringLiteral("\n"), grayFormat);

    m_usiLog->setTextCursor(cursor);
    m_usiLog->ensureCursorVisible();
}

void EngineAnalysisTab::setModels(ShogiEngineThinkingModel* m1, ShogiEngineThinkingModel* m2,
                                  UsiCommLogModel* log1, UsiCommLogModel* log2)
{
    m_model1 = m1;  m_model2 = m2;
    m_log1   = log1; m_log2  = log2;

    if (m_view1) m_view1->setModel(m1);
    if (m_view2) m_view2->setModel(m2);

    if (m_info1) m_info1->setModel(log1);
    if (m_info2) m_info2->setModel(log2);

    // モデル設定後に列幅を適用（モデルがないと列幅が適用されない）
    applyThinkingViewColumnWidths(m_view1, 0);
    applyThinkingViewColumnWidths(m_view2, 1);

    // モデル設定直後に数値フォーマットを適用
    reapplyViewTuning(m_view1, m_model1);
    reapplyViewTuning(m_view2, m_model2);

    // modelReset時に再適用（ラムダ→関数スロット）
    if (m_model1) {
        QObject::connect(m_model1, &QAbstractItemModel::modelReset,
                         this, &EngineAnalysisTab::onModel1Reset, Qt::UniqueConnection);
    }
    if (m_model2) {
        QObject::connect(m_model2, &QAbstractItemModel::modelReset,
                         this, &EngineAnalysisTab::onModel2Reset, Qt::UniqueConnection);
    }

    // USIログ反映（ラムダ→関数スロット）
    if (m_log1) {
        QObject::connect(m_log1, &UsiCommLogModel::usiCommLogChanged,
                         this, &EngineAnalysisTab::onLog1Changed, Qt::UniqueConnection);
        // エンジン名変更シグナルを接続
        QObject::connect(m_log1, &UsiCommLogModel::engineNameChanged,
                         this, &EngineAnalysisTab::onEngine1NameChanged, Qt::UniqueConnection);
        // 初期値を設定
        onEngine1NameChanged();
    }
    if (m_log2) {
        QObject::connect(m_log2, &UsiCommLogModel::usiCommLogChanged,
                         this, &EngineAnalysisTab::onLog2Changed, Qt::UniqueConnection);
        // エンジン名変更シグナルを接続
        QObject::connect(m_log2, &UsiCommLogModel::engineNameChanged,
                         this, &EngineAnalysisTab::onEngine2NameChanged, Qt::UniqueConnection);
        // 初期値を設定
        onEngine2NameChanged();
    }
}

QTabWidget* EngineAnalysisTab::tab() const { return m_tab; }

void EngineAnalysisTab::setAnalysisVisible(bool on)
{
    if (this->isVisible() != on) this->setVisible(on);
}

void EngineAnalysisTab::setCommentHtml(const QString& html)
{
    ensureCommentEditor();
    m_commentEditor->setCommentHtml(html);
}

// ===================== 分岐ツリー：BranchTreeManager への委譲 =====================

void EngineAnalysisTab::ensureBranchTreeManager()
{
    if (!m_branchTreeManager) {
        m_branchTreeManager = new BranchTreeManager(this);
    }
}

BranchTreeManager* EngineAnalysisTab::branchTreeManager()
{
    ensureBranchTreeManager();
    return m_branchTreeManager;
}

void EngineAnalysisTab::setBranchTreeRows(const QVector<ResolvedRowLite>& rows)
{
    ensureBranchTreeManager();
    m_branchTreeManager->setBranchTreeRows(rows);
}

void EngineAnalysisTab::highlightBranchTreeAt(int row, int ply, bool centerOn)
{
    ensureBranchTreeManager();
    m_branchTreeManager->highlightBranchTreeAt(row, ply, centerOn);
}

void EngineAnalysisTab::setBranchTreeClickEnabled(bool enabled)
{
    ensureBranchTreeManager();
    m_branchTreeManager->setBranchTreeClickEnabled(enabled);
}

bool EngineAnalysisTab::isBranchTreeClickEnabled() const
{
    if (!m_branchTreeManager) return true;
    return m_branchTreeManager->isBranchTreeClickEnabled();
}

// ===================== CommentEditorPanel 初期化 =====================

void EngineAnalysisTab::ensureCommentEditor()
{
    if (!m_commentEditor) {
        m_commentEditor = new CommentEditorPanel(this);
        // シグナル転送（CommentEditorPanel → EngineAnalysisTab）
        connect(m_commentEditor, &CommentEditorPanel::commentUpdated,
                this, &EngineAnalysisTab::commentUpdated);
        connect(m_commentEditor, &CommentEditorPanel::requestApplyStart,
                this, &EngineAnalysisTab::requestApplyStart);
        connect(m_commentEditor, &CommentEditorPanel::requestApplyMainAtPly,
                this, &EngineAnalysisTab::requestApplyMainAtPly);
    }
}

CommentEditorPanel* EngineAnalysisTab::commentEditor()
{
    ensureCommentEditor();
    return m_commentEditor;
}

// ===== 互換API 実装 =====
void EngineAnalysisTab::setSecondEngineVisible(bool on)
{
    if (m_info2)  m_info2->setVisible(on);
    if (m_view2)  m_view2->setVisible(on);
}

void EngineAnalysisTab::setEngine1ThinkingModel(ShogiEngineThinkingModel* m)
{
    m_model1 = m;
    if (m_view1) m_view1->setModel(m);
    if (m_view1 && m_view1->horizontalHeader())
        m_view1->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
}

void EngineAnalysisTab::setEngine2ThinkingModel(ShogiEngineThinkingModel* m)
{
    m_model2 = m;
    if (m_view2) m_view2->setModel(m);
    if (m_view2 && m_view2->horizontalHeader())
        m_view2->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
}

void EngineAnalysisTab::setCommentText(const QString& text)
{
    ensureCommentEditor();
    m_commentEditor->setCommentText(text);
}

// ヘッダの基本設定（モデル設定前でもOK）
void EngineAnalysisTab::setupThinkingViewHeader(QTableView* v)
{
    if (!v) return;
    auto* h = v->horizontalHeader();
    if (!h) return;

    // ヘッダーの背景色と文字色を設定（棋譜タブと統一）
    v->setStyleSheet(QStringLiteral(
        "QHeaderView::section {"
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        "    stop:0 #40acff, stop:1 #209cee);"
        "  color: white;"
        "  font-weight: normal;"
        "  padding: 2px 6px;"
        "  border: none;"
        "  border-bottom: 1px solid #209cee;"
        "}"));

    // 全ての列をInteractive（ユーザーがリサイズ可能）に設定
    h->setDefaultSectionSize(100);
    h->setMinimumSectionSize(24);
    h->setStretchLastSection(true);

    // 行の高さを文字サイズに合わせて固定（余白を最小限に）
    auto* vh = v->verticalHeader();
    if (vh) {
        vh->setVisible(false);
        const int rowHeight = v->fontMetrics().height() + 4;
        vh->setDefaultSectionSize(rowHeight);
        vh->setSectionResizeMode(QHeaderView::Fixed);
    }
    v->setWordWrap(false);
}

// 列幅の適用（モデル設定後に呼ぶ）
void EngineAnalysisTab::applyThinkingViewColumnWidths(QTableView* v, int viewIndex)
{
    if (!v || !v->model()) return;
    auto* h = v->horizontalHeader();
    if (!h) return;

    constexpr int kColCount = 6;  // 「時間」「深さ」「ノード数」「評価値」「盤面」「読み筋」

    // 全ての列をInteractive（ユーザーがリサイズ可能）に設定
    for (int col = 0; col < kColCount; ++col) {
        h->setSectionResizeMode(col, QHeaderView::Interactive);
    }

    // デフォルトの列幅
    // 「時間」「深さ」「ノード数」「評価値」「盤面」「読み筋」
    // 読み筋はsetStretchLastSection(true)で自動伸縮するため小さめに設定
    const int defaultWidths[] = {50, 40, 80, 60, 45, 100};
    constexpr int kMaxTotalWidth = 450;  // 読み筋以外の列幅合計の上限

    // 設定ファイルから列幅を読み込む
    QList<int> savedWidths = SettingsService::thinkingViewColumnWidths(viewIndex);

    // 保存された列幅の合計をチェック（読み筋列を除く）
    bool useSavedWidths = false;
    if (savedWidths.size() == kColCount) {
        int totalWidth = 0;
        for (int col = 0; col < kColCount - 1; ++col) {  // 読み筋列を除く
            totalWidth += savedWidths.at(col);
        }
        // 読み筋以外の列幅合計が上限以下なら保存された幅を使用
        useSavedWidths = (totalWidth <= kMaxTotalWidth);
    }

    h->blockSignals(true);
    if (useSavedWidths) {
        // 保存された列幅を適用
        for (int col = 0; col < kColCount; ++col) {
            if (savedWidths.at(col) > 0) {
                v->setColumnWidth(col, savedWidths.at(col));
            }
        }
    } else {
        // デフォルトの列幅を設定
        for (int col = 0; col < kColCount; ++col) {
            v->setColumnWidth(col, defaultWidths[col]);
        }
    }
    h->blockSignals(false);

    // 列幅読み込み済みフラグを遅延で設定
    QTimer::singleShot(500, this, [this, viewIndex]() {
        if (viewIndex == 0) {
            m_thinkingView1WidthsLoaded = true;
        } else {
            m_thinkingView2WidthsLoaded = true;
        }
    });
}

// 追加：ヘッダー名で列を探す（大文字小文字は無視）
int EngineAnalysisTab::findColumnByHeader(QAbstractItemModel* model, const QString& title)
{
    if (!model) return -1;
    const int cols = model->columnCount();
    for (int c = 0; c < cols; ++c) {
        const QVariant hd = model->headerData(c, Qt::Horizontal, Qt::DisplayRole);
        const QString h = hd.toString().trimmed();
        if (QString::compare(h, title, Qt::CaseInsensitive) == 0) {
            return c;
        }
    }
    return -1;
}

// 追加：Time/Depth/Nodes/Score を「右寄せ＋3桁カンマ」で表示するデリゲートを適用
void EngineAnalysisTab::applyNumericFormattingTo(QTableView* view, QAbstractItemModel* model)
{
    if (!view || !model) return;

    // 同じデリゲートを複数列に使い回す。親を view にしてメモリ管理を任せる
    auto* delegate = new NumericRightAlignCommaDelegate(view);

    // 日本語と英語の両方のヘッダー名に対応
    const QStringList targets = { 
        "Time", "Depth", "Nodes", "Score",
        "時間", "深さ", "ノード数", "評価値"
    };
    for (const QString& t : targets) {
        const int col = findColumnByHeader(model, t);
        if (col >= 0) {
            view->setItemDelegateForColumn(col, delegate);
        }
    }
}

// USI通信ログツールバーを構築
void EngineAnalysisTab::buildUsiLogToolbar()
{
    m_usiLogToolbar = new QWidget(m_usiLogContainer);
    QHBoxLayout* toolbarLayout = new QHBoxLayout(m_usiLogToolbar);
    toolbarLayout->setContentsMargins(2, 2, 2, 2);
    toolbarLayout->setSpacing(4);

    // フォントサイズ減少ボタン
    m_btnUsiLogFontDecrease = new QToolButton(m_usiLogToolbar);
    m_btnUsiLogFontDecrease->setText(QStringLiteral("A-"));
    m_btnUsiLogFontDecrease->setToolTip(tr("フォントサイズを小さくする"));
    m_btnUsiLogFontDecrease->setFixedSize(28, 24);
    m_btnUsiLogFontDecrease->setStyleSheet(ButtonStyles::fontButton());
    connect(m_btnUsiLogFontDecrease, &QToolButton::clicked, this, &EngineAnalysisTab::onUsiLogFontDecrease);

    // フォントサイズ増加ボタン
    m_btnUsiLogFontIncrease = new QToolButton(m_usiLogToolbar);
    m_btnUsiLogFontIncrease->setText(QStringLiteral("A+"));
    m_btnUsiLogFontIncrease->setToolTip(tr("フォントサイズを大きくする"));
    m_btnUsiLogFontIncrease->setFixedSize(28, 24);
    m_btnUsiLogFontIncrease->setStyleSheet(ButtonStyles::fontButton());
    connect(m_btnUsiLogFontIncrease, &QToolButton::clicked, this, &EngineAnalysisTab::onUsiLogFontIncrease);

    // エンジン名ラベル（E1: xxx, E2: xxx）
    m_usiLogEngine1Label = new QLabel(QStringLiteral("E1: ---"), m_usiLogToolbar);
    m_usiLogEngine1Label->setStyleSheet(QStringLiteral("QLabel { color: #2060a0; font-weight: bold; }"));
    m_usiLogEngine2Label = new QLabel(QStringLiteral("E2: ---"), m_usiLogToolbar);
    m_usiLogEngine2Label->setStyleSheet(QStringLiteral("QLabel { color: #a02060; font-weight: bold; }"));

    toolbarLayout->addWidget(m_btnUsiLogFontDecrease);
    toolbarLayout->addWidget(m_btnUsiLogFontIncrease);
    toolbarLayout->addSpacing(20);
    toolbarLayout->addWidget(m_usiLogEngine1Label);
    toolbarLayout->addSpacing(20);
    toolbarLayout->addWidget(m_usiLogEngine2Label);
    toolbarLayout->addStretch();

    m_usiLogToolbar->setLayout(toolbarLayout);
    relaxToolbarWidth(m_usiLogToolbar);
}

void EngineAnalysisTab::initUsiLogFontManager()
{
    m_usiLogFontSize = SettingsService::usiLogFontSize();
    m_usiLogFontManager = std::make_unique<LogViewFontManager>(m_usiLogFontSize, m_usiLog);
    m_usiLogFontManager->setPostApplyCallback([this](int size) {
        QFont font;
        font.setPointSize(size);
        if (m_usiTargetCombo) m_usiTargetCombo->setFont(font);
        if (m_usiCommandInput) m_usiCommandInput->setFont(font);
        SettingsService::setUsiLogFontSize(size);
    });
    m_usiLogFontManager->apply();
}

void EngineAnalysisTab::onUsiLogFontIncrease()
{
    m_usiLogFontManager->increase();
}

void EngineAnalysisTab::onUsiLogFontDecrease()
{
    m_usiLogFontManager->decrease();
}

// USIコマンドバーを構築
void EngineAnalysisTab::buildUsiCommandBar()
{
    m_usiCommandBar = new QWidget(m_usiLogContainer);
    QHBoxLayout* layout = new QHBoxLayout(m_usiCommandBar);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    // 送信先選択コンボボックス
    m_usiTargetCombo = new QComboBox(m_usiCommandBar);
    m_usiTargetCombo->addItem(QStringLiteral("E1"), 0);
    m_usiTargetCombo->addItem(QStringLiteral("E2"), 1);
    m_usiTargetCombo->addItem(QStringLiteral("E1+E2"), 2);
    m_usiTargetCombo->setMinimumWidth(70);
    m_usiTargetCombo->setToolTip(tr("コマンドの送信先を選択"));

    // コマンド入力欄
    m_usiCommandInput = new QLineEdit(m_usiCommandBar);
    m_usiCommandInput->setPlaceholderText(tr("USIコマンドを入力してEnter"));

    layout->addWidget(m_usiTargetCombo);
    layout->addWidget(m_usiCommandInput, 1);  // stretchで伸縮

    // フォントサイズを適用
    {
        QFont font;
        font.setPointSize(m_usiLogFontSize);
        m_usiTargetCombo->setFont(font);
        m_usiCommandInput->setFont(font);
    }

    m_usiCommandBar->setLayout(layout);
    relaxToolbarWidth(m_usiCommandBar);

    // シグナル接続
    connect(m_usiCommandInput, &QLineEdit::returnPressed,
            this, &EngineAnalysisTab::onUsiCommandEntered);
}

// USIコマンド入力処理（Enterキー）
void EngineAnalysisTab::onUsiCommandEntered()
{
    if (!m_usiCommandInput || !m_usiTargetCombo) {
        return;
    }

    QString command = m_usiCommandInput->text().trimmed();
    if (command.isEmpty()) {
        return;
    }

    int target = m_usiTargetCombo->currentData().toInt();
    emit usiCommandRequested(target, command);

    // 入力欄をクリア
    m_usiCommandInput->clear();
}

// エンジン1名変更時のスロット
void EngineAnalysisTab::onEngine1NameChanged()
{
    if (m_usiLogEngine1Label && m_log1) {
        QString name = m_log1->engineName();
        if (name.isEmpty()) {
            name = QStringLiteral("---");
        }
        m_usiLogEngine1Label->setText(QStringLiteral("E1: %1").arg(name));
    }
}

// エンジン2名変更時のスロット
void EngineAnalysisTab::onEngine2NameChanged()
{
    if (m_usiLogEngine2Label && m_log2) {
        QString name = m_log2->engineName();
        if (name.isEmpty()) {
            name = QStringLiteral("---");
        }
        m_usiLogEngine2Label->setText(QStringLiteral("E2: %1").arg(name));
    }
}

void EngineAnalysisTab::initThinkingFontManager()
{
    m_thinkingFontSize = SettingsService::thinkingFontSize();
    m_thinkingFontManager = std::make_unique<LogViewFontManager>(m_thinkingFontSize, nullptr);
    m_thinkingFontManager->setPostApplyCallback([this](int size) {
        QFont font;
        font.setPointSize(size);

        // 上段（EngineInfoWidget）のフォントサイズ変更
        if (m_info1) m_info1->setFontSize(size);
        if (m_info2) m_info2->setFontSize(size);

        // 下段（TableView）のフォントサイズ変更
        QString headerStyle = QStringLiteral(
            "QHeaderView::section {"
            "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
            "    stop:0 #40acff, stop:1 #209cee);"
            "  color: white;"
            "  font-weight: normal;"
            "  padding: 2px 6px;"
            "  border: none;"
            "  border-bottom: 1px solid #209cee;"
            "  font-size: %1pt;"
            "}")
            .arg(size);

        if (m_view1) {
            m_view1->setFont(font);
            m_view1->setStyleSheet(headerStyle);
            const int rowHeight = m_view1->fontMetrics().height() + 4;
            m_view1->verticalHeader()->setDefaultSectionSize(rowHeight);
        }
        if (m_view2) {
            m_view2->setFont(font);
            m_view2->setStyleSheet(headerStyle);
            const int rowHeight = m_view2->fontMetrics().height() + 4;
            m_view2->verticalHeader()->setDefaultSectionSize(rowHeight);
        }

        SettingsService::setThinkingFontSize(size);
    });
    if (m_thinkingFontSize != 10) {
        m_thinkingFontManager->apply();
    }
}

void EngineAnalysisTab::onThinkingFontIncrease()
{
    m_thinkingFontManager->increase();
}

void EngineAnalysisTab::onThinkingFontDecrease()
{
    m_thinkingFontManager->decrease();
}

// ===================== CommentEditorPanel への委譲 =====================

void EngineAnalysisTab::setCurrentMoveIndex(int index)
{
    ensureCommentEditor();
    m_commentEditor->setCurrentMoveIndex(index);
}

int EngineAnalysisTab::currentMoveIndex() const
{
    if (!m_commentEditor) return -1;
    return m_commentEditor->currentMoveIndex();
}

bool EngineAnalysisTab::hasUnsavedComment() const
{
    if (!m_commentEditor) return false;
    return m_commentEditor->hasUnsavedComment();
}

bool EngineAnalysisTab::confirmDiscardUnsavedComment()
{
    ensureCommentEditor();
    return m_commentEditor->confirmDiscardUnsavedComment();
}

void EngineAnalysisTab::clearCommentDirty()
{
    ensureCommentEditor();
    m_commentEditor->clearCommentDirty();
}

// エンジン1の読み筋テーブルクリック処理
void EngineAnalysisTab::onView1Clicked(const QModelIndex& index)
{
    if (!index.isValid()) return;
    // 「盤面」列（列4）のクリック時のみ読み筋表示ウィンドウを開く
    if (index.column() == 4) {
        qCDebug(lcUi) << "[EngineAnalysisTab] onView1Clicked: row=" << index.row() << "(盤面ボタン)";
        emit pvRowClicked(0, index.row());
    }
}

// エンジン2の読み筋テーブルクリック処理
void EngineAnalysisTab::onView2Clicked(const QModelIndex& index)
{
    if (!index.isValid()) return;
    // 「盤面」列（列4）のクリック時のみ読み筋表示ウィンドウを開く
    if (index.column() == 4) {
        qCDebug(lcUi) << "[EngineAnalysisTab] onView2Clicked: row=" << index.row() << "(盤面ボタン)";
        emit pvRowClicked(1, index.row());
    }
}

// エンジン情報ウィジェットの列幅変更時の保存
void EngineAnalysisTab::onEngineInfoColumnWidthChanged()
{
    EngineInfoWidget* sender = qobject_cast<EngineInfoWidget*>(QObject::sender());
    if (!sender) return;
    
    int widgetIndex = sender->widgetIndex();
    QList<int> widths = sender->columnWidths();
    
    // 設定ファイルに保存
    SettingsService::setEngineInfoColumnWidths(widgetIndex, widths);
}

// 思考タブ下段（読み筋テーブル）の列幅変更時の保存
void EngineAnalysisTab::onThinkingViewColumnWidthChanged(int viewIndex)
{
    QTableView* view = (viewIndex == 0) ? m_view1 : m_view2;
    if (!view) return;
    
    QList<int> widths;
    const int colCount = view->horizontalHeader()->count();
    for (int col = 0; col < colCount; ++col) {
        widths.append(view->columnWidth(col));
    }
    
    // 設定ファイルに保存
    SettingsService::setThinkingViewColumnWidths(viewIndex, widths);
}

// view1の列幅変更スロット
void EngineAnalysisTab::onView1SectionResized(int logicalIndex, int oldSize, int newSize)
{
    Q_UNUSED(logicalIndex)
    Q_UNUSED(oldSize)
    Q_UNUSED(newSize)
    
    // 設定ファイルから読み込み済みの場合、初期化時のリサイズイベントは無視しない
    // ただし、初期表示の自動調整による保存は避ける必要がある
    // m_thinkingView1WidthsLoadedがtrueの場合のみ、ユーザー操作とみなして保存
    if (m_thinkingView1WidthsLoaded) {
        onThinkingViewColumnWidthChanged(0);
    }
}

// view2の列幅変更スロット
void EngineAnalysisTab::onView2SectionResized(int logicalIndex, int oldSize, int newSize)
{
    Q_UNUSED(logicalIndex)
    Q_UNUSED(oldSize)
    Q_UNUSED(newSize)
    
    if (m_thinkingView2WidthsLoaded) {
        onThinkingViewColumnWidthChanged(1);
    }
}

// CSA通信ログツールバーを構築
void EngineAnalysisTab::buildCsaLogToolbar()
{
    m_csaLogToolbar = new QWidget(m_csaLogContainer);
    QHBoxLayout* toolbarLayout = new QHBoxLayout(m_csaLogToolbar);
    toolbarLayout->setContentsMargins(2, 2, 2, 2);
    toolbarLayout->setSpacing(4);

    // フォントサイズ減少ボタン
    m_btnCsaLogFontDecrease = new QToolButton(m_csaLogToolbar);
    m_btnCsaLogFontDecrease->setText(QStringLiteral("A-"));
    m_btnCsaLogFontDecrease->setToolTip(tr("フォントサイズを小さくする"));
    m_btnCsaLogFontDecrease->setFixedSize(28, 24);
    m_btnCsaLogFontDecrease->setStyleSheet(ButtonStyles::fontButton());
    connect(m_btnCsaLogFontDecrease, &QToolButton::clicked, this, &EngineAnalysisTab::onCsaLogFontDecrease);

    // フォントサイズ増加ボタン
    m_btnCsaLogFontIncrease = new QToolButton(m_csaLogToolbar);
    m_btnCsaLogFontIncrease->setText(QStringLiteral("A+"));
    m_btnCsaLogFontIncrease->setToolTip(tr("フォントサイズを大きくする"));
    m_btnCsaLogFontIncrease->setFixedSize(28, 24);
    m_btnCsaLogFontIncrease->setStyleSheet(ButtonStyles::fontButton());
    connect(m_btnCsaLogFontIncrease, &QToolButton::clicked, this, &EngineAnalysisTab::onCsaLogFontIncrease);

    toolbarLayout->addWidget(m_btnCsaLogFontDecrease);
    toolbarLayout->addWidget(m_btnCsaLogFontIncrease);
    toolbarLayout->addStretch();

    m_csaLogToolbar->setLayout(toolbarLayout);
    relaxToolbarWidth(m_csaLogToolbar);
}

void EngineAnalysisTab::initCsaLogFontManager()
{
    m_csaLogFontSize = SettingsService::csaLogFontSize();
    m_csaLogFontManager = std::make_unique<LogViewFontManager>(m_csaLogFontSize, m_csaLog);
    m_csaLogFontManager->setPostApplyCallback([this](int size) {
        QFont font;
        font.setPointSize(size);
        if (m_btnCsaSendToServer) m_btnCsaSendToServer->setFont(font);
        if (m_csaCommandInput) m_csaCommandInput->setFont(font);
        SettingsService::setCsaLogFontSize(size);
    });
    m_csaLogFontManager->apply();
}

void EngineAnalysisTab::onCsaLogFontIncrease()
{
    m_csaLogFontManager->increase();
}

void EngineAnalysisTab::onCsaLogFontDecrease()
{
    m_csaLogFontManager->decrease();
}

// CSA通信ログ追記
void EngineAnalysisTab::appendCsaLog(const QString& line)
{
    if (m_csaLog) {
        m_csaLog->appendPlainText(line);
        // 自動スクロール
        QTextCursor cursor = m_csaLog->textCursor();
        cursor.movePosition(QTextCursor::End);
        m_csaLog->setTextCursor(cursor);
    }
}

// CSA通信ログクリア
void EngineAnalysisTab::clearCsaLog()
{
    if (m_csaLog) {
        m_csaLog->clear();
    }
}

// CSAコマンド入力バーを構築
void EngineAnalysisTab::buildCsaCommandBar()
{
    m_csaCommandBar = new QWidget(m_csaLogContainer);
    QHBoxLayout* layout = new QHBoxLayout(m_csaCommandBar);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    // CSAサーバーへ送信ボタン（ラベル表示用）
    m_btnCsaSendToServer = new QPushButton(tr("CSAサーバーへ送信"), m_csaCommandBar);
    m_btnCsaSendToServer->setEnabled(false);  // クリック不可（ラベルとして表示）
    m_btnCsaSendToServer->setFlat(true);      // フラットスタイル
    m_btnCsaSendToServer->setMinimumWidth(130);

    layout->addWidget(m_btnCsaSendToServer);

    // コマンド入力欄
    m_csaCommandInput = new QLineEdit(m_csaCommandBar);
    m_csaCommandInput->setPlaceholderText(tr("コマンドを入力してEnter"));
    layout->addWidget(m_csaCommandInput, 1);  // stretchで伸縮

    // コマンド入力部分のフォントサイズを設定
    {
        QFont cmdFont;
        cmdFont.setPointSize(m_csaLogFontSize);
        m_btnCsaSendToServer->setFont(cmdFont);
        m_csaCommandInput->setFont(cmdFont);
    }

    m_csaCommandBar->setLayout(layout);
    relaxToolbarWidth(m_csaCommandBar);

    // コマンド入力のEnterキー処理を接続
    connect(m_csaCommandInput, &QLineEdit::returnPressed,
            this, &EngineAnalysisTab::onCsaCommandEntered);
}

// CSAコマンド入力処理
void EngineAnalysisTab::onCsaCommandEntered()
{
    if (!m_csaCommandInput) {
        return;
    }

    QString command = m_csaCommandInput->text().trimmed();
    if (command.isEmpty()) {
        return;
    }

    // CSAサーバーへ送信シグナルを発行
    emit csaRawCommandRequested(command);

    // 入力欄をクリア
    m_csaCommandInput->clear();
}

// ===================== ConsiderationTabManager 初期化・委譲 =====================

void EngineAnalysisTab::ensureConsiderationTabManager()
{
    if (!m_considerationTabManager) {
        m_considerationTabManager = new ConsiderationTabManager(this);
        // シグナル転送（ConsiderationTabManager → EngineAnalysisTab）
        connect(m_considerationTabManager, &ConsiderationTabManager::considerationMultiPVChanged,
                this, &EngineAnalysisTab::considerationMultiPVChanged);
        connect(m_considerationTabManager, &ConsiderationTabManager::stopConsiderationRequested,
                this, &EngineAnalysisTab::stopConsiderationRequested);
        connect(m_considerationTabManager, &ConsiderationTabManager::startConsiderationRequested,
                this, &EngineAnalysisTab::startConsiderationRequested);
        connect(m_considerationTabManager, &ConsiderationTabManager::engineSettingsRequested,
                this, &EngineAnalysisTab::engineSettingsRequested);
        connect(m_considerationTabManager, &ConsiderationTabManager::considerationTimeSettingsChanged,
                this, &EngineAnalysisTab::considerationTimeSettingsChanged);
        connect(m_considerationTabManager, &ConsiderationTabManager::showArrowsChanged,
                this, &EngineAnalysisTab::showArrowsChanged);
        connect(m_considerationTabManager, &ConsiderationTabManager::considerationEngineChanged,
                this, &EngineAnalysisTab::considerationEngineChanged);
        connect(m_considerationTabManager, &ConsiderationTabManager::pvRowClicked,
                this, &EngineAnalysisTab::pvRowClicked);
    }
}

ConsiderationTabManager* EngineAnalysisTab::considerationTabManager()
{
    ensureConsiderationTabManager();
    return m_considerationTabManager;
}

EngineInfoWidget* EngineAnalysisTab::considerationInfo() const
{
    if (!m_considerationTabManager) return nullptr;
    return m_considerationTabManager->considerationInfo();
}

QTableView* EngineAnalysisTab::considerationView() const
{
    if (!m_considerationTabManager) return nullptr;
    return m_considerationTabManager->considerationView();
}

void EngineAnalysisTab::setConsiderationThinkingModel(ShogiEngineThinkingModel* m)
{
    ensureConsiderationTabManager();
    m_considerationTabManager->setConsiderationThinkingModel(m);
}

void EngineAnalysisTab::switchToConsiderationTab()
{
    if (m_tab && m_considerationTabIndex >= 0) {
        m_tab->setCurrentIndex(m_considerationTabIndex);
    }
}

void EngineAnalysisTab::switchToThinkingTab()
{
    if (m_tab) {
        m_tab->setCurrentIndex(0);
    }
}

int EngineAnalysisTab::considerationMultiPV() const
{
    if (!m_considerationTabManager) return 1;
    return m_considerationTabManager->considerationMultiPV();
}

void EngineAnalysisTab::setConsiderationMultiPV(int value)
{
    ensureConsiderationTabManager();
    m_considerationTabManager->setConsiderationMultiPV(value);
}

void EngineAnalysisTab::clearThinkingViewSelection(int engineIndex)
{
    QTableView* view = nullptr;
    if (engineIndex == 0) {
        view = m_view1;
    } else if (engineIndex == 1) {
        view = m_view2;
    } else if (engineIndex == 2) {
        view = considerationView();
    }
    if (!view) return;

    if (auto* sel = view->selectionModel()) {
        sel->clearSelection();
        sel->setCurrentIndex(QModelIndex(), QItemSelectionModel::NoUpdate);
    }
}

void EngineAnalysisTab::setConsiderationTimeLimit(bool unlimited, int byoyomiSecVal)
{
    ensureConsiderationTabManager();
    m_considerationTabManager->setConsiderationTimeLimit(unlimited, byoyomiSecVal);
}

void EngineAnalysisTab::setConsiderationEngineName(const QString& name)
{
    ensureConsiderationTabManager();
    m_considerationTabManager->setConsiderationEngineName(name);
}

void EngineAnalysisTab::startElapsedTimer()
{
    ensureConsiderationTabManager();
    m_considerationTabManager->startElapsedTimer();
}

void EngineAnalysisTab::stopElapsedTimer()
{
    if (m_considerationTabManager) {
        m_considerationTabManager->stopElapsedTimer();
    }
}

void EngineAnalysisTab::resetElapsedTimer()
{
    if (m_considerationTabManager) {
        m_considerationTabManager->resetElapsedTimer();
    }
}

void EngineAnalysisTab::setConsiderationRunning(bool running)
{
    ensureConsiderationTabManager();
    m_considerationTabManager->setConsiderationRunning(running);
}

void EngineAnalysisTab::loadEngineList()
{
    ensureConsiderationTabManager();
    m_considerationTabManager->loadEngineList();
}

void EngineAnalysisTab::loadConsiderationTabSettings()
{
    ensureConsiderationTabManager();
    m_considerationTabManager->loadConsiderationTabSettings();
}

void EngineAnalysisTab::saveConsiderationTabSettings()
{
    if (m_considerationTabManager) {
        m_considerationTabManager->saveConsiderationTabSettings();
    }
}

int EngineAnalysisTab::selectedEngineIndex() const
{
    if (!m_considerationTabManager) return 0;
    return m_considerationTabManager->selectedEngineIndex();
}

QString EngineAnalysisTab::selectedEngineName() const
{
    if (!m_considerationTabManager) return QString();
    return m_considerationTabManager->selectedEngineName();
}

bool EngineAnalysisTab::isUnlimitedTime() const
{
    if (!m_considerationTabManager) return true;
    return m_considerationTabManager->isUnlimitedTime();
}

int EngineAnalysisTab::byoyomiSec() const
{
    if (!m_considerationTabManager) return 20;
    return m_considerationTabManager->byoyomiSec();
}

bool EngineAnalysisTab::isShowArrowsChecked() const
{
    if (!m_considerationTabManager) return true;
    return m_considerationTabManager->isShowArrowsChecked();
}
