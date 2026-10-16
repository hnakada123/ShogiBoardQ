/// @file josekiwindow.cpp
/// @brief 定跡ウィンドウクラスの実装

#include "josekiwindow.h"
#include "recentfilesmenu.h"
#include "fontsizecontrol.h"
#include "buttonstyles.h"
#include "dialogutils.h"
#include "handicaps.h"
#include "josekibooksession.h"
#include "josekifilecontroller.h"
#include "josekimergedialog.h"
#include "josekimergeentries.h"
#include "josekimovedialog.h"
#include "josekisettings.h"
#include "josekitablemodel.h"
#include "kifumovedelegate.h"
#include "kifupresentation.h"
#include "logcategories.h"
#include "sfenutils.h"
#include "tablebuttondelegate.h"
#include "tablestyles.h"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QCloseEvent>
#include <QDir>
#include <QDockWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QLabel>
#include <QLocale>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QShowEvent>
#include <QTableView>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <utility>

namespace {
constexpr QSize kDefaultSize{950, 600};

QPushButton *makeToolButton(const QString &text, const QString &toolTip, const QString &style, QWidget *parent)
{
    auto *button = new QPushButton(text, parent);
    button->setToolTip(toolTip);
    button->setStyleSheet(style);
    return button;
}

QGroupBox *makeGroup(const QString &title, QWidget *parent, QHBoxLayout **layout)
{
    auto *group = new QGroupBox(title, parent);
    *layout = new QHBoxLayout(group);
    (*layout)->setContentsMargins(8, 4, 8, 4);
    (*layout)->setSpacing(4);
    return group;
}

/// 局面の要約に使う情報（手番・手数・開始局面の手合）
struct PositionInfo {
    bool blackToMove = true;
    int ply = 1;
    /// 1手目の平手・駒落ちの開始局面なら手合の名前。それ以外は空（任意の1手目の局面を「駒落ち」と呼ばない）
    QString handicap;
};

PositionInfo describePosition(const QString &sfen)
{
    PositionInfo info;
    const QStringList parts = sfen.split(QLatin1Char(' '));
    if (parts.size() >= 2) info.blackToMove = parts.at(1) == QStringLiteral("b");
    if (parts.size() >= 4) {
        bool ok = false;
        info.ply = parts.at(3).toInt(&ok);
        if (!ok) info.ply = 1;
    }
    if (info.ply == 1) {
        if (const Handicaps::Handicap *handicap = Handicaps::byBoard(sfen))
            info.handicap = Handicaps::labelOf(*handicap);
    }
    return info;
}
} // namespace

JosekiWindow::JosekiWindow(QWidget *parent)
    : QWidget(parent, Qt::Window)
    , m_session(new JosekiBookSession(this))
    , m_model(new JosekiTableModel(this))
    , m_fontHelper(JosekiSettings::josekiWindowFontSize, 6, 24, 1)
{
    m_files = new JosekiFileController({m_session, this}, this);
    setupUi();
    connect(m_session, &JosekiBookSession::modifiedChanged, this, &JosekiWindow::onModifiedChanged);
    connect(m_session, &JosekiBookSession::busyChanged, this, &JosekiWindow::onBusyChanged);
    connect(m_session, &JosekiBookSession::recentFilesChanged, this, &JosekiWindow::onRecentFilesChanged);
    connect(m_session, &JosekiBookSession::filePathChanged, this, &JosekiWindow::onFilePathChanged);
    connect(m_session, &JosekiBookSession::loaded, this, &JosekiWindow::onBookLoaded);
    connect(m_files, &JosekiFileController::bookSwitching, this, &JosekiWindow::closeMergeDialogs);
    connect(m_files, &JosekiFileController::statusMessage, this, &JosekiWindow::showTransientStatus);
    connect(m_recentFilesMenu, &RecentFilesMenu::fileTriggered, m_files, &JosekiFileController::openRecent);
    connect(m_recentFilesMenu, &RecentFilesMenu::clearTriggered, m_files, &JosekiFileController::clearRecentFiles);
    loadSettings();
}

JosekiWindow::~JosekiWindow() = default;

// ============================================================
// ウィンドウライフサイクル
// ============================================================

void JosekiWindow::setDockWidget(QDockWidget *dock)
{
    m_dockWidget = dock;
    if (m_dockWidget) m_dockWidget->installEventFilter(this);
}

bool JosekiWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_dockWidget && event->type() == QEvent::Close) {
        if (!confirmClose()) { event->ignore(); return true; }
    }
    return QWidget::eventFilter(obj, event);
}

void JosekiWindow::closeEvent(QCloseEvent *event)
{
    if (!confirmClose()) { event->ignore(); return; }
    QWidget::closeEvent(event);
}

void JosekiWindow::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    updateJosekiDisplay();
}

bool JosekiWindow::confirmClose()
{
    if (!m_files->confirmDiscardChanges()) return false;
    saveSettings();
    return true;
}

// ============================================================
// 構築
// ============================================================

