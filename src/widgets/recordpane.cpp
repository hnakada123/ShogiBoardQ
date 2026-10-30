/// @file recordpane.cpp
/// @brief 棋譜欄ペインクラスの実装

#include "recordpane.h"
#include "panelbutton.h"
#include "stepbuttons.h"
#include "fontsizecontrol.h"
#include "incrementaltableview.h"
#include "tablemodelutils.h"
#include "kifurecordlistmodel.h"
#include "kifubranchlistmodel.h"
#include "kifumovedelegate.h"
#include "recordhighlightdelegate.h"
#include "gamesettings.h"
#include "tablestyles.h"

#include "logcategories.h"
#include <QTextBrowser>
#include <QTableView>
#include <QHeaderView>
#include <QScrollBar>
#include <QAbstractItemView>
#include <QPushButton>
#include <QToolButton>
#include <QSplitter>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QIcon>
#include <QSize>
#include <QModelIndex>
#include <QItemSelectionModel>
#include <QPalette>
#include <QSignalBlocker>
#include <QTimer>

namespace {
using Column = KifuRecordListModel::Column;

/// 行を選ぶ表と、選んだ行の色（棋譜欄と分岐候補欄で同じ）
void setupSelection(QTableView* view)
{
    view->setSelectionBehavior(QAbstractItemView::SelectRows);
    view->setSelectionMode(QAbstractItemView::SingleSelection);
    QPalette palette = view->palette();
    for (const QPalette::ColorGroup group : {QPalette::Active, QPalette::Inactive}) {
        palette.setColor(group, QPalette::Highlight, TableStyles::selectionBackground());
        palette.setColor(group, QPalette::HighlightedText, TableStyles::selectionText());
    }
    view->setPalette(palette);
}

/// 表の文字の大きさと行の高さ
void applyTableFont(QTableView* view, int fontSize)
{
    QFont font = view->font();
    font.setPointSize(fontSize);
    view->setFont(font);
    view->verticalHeader()->setDefaultSectionSize(KifuMoveDelegate::textHeight(view->font()) + 4);
}
} // namespace

RecordPane::RecordPane(QWidget* parent)
    : QWidget(parent)
    , m_fontSize(GameSettings::kifuPaneFontSize)
{
    m_appearance.fontSize = m_fontSize.fontSize();
    m_appearance.timeVisible = GameSettings::kifuTimeColumnVisible();
    m_appearance.bookmarkVisible = GameSettings::kifuBookmarkColumnVisible();
    m_appearance.commentVisible = GameSettings::kifuCommentColumnVisible();
    buildUi();
    wireSignals(); // モデルに依存しないシグナルだけ先に配線
    applyAppearance();
}

QString RecordPane::tableStyleSheet(int fontSize)
{
    return TableStyles::header(fontSize) + TableStyles::selection()
         + QStringLiteral("QTableView { background-color: #ffffff; color: #303841; }");
}

