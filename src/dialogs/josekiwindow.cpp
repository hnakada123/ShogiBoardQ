/// @file josekiwindow.cpp
/// @brief 定跡ウィンドウクラスの実装

#include "josekiwindow.h"
#include "recentfilesmenu.h"
#include "fontsizecontrol.h"
#include "flowlayout.h"
#include "buttonstyles.h"
#include "dialogutils.h"
#include "handicaps.h"
#include "josekibooksession.h"
#include "josekieditcontroller.h"
#include "josekiexpanddialog.h"
#include "josekiimportdialog.h"
#include "josekifilecontroller.h"
#include "josekimergecontroller.h"
#include "josekisettings.h"
#include "josekitablemodel.h"
#include "josekitableview.h"
#include "kifupresentation.h"
#include "logcategories.h"
#include "sfenreplay.h"
#include "sfenutils.h"
#include "tablestyles.h"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QCloseEvent>
#include <QDockWidget>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
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
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <utility>

namespace {
constexpr QSize kDefaultSize{950, 600};

QString toolButtonStyle()
{
    return ButtonStyles::panelToolButton()
        + QStringLiteral("QPushButton, QToolButton { padding: 4px 8px; }");
}

QPushButton *makeToolButton(const QString &text, const QString &toolTip, const QString &style, QWidget *parent)
{
    auto *button = new QPushButton(text, parent);
    button->setToolTip(toolTip);
    button->setStyleSheet(style);
    button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    return button;
}

QGroupBox *makeGroup(const QString &title, QWidget *parent, QHBoxLayout **layout)
{
    auto *group = new QGroupBox(title, parent);
    *layout = new QHBoxLayout(group);
    (*layout)->setContentsMargins(8, 6, 8, 6);
    (*layout)->setSpacing(6);
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

JosekiWindow::JosekiWindow(JosekiBookSession *session, QWidget *parent)
    : QWidget(parent, Qt::Window)
    , m_session(session)
    , m_model(new JosekiTableModel(this))
    , m_fontHelper(JosekiSettings::josekiWindowFontSize, 6, 24, 1)
{
    Q_ASSERT(m_session);
    m_files = new JosekiFileController({m_session, this}, this);
    m_merge = new JosekiMergeController({m_session, m_files, this}, this);
    m_edit = new JosekiEditController({m_session, this}, this);
    setAcceptDrops(true);
    setupUi();
    connect(m_session, &JosekiBookSession::modifiedChanged, this, &JosekiWindow::onModifiedChanged);
    connect(m_session, &JosekiBookSession::busyChanged, this, &JosekiWindow::onBusyChanged);
    connect(m_session, &JosekiBookSession::recentFilesChanged, this, &JosekiWindow::onRecentFilesChanged);
    connect(m_session, &JosekiBookSession::filePathChanged, this, &JosekiWindow::onFilePathChanged);
    connect(m_session, &JosekiBookSession::loaded, this, &JosekiWindow::onBookLoaded);
    connect(m_session, &JosekiBookSession::mergeFinished, this, &JosekiWindow::onMergeFinished);
    connect(m_session, &JosekiBookSession::contentChanged, this, &JosekiWindow::onContentChanged);
    connect(m_merge, &JosekiMergeController::kifuDataRequested, this, &JosekiWindow::requestKifuDataForMerge);
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
        // 閉じても定跡は盤の矢印と対局で使い続けるので、「破棄」を選んだ変更は保存した内容に戻す
        m_files->revertChanges();
    }
    return QWidget::eventFilter(obj, event);
}

void JosekiWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (!isIoBusy() && !JosekiImportDialog::localPaths(event->mimeData()).isEmpty()) event->acceptProposedAction();
}

void JosekiWindow::dropEvent(QDropEvent *event)
{
    const QStringList paths = JosekiImportDialog::localPaths(event->mimeData());
    if (isIoBusy() || paths.isEmpty()) return;
    event->acceptProposedAction();
    // ドロップの処理の中ではダイアログを開かない（ドラッグ元のアプリを待たせない）
    m_droppedPaths = paths;
    QTimer::singleShot(0, this, &JosekiWindow::openDroppedPaths);
}