QWidget *JosekiWindow::buildToolbar()
{
    auto *toolbar = new QWidget(this);
    auto *toolbarLayout = new QHBoxLayout(toolbar);
    toolbarLayout->setContentsMargins(0, 0, 0, 0);
    toolbarLayout->setSpacing(12);
    const QString buttonStyle = ButtonStyles::panelToolButton() + QStringLiteral("QPushButton { padding: 4px 8px; }");

    QHBoxLayout *fileGroupLayout = nullptr;
    QGroupBox *fileGroup = makeGroup(tr("ファイル"), this, &fileGroupLayout);
    m_newButton = makeToolButton(tr("新規"), tr("新しい空の定跡ファイルを作成"), buttonStyle, this);
    m_newButton->setIcon(QIcon(QStringLiteral(":/images/actions/actionNewGame.svg")));
    m_openButton = makeToolButton(tr("開く"), tr("定跡ファイル(.db)を開く"), buttonStyle, this);
    m_openButton->setIcon(QIcon(QStringLiteral(":/images/actions/actionOpenKifuFile.svg")));
    m_saveButton = makeToolButton(tr("保存"), tr("現在のファイルに上書き保存"), buttonStyle, this);
    m_saveButton->setIcon(QIcon(QStringLiteral(":/images/actions/actionSave.svg")));
    m_saveButton->setEnabled(false);
    m_saveAsButton = makeToolButton(tr("別名保存"), tr("別の名前で保存"), buttonStyle, this);
    m_saveAsButton->setIcon(QIcon(QStringLiteral(":/images/actions/actionSaveAs.svg")));
    m_recentButton = makeToolButton(tr("履歴"), tr("最近使ったファイルを開く"), buttonStyle, this);
    auto *recentMenu = new QMenu(this);
    recentMenu->setObjectName(QStringLiteral("josekiRecentMenu"));
    recentMenu->setProperty("automationMenu", true);
    m_recentButton->setMenu(recentMenu);
    m_recentFilesMenu = new RecentFilesMenu(recentMenu);
    for (auto *button : {m_newButton, m_openButton, m_saveButton, m_saveAsButton, m_recentButton})
        fileGroupLayout->addWidget(button);
    toolbarLayout->addWidget(fileGroup, 0, Qt::AlignLeft);

    QHBoxLayout *displayGroupLayout = nullptr;
    QGroupBox *displayGroup = makeGroup(tr("表示"), this, &displayGroupLayout);
    auto *fontDecrease = new QPushButton(this);
    fontDecrease->setObjectName(QStringLiteral("josekiFontDecrease"));
    auto *fontIncrease = new QPushButton(this);
    fontIncrease->setObjectName(QStringLiteral("josekiFontIncrease"));
    new FontSizeControl(fontDecrease, fontIncrease, &m_fontHelper);
    m_autoLoadCheckBox = new QCheckBox(tr("自動読込"), this);
    m_autoLoadCheckBox->setToolTip(tr("定跡ウィンドウ表示時に前回のファイルを自動で読み込む"));
    m_autoLoadCheckBox->setChecked(true);
    displayGroupLayout->addWidget(fontDecrease);
    displayGroupLayout->addWidget(fontIncrease);
    displayGroupLayout->addWidget(m_autoLoadCheckBox);
    toolbarLayout->addWidget(displayGroup, 0, Qt::AlignLeft);
    toolbarLayout->addStretch();

    QHBoxLayout *operationGroupLayout = nullptr;
    QGroupBox *operationGroup = makeGroup(tr("操作"), this, &operationGroupLayout);
    m_addMoveButton = makeToolButton(tr("＋追加"), tr("現在の局面に定跡手を追加"), buttonStyle, this);
    m_mergeButton = new QToolButton(this);
    m_mergeButton->setText(tr("マージ ▼"));
    m_mergeButton->setToolTip(tr("棋譜から定跡をマージ"));
    m_mergeButton->setPopupMode(QToolButton::InstantPopup);
    // 表示文字に ▼ を含むので、Qt のメニュー矢印は重ねて描かない
    m_mergeButton->setStyleSheet(buttonStyle + QStringLiteral("QToolButton::menu-indicator { image: none; width: 0px; }"));
    auto *mergeMenu = new QMenu(this);
    mergeMenu->setObjectName(QStringLiteral("josekiMergeMenu"));
    mergeMenu->setProperty("automationMenu", true);
    mergeMenu->addAction(tr("現在の棋譜から"), this, &JosekiWindow::onMergeFromCurrentKifu);
    mergeMenu->addAction(tr("棋譜ファイルから"), this, &JosekiWindow::onMergeFromKifuFile);
    m_mergeButton->setMenu(mergeMenu);
    m_stopButton = makeToolButton(tr("■ 停止"), tr("定跡表示を停止/再開"), buttonStyle, this);
    m_stopButton->setCheckable(true);
    operationGroupLayout->addWidget(m_addMoveButton);
    operationGroupLayout->addWidget(m_mergeButton);
    operationGroupLayout->addWidget(m_stopButton);
    toolbarLayout->addWidget(operationGroup, 0, Qt::AlignRight);

    connect(m_newButton, &QPushButton::clicked, m_files, &JosekiFileController::newBook);
    connect(m_openButton, &QPushButton::clicked, m_files, &JosekiFileController::open);
    connect(m_saveButton, &QPushButton::clicked, m_files, &JosekiFileController::save);
    connect(m_saveAsButton, &QPushButton::clicked, m_files, &JosekiFileController::saveAs);
    connect(m_addMoveButton, &QPushButton::clicked, this, &JosekiWindow::onAddMoveButtonClicked);
    connect(&m_fontHelper, &FontSizeHelper::fontSizeChanged, this, &JosekiWindow::applyFontSize);
    connect(m_autoLoadCheckBox, &QCheckBox::checkStateChanged, this, &JosekiWindow::onAutoLoadCheckBoxChanged);
    connect(m_stopButton, &QPushButton::clicked, this, &JosekiWindow::onStopButtonClicked);
    return toolbar;
}