void RecordPane::applyAppearance()
{
    // 文字の大きさとスタイルシート（行のクリックを止めている間は、選択とフォーカスの印を出さない）
    applyTableFont(m_kifu, m_appearance.fontSize);
    applyTableFont(m_branch, m_appearance.fontSize);
    QString kifuStyle = tableStyleSheet(m_appearance.fontSize);
    if (!m_appearance.navigationEnabled) {
        kifuStyle += QStringLiteral("QTableView::item:focus { background-color: transparent; }"
                                    "QTableView::item:selected:focus { background-color: transparent; }");
    }
    m_kifu->setStyleSheet(kifuStyle);
    m_branch->setStyleSheet(tableStyleSheet(m_appearance.fontSize));

    // 行のクリック
    m_kifu->viewport()->setAttribute(Qt::WA_TransparentForMouseEvents, !m_appearance.navigationEnabled);
    m_kifu->setFocusPolicy(m_appearance.navigationEnabled ? Qt::StrongFocus : Qt::NoFocus);

    // 列の表示。指し手・消費時間は内容に合わせ、しおり・コメントの最右の表示列だけを伸ばす
    m_kifu->setColumnHidden(Column::TimeColumn, !m_appearance.timeVisible);
    m_kifu->setColumnHidden(Column::BookmarkColumn, !m_appearance.bookmarkVisible);
    m_kifu->setColumnHidden(Column::CommentColumn, !m_appearance.commentVisible);
    if (QHeaderView* header = m_kifu->horizontalHeader(); header && header->count() == Column::ColumnCount) {
        header->setSectionResizeMode(Column::MoveColumn, QHeaderView::ResizeToContents);
        header->setSectionResizeMode(Column::TimeColumn, QHeaderView::ResizeToContents);
        header->setSectionResizeMode(Column::BookmarkColumn,
                                     m_appearance.bookmarkVisible && !m_appearance.commentVisible
                                         ? QHeaderView::Stretch : QHeaderView::ResizeToContents);
        header->setSectionResizeMode(Column::CommentColumn, QHeaderView::Stretch);
    }
    updateBranchAppearance();
    m_kifu->viewport()->update();
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
    // 1手ごとに末尾へ行を足すので、追加した行だけを描き直す表を使う
    m_kifu = new IncrementalTableView(this);
    m_kifu->setObjectName(QStringLiteral("kifuTable"));
    m_kifu->setItemDelegate(new RecordHighlightDelegate(m_kifu));
    m_kifu->setSelectionMode(QAbstractItemView::SingleSelection);
    m_kifu->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_kifu->verticalHeader()->setVisible(false);
    const int rowHeight = m_kifu->fontMetrics().height() + 4;
    m_kifu->verticalHeader()->setDefaultSectionSize(rowHeight);
    m_kifu->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    m_kifu->setWordWrap(false);
    m_kifu->setTextElideMode(Qt::ElideRight);
    // 行が増えてスクロールが必要になっても、棋譜欄とウィンドウの横幅を変えない。
    m_kifu->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
}

void RecordPane::buildToolButtons()
{
    // --- 文字サイズ変更ボタン ---
    m_btnFontUp = new QPushButton(this);
    m_btnFontDown = new QPushButton(this);
    new FontSizeControl(m_btnFontDown, m_btnFontUp, &m_fontSize);

    // --- しおり編集ボタン ---
    m_btnBookmarkEdit = new QPushButton(this);
    m_btnBookmarkEdit->setObjectName(QStringLiteral("kifuBookmarkEdit"));
    m_btnBookmarkEdit->setIcon(QIcon(QStringLiteral(":/images/actions/actionEditBookmark.svg")));
    m_btnBookmarkEdit->setToolTip(tr("しおりを編集"));
    PanelButton::setup(m_btnBookmarkEdit);

    // --- 列表示トグルボタン ---

    m_btnToggleTime = new QPushButton(this);
    m_btnToggleTime->setObjectName(QStringLiteral("kifuToggleTime"));
    m_btnToggleTime->setCheckable(true);
    m_btnToggleTime->setChecked(m_appearance.timeVisible);
    m_btnToggleTime->setIcon(QIcon(QStringLiteral(":/images/actions/actionToggleTimeColumn.svg")));
    m_btnToggleTime->setToolTip(tr("消費時間列の表示/非表示"));
    PanelButton::setup(m_btnToggleTime);

    m_btnToggleBookmark = new QPushButton(this);
    m_btnToggleBookmark->setObjectName(QStringLiteral("kifuToggleBookmark"));
    m_btnToggleBookmark->setCheckable(true);
    m_btnToggleBookmark->setChecked(m_appearance.bookmarkVisible);
    m_btnToggleBookmark->setIcon(QIcon(QStringLiteral(":/images/actions/actionToggleBookmarkColumn.svg")));
    m_btnToggleBookmark->setToolTip(tr("しおり列の表示/非表示"));
    PanelButton::setup(m_btnToggleBookmark);

    m_btnToggleComment = new QPushButton(this);
    m_btnToggleComment->setObjectName(QStringLiteral("kifuToggleComment"));
    m_btnToggleComment->setCheckable(true);
    m_btnToggleComment->setChecked(m_appearance.commentVisible);
    m_btnToggleComment->setIcon(QIcon(QStringLiteral(":/images/actions/actionToggleCommentColumn.svg")));
    m_btnToggleComment->setToolTip(tr("コメント列の表示/非表示"));
    PanelButton::setup(m_btnToggleComment);
}