void JosekiWindow::openDroppedPaths()
{
    const QStringList paths = std::exchange(m_droppedPaths, QStringList());
    if (paths.isEmpty()) return;
    // 定跡ファイル（.db）1つなら開き、それ以外は複数の棋譜から登録するダイアログに並べる
    if (paths.size() == 1 && paths.constFirst().endsWith(QStringLiteral(".db"), Qt::CaseInsensitive)) {
        m_files->openRecent(paths.constFirst());
        return;
    }
    m_merge->importPaths(paths);
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
    // 操作を左にまとめ、ドックの幅や文字サイズに応じてグループ単位で折り返す。
    auto *toolbarLayout = new FlowLayout(toolbar, 0, 12, 8);
    const QString buttonStyle = toolButtonStyle();

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
    toolbarLayout->addWidget(fileGroup);

    QHBoxLayout *operationGroupLayout = nullptr;
    QGroupBox *operationGroup = makeGroup(tr("操作"), this, &operationGroupLayout);
    m_addMoveButton = makeToolButton(tr("＋追加"), tr("現在の局面に定跡手を追加"), buttonStyle, this);
    m_mergeButton = new QToolButton(this);
    m_mergeButton->setText(tr("マージ ▼"));
    m_mergeButton->setToolTip(tr("棋譜や別の定跡ファイルから定跡に登録（棋譜ファイル・フォルダはウィンドウへのドロップでも登録できる）"));
    m_mergeButton->setPopupMode(QToolButton::InstantPopup);
    // 表示文字に ▼ を含むので、Qt のメニュー矢印は重ねて描かない
    m_mergeButton->setStyleSheet(buttonStyle + QStringLiteral("QToolButton::menu-indicator { image: none; width: 0px; }"));
    auto *mergeMenu = new QMenu(this);
    mergeMenu->setObjectName(QStringLiteral("josekiMergeMenu"));
    mergeMenu->setProperty("automationMenu", true);
    mergeMenu->addAction(tr("現在の棋譜から"), m_merge, &JosekiMergeController::mergeFromCurrentKifu);
    mergeMenu->addAction(tr("棋譜ファイルから"), m_merge, &JosekiMergeController::mergeFromKifuFile);
    mergeMenu->addSeparator();
    mergeMenu->addAction(tr("複数の棋譜から..."), m_merge, &JosekiMergeController::mergeFromKifuFiles);
    mergeMenu->addAction(tr("定跡ファイルから..."), m_merge, &JosekiMergeController::mergeFromBookFile);
    m_mergeButton->setMenu(mergeMenu);
    m_expandButton = makeToolButton(tr("棋譜に展開..."), tr("表示中の局面から定跡手をたどり、棋譜の変化として足す"),
                                    buttonStyle, this);
    m_expandButton->setObjectName(QStringLiteral("josekiExpand"));
    m_recordBoardMovesCheckBox = new QCheckBox(tr("盤の手を登録"), this);
    m_recordBoardMovesCheckBox->setObjectName(QStringLiteral("josekiRecordBoardMoves"));
    m_recordBoardMovesCheckBox->setToolTip(tr("盤で指した手と対局で指された手（人間・エンジンとも）を、指す前の局面の定跡手として"
                                              "登録する（登録済みの手は変えない。局面編集の手は登録しない。"
                                              "アプリを起動し直すとオフに戻る）"));
    m_stopButton = makeToolButton(tr("■ 停止"), tr("定跡表示を停止/再開"), buttonStyle, this);
    m_stopButton->setCheckable(true);
    operationGroupLayout->addWidget(m_addMoveButton);
    operationGroupLayout->addWidget(m_mergeButton);
    operationGroupLayout->addWidget(m_expandButton);
    operationGroupLayout->addWidget(m_recordBoardMovesCheckBox);
    operationGroupLayout->addWidget(m_stopButton);
    toolbarLayout->addWidget(operationGroup);

    QHBoxLayout *displayGroupLayout = nullptr;
    QGroupBox *displayGroup = makeGroup(tr("表示"), this, &displayGroupLayout);
    m_fontDecreaseButton = new QPushButton(this);
    m_fontDecreaseButton->setObjectName(QStringLiteral("josekiFontDecrease"));
    m_fontIncreaseButton = new QPushButton(this);
    m_fontIncreaseButton->setObjectName(QStringLiteral("josekiFontIncrease"));
    new FontSizeControl(m_fontDecreaseButton, m_fontIncreaseButton, &m_fontHelper);
    for (auto *button : {m_fontDecreaseButton, m_fontIncreaseButton})
        button->setStyleSheet(buttonStyle + QStringLiteral("QPushButton { padding: 4px 6px; }"));
    m_autoLoadCheckBox = new QCheckBox(tr("自動読込"), this);
    m_autoLoadCheckBox->setToolTip(tr("定跡ウィンドウ表示時に前回のファイルを自動で読み込む"));
    m_autoLoadCheckBox->setChecked(true);
    m_boardArrowsCheckBox = new QCheckBox(tr("盤に矢印"), this);
    m_boardArrowsCheckBox->setObjectName(QStringLiteral("josekiBoardArrows"));
    m_boardArrowsCheckBox->setToolTip(tr("表示中の局面の定跡手を盤に矢印で表示する（「表示」メニューの「盤に定跡手を表示」と同じ）"));
    displayGroupLayout->addWidget(m_fontDecreaseButton);
    displayGroupLayout->addWidget(m_fontIncreaseButton);
    displayGroupLayout->addWidget(m_autoLoadCheckBox);
    displayGroupLayout->addWidget(m_boardArrowsCheckBox);
    toolbarLayout->addWidget(displayGroup);

    connect(m_newButton, &QPushButton::clicked, m_files, &JosekiFileController::newBook);
    connect(m_openButton, &QPushButton::clicked, m_files, &JosekiFileController::open);
    connect(m_saveButton, &QPushButton::clicked, m_files, &JosekiFileController::save);
    connect(m_saveAsButton, &QPushButton::clicked, m_files, &JosekiFileController::saveAs);
    connect(m_addMoveButton, &QPushButton::clicked, this, &JosekiWindow::onAddMoveButtonClicked);
    connect(&m_fontHelper, &FontSizeHelper::fontSizeChanged, this, &JosekiWindow::applyFontSize);
    connect(m_autoLoadCheckBox, &QCheckBox::checkStateChanged, this, &JosekiWindow::onAutoLoadCheckBoxChanged);
    connect(m_boardArrowsCheckBox, &QCheckBox::toggled, this, &JosekiWindow::boardArrowsToggled);
    connect(m_stopButton, &QPushButton::clicked, this, &JosekiWindow::onStopButtonClicked);
    connect(m_recordBoardMovesCheckBox, &QCheckBox::toggled, this, &JosekiWindow::onRecordBoardMovesToggled);
    connect(m_expandButton, &QPushButton::clicked, this, &JosekiWindow::onExpandButtonClicked);
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
    auto *infoLayout = new QGridLayout();
    infoLayout->setHorizontalSpacing(10);
    infoLayout->setVerticalSpacing(6);
    infoLayout->setColumnStretch(1, 1);
    infoLayout->addWidget(new QLabel(tr("ファイル:"), this), 0, 0);
    m_filePathLabel = new QLabel(tr("未選択"), this);
    m_filePathLabel->setStyleSheet(QStringLiteral("color: gray;"));
    m_filePathLabel->setTextFormat(Qt::PlainText);
    m_filePathLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    m_filePathLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    infoLayout->addWidget(m_filePathLabel, 0, 1);
    m_fileStatusLabel = new QLabel(this);
    m_fileStatusLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    infoLayout->addWidget(m_fileStatusLabel, 0, 2);

    auto *positionInfoLayout = new QHBoxLayout();
    positionInfoLayout->setSpacing(8);
    infoLayout->addWidget(new QLabel(tr("局面:"), this), 1, 0);
    m_positionSummaryLabel = new QLabel(tr("(未設定)"), this);
    m_positionSummaryLabel->setStyleSheet(QStringLiteral("color: #0066cc; font-weight: bold;"));
    positionInfoLayout->addWidget(m_positionSummaryLabel);
    m_showSfenDetailBtn = makeToolButton(tr("詳細"), tr("SFENの詳細を表示/非表示"), toolButtonStyle(), this);
    m_showSfenDetailBtn->setCheckable(true);
    positionInfoLayout->addWidget(m_showSfenDetailBtn);
    m_deviationButton = new QPushButton(this);
    m_deviationButton->setObjectName(QStringLiteral("josekiDeviation"));
    m_deviationButton->setFlat(true);
    m_deviationButton->setCursor(Qt::PointingHandCursor);
    m_deviationButton->setStyleSheet(QStringLiteral("QPushButton { color: #cc6600; text-align: left; border: none; }"
                                                    "QPushButton:disabled { color: #228b22; }"));
    m_deviationButton->setVisible(false);
    positionInfoLayout->addWidget(m_deviationButton);
    positionInfoLayout->addStretch();
    infoLayout->addLayout(positionInfoLayout, 1, 1, 1, 2);
    mainLayout->addLayout(infoLayout);

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

    // --- 定跡手の表（ボタンの列・右クリックの操作は rowActionRequested で受ける） ---
    m_tableView = new JosekiTableView(m_model, this);
    m_tableView->setActionAvailability(std::bind(&JosekiWindow::isRowActionAvailable, this,
                                                 std::placeholders::_1, std::placeholders::_2));
    mainLayout->addWidget(m_tableView, 1);

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

    connect(m_showSfenDetailBtn, &QPushButton::toggled, this, &JosekiWindow::onSfenDetailToggled);
    connect(m_deviationButton, &QPushButton::clicked, this, &JosekiWindow::onDeviationClicked);
    connect(m_tableView, &JosekiTableView::rowActionRequested, this, &JosekiWindow::onRowActionRequested);

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
        if (col == JosekiTableModel::Move) sample = QStringLiteral("▲７六歩(77)");
        if (col == JosekiTableModel::NextMove) sample = JosekiTableModel::derivedReplyText(QStringLiteral("▲７六歩(77)"));
        if (col == JosekiTableModel::Frequency) sample = QStringLiteral("999,999,999");
        if (col == JosekiTableModel::Ratio) sample = QStringLiteral("100.0%");
        m_tableView->setColumnWidth(col, qMax(m_tableView->columnWidth(col), fm.horizontalAdvance(sample) + 24));
    }
    header->setStretchLastSection(true);
    // 文字・枠・余白を含む sizeHint に幅を任せ、文字を拡大しても欠けないようにする。
    // QPushButton と QToolButton は標準の高さが違うため、このウィンドウ内でそろえる。
    const QList<QAbstractButton *> buttons = {m_newButton, m_openButton, m_saveButton, m_saveAsButton,
        m_recentButton, m_addMoveButton, m_mergeButton, m_stopButton,
        m_fontDecreaseButton, m_fontIncreaseButton, m_showSfenDetailBtn};
    int buttonHeight = qMax(32, fm.height() + 12);
    for (QAbstractButton *button : buttons) buttonHeight = qMax(buttonHeight, button->sizeHint().height());
    for (QAbstractButton *button : buttons) button->setFixedHeight(buttonHeight);
    const int fontButtonWidth = qMax(36, qMax(m_fontDecreaseButton->sizeHint().width(), m_fontIncreaseButton->sizeHint().width()));
    for (QPushButton *button : {m_fontDecreaseButton, m_fontIncreaseButton})
        button->setFixedWidth(fontButtonWidth);

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
    // 比率の列を足す前の保存（列が1つ少ない）は、比率より前の列の幅だけを使う
    const auto widths = JosekiSettings::josekiWindowColumnWidths();
    const int restorable = widths.size() == JosekiTableModel::ColumnCount ? JosekiTableModel::ColumnCount - 1
                                                                           : JosekiTableModel::Ratio;
    for (int col = 0; col < widths.size() && col < restorable; ++col) {
        if (widths.at(col) > 0) m_tableView->setColumnWidth(col, widths.at(col));
    }
    m_autoLoadEnabled = JosekiSettings::josekiWindowAutoLoadEnabled();
    m_autoLoadCheckBox->setChecked(m_autoLoadEnabled);
    // 前回のファイルは履歴を入れる前に読む（履歴を入れると設定を保存し直す）。読み込みは初めて表示したとき
    const QString lastFilePath = JosekiSettings::josekiWindowLastFilePath();
    if (m_autoLoadEnabled && !lastFilePath.isEmpty() && QFileInfo::exists(lastFilePath)) {
        m_pendingAutoLoadPath = lastFilePath;
        m_filePathLabel->setText(lastFilePath + tr(" (未読込)"));
    }
    m_session->setRecentFiles(JosekiSettings::josekiWindowRecentFiles());
    m_displayEnabled = JosekiSettings::josekiWindowDisplayEnabled();
    if (!m_displayEnabled) {
        m_stopButton->setChecked(true);
        m_stopButton->setText(tr("▶ 再開"));
    }
    const bool sfenDetailVisible = JosekiSettings::josekiWindowSfenDetailVisible();
    m_showSfenDetailBtn->setChecked(sfenDetailVisible);
    m_sfenDetailWidget->setVisible(sfenDetailVisible);
}