void JosekiWindow::setupUi()
{
    setWindowTitle(tr("定跡ウィンドウ"));
    resize(kDefaultSize);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(6);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->addWidget(buildToolbar());

    // --- ファイルと局面の情報 ---
    auto *fileInfoLayout = new QHBoxLayout();
    fileInfoLayout->addWidget(new QLabel(tr("ファイル:"), this));
    m_filePathLabel = new QLabel(tr("未選択"), this);
    m_filePathLabel->setStyleSheet(QStringLiteral("color: gray;"));
    m_filePathLabel->setTextFormat(Qt::PlainText);
    m_filePathLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    m_filePathLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    fileInfoLayout->addWidget(m_filePathLabel, 1);
    m_fileStatusLabel = new QLabel(this);
    m_fileStatusLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    fileInfoLayout->addWidget(m_fileStatusLabel);
    mainLayout->addLayout(fileInfoLayout);

    auto *positionInfoLayout = new QHBoxLayout();
    positionInfoLayout->addWidget(new QLabel(tr("局面:"), this));
    m_positionSummaryLabel = new QLabel(tr("(未設定)"), this);
    m_positionSummaryLabel->setStyleSheet(QStringLiteral("color: #0066cc; font-weight: bold;"));
    positionInfoLayout->addWidget(m_positionSummaryLabel);
    m_showSfenDetailBtn = new QPushButton(tr("詳細"), this);
    m_showSfenDetailBtn->setToolTip(tr("SFENの詳細を表示/非表示"));
    m_showSfenDetailBtn->setCheckable(true);
    m_showSfenDetailBtn->setFixedWidth(50);
    positionInfoLayout->addWidget(m_showSfenDetailBtn);
    positionInfoLayout->addStretch();
    mainLayout->addLayout(positionInfoLayout);

    m_sfenDetailWidget = new QWidget(this);
    auto *sfenDetailLayout = new QVBoxLayout(m_sfenDetailWidget);
    sfenDetailLayout->setContentsMargins(20, 0, 0, 0);
    sfenDetailLayout->setSpacing(2);
    m_currentSfenLabel = new QLabel(this);
    m_sfenLineLabel = new QLabel(this);
    for (QLabel *label : {m_currentSfenLabel, m_sfenLineLabel}) {
        label->setTextInteractionFlags(Qt::TextSelectableByMouse);
        label->setWordWrap(true);
        sfenDetailLayout->addWidget(label);
    }
    m_sfenDetailWidget->setVisible(false);
    mainLayout->addWidget(m_sfenDetailWidget);

    // --- 定跡手の表（「着手」「編集」「削除」はボタンとして描き、押した列で処理を決める） ---
    m_tableView = new QTableView(this);
    m_tableView->setObjectName(QStringLiteral("josekiTable"));
    m_tableView->setModel(m_model);
    auto *moveDelegate = new KifuMoveDelegate(m_tableView);
    m_tableView->setItemDelegateForColumn(JosekiTableModel::Move, moveDelegate);
    m_tableView->setItemDelegateForColumn(JosekiTableModel::NextMove, moveDelegate);
    auto *buttonDelegate = new TableButtonDelegate(m_tableView);
    for (int column : {JosekiTableModel::Play, JosekiTableModel::Edit, JosekiTableModel::Delete})
        m_tableView->setItemDelegateForColumn(column, buttonDelegate);
    m_tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tableView->setAlternatingRowColors(true);
    m_tableView->setShowGrid(false);
    m_tableView->setWordWrap(false);
    m_tableView->setMouseTracking(true);
    m_tableView->verticalHeader()->setVisible(false);
    m_tableView->setContextMenuPolicy(Qt::CustomContextMenu);
    const int defaultWidths[] = {40, 50, 120, 120, 50, 50, 70, 50, 80};
    for (int col = 0; col < 9; ++col) m_tableView->setColumnWidth(col, defaultWidths[col]);
    m_tableView->horizontalHeader()->setStretchLastSection(true);
    mainLayout->addWidget(m_tableView, 1);

    m_emptyGuideLabel = new QLabel(m_tableView->viewport());
    m_emptyGuideLabel->setWordWrap(true);
    m_emptyGuideLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    auto *emptyLayout = new QVBoxLayout(m_tableView->viewport());
    emptyLayout->addWidget(m_emptyGuideLabel);
    m_emptyGuideLabel->setAlignment(Qt::AlignCenter);
    m_emptyGuideLabel->setVisible(false);

    // --- 状態表示 ---
    auto *statusFrame = new QFrame(this);
    statusFrame->setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
    auto *statusLayout = new QHBoxLayout(statusFrame);
    statusLayout->setContentsMargins(8, 2, 8, 2);
    m_statusLabel = new QLabel(this);
    statusLayout->addWidget(m_statusLabel, 1);
    mainLayout->addWidget(statusFrame);

    m_noticeLabel = new QLabel(tr("※ 追加・編集・削除後は「保存」ボタンで定跡ファイルに保存してください"), this);
    mainLayout->addWidget(m_noticeLabel);

    // --- 表のコンテキストメニュー ---
    m_tableContextMenu = new QMenu(this);
    m_actionPlay = m_tableContextMenu->addAction(tr("着手"));
    m_tableContextMenu->addSeparator();
    m_actionEdit = m_tableContextMenu->addAction(tr("編集..."));
    m_actionDelete = m_tableContextMenu->addAction(tr("削除"));
    m_tableContextMenu->addSeparator();
    QAction *actionCopyMove = m_tableContextMenu->addAction(tr("指し手をコピー"));

    connect(m_showSfenDetailBtn, &QPushButton::toggled, this, &JosekiWindow::onSfenDetailToggled);
    connect(m_tableView, &QTableView::clicked, this, &JosekiWindow::onTableClicked);
    connect(m_tableView, &QTableView::doubleClicked, this, &JosekiWindow::onTableDoubleClicked);
    connect(m_tableView, &QTableView::customContextMenuRequested, this, &JosekiWindow::onTableContextMenu);
    connect(m_actionPlay, &QAction::triggered, this, &JosekiWindow::onContextMenuPlay);
    connect(m_actionEdit, &QAction::triggered, this, &JosekiWindow::onContextMenuEdit);
    connect(m_actionDelete, &QAction::triggered, this, &JosekiWindow::onContextMenuDelete);
    connect(actionCopyMove, &QAction::triggered, this, &JosekiWindow::onContextMenuCopyMove);

    updateRecentFilesMenu();
    updateStatusDisplay();
    updateWindowTitle();
}