void RecordPane::buildNavigationPanel()
{
    m_stepButtons = new StepButtons(Qt::Vertical, QStringLiteral("kifuNav"), this);
    m_stepButtons->setObjectName(QStringLiteral("kifuNavigation"));

    // ナビゲーションボタンを縦に配置（ツール・トグルボタンも含む）
    auto* navLay = new QVBoxLayout;
    navLay->setContentsMargins(2, 4, 2, 4);
    navLay->setSpacing(3);
    navLay->addWidget(m_btnFontDown, 0, Qt::AlignHCenter);
    navLay->addWidget(m_btnFontUp, 0, Qt::AlignHCenter);
    navLay->addSpacing(8);
    navLay->addWidget(m_btnBookmarkEdit, 0, Qt::AlignHCenter);
    navLay->addWidget(m_btnToggleTime, 0, Qt::AlignHCenter);
    navLay->addWidget(m_btnToggleBookmark, 0, Qt::AlignHCenter);
    navLay->addWidget(m_btnToggleComment, 0, Qt::AlignHCenter);
    navLay->addSpacing(8);
    navLay->addWidget(m_stepButtons, 0, Qt::AlignHCenter);
    navLay->addStretch();

    m_navButtons = new QWidget(this);
    m_navButtons->setLayout(navLay);
    m_navButtons->setFixedWidth(50);
    m_navButtons->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
}

void RecordPane::buildBranchPanel()
{
    m_branch = new QTableView(this);
    m_branch->setObjectName(QStringLiteral("kifuBranchTable"));
    m_branch->setItemDelegate(new RecordHighlightDelegate(m_branch));
    m_branch->setSelectionMode(QAbstractItemView::SingleSelection);
    m_branch->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_branch->verticalHeader()->setVisible(false);
    const int branchRowHeight = m_branch->fontMetrics().height() + 4;
    m_branch->verticalHeader()->setDefaultSectionSize(branchRowHeight);
    m_branch->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    m_branch->setWordWrap(false);

    m_branchContainer = new QWidget(this);
    auto* branchLay = new QVBoxLayout(m_branchContainer);
    branchLay->setContentsMargins(0, 0, 0, 0);
    branchLay->setSpacing(0);
    branchLay->setAlignment(Qt::AlignTop);
    m_branch->horizontalHeader()->hide();
    m_branchToggle = new QToolButton(m_branchContainer);
    m_branchToggle->setObjectName(QStringLiteral("kifuBranchToggle"));
    m_branchToggle->setCheckable(true);
    m_branchToggle->setChecked(GameSettings::kifuBranchExpanded());
    m_branchToggle->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_branchToggle->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_branchToggle->setStyleSheet(QStringLiteral(
        "QToolButton { background: #f1f3f5; color: #46515c; border: none;"
        " border-bottom: 1px solid #d8dee5; padding: 2px; }"
        "QToolButton:hover { background: #e7f0f8; }"
        "QToolButton:focus { border: 1px solid #52799c; }"));
    branchLay->addWidget(m_branchToggle);
    branchLay->addWidget(m_branch, 1);
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
    connect(m_branchToggle, &QToolButton::toggled, this, &RecordPane::onBranchToggled);
    // 列の表示・内容・フォントとスクロールバーの変化に追従して余白を詰める。
    // ヘッダーの列幅計算が完了してから表とペインの幅を更新する。
    connect(m_kifu->horizontalHeader(), &QHeaderView::sectionResized,
            this, &RecordPane::updateKifuTableWidth, Qt::QueuedConnection);
    connect(m_kifu->horizontalHeader(), &QHeaderView::geometriesChanged,
            this, &RecordPane::updateKifuTableWidth, Qt::QueuedConnection);
    connect(m_kifu->verticalScrollBar(), &QScrollBar::rangeChanged,
            this, &RecordPane::updateKifuTableWidth, Qt::QueuedConnection);

    // 分岐候補欄: マウスクリックと活性化（Enter キー等）の両方でナビゲーションする。
    // シングルクリックで activated も発火する環境では同じ行が二重処理されるため、
    // スロット側で clicked 直後の activated を1周回だけ無視する。
    connect(m_branch, &QTableView::clicked, this, &RecordPane::onBranchClicked);
    m_branch->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_branch, &QWidget::customContextMenuRequested, this, &RecordPane::onBranchContextMenuRequested);
    connect(m_branch, &QTableView::activated, this, &RecordPane::onBranchActivated);

    // 文字サイズ変更ボタンの接続
    connect(&m_fontSize, &FontSizeHelper::fontSizeChanged, this, &RecordPane::onFontSizeChanged);

    // しおり編集ボタンの接続
    connect(m_btnBookmarkEdit, &QPushButton::clicked, this, &RecordPane::bookmarkEditRequested);

    // 列表示トグルボタンの接続
    connect(m_btnToggleTime, &QPushButton::toggled, this, &RecordPane::onToggleTimeColumn);
    connect(m_btnToggleBookmark, &QPushButton::toggled, this, &RecordPane::onToggleBookmarkColumn);
    connect(m_btnToggleComment, &QPushButton::toggled, this, &RecordPane::onToggleCommentColumn);

    // 棋譜表と分岐候補欄の選択ハイライトを揃える
    setupSelection(m_kifu);
    setupSelection(m_branch);
}

