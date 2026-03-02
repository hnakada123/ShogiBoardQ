/// @file recordpane.cpp
/// @brief 棋譜欄ペインクラスの実装

#include "recordpane.h"
#include "buttonstyles.h"
#include "kifurecordlistmodel.h"
#include "kifubranchlistmodel.h"
#include "gamesettings.h"

#include "logcategories.h"
#include <QTextBrowser>
#include <QTableView>
#include <QHeaderView>
#include <QAbstractItemView>
#include <QPushButton>
#include <QSplitter>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QIcon>
#include <QSize>
#include <QModelIndex>
#include <QItemSelectionModel>
#include <QTimer>

RecordPane::RecordPane(QWidget* parent)
    : QWidget(parent)
    , m_appearanceManager(GameSettings::kifuPaneFontSize())
{
    buildUi();
    wireSignals(); // モデルに依存しないシグナルだけ先に配線
}

void RecordPane::buildUi()
{
    buildKifuTable();
    buildToolButtons();
    buildNavigationPanel();
    buildBranchPanel();
    buildMainLayout();
}

void RecordPane::buildKifuTable()
{
    m_kifu = new QTableView(this);
    m_kifu->setSelectionMode(QAbstractItemView::SingleSelection);
    m_kifu->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_kifu->verticalHeader()->setVisible(false);
    const int rowHeight = m_kifu->fontMetrics().height() + 4;
    m_kifu->verticalHeader()->setDefaultSectionSize(rowHeight);
    m_kifu->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    m_kifu->setWordWrap(false);
    m_kifu->setTextElideMode(Qt::ElideRight);
    m_kifu->setStyleSheet(RecordPaneAppearanceManager::kifuTableStyleSheet(m_appearanceManager.fontSize()));
}