void JosekiWindow::applyFontSize()
{
    // メニュー（履歴・マージ・表の右クリック）も DialogUtils が同じ文字にする
    const QFont font = m_fontHelper.applyTo(this);
    const int fontSize = m_fontHelper.fontSize();
    const QFontMetrics fm(font);

    QHeaderView *header = m_tableView->horizontalHeader();
    header->setFont(font);
    header->setFixedHeight(fm.height() + 12);
    m_tableView->verticalHeader()->setDefaultSectionSize(fm.height() + 12);
    m_tableView->setStyleSheet(TableStyles::thinking(fontSize) + QStringLiteral(
        "QTableView { alternate-background-color: #f7f9fb; }"
        "QTableView::item { padding: 3px 6px; }"));
    for (int col = 0; col < JosekiTableModel::ColumnCount - 1; ++col) {
        QString sample = m_model->headerData(col, Qt::Horizontal).toString();
        if (col == JosekiTableModel::Move || col == JosekiTableModel::NextMove) sample = QStringLiteral("▲７六歩(77)");
        if (col == JosekiTableModel::Frequency) sample = QStringLiteral("999,999,999");
        m_tableView->setColumnWidth(col, qMax(m_tableView->columnWidth(col), fm.horizontalAdvance(sample) + 24));
    }
    header->setStretchLastSection(true);
    m_showSfenDetailBtn->setFixedWidth(fm.horizontalAdvance(m_showSfenDetailBtn->text()) + 20);

    // 注意書きと SFEN の詳細は1段小さい文字で出す
    const int smallSize = qMax(fontSize - 1, 6);
    m_noticeLabel->setStyleSheet(QStringLiteral("color: #cc6600; font-size: %1pt;").arg(smallSize));
    m_currentSfenLabel->setStyleSheet(QStringLiteral("color: #0066cc; font-size: %1pt;").arg(smallSize));
    m_sfenLineLabel->setStyleSheet(QStringLiteral("color: #228b22; font-size: %1pt;").arg(smallSize));
    updateJosekiDisplay();
}

// ============================================================
// 設定
// ============================================================

void JosekiWindow::loadSettings()
{
    applyFontSize();
    DialogUtils::restoreDialogSize(this, JosekiSettings::josekiWindowSize());
    const auto widths = JosekiSettings::josekiWindowColumnWidths();
    for (int col = 0; col < widths.size() && col < JosekiTableModel::ColumnCount - 1; ++col) {
        if (widths.at(col) > 0) m_tableView->setColumnWidth(col, widths.at(col));
    }
    m_autoLoadEnabled = JosekiSettings::josekiWindowAutoLoadEnabled();
    m_autoLoadCheckBox->setChecked(m_autoLoadEnabled);
    m_session->setRecentFiles(JosekiSettings::josekiWindowRecentFiles());
    m_displayEnabled = JosekiSettings::josekiWindowDisplayEnabled();
    if (!m_displayEnabled) {
        m_stopButton->setChecked(true);
        m_stopButton->setText(tr("▶ 再開"));
    }
    const bool sfenDetailVisible = JosekiSettings::josekiWindowSfenDetailVisible();
    m_showSfenDetailBtn->setChecked(sfenDetailVisible);
    m_sfenDetailWidget->setVisible(sfenDetailVisible);
    const QString lastFilePath = JosekiSettings::josekiWindowLastFilePath();
    if (m_autoLoadEnabled && !lastFilePath.isEmpty() && QFileInfo::exists(lastFilePath)) {
        m_pendingAutoLoadPath = lastFilePath;
        m_filePathLabel->setText(lastFilePath + tr(" (未読込)"));
    }
}

