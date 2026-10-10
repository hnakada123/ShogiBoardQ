/// @file engineanalysistab.cpp
/// @brief エンジン解析タブクラスの実装

#include "engineanalysistab.h"
#include "commenteditorpanel.h"
#include "considerationtabmanager.h"
#include "usilogpanel.h"
#include "csalogpanel.h"
#include "engineanalysispresenter.h"
#include "candidatearrowcontroller.h"
#include "analysissettings.h"
#include "buttonstyles.h"
#include <QCheckBox>
#include <QLabel>
#include <QHBoxLayout>

#include <QTabWidget>
#include <QTableView>
#include <QToolButton>
#include <QVBoxLayout>
#include <QGraphicsView>
#include <QHeaderView>
#include <QSizePolicy>

#include "engineinfowidget.h"
#include "shogienginethinkingmodel.h"
#include "usicommlogmodel.h"

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
        m_tab = new QTabWidget(this);
        m_tab->setObjectName(QStringLiteral("analysisTabs"));
        m_tab->setMinimumWidth(0);
        m_tab->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);

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
    QWidget* thinkingPage = buildThinkingPageContent(m_tab);
    m_tab->addTab(thinkingPage, tr("思考"));

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
    ensureUsiLogPanel();
    QWidget* usiLogPage = m_usiLogPanel->buildUi(m_tab);
    m_tab->addTab(usiLogPage, tr("USI通信ログ"));

    // --- CSA通信ログ ---
    ensureCsaLogPanel();
    QWidget* csaLogPage = m_csaLogPanel->buildUi(m_tab);
    m_tab->addTab(csaLogPage, tr("CSA通信ログ"));

    // --- 棋譜コメント ---
    ensureCommentEditor();
    QWidget* commentContainer = m_commentEditor->buildCommentUi(m_tab);
    m_tab->addTab(commentContainer, tr("棋譜コメント"));

    // --- 分岐ツリー ---
    {
        auto* branchView = new QGraphicsView(m_tab);
        branchView->setObjectName(QStringLiteral("branchTreeView"));
        branchView->setRenderHint(QPainter::Antialiasing, true);
        branchView->setRenderHint(QPainter::TextAntialiasing, true);
        branchView->setRenderHint(QPainter::SmoothPixmapTransform, true);
        branchView->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        branchView->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        branchView->setAlignment(Qt::AlignLeft | Qt::AlignTop);

        ensureBranchTreeManager();
        m_branchTreeManager->setView(branchView);
        connect(m_branchTreeManager, &BranchTreeManager::branchNodeActivated,
                this, &EngineAnalysisTab::branchNodeActivated);

        m_tab->addTab(branchView, tr("分岐ツリー"));
    }
}