void RecordPane::buildToolButtons()
{
    // --- 文字サイズ変更ボタン ---
    m_btnFontUp = new QPushButton(this);
    m_btnFontDown = new QPushButton(this);
    m_btnFontUp->setText(QStringLiteral("A+"));
    m_btnFontDown->setText(QStringLiteral("A-"));
    m_btnFontUp->setToolTip(tr("文字を大きくする"));
    m_btnFontDown->setToolTip(tr("文字を小さくする"));

    m_btnFontUp->setStyleSheet(ButtonStyles::fontButton());
    m_btnFontDown->setStyleSheet(ButtonStyles::fontButton());
    m_btnFontUp->setFixedSize(36, 24);
    m_btnFontDown->setFixedSize(36, 24);
    m_btnFontUp->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_btnFontDown->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    // --- しおり編集ボタン ---
    m_btnBookmarkEdit = new QPushButton(this);
    m_btnBookmarkEdit->setIcon(QIcon(QStringLiteral(":/images/actions/actionEditBookmark.svg")));
    m_btnBookmarkEdit->setIconSize(QSize(20, 20));
    m_btnBookmarkEdit->setToolTip(tr("しおりを編集"));
    m_btnBookmarkEdit->setStyleSheet(ButtonStyles::fontButton());
    m_btnBookmarkEdit->setFixedSize(36, 24);
    m_btnBookmarkEdit->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    // --- 列表示トグルボタン ---
    const QString toggleBtnStyle = ButtonStyles::toggleButton();

    m_btnToggleTime = new QPushButton(this);
    m_btnToggleTime->setCheckable(true);
    m_btnToggleTime->setChecked(GameSettings::kifuTimeColumnVisible());
    m_btnToggleTime->setIcon(QIcon(QStringLiteral(":/images/actions/actionToggleTimeColumn.svg")));
    m_btnToggleTime->setIconSize(QSize(20, 20));
    m_btnToggleTime->setToolTip(tr("消費時間列の表示/非表示"));
    m_btnToggleTime->setStyleSheet(toggleBtnStyle);
    m_btnToggleTime->setFixedSize(36, 24);
    m_btnToggleTime->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    m_btnToggleBookmark = new QPushButton(this);
    m_btnToggleBookmark->setCheckable(true);
    m_btnToggleBookmark->setChecked(GameSettings::kifuBookmarkColumnVisible());
    m_btnToggleBookmark->setIcon(QIcon(QStringLiteral(":/images/actions/actionToggleBookmarkColumn.svg")));
    m_btnToggleBookmark->setIconSize(QSize(20, 20));
    m_btnToggleBookmark->setToolTip(tr("しおり列の表示/非表示"));
    m_btnToggleBookmark->setStyleSheet(toggleBtnStyle);
    m_btnToggleBookmark->setFixedSize(36, 24);
    m_btnToggleBookmark->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    m_btnToggleComment = new QPushButton(this);
    m_btnToggleComment->setCheckable(true);
    m_btnToggleComment->setChecked(GameSettings::kifuCommentColumnVisible());
    m_btnToggleComment->setIcon(QIcon(QStringLiteral(":/images/actions/actionToggleCommentColumn.svg")));
    m_btnToggleComment->setIconSize(QSize(20, 20));
    m_btnToggleComment->setToolTip(tr("コメント列の表示/非表示"));
    m_btnToggleComment->setStyleSheet(toggleBtnStyle);
    m_btnToggleComment->setFixedSize(36, 24);
    m_btnToggleComment->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

void RecordPane::buildNavigationPanel()
{
    m_btn1 = new QPushButton(this);
    m_btn2 = new QPushButton(this);
    m_btn3 = new QPushButton(this);
    m_btn4 = new QPushButton(this);
    m_btn5 = new QPushButton(this);
    m_btn6 = new QPushButton(this);

    m_btn1->setText(tr("▲|"));
    m_btn2->setText(tr("▲▲"));
    m_btn3->setText(tr("▲"));
    m_btn4->setText(tr("▼"));
    m_btn5->setText(tr("▼▼"));
    m_btn6->setText(tr("▼|"));

    m_btn1->setToolTip(tr("最初に戻る"));
    m_btn2->setToolTip(tr("10手戻る"));
    m_btn3->setToolTip(tr("1手戻る"));
    m_btn4->setToolTip(tr("1手進む"));
    m_btn5->setToolTip(tr("10手進む"));
    m_btn6->setToolTip(tr("最後に進む"));

    const QString btnStyle = ButtonStyles::navigationButton();
    const QList<QPushButton*> allBtns = {m_btn1, m_btn2, m_btn3, m_btn4, m_btn5, m_btn6};
    for (QPushButton* const b : allBtns) {
        b->setStyleSheet(btnStyle);
        b->setFixedSize(36, 24);
        b->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    }

    // ナビゲーションボタンを縦に配置（ツール・トグルボタンも含む）
    auto* navLay = new QVBoxLayout;
    navLay->setContentsMargins(2, 4, 2, 4);
    navLay->setSpacing(3);
    navLay->addWidget(m_btnFontUp, 0, Qt::AlignHCenter);
    navLay->addWidget(m_btnFontDown, 0, Qt::AlignHCenter);
    navLay->addWidget(m_btnBookmarkEdit, 0, Qt::AlignHCenter);
    navLay->addWidget(m_btnToggleTime, 0, Qt::AlignHCenter);
    navLay->addWidget(m_btnToggleBookmark, 0, Qt::AlignHCenter);
    navLay->addWidget(m_btnToggleComment, 0, Qt::AlignHCenter);
    navLay->addWidget(m_btn1, 0, Qt::AlignHCenter);
    navLay->addWidget(m_btn2, 0, Qt::AlignHCenter);
    navLay->addWidget(m_btn3, 0, Qt::AlignHCenter);
    navLay->addWidget(m_btn4, 0, Qt::AlignHCenter);
    navLay->addWidget(m_btn5, 0, Qt::AlignHCenter);
    navLay->addWidget(m_btn6, 0, Qt::AlignHCenter);
    navLay->addStretch();

    m_navButtons = new QWidget(this);
    m_navButtons->setLayout(navLay);
    m_navButtons->setFixedWidth(50);
    m_navButtons->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
}

void RecordPane::buildBranchPanel()
{
    m_branch = new QTableView(this);
    m_branch->setSelectionMode(QAbstractItemView::SingleSelection);
    m_branch->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_branch->verticalHeader()->setVisible(false);
    const int branchRowHeight = m_branch->fontMetrics().height() + 4;
    m_branch->verticalHeader()->setDefaultSectionSize(branchRowHeight);
    m_branch->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    m_branch->setWordWrap(false);
    m_branch->setStyleSheet(RecordPaneAppearanceManager::branchTableStyleSheet(m_appearanceManager.fontSize()));

    m_branchContainer = new QWidget(this);
    auto* branchLay = new QVBoxLayout(m_branchContainer);
    branchLay->setContentsMargins(0, 0, 0, 0);
    branchLay->setSpacing(2);
    branchLay->addWidget(m_branch);
    m_branchContainer->setFixedWidth(120);
}

void RecordPane::buildMainLayout()
{
    m_lr = new QSplitter(Qt::Horizontal, this);
    m_lr->addWidget(m_kifu);
    m_lr->addWidget(m_navButtons);
    m_lr->addWidget(m_branchContainer);
    m_lr->setChildrenCollapsible(false);
    m_lr->setSizes({800, 50, 120});

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0,0,0,0);
    root->setSpacing(0);
    root->addWidget(m_lr);
    setLayout(root);
}