void JosekiWindow::saveSettings()
{
    DialogUtils::saveDialogSize(this, JosekiSettings::josekiWindowSize);
    // 「履歴をクリア」した後は、開き直すか保存して履歴に戻るまで前回のファイルとして記録しない
    JosekiSettings::josekiWindowLastFilePath.set(m_session->lastFilePathForSettings());
    JosekiSettings::josekiWindowAutoLoadEnabled.set(m_autoLoadEnabled);
    JosekiSettings::josekiWindowRecentFiles.set(m_session->recentFiles());
    QList<int> widths;
    for (int i = 0; i < JosekiTableModel::ColumnCount; ++i) widths.append(m_tableView->columnWidth(i));
    JosekiSettings::josekiWindowColumnWidths.set(widths);
}

// ============================================================
// 開いている定跡の変化
// ============================================================

void JosekiWindow::onFilePathChanged()
{
    showFilePath();
    updateWindowTitle();
    updateJosekiDisplay();
}

void JosekiWindow::onBookLoaded()
{
    updateJosekiDisplay();
}

void JosekiWindow::onRecentFilesChanged()
{
    updateRecentFilesMenu();
    saveSettings();
}

void JosekiWindow::onBusyChanged(bool busy)
{
    m_openButton->setEnabled(!busy);
    m_newButton->setEnabled(!busy);
    m_saveButton->setEnabled(!busy && m_session->isModified());
    m_saveAsButton->setEnabled(!busy);
    m_recentButton->setEnabled(!busy);
    m_mergeButton->setEnabled(!busy);
    m_tableView->setEnabled(!busy);
    m_actionEdit->setEnabled(!busy);
    m_actionDelete->setEnabled(!busy);
    const auto dialogs = findChildren<JosekiMergeDialog *>();
    for (JosekiMergeDialog *dialog : dialogs) dialog->setEnabled(!busy);
    updateStatusDisplay();
}

void JosekiWindow::onModifiedChanged(bool modified)
{
    m_saveButton->setEnabled(!isIoBusy() && modified);
    updateWindowTitle();
    if (m_dockWidget) m_dockWidget->setWindowTitle(modified ? tr("定跡") + QStringLiteral(" *") : tr("定跡"));
    updateStatusDisplay();
}

void JosekiWindow::updateRecentFilesMenu()
{
    m_recentFilesMenu->setFiles(m_session->recentFiles());
}

// ============================================================
// 局面と着手の可否
// ============================================================

void JosekiWindow::setCurrentSfen(const QString &sfen)
{
    QString next = sfen;
    next.remove(QLatin1Char('\r'));
    if (next == QStringLiteral("startpos"))
        next = SfenUtils::hirateSfen();
    const bool sameSfen = (next == m_currentSfen);
    m_currentSfen = next;

    if (m_currentSfenLabel)
        m_currentSfenLabel->setText(m_currentSfen.isEmpty() ? tr("(未設定)") : m_currentSfen);

    // 1手ごとに複数の経路から同じ局面が届く。表に出している局面と同じで、人間が指せるかも
    // 変わらなければ作り直さない（定跡データの変更は、変更した処理が自分で表示を更新する）
    if (sameSfen && m_pendingAutoLoadPath.isEmpty() && m_displayedSfen == m_currentSfen) {
        const bool couldPlay = m_humanCanPlay;
        refreshHumanCanPlay();
        if (m_humanCanPlay == couldPlay) return;
    }

    updateJosekiDisplay();
}

void JosekiWindow::setHumanCanPlayProvider(std::function<bool()> provider)
{
    m_humanCanPlayProvider = std::move(provider);
}

void JosekiWindow::refreshHumanCanPlay()
{
    if (m_humanCanPlayProvider) m_humanCanPlay = m_humanCanPlayProvider();
}

void JosekiWindow::setHumanCanPlay(bool canPlay)
{
    if (m_humanCanPlay != canPlay) { m_humanCanPlay = canPlay; updateJosekiDisplay(); }
}

void JosekiWindow::showMoveFailed(const QString &usiMove)
{
    QMessageBox::warning(this, tr("着手エラー"),
        tr("定跡手「%1」を指すことができませんでした。\n\n"
           "この定跡手は現在の局面では合法手ではない可能性があります。\n"
           "定跡ファイルのデータに誤りがある可能性があります。").arg(usiMove));
}

// ============================================================
// 定跡手の操作
// ============================================================

bool JosekiWindow::isIoBusy() const
{
    return m_session->isBusy();
}

void JosekiWindow::playMoveAt(int row)
{
    if (row < 0 || row >= m_model->moves().size()) return;
    refreshHumanCanPlay();
    if (!m_humanCanPlay) {
        QMessageBox::information(this, tr("情報"), tr("定跡手は、対局中の自分の手番か、対局していないときに指せます。"));
        return;
    }
    emit josekiMoveSelected(m_model->moves().at(row).move);
}