void JosekiWindow::saveSettings()
{
    DialogUtils::saveDialogSize(this, JosekiSettings::josekiWindowSize);
    // 自動読込を待っている間（まだ表示していない）は、その前回のファイルを覚えたままにする。
    // ファイルを開いていない（自動読込をオフにして起動した・新規にした）ときも、履歴に残っている前回のファイルを覚えたままにする。
    // 「履歴をクリア」した後は、開き直すか保存して履歴に戻るまで前回のファイルとして記録しない
    QString lastFilePath = m_pendingAutoLoadPath.isEmpty() ? m_session->lastFilePathForSettings() : m_pendingAutoLoadPath;
    if (lastFilePath.isEmpty() && m_session->filePath().isEmpty()) {
        const QString previous = JosekiSettings::josekiWindowLastFilePath();
        if (m_session->recentFiles().contains(previous)) lastFilePath = previous;
    }
    JosekiSettings::josekiWindowLastFilePath.set(lastFilePath);
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
    // 自動読込の前に別のファイルを開いた・作った・保存したときは、前回のファイルを読み込まない
    m_pendingAutoLoadPath.clear();
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
    // 履歴から消えた（「履歴をクリア」した）前回のファイルは、自動で読み込まない
    if (!m_pendingAutoLoadPath.isEmpty() && !m_session->recentFiles().contains(m_pendingAutoLoadPath)) {
        m_pendingAutoLoadPath.clear();
        m_filePathLabel->setText(tr("未選択"));
    }
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
    if (row < 0 || row >= m_model->moves().size()) return;
    // ダイアログを開いている間に局面が進んでも、開いたときの局面の手を書き換える
    if (m_edit->editMove(m_currentSfen, m_model->moves().at(row))) updateJosekiDisplay();
}