// 思考ページ構築ヘルパ（buildUi/createThinkingPage共通）
QWidget* EngineAnalysisTab::buildThinkingPageContent(QWidget* parent)
{
    QWidget* page = new QWidget(parent);
    auto* v = new QVBoxLayout(page);
    v->setContentsMargins(4, 4, 4, 4);
    v->setSpacing(4);

    auto* arrowControls = new QHBoxLayout;
    auto* fontDecrease = new QToolButton(page);
    fontDecrease->setObjectName(QStringLiteral("thinkingFontDecrease"));
    fontDecrease->setText(QStringLiteral("A-"));
    fontDecrease->setToolTip(tr("フォントサイズを小さくする"));
    auto* fontIncrease = new QToolButton(page);
    fontIncrease->setObjectName(QStringLiteral("thinkingFontIncrease"));
    fontIncrease->setText(QStringLiteral("A+"));
    fontIncrease->setToolTip(tr("フォントサイズを大きくする"));
    for (auto* button : {fontDecrease, fontIncrease}) {
        button->setFixedSize(36, 24);
        button->setStyleSheet(ButtonStyles::panelToolButton());
        arrowControls->addWidget(button);
    }
    m_matchArrows = new QCheckBox(tr("対局中の矢印表示"), page);
    m_matchArrows->setObjectName(QStringLiteral("matchArrows"));
    m_matchArrows->setChecked(AnalysisSettings::matchArrowsVisible());
    m_ponderArrows = new QCheckBox(tr("先読み側も表示"), page);
    m_ponderArrows->setObjectName(QStringLiteral("ponderArrows"));
    m_ponderArrows->setChecked(AnalysisSettings::ponderArrowsVisible());
    m_ponderArrows->setToolTip(tr("Ponder中の応手を青の破線で表示します。エンジンの先読み設定は変更しません。"));
    arrowControls->addWidget(m_matchArrows);
    arrowControls->addWidget(m_ponderArrows);
    arrowControls->addStretch();
    v->addLayout(arrowControls);
    m_arrowDescription = new QLabel(page);
    m_arrowDescription->setObjectName(QStringLiteral("matchArrowDescription"));
    m_arrowDescription->setWordWrap(true);
    v->addWidget(m_arrowDescription);
    connect(m_matchArrows, &QCheckBox::toggled, this, &EngineAnalysisTab::onMatchArrowsToggled);
    connect(m_ponderArrows, &QCheckBox::toggled, this, &EngineAnalysisTab::onPonderArrowsToggled);
    updateArrowControls();

    m_info1 = new EngineInfoWidget(page);
    m_info1->setWidgetIndex(0);
    m_view1 = new QTableView(page);
    m_view1->setObjectName(QStringLiteral("thinkingView1"));
    m_info2 = new EngineInfoWidget(page);
    m_info2->setWidgetIndex(1);
    m_view2 = new QTableView(page);

    ensurePresenter();
    m_presenter->setViews(m_view1, m_view2, m_info1, m_info2);

    // フォントサイズ変更シグナルを接続
    connect(fontIncrease, &QToolButton::clicked,
            m_presenter, &EngineAnalysisPresenter::onThinkingFontIncrease);
    connect(fontDecrease, &QToolButton::clicked,
            m_presenter, &EngineAnalysisPresenter::onThinkingFontDecrease);

    // EngineInfo 列幅管理
    m_presenter->loadEngineInfoColumnWidths();
    m_presenter->wireEngineInfoColumnWidthSignals();

    // ヘッダ基本設定・シグナル接続
    m_presenter->setupThinkingViewHeader(m_view1);
    m_presenter->setupThinkingViewHeader(m_view2);
    m_presenter->wireThinkingViewSignals();

    v->addWidget(m_info1);
    v->addWidget(m_view1, 1);
    v->addWidget(m_info2);
    v->addWidget(m_view2, 1);

    // フォントマネージャーの初期化
    m_presenter->initThinkingFontManager();

    return page;
}

// 思考ページを独立したウィジェットとして作成
QWidget* EngineAnalysisTab::createThinkingPage(QWidget* parent)
{
    QWidget* page = buildThinkingPageContent(parent);

    // 起動時はエンジン2を非表示
    m_info2->setVisible(false);
    m_view2->setVisible(false);

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
    ensureUsiLogPanel();
    return m_usiLogPanel->buildUi(parent);
}

// CSA通信ログページを独立したウィジェットとして作成
QWidget* EngineAnalysisTab::createCsaLogPage(QWidget* parent)
{
    ensureCsaLogPanel();
    return m_csaLogPanel->buildUi(parent);
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
    branchView->setObjectName(QStringLiteral("branchTreeView"));
    branchView->setRenderHint(QPainter::Antialiasing, true);
    branchView->setRenderHint(QPainter::TextAntialiasing, true);
    branchView->setRenderHint(QPainter::SmoothPixmapTransform, true);
    branchView->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    branchView->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    branchView->setAlignment(Qt::AlignLeft | Qt::AlignTop);

    ensureBranchTreeManager();
    m_branchTreeManager->setView(branchView);
    connect(m_branchTreeManager, &BranchTreeManager::branchNodeActivated,
            this, &EngineAnalysisTab::branchNodeActivated);

    return branchView;
}

// ===================== モデル設定 =====================

void EngineAnalysisTab::setModels(ShogiEngineThinkingModel* m1, ShogiEngineThinkingModel* m2,
                                  UsiCommLogModel* log1, UsiCommLogModel* log2)
{
    // 思考モデルをプレゼンタに委譲
    ensurePresenter();
    m_presenter->setModels(m1, m2);

    // エンジン情報ウィジェットにモデル設定
    if (m_info1) m_info1->setModel(log1);
    if (m_info2) m_info2->setModel(log2);

    // USI通信ログパネルにモデル設定
    ensureUsiLogPanel();
    m_usiLogPanel->setModels(log1, log2);
}

QTabWidget* EngineAnalysisTab::tab() const { return m_tab; }

void EngineAnalysisTab::setArrowController(CandidateArrowController* controller)
{
    if (m_arrowController == controller) return;
    if (m_arrowController) disconnect(m_arrowController, nullptr, this, nullptr);
    m_arrowController = controller;
    if (controller) {
        connect(controller, &CandidateArrowController::displayStateChanged,
                this, &EngineAnalysisTab::updateArrowControls);
    }
    updateArrowControls();
}