void JosekiWindow::editMoveAt(int row)
{
    if (isIoBusy() || row < 0 || row >= m_model->moves().size()) return;

    const JosekiMove current = m_model->moves().at(row);
    JosekiMoveDialog dialog(this, true);
    dialog.setMove(current.move);
    dialog.setNextMove(current.nextMove);
    dialog.setCurrentSfen(m_currentSfen);
    dialog.setValue(current.value);
    dialog.setDepth(current.depth);
    dialog.setFrequency(current.frequency);
    dialog.setComment(current.comment);
    dialog.setEditMoveDisplay(JosekiTableModel::moveText(current.move, m_currentSfen));
    if (dialog.exec() != QDialog::Accepted) return;

    const JosekiMove updated{current.move, dialog.nextMove(), dialog.value(),
                             dialog.depth(), dialog.frequency(), dialog.comment()};
    if (m_session->updateMove(m_currentSfen, updated)) updateJosekiDisplay();
}

void JosekiWindow::deleteMoveAt(int row)
{
    if (isIoBusy() || row < 0 || row >= m_model->moves().size()) return;

    const QString usiMove = m_model->moves().at(row).move;
    if (!DialogUtils::confirmAction(this, tr("削除確認"),
                                    tr("定跡手「%1」を削除しますか？")
                                        .arg(JosekiTableModel::moveText(usiMove, m_currentSfen)),
                                    tr("削除する"))) {
        return;
    }
    if (m_session->removeMove(m_currentSfen, usiMove)) updateJosekiDisplay();
}

void JosekiWindow::onAddMoveButtonClicked()
{
    if (isIoBusy()) return;
    if (m_currentSfen.isEmpty()) {
        QMessageBox::warning(this, tr("定跡手追加"),
            tr("局面が設定されていません。\n将棋盤で局面を表示してから定跡手を追加してください。"));
        return;
    }

    // ダイアログを開いている間に局面が進んでも、開いたときの局面に追加する
    const QString targetSfen = m_currentSfen;
    JosekiMoveDialog dialog(this, false);
    dialog.setCurrentSfen(targetSfen);
    if (dialog.exec() != QDialog::Accepted) return;

    const JosekiMove newMove{dialog.move(), dialog.nextMove(), dialog.value(),
                             dialog.depth(), dialog.frequency(), dialog.comment()};
    if (m_session->hasMove(targetSfen, newMove.move)
        && !DialogUtils::confirmAction(this, tr("確認"),
                tr("指し手「%1」は既に登録されています。\n上書きしますか？")
                    .arg(JosekiTableModel::moveText(newMove.move, targetSfen)),
                tr("上書きする"))) {
        return;
    }
    m_session->addMove(targetSfen, newMove);
    updateJosekiDisplay();
}

void JosekiWindow::onTableClicked(const QModelIndex &index)
{
    if (!index.isValid()) return;
    switch (index.column()) {
    case JosekiTableModel::Play:
        // 人間が指せないときは「着手」のボタンを描かない
        if (m_model->humanCanPlay()) playMoveAt(index.row());
        break;
    case JosekiTableModel::Edit: editMoveAt(index.row()); break;
    case JosekiTableModel::Delete: deleteMoveAt(index.row()); break;
    default: break;
    }
}

void JosekiWindow::onTableDoubleClicked(const QModelIndex &index)
{
    // ボタンの列は1回目のクリックで処理済み
    const int column = index.column();
    if (!index.isValid() || column == JosekiTableModel::Play || column == JosekiTableModel::Edit
        || column == JosekiTableModel::Delete) {
        return;
    }
    playMoveAt(index.row());
}

void JosekiWindow::onTableContextMenu(const QPoint &pos)
{
    if (isIoBusy()) return;
    const QModelIndex index = m_tableView->indexAt(pos);
    if (!index.isValid()) return;
    refreshHumanCanPlay();
    m_actionPlay->setEnabled(m_humanCanPlay);
    m_tableView->selectRow(index.row());
    m_tableContextMenu->exec(m_tableView->viewport()->mapToGlobal(pos));
}

void JosekiWindow::onContextMenuPlay() { playMoveAt(m_tableView->currentIndex().row()); }
void JosekiWindow::onContextMenuEdit() { editMoveAt(m_tableView->currentIndex().row()); }
void JosekiWindow::onContextMenuDelete() { deleteMoveAt(m_tableView->currentIndex().row()); }

void JosekiWindow::onContextMenuCopyMove()
{
    const int row = m_tableView->currentIndex().row();
    if (row < 0 || row >= m_model->moves().size()) return;
    const QString usiMove = m_model->moves().at(row).move;
    QApplication::clipboard()->setText(usiMove);
    showTransientStatus(tr("「%1」をコピーしました").arg(usiMove));
}

// ============================================================
// 表示の設定
// ============================================================

void JosekiWindow::onAutoLoadCheckBoxChanged(Qt::CheckState state)
{
    m_autoLoadEnabled = (state == Qt::Checked);
    JosekiSettings::josekiWindowAutoLoadEnabled.set(m_autoLoadEnabled);
}