void JosekiWindow::deleteMoveAt(int row)
{
    if (row < 0 || row >= m_model->moves().size()) return;
    if (m_edit->deleteMove(m_currentSfen, m_model->moves().at(row).move)) updateJosekiDisplay();
}

void JosekiWindow::onAddMoveButtonClicked()
{
    if (m_edit->addMove(m_currentSfen)) updateJosekiDisplay();
}

bool JosekiWindow::isRowActionAvailable(JosekiTableView::RowAction action, int row)
{
    using Action = JosekiTableView::RowAction;
    const int count = static_cast<int>(m_model->moves().size());
    switch (action) {
    case Action::Play: refreshHumanCanPlay(); return m_humanCanPlay;
    case Action::CopyMove: return true;
    case Action::MoveToTop:
    case Action::MoveUp: return !isIoBusy() && row > 0;
    case Action::MoveDown: return !isIoBusy() && row + 1 < count;
    case Action::SortByFrequency:
    case Action::SortByValue: return !isIoBusy() && count > 1;
    default: return !isIoBusy();
    }
}

void JosekiWindow::onRowActionRequested(JosekiTableView::RowAction action, int row)
{
    if (row < 0 || row >= m_model->moves().size()) return;
    using Action = JosekiTableView::RowAction;
    switch (action) {
    case Action::Play: playMoveAt(row); break;
    case Action::Edit: editMoveAt(row); break;
    case Action::Delete: deleteMoveAt(row); break;
    case Action::CopyMove: {
        const QString usiMove = m_model->moves().at(row).move;
        QApplication::clipboard()->setText(usiMove);
        showTransientStatus(tr("「%1」をコピーしました").arg(usiMove));
        break;
    }
    default: reorderMoveAt(action, row); break;
    }
}