void EngineAnalysisTab::onMatchArrowsToggled(bool checked)
{
    AnalysisSettings::setMatchArrowsVisible(checked);
    updateArrowControls();
    if (m_arrowController) m_arrowController->scheduleRefresh();
}

void EngineAnalysisTab::onPonderArrowsToggled(bool checked)
{
    AnalysisSettings::setPonderArrowsVisible(checked);
    updateArrowControls();
    if (m_arrowController) m_arrowController->scheduleRefresh();
}

void EngineAnalysisTab::updateArrowControls()
{
    if (!m_matchArrows || !m_ponderArrows || !m_arrowDescription) return;
    const bool consideration = m_arrowController && m_arrowController->considerationActive();
    m_matchArrows->setEnabled(!consideration);
    m_ponderArrows->setEnabled(!consideration && m_matchArrows->isChecked());
    QString description;
    if (consideration) {
        description = tr("検討中の矢印表示は検討タブで設定します。");
    } else if (m_matchArrows->isChecked()) {
        description = tr("赤の実線：手番側");
        if (m_ponderArrows->isChecked()) description += tr(" ／ 青の破線：先読み側");
        if (m_arrowController && !m_arrowController->ponderDescription().isEmpty())
            description += QStringLiteral(" — ") + m_arrowController->ponderDescription();
    }
    m_arrowDescription->setText(description);
    m_arrowDescription->setVisible(!description.isEmpty());
}

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

void EngineAnalysisTab::setBranchTreeRows(const QList<ResolvedRowLite>& rows)
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

// ===================== CommentEditorPanel 初期化・委譲 =====================

void EngineAnalysisTab::ensureCommentEditor()
{
    if (!m_commentEditor) {
        m_commentEditor = new CommentEditorPanel(this);
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

// ===================== UsiLogPanel 初期化・委譲 =====================

void EngineAnalysisTab::ensureUsiLogPanel()
{
    if (!m_usiLogPanel) {
        m_usiLogPanel = new UsiLogPanel(this);
        connect(m_usiLogPanel, &UsiLogPanel::usiCommandRequested,
                this, &EngineAnalysisTab::usiCommandRequested);
    }
}

// ===================== CsaLogPanel 初期化・委譲 =====================

void EngineAnalysisTab::ensureCsaLogPanel()
{
    if (!m_csaLogPanel) {
        m_csaLogPanel = new CsaLogPanel(this);
        connect(m_csaLogPanel, &CsaLogPanel::csaRawCommandRequested,
                this, &EngineAnalysisTab::csaRawCommandRequested);
    }
}

// ===================== EngineAnalysisPresenter 初期化・委譲 =====================

void EngineAnalysisTab::ensurePresenter()
{
    if (!m_presenter) {
        m_presenter = new EngineAnalysisPresenter(this);
        connect(m_presenter, &EngineAnalysisPresenter::pvRowClicked,
                this, &EngineAnalysisTab::pvRowClicked);
    }
}

// ===== 互換API 実装 =====
void EngineAnalysisTab::setSecondEngineVisible(bool on)
{
    if (m_info2)  m_info2->setVisible(on);
    if (m_view2)  m_view2->setVisible(on);
}

void EngineAnalysisTab::setEngine1ThinkingModel(ShogiEngineThinkingModel* m)
{
    ensurePresenter();
    m_presenter->setEngine1ThinkingModel(m);
}

void EngineAnalysisTab::setEngine2ThinkingModel(ShogiEngineThinkingModel* m)
{
    ensurePresenter();
    m_presenter->setEngine2ThinkingModel(m);
}

void EngineAnalysisTab::setCommentText(const QString& text)
{
    ensureCommentEditor();
    m_commentEditor->setCommentText(text);
}

void EngineAnalysisTab::appendCsaLog(const QString& line)
{
    ensureCsaLogPanel();
    m_csaLogPanel->append(line);
}

void EngineAnalysisTab::clearCsaLog()
{
    ensureCsaLogPanel();
    m_csaLogPanel->clear();
}

void EngineAnalysisTab::setCsaConnected(bool connected)
{
    ensureCsaLogPanel();
    m_csaLogPanel->setConnected(connected);
}

void EngineAnalysisTab::clearUsiLog()
{
    ensureUsiLogPanel();
    m_usiLogPanel->clear();
}

void EngineAnalysisTab::appendUsiLogStatus(const QString& message)
{
    ensureUsiLogPanel();
    m_usiLogPanel->appendStatus(message);
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

// ===================== EngineAnalysisPresenter への委譲 =====================

void EngineAnalysisTab::clearThinkingViewSelection(int engineIndex)
{
    ensurePresenter();
    m_presenter->clearThinkingViewSelection(engineIndex, considerationView());
}