void JosekiWindow::onStopButtonClicked()
{
    m_displayEnabled = !m_stopButton->isChecked();
    m_stopButton->setText(m_displayEnabled ? tr("■ 停止") : tr("▶ 再開"));
    if (m_displayEnabled) updateJosekiDisplay();
    else clearTable();
    JosekiSettings::josekiWindowDisplayEnabled.set(m_displayEnabled);
    updateStatusDisplay();
}

void JosekiWindow::onSfenDetailToggled(bool checked)
{
    m_sfenDetailWidget->setVisible(checked);
    JosekiSettings::josekiWindowSfenDetailVisible.set(checked);
}

void JosekiWindow::onRestoreStatusDisplay() { updateStatusDisplay(); }

void JosekiWindow::showTransientStatus(const QString &text)
{
    m_statusLabel->setText(text);
    QTimer::singleShot(2000, this, &JosekiWindow::onRestoreStatusDisplay);
}

// ============================================================
// 定跡手の表と状態表示
// ============================================================

void JosekiWindow::updateJosekiDisplay()
{
    refreshHumanCanPlay();

    if (!m_pendingAutoLoadPath.isEmpty()) {
        const QString pathToLoad = std::exchange(m_pendingAutoLoadPath, QString());
        qCDebug(lcUi) << "Performing deferred auto-load:" << pathToLoad;
        m_files->openFile(pathToLoad);
        return;
    }

    // 局面が変わるたび（1手ごと）に呼ばれる。表の行数を合わせて中身を入れ替え（列幅は保つ）、
    // 状態表示（ラベルのスタイルと案内の表示）は最後に1回だけ更新する
    updatePositionSummary();
    m_displayedSfen = m_currentSfen;

    QList<JosekiMove> moves;
    QString sfenLine;
    if (m_displayEnabled && !m_currentSfen.isEmpty()) {
        if (m_session->hasPosition(m_currentSfen)) {
            moves = m_session->movesAt(m_currentSfen);
            const QString registered = m_session->registeredSfen(m_currentSfen);
            sfenLine = tr("定跡SFEN: %1")
                           .arg(registered.isEmpty() ? JosekiBookSession::positionKey(m_currentSfen) : registered);
        } else {
            sfenLine = tr("定跡: (該当なし)");
        }
    }
    m_sfenLineLabel->setText(sfenLine);
    // 選択と表示位置は初めに戻す
    m_tableView->setCurrentIndex(QModelIndex());
    m_tableView->clearSelection();
    m_model->setMoves(m_currentSfen, moves, m_humanCanPlay);
    if (!moves.isEmpty()) m_tableView->scrollToTop();
    updateStatusDisplay();
}

void JosekiWindow::clearTable()
{
    m_model->setMoves(QString(), {}, m_humanCanPlay);
    m_displayedSfen.clear();
}

void JosekiWindow::updateStatusDisplay()
{
    const QString &filePath = m_session->filePath();
    const bool modified = m_session->isModified();
    const bool hasData = !m_session->isEmpty();
    m_addMoveButton->setEnabled(!isIoBusy() && !m_currentSfen.isEmpty());
    if (m_filePathLabel->toolTip() != filePath) m_filePathLabel->setToolTip(filePath);
    m_noticeLabel->setVisible(modified);

    QString text;
    QString style;
    if (modified) {
        text = tr("未保存");
        style = QStringLiteral("color: #cc6600; font-weight: bold;");
    } else if (hasData) {
        text = tr("✓読込済");
        style = QStringLiteral("color: green; font-weight: bold;");
    }
    m_fileStatusLabel->setText(text);
    // 同じスタイルシートでも付け直すと Qt はスタイルを適用し直し、配置の計算が
    // メインウィンドウまで伝わる。1手ごとに呼ばれるので、変わったときだけ付ける
    if (m_fileStatusLabel->styleSheet() != style) m_fileStatusLabel->setStyleSheet(style);
    m_stopButton->setVisible(hasData || !m_displayEnabled);

    const int moveCount = static_cast<int>(m_model->moves().size());
    QStringList statusParts;
    statusParts << (filePath.isEmpty() ? tr("ファイル: 未選択") : tr("ファイル: %1").arg(QFileInfo(filePath).fileName()));
    statusParts << tr("局面数: %1").arg(QLocale().toString(m_session->positionCount()));
    statusParts << (m_displayEnabled ? tr("定跡: %1件").arg(moveCount) : tr("【停止中】"));
    m_statusLabel->setText(statusParts.join(QStringLiteral("  |  ")));

    const bool showGuide = moveCount == 0;
    if (!m_displayEnabled)
        m_emptyGuideLabel->setText(tr("定跡表示を停止しています。「再開」で表示を再開します。"));
    else if (m_currentSfen.isEmpty())
        m_emptyGuideLabel->setText(tr("将棋盤で局面を表示すると、その局面の定跡を確認できます。"));
    else if (filePath.isEmpty() && !hasData)
        m_emptyGuideLabel->setText(tr("「開く」で定跡ファイルを読み込むと、表示中の局面の定跡手が表示されます。\n"
                                    "「新規」で新しい定跡ファイルを作ることもできます。"));
    else
        m_emptyGuideLabel->setText(tr("この局面には定跡が登録されていません。\n"
                                    "「＋追加」で指し手を登録するか、「マージ」から棋譜を取り込めます。"));
    m_emptyGuideLabel->setVisible(showGuide);
    if (showGuide) m_emptyGuideLabel->raise();
}