void RecordPane::updateKifuTableWidth()
{
    updateBranchAppearance();
    if (!m_kifu->model()) return;

    const auto* scrollBar = m_kifu->verticalScrollBar();
    const int scrollBarWidth = scrollBar->sizeHint().width();
    const auto* header = m_kifu->horizontalHeader();
    int tableWidth = scrollBarWidth + 2 * m_kifu->frameWidth();
    bool hasStretchColumn = false;
    for (int column = 0; column < header->count(); ++column) {
        if (m_kifu->isColumnHidden(column)) continue;
        if (header->sectionResizeMode(column) == QHeaderView::Stretch) {
            // 長いコメントでウィンドウを広げすぎず、見出しと閲覧用の幅を確保する。
            tableWidth += qMax(120, header->sectionSizeHint(column));
            hasStretchColumn = true;
        } else {
            tableWidth += header->sectionSize(column);
        }
    }

    const int paneWidth = tableWidth + m_navButtons->width() + m_branchContainer->width()
        + m_lr->handleWidth() * (m_lr->count() - 1);
    if (hasStretchColumn) {
        // しおり・コメントを広げる場合も指し手・消費時間の幅は確保する。
        m_kifu->setMaximumWidth(QWIDGETSIZE_MAX);
        m_kifu->setMinimumWidth(tableWidth);
        setMaximumWidth(QWIDGETSIZE_MAX);
        setMinimumWidth(paneWidth);
    } else {
        m_kifu->setFixedWidth(tableWidth);
        setFixedWidth(paneWidth);
    }
}