void RecordPane::wireSignals()
{
    // 既存：
    connect(m_branch, &QTableView::activated, this, &RecordPane::branchActivated, Qt::UniqueConnection);
    connect(m_branch, &QTableView::clicked, this, &RecordPane::branchActivated, Qt::UniqueConnection);

    // 文字サイズ変更ボタンの接続
    connect(m_btnFontUp, &QPushButton::clicked, this, &RecordPane::onFontIncrease);
    connect(m_btnFontDown, &QPushButton::clicked, this, &RecordPane::onFontDecrease);

    // しおり編集ボタンの接続
    connect(m_btnBookmarkEdit, &QPushButton::clicked, this, &RecordPane::bookmarkEditRequested);

    // 列表示トグルボタンの接続
    connect(m_btnToggleTime, &QPushButton::toggled, this, &RecordPane::onToggleTimeColumn);
    connect(m_btnToggleBookmark, &QPushButton::toggled, this, &RecordPane::onToggleBookmarkColumn);
    connect(m_btnToggleComment, &QPushButton::toggled, this, &RecordPane::onToggleCommentColumn);

    // 棋譜表の選択ハイライトを黄色に
    setupKifuSelectionAppearance();

    // 分岐候補欄の選択ハイライトを黄色に
    setupBranchViewSelectionAppearance();

    // 初期フォントサイズを適用
    m_appearanceManager.applyFontToViews(m_kifu, m_branch);
}