void JosekiWindow::updatePositionSummary()
{
    if (m_currentSfen.isEmpty()) {
        m_positionSummaryLabel->setText(tr("(未設定)"));
        m_currentSfenLabel->setText(QString());
        return;
    }
    const PositionInfo info = describePosition(m_currentSfen);
    const QString turn = info.blackToMove ? tr("先手") : tr("後手");
    // 平手・駒落ちの開始局面は手合の名前、それ以外は手数で示す（任意の局面を「駒落ち」と呼ばない）
    QString positionDesc;
    if (info.handicap == QStringLiteral("平手"))
        positionDesc = tr("初期配置");
    else if (!info.handicap.isEmpty())
        positionDesc = KifuPresentation::infoValue(QStringLiteral("手合割"), info.handicap);
    else
        positionDesc = tr("%1手目").arg(info.ply);
    m_positionSummaryLabel->setText(tr("%1 (%2番)").arg(positionDesc, turn));
    m_positionSummaryLabel->setToolTip(m_currentSfen);
    m_currentSfenLabel->setText(tr("局面SFEN: %1").arg(m_currentSfen));
}

void JosekiWindow::updateWindowTitle()
{
    QString title = tr("定跡ウィンドウ");
    if (!m_session->filePath().isEmpty())
        title = QFileInfo(m_session->filePath()).fileName() + QStringLiteral(" - ") + title;
    if (m_session->isModified())
        title = QStringLiteral("* ") + title;
    setWindowTitle(title);
}

void JosekiWindow::showFilePath()
{
    const QString &filePath = m_session->filePath();
    if (filePath.isEmpty()) {
        m_filePathLabel->setText(tr("新規ファイル（未保存）"));
        m_filePathLabel->setStyleSheet(QStringLiteral("color: blue;"));
        return;
    }
    m_filePathLabel->setText(filePath);
    m_filePathLabel->setStyleSheet(QString());
}

// ============================================================
// 棋譜からのマージ
// ============================================================

void JosekiWindow::onMergeFromCurrentKifu()
{
    if (isIoBusy() || !m_files->ensureFilePath()) return;
    emit requestKifuDataForMerge();
}

void JosekiWindow::setKifuDataForMerge(const QStringList &sfenList, const QStringList &moveList,
                                       const QStringList &japaneseMoveList, int currentPly)
{
    if (isIoBusy()) return;
    if (moveList.isEmpty()) {
        QMessageBox::information(this, tr("情報"), tr("棋譜に指し手がありません。"));
        return;
    }
    openMergeDialog(JosekiMergeEntries::fromLine(sfenList, moveList, japaneseMoveList, currentPly), QString());
}

void JosekiWindow::onMergeFromKifuFile()
{
    if (isIoBusy() || !m_files->ensureFilePath()) return;

    const QString kifFilePath = QFileDialog::getOpenFileName(
        this, tr("棋譜ファイルを選択"), QDir::homePath(),
        tr("KIF形式 (*.kif *.kifu);;すべてのファイル (*)"));
    if (kifFilePath.isEmpty()) return;

    QList<KifuMergeEntry> entries;
    QString errorMessage;
    if (!JosekiMergeEntries::fromKifFile(kifFilePath, entries, &errorMessage)) {
        if (errorMessage.isEmpty())
            QMessageBox::information(this, tr("情報"), tr("棋譜に指し手がありません。"));
        else
            QMessageBox::warning(this, tr("エラー"), tr("棋譜ファイルの読み込みに失敗しました。\n%1").arg(errorMessage));
        return;
    }
    openMergeDialog(entries, tr("棋譜から定跡にマージ - %1").arg(QFileInfo(kifFilePath).fileName()));
}

void JosekiWindow::openMergeDialog(const QList<KifuMergeEntry> &entries, const QString &title)
{
    if (entries.isEmpty()) {
        QMessageBox::information(this, tr("情報"), tr("登録可能な指し手がありません。"));
        return;
    }
    auto *dialog = new JosekiMergeDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setTargetJosekiFile(m_session->filePath());
    dialog->setRegisteredMoves(m_session->mergeRegisteredMoves());
    dialog->setRegistrar(std::bind(&JosekiWindow::registerMergeEntries, this, std::placeholders::_1));
    if (!title.isEmpty()) dialog->setWindowTitle(title);
    dialog->setKifuData(entries);
    dialog->show();
}

bool JosekiWindow::registerMergeEntries(const QList<KifuMergeEntry> &entries)
{
    if (isIoBusy()) return false;
    QString errorMessage;
    const bool registered = m_session->registerMergeMoves(entries, &errorMessage);
    if (!registered) QMessageBox::warning(this, tr("エラー"), errorMessage);
    updateJosekiDisplay();
    return registered;
}

void JosekiWindow::closeMergeDialogs()
{
    // 別のファイルに替えた後は、古いダイアログからは登録させない
    const auto dialogs = findChildren<JosekiMergeDialog *>();
    for (JosekiMergeDialog *dialog : dialogs) {
        dialog->setRegistrar({});
        dialog->setEnabled(false);
        dialog->reject();
    }
}