void JosekiWindow::reorderMoveAt(JosekiTableView::RowAction action, int row)
{
    if (!isRowActionAvailable(action, row)) return;
    using Action = JosekiTableView::RowAction;
    // 並べ替えた後も同じ手を選んだままにする
    const QString usiMove = m_model->moves().at(row).move;
    bool changed = false;
    switch (action) {
    case Action::MoveToTop: changed = m_session->moveMoveTo(m_currentSfen, usiMove, 0); break;
    case Action::MoveUp: changed = m_session->moveMoveTo(m_currentSfen, usiMove, row - 1); break;
    case Action::MoveDown: changed = m_session->moveMoveTo(m_currentSfen, usiMove, row + 1); break;
    case Action::SortByFrequency: changed = m_session->sortMoves(m_currentSfen, JosekiBook::SortKey::Frequency); break;
    case Action::SortByValue: changed = m_session->sortMoves(m_currentSfen, JosekiBook::SortKey::Value); break;
    default: break;
    }
    if (!changed) return;
    updateJosekiDisplay();
    const QList<JosekiMove> &moves = m_model->moves();
    for (int i = 0; i < moves.size(); ++i) {
        if (moves.at(i).move == usiMove) m_tableView->selectRow(i);
    }
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
    emit displayEnabledChanged(m_displayEnabled);
}