void RecordPane::setModels(KifuRecordListModel* recModel, KifuBranchListModel* brModel)
{
    // --- 棋譜テーブル（前のモデルの知らせは外す。選択モデルは付け替えで作り直される）---
    if (QAbstractItemModel* previous = m_kifu->model()) disconnect(previous, nullptr, this, nullptr);
    TableModelUtils::setModel(m_kifu, recModel);
    if (recModel) {
        // 行が足されたら末尾へ送る
        connect(recModel, &QAbstractItemModel::rowsInserted, this, &RecordPane::onKifuRowsInserted);
    }
    if (QItemSelectionModel* selection = m_kifu->selectionModel()) {
        connect(selection, &QItemSelectionModel::currentRowChanged, this, &RecordPane::onKifuCurrentRowChanged);
        // 行がある場合、最初の行を選択する
        if (recModel && recModel->rowCount() > 0) {
            selection->setCurrentIndex(recModel->index(0, 0),
                                       QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        }
    }
    applyAppearance();

    // --- 分岐テーブル ---
    TableModelUtils::setModel(m_branch, brModel);
    if (auto* hh2 = m_branch->horizontalHeader()) {
        hh2->setSectionResizeMode(0, QHeaderView::Stretch);
    }
}

QTableView* RecordPane::kifuView() const { return m_kifu; }
QTableView* RecordPane::branchView() const { return m_branch; }

void RecordPane::setKifuViewEnabled(bool on)
{
    qCDebug(lcUi) << "setKifuViewEnabled called, on=" << on;
    m_appearance.navigationEnabled = on;
    if (!on) {
        // 対局中は行を選ばない（選択の知らせは出さない）
        m_kifu->clearFocus();
        if (QItemSelectionModel* selection = m_kifu->selectionModel()) {
            const QSignalBlocker blocker(selection);
            selection->clearSelection();
            selection->setCurrentIndex(QModelIndex(), QItemSelectionModel::Clear);
        }
    }
    applyAppearance();
}

void RecordPane::onKifuRowsInserted(const QModelIndex& parent, int first, int last)
{
    if (!m_kifu) return;

    m_kifu->scrollToBottom();
    // 長い指し手が追加された場合も、内容に合わせて列とペインを広げる。
    // 列幅は全行の最大なので、末尾に足した行が今の幅に収まるなら測り直しても同じ幅になる。
    // 1手ごとに全行を測り直す（手数に比例して重くなる）のを避け、足した行だけ測って比べる。
    // 先頭からの追加（作り直し）は従来どおり全行を測る。
    if (first > 0 && m_kifu->model() && !m_kifu->horizontalHeader()->isSectionHidden(0)) {
        int added = 0;
        for (int row = first; row <= last; ++row)
            added = qMax(added, m_kifu->sizeHintForIndex(m_kifu->model()->index(row, 0, parent)).width());
        if (m_kifu->showGrid()) added += 1;
        if (added <= m_kifu->horizontalHeader()->sectionSize(0)) return;
    }
    m_kifu->resizeColumnToContents(0);
}

void RecordPane::onKifuCurrentRowChanged(const QModelIndex& cur, const QModelIndex&)
{
    const int row = cur.isValid() ? cur.row() : 0;
    qCDebug(lcUi).noquote() << "[RecordPane] onKifuCurrentRowChanged: emitting mainRowChanged(" << row << ")";
    emit mainRowChanged(row);
}

void RecordPane::onFontSizeChanged(int size)
{
    m_appearance.fontSize = size;
    applyAppearance();
}

void RecordPane::onToggleTimeColumn(bool checked)
{
    m_appearance.timeVisible = checked;
    GameSettings::kifuTimeColumnVisible.set(checked);
    applyAppearance();
}

void RecordPane::onToggleBookmarkColumn(bool checked)
{
    m_appearance.bookmarkVisible = checked;
    GameSettings::kifuBookmarkColumnVisible.set(checked);
    applyAppearance();
}

void RecordPane::onToggleCommentColumn(bool checked)
{
    m_appearance.commentVisible = checked;
    GameSettings::kifuCommentColumnVisible.set(checked);
    applyAppearance();
}

// ============================================================
// 棋譜欄の分岐候補の開閉と選択操作
// ============================================================

void RecordPane::onBranchToggled(bool expanded)
{
    GameSettings::kifuBranchExpanded.set(expanded);
    updateBranchAppearance();
    updateKifuTableWidth();
}

void RecordPane::updateBranchAppearance()
{
    if (!m_branchToggle) return;
    const bool expanded = m_branchToggle->isChecked();
    m_branchToggle->setFont(m_branch->font());
    m_branchToggle->setText(expanded ? tr("分岐候補") : QString());
    m_branchToggle->setAccessibleName(tr("分岐候補"));
    m_branchToggle->setToolTip(expanded ? tr("分岐候補を折りたたむ") : tr("分岐候補を展開する"));
    m_branchToggle->setArrowType(expanded ? Qt::LeftArrow : Qt::RightArrow);
    m_branchToggle->setFixedHeight(m_kifu->horizontalHeader()->sizeHint().height());
    m_branch->setVisible(expanded);
    const int expandedWidth = qMax(120, m_branchToggle->fontMetrics().horizontalAdvance(tr("分岐候補")) + 32);
    m_branchContainer->setFixedWidth(expanded ? expandedWidth : 28);
}

void RecordPane::onBranchClicked(const QModelIndex& index)
{
    m_lastClickedBranchIndex = index;
    m_branchClickGuard = true;
    QTimer::singleShot(0, this, &RecordPane::clearBranchClickGuard);
    if (index.isValid()) emit branchRowActivated(index.row());
}

void RecordPane::onBranchActivated(const QModelIndex& index)
{
    if (m_branchClickGuard && index.isValid() && index == m_lastClickedBranchIndex) {
        qCDebug(lcUi).noquote() << "[RecordPane] onBranchActivated: skipped (already handled by clicked) row=" << index.row();
        return;
    }
    if (index.isValid()) emit branchRowActivated(index.row());
}

void RecordPane::clearBranchClickGuard()
{
    m_branchClickGuard = false;
    m_lastClickedBranchIndex = QPersistentModelIndex();
}

void RecordPane::onBranchContextMenuRequested(const QPoint& pos)
{
    const QModelIndex index = m_branch->indexAt(pos);
    emit branchContextMenuRequested(index.isValid() ? index.row() : -1, m_branch->viewport()->mapToGlobal(pos));
}