void RecordPane::setModels(KifuRecordListModel* recModel, KifuBranchListModel* brModel)
{
    if (!m_kifu || !m_branch) {
        qCWarning(lcUi).noquote() << "[RecordPane] setModels: m_kifu or m_branch is null, rebuilding UI";
        buildUi();
    }

    qCDebug(lcUi) << "setModels called";
    qCDebug(lcUi) << "m_kifu styleSheet in setModels:" << m_kifu->styleSheet().left(100) << "...";

    // --- 棋譜テーブル ---
    m_kifu->setModel(recModel);

    // モデルに行がある場合、最初の行を選択する
    if (recModel && recModel->rowCount() > 0) {
        qCDebug(lcUi) << "[RecordPane] setModels: model has rows, selecting first row";
        QTimer::singleShot(0, this, [this]() {
            if (m_kifu && m_kifu->model() && m_kifu->model()->rowCount() > 0) {
                if (auto* sel = m_kifu->selectionModel()) {
                    const QModelIndex top = m_kifu->model()->index(0, 0);
                    sel->setCurrentIndex(top, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
                    qCDebug(lcUi) << "[RecordPane] initial row selected";
                }
            }
        });
    }

    // 行追加→自動スクロール（多重接続防止）
    if (auto* model = m_kifu->model()) {
        if (m_connRowsInserted)
            disconnect(m_connRowsInserted);
        m_connRowsInserted = connect(model, &QAbstractItemModel::rowsInserted,
                                     this, &RecordPane::onKifuRowsInserted);
    }

    // 注意: 行選択の中継は後続の connectCurrentRow で行うため、ここでは接続しない
    if (m_connRowChanged) {
        disconnect(m_connRowChanged);
        m_connRowChanged = {};
    }

    m_appearanceManager.applyColumnVisibility(m_kifu);
    m_kifu->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_kifu->setSelectionMode(QAbstractItemView::SingleSelection);

    // ① 先に既存接続を解除
    if (m_connKifuCurrentRow) {
        QObject::disconnect(m_connKifuCurrentRow);
        m_connKifuCurrentRow = {};
    }

    // ② selectionModel() が確実にできてから接続
    if (auto* sel = m_kifu->selectionModel()) {
        m_connKifuCurrentRow =
            connect(sel, &QItemSelectionModel::currentRowChanged,
                    this, &RecordPane::onKifuCurrentRowChanged);
    } else {
        QTimer::singleShot(0, this, &RecordPane::connectKifuCurrentRowChanged);
    }

    // --- 分岐テーブル ---
    m_branch->setModel(brModel);
    if (auto* hh2 = m_branch->horizontalHeader()) {
        hh2->setSectionResizeMode(0, QHeaderView::Stretch);
    }

    // 分岐テーブルの選択変更時にモデルのハイライト行を更新
    if (auto* sel = m_branch->selectionModel()) {
        if (m_connBranchCurrentRow) {
            disconnect(m_connBranchCurrentRow);
        }
        m_connBranchCurrentRow = connect(sel, &QItemSelectionModel::currentRowChanged,
                this, &RecordPane::onBranchCurrentRowChanged);
    }
}

QTableView* RecordPane::kifuView() const { return m_kifu; }
QTableView* RecordPane::branchView() const { return m_branch; }

void RecordPane::setArrowButtonsEnabled(bool on)
{
    const QList<QPushButton*> btns = {m_btn1, m_btn2, m_btn3, m_btn4, m_btn5, m_btn6};
    for (QPushButton* const b : btns)
        if (b) b->setEnabled(on);
}

void RecordPane::setKifuViewEnabled(bool on)
{
    m_appearanceManager.setKifuViewEnabled(m_kifu, on, m_navigationDisabled);
}

void RecordPane::setNavigationEnabled(bool on)
{
    setArrowButtonsEnabled(on);
    setKifuViewEnabled(on);
}

void RecordPane::onKifuRowsInserted(const QModelIndex&, int, int)
{
    if (m_kifu) m_kifu->scrollToBottom();
}

void RecordPane::onKifuCurrentRowChanged(const QModelIndex& cur, const QModelIndex&)
{
    const int row = cur.isValid() ? cur.row() : 0;
    qCDebug(lcUi).noquote() << "[RecordPane] onKifuCurrentRowChanged: emitting mainRowChanged(" << row << ")";
    emit mainRowChanged(row);
}

void RecordPane::onBranchCurrentRowChanged(const QModelIndex& current, const QModelIndex&)
{
    auto* brModel = qobject_cast<KifuBranchListModel*>(m_branch->model());
    if (brModel && current.isValid()) {
        brModel->setCurrentHighlightRow(current.row());
    }
}

void RecordPane::connectKifuCurrentRowChanged()
{
    if (!m_kifu) return;
    if (auto* sel = m_kifu->selectionModel()) {
        if (m_connKifuCurrentRow) {
            QObject::disconnect(m_connKifuCurrentRow);
        }
        m_connKifuCurrentRow =
            connect(sel, &QItemSelectionModel::currentRowChanged,
                    this, &RecordPane::onKifuCurrentRowChanged);
    }
}

void RecordPane::setBranchCommentText(const QString& text)
{
    Q_UNUSED(text)
}

void RecordPane::setBranchCommentHtml(const QString& html)
{
    Q_UNUSED(html)
}

CommentTextAdapter* RecordPane::commentLabel()
{
    return &m_commentAdapter;
}

QPushButton* RecordPane::backToMainButton()
{
    if (!m_branchContainer) return nullptr;

    if (auto* existed = this->findChild<QPushButton*>("backToMainButton"))
        return existed;

    auto* btn = new QPushButton(tr("本譜に戻る"), this);
    btn->setObjectName(QStringLiteral("backToMainButton"));
    btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    btn->setVisible(false);
    btn->setToolTip(tr("現在の手数で本譜（メインライン）に戻る"));

    if (auto* lay = qobject_cast<QVBoxLayout*>(m_branchContainer->layout())) {
        lay->addWidget(btn);
    }
    return btn;
}

void RecordPane::setupKifuSelectionAppearance()
{
    RecordPaneAppearanceManager::setupSelectionPalette(m_kifu);
}

void RecordPane::setupBranchViewSelectionAppearance()
{
    RecordPaneAppearanceManager::setupSelectionPalette(m_branch);
}

void RecordPane::onFontIncrease(bool /*checked*/)
{
    if (m_appearanceManager.tryIncreaseFontSize()) {
        m_appearanceManager.applyFontToViews(m_kifu, m_branch);
    }
}

void RecordPane::onFontDecrease(bool /*checked*/)
{
    if (m_appearanceManager.tryDecreaseFontSize()) {
        m_appearanceManager.applyFontToViews(m_kifu, m_branch);
    }
}

void RecordPane::onToggleTimeColumn(bool checked)
{
    m_appearanceManager.toggleTimeColumn(m_kifu, checked);
}

void RecordPane::onToggleBookmarkColumn(bool checked)
{
    m_appearanceManager.toggleBookmarkColumn(m_kifu, checked);
}

void RecordPane::onToggleCommentColumn(bool checked)
{
    m_appearanceManager.toggleCommentColumn(m_kifu, checked);
}