void JosekiWindow::onRecordBoardMovesToggled(bool checked)
{
    (void)checked;
    updateStatusDisplay();
}

bool JosekiWindow::isRecordingBoardMoves() const
{
    return m_recordBoardMovesCheckBox->isChecked();
}

bool JosekiWindow::canRegisterMoves() const
{
    return m_pendingAutoLoadPath.isEmpty() && !isIoBusy();
}

void JosekiWindow::recordBoardMove(const QString &sfen, const QString &usiMove)
{
    if (!isRecordingBoardMoves() || sfen.isEmpty() || usiMove.isEmpty()) return;
    if (!canRegisterMoves()) {
        showTransientStatus(tr("定跡の読み書き中のため、盤の手を登録できませんでした"));
        return;
    }
    if (m_session->hasMove(sfen, usiMove)) return;
    m_session->addMove(sfen, {usiMove, QStringLiteral("none"), 0, 0, 1, {}});
    showTransientStatus(tr("%1 を定跡に登録しました").arg(JosekiTableModel::moveText(usiMove, sfen)));
}

void JosekiWindow::onExpandButtonClicked()
{
    if (isIoBusy() || m_model->moves().isEmpty()) return;
    JosekiExpandDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted && !isIoBusy()) emit expandRequested(dialog.options());
}

void JosekiWindow::setBookDeviation(int ply, const QString &moveText)
{
    m_deviationPly = ply;
    m_deviationButton->setVisible(ply >= 0);
    // すべて定跡どおりなら押す先がないので押せなくする（色でも分ける）
    m_deviationButton->setEnabled(ply > 0);
    if (ply > 0) {
        m_deviationButton->setText(tr("%1手目の%2で定跡を外れています").arg(ply).arg(moveText));
        m_deviationButton->setToolTip(tr("押すとその手へ移ります"));
    } else if (ply == 0) {
        m_deviationButton->setText(tr("表示中の手順はすべて定跡にあります"));
        m_deviationButton->setToolTip(QString());
    }
}

void JosekiWindow::onDeviationClicked()
{
    if (m_deviationPly > 0) emit deviationPlyRequested(m_deviationPly);
}

void JosekiWindow::setBoardArrowsChecked(bool checked)
{
    m_boardArrowsCheckBox->setChecked(checked);
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
            sfenLine = tr("定跡SFEN: %1").arg(m_session->registeredSfen(m_currentSfen));
            // 同じ局面を手数違いで別々に登録していれば、その手数も示す（表示中の手数の登録、無ければ最初の登録を出す）
            const QList<int> plies = m_session->registeredPlies(m_currentSfen);
            if (plies.size() > 1) {
                QStringList plyTexts;
                for (const int ply : plies) plyTexts.append(QString::number(ply));
                sfenLine += QLatin1Char(' ') + tr("（手数違いの登録: %1）").arg(plyTexts.join(QStringLiteral(", ")));
            }
        } else {
            sfenLine = tr("定跡: (該当なし)");
        }
    }
    m_sfenLineLabel->setText(sfenLine);
    // 選択と表示位置は初めに戻す
    m_tableView->setCurrentIndex(QModelIndex());
    m_tableView->clearSelection();
    // 予想応手が「なし」の手には、やねうら王が先読みに使う次の局面の定跡手を添える（表に灰色で出す）
    QStringList derivedReplies;
    for (const JosekiMove &move : std::as_const(moves)) {
        derivedReplies.append(JosekiTableModel::hasNoReply(move)
                                  ? m_session->leadingMove(SfenReplay::positionAfter(m_currentSfen, move.move))
                                  : QString());
    }
    m_model->setMoves(m_currentSfen, moves, m_humanCanPlay, derivedReplies);
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
    m_expandButton->setEnabled(!isIoBusy() && !m_model->moves().isEmpty());
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
    const JosekiBookSession::Activity activity = m_session->activity();
    QStringList statusParts;
    // 読み書きの最中は終わるまで出し続ける（大きな定跡は読み込みに数秒かかる）
    if (activity == JosekiBookSession::Activity::Loading) statusParts << tr("読み込み中...");
    else if (activity == JosekiBookSession::Activity::Saving) statusParts << tr("保存中...");
    else if (activity == JosekiBookSession::Activity::Merging) statusParts << tr("定跡ファイルを足しています...");
    statusParts << (filePath.isEmpty() ? tr("ファイル: 未選択") : tr("ファイル: %1").arg(QFileInfo(filePath).fileName()));
    statusParts << tr("局面数: %1").arg(QLocale().toString(m_session->positionCount()));
    statusParts << (m_displayEnabled ? tr("定跡: %1件").arg(moveCount) : tr("【停止中】"));
    if (isRecordingBoardMoves()) statusParts << tr("盤の手を登録中");
    m_statusLabel->setText(statusParts.join(QStringLiteral("  |  ")));

    QString guide;
    if (moveCount > 0)
        guide.clear();
    else if (activity == JosekiBookSession::Activity::Loading)
        guide = tr("定跡ファイルを読み込んでいます...");
    else if (!m_displayEnabled)
        guide = tr("定跡表示を停止しています。「再開」で表示を再開します。");
    else if (m_currentSfen.isEmpty())
        guide = tr("将棋盤で局面を表示すると、その局面の定跡を確認できます。");
    else if (filePath.isEmpty() && !hasData)
        guide = tr("「開く」で定跡ファイルを読み込むと、表示中の局面の定跡手が表示されます。\n"
                   "「新規」で新しい定跡ファイルを作ることもできます。");
    else
        guide = tr("この局面には定跡が登録されていません。\n"
                   "「＋追加」で指し手を登録するか、「マージ」から棋譜を取り込めます。");
    m_tableView->setEmptyGuide(guide);
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
// 棋譜からのマージ（JosekiMergeController へ渡す）
// ============================================================

void JosekiWindow::setKifuDataForMerge(const QStringList &sfenList, const QStringList &moveList,
                                       const QStringList &japaneseMoveList, int currentPly)
{
    m_merge->setKifuData(sfenList, moveList, japaneseMoveList, currentPly);
}

void JosekiWindow::onContentChanged()
{
    // 表に出している局面を忘れ、裏に回っていたドックが前面に出たときにも作り直させる
    m_displayedSfen.clear();
    if (isVisible() && m_pendingAutoLoadPath.isEmpty()) updateJosekiDisplay();
}

void JosekiWindow::onMergeFinished()
{
    updateJosekiDisplay();
}
