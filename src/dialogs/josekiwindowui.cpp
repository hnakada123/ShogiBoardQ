/// @file josekiwindowui.cpp
/// @brief JosekiWindow の UI構築・表示更新メソッド群

#include "josekiwindow.h"
#include "josekirepository.h"
#include "josekipresenter.h"
#include "josekisettings.h"
#include "sfenpositiontracer.h"
#include "buttonstyles.h"
#include "tablestyles.h"
#include "dialogutils.h"
#include "logcategories.h"

#include <QVBoxLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QFrame>
#include <QToolButton>
#include <QFileInfo>
#include <QLocale>

namespace {
constexpr QSize kDefaultSize{950, 600};
} // namespace

static QTableWidgetItem *numericItem(const QLocale &locale, int value)
{
    auto *item = new QTableWidgetItem(locale.toString(value));
    item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    return item;
}

static void addRowButton(QTableWidget *table, int row, int col,
                         const QString &text, const QString &style,
                         JosekiWindow *win, void (JosekiWindow::*slot)())
{
    auto *btn = new QPushButton(text, win);
    btn->setProperty("row", row);
    btn->setFont(win->font());
    btn->setStyleSheet(style);
    btn->setCursor(Qt::PointingHandCursor);
    QObject::connect(btn, &QPushButton::clicked, win, slot);
    table->setCellWidget(row, col, btn);
}

void JosekiWindow::setupUi()
{
    setWindowTitle(tr("定跡ウィンドウ"));
    resize(kDefaultSize);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(6);
    mainLayout->setContentsMargins(8, 8, 8, 8);

    QHBoxLayout *toolbarLayout = new QHBoxLayout();
    toolbarLayout->setSpacing(12);

    const QString fileBtnStyle = ButtonStyles::panelToolButton() + QStringLiteral("QPushButton { padding: 4px 8px; }");
    const QString fontBtnStyle = ButtonStyles::panelToolButton();
    const QString editBtnStyle = fileBtnStyle;
    const QString stopBtnStyle = fileBtnStyle;

    QGroupBox *fileGroup = new QGroupBox(tr("ファイル"), this);
    QHBoxLayout *fileGroupLayout = new QHBoxLayout(fileGroup);
    fileGroupLayout->setContentsMargins(8, 4, 8, 4);
    fileGroupLayout->setSpacing(4);

    m_newButton = new QPushButton(tr("新規"), this);
    m_newButton->setToolTip(tr("新しい空の定跡ファイルを作成"));
    m_newButton->setIcon(QIcon(QStringLiteral(":/images/actions/actionNewGame.svg")));
    m_newButton->setStyleSheet(fileBtnStyle);
    fileGroupLayout->addWidget(m_newButton);

    m_openButton = new QPushButton(tr("開く"), this);
    m_openButton->setToolTip(tr("定跡ファイル(.db)を開く"));
    m_openButton->setIcon(QIcon(QStringLiteral(":/images/actions/actionOpenKifuFile.svg")));
    m_openButton->setStyleSheet(fileBtnStyle);
    fileGroupLayout->addWidget(m_openButton);

    m_saveButton = new QPushButton(tr("保存"), this);
    m_saveButton->setToolTip(tr("現在のファイルに上書き保存"));
    m_saveButton->setIcon(QIcon(QStringLiteral(":/images/actions/actionSave.svg")));
    m_saveButton->setStyleSheet(fileBtnStyle);
    m_saveButton->setEnabled(false);
    fileGroupLayout->addWidget(m_saveButton);

    m_saveAsButton = new QPushButton(tr("別名保存"), this);
    m_saveAsButton->setToolTip(tr("別の名前で保存"));
    m_saveAsButton->setIcon(QIcon(QStringLiteral(":/images/actions/actionSaveAs.svg")));
    m_saveAsButton->setStyleSheet(fileBtnStyle);
    fileGroupLayout->addWidget(m_saveAsButton);

    m_recentButton = new QPushButton(tr("履歴"), this);
    m_recentButton->setToolTip(tr("最近使ったファイルを開く"));
    m_recentButton->setStyleSheet(fileBtnStyle);
    m_recentFilesMenu = new QMenu(this);
    m_recentFilesMenu->setObjectName(QStringLiteral("josekiRecentMenu"));
    m_recentFilesMenu->setProperty("automationMenu", true);
    m_recentButton->setMenu(m_recentFilesMenu);
    fileGroupLayout->addWidget(m_recentButton);
    toolbarLayout->addWidget(fileGroup, 0, Qt::AlignLeft);

    QGroupBox *displayGroup = new QGroupBox(tr("表示"), this);
    QHBoxLayout *displayGroupLayout = new QHBoxLayout(displayGroup);
    displayGroupLayout->setContentsMargins(8, 4, 8, 4);
    displayGroupLayout->setSpacing(4);

    m_fontDecreaseBtn = new QPushButton(tr("A-"), this);
    m_fontDecreaseBtn->setToolTip(tr("フォントサイズを縮小"));
    m_fontDecreaseBtn->setFixedWidth(36);
    m_fontDecreaseBtn->setStyleSheet(fontBtnStyle);
    displayGroupLayout->addWidget(m_fontDecreaseBtn);

    m_fontIncreaseBtn = new QPushButton(tr("A+"), this);
    m_fontIncreaseBtn->setToolTip(tr("フォントサイズを拡大"));
    m_fontIncreaseBtn->setFixedWidth(36);
    m_fontIncreaseBtn->setStyleSheet(fontBtnStyle);
    displayGroupLayout->addWidget(m_fontIncreaseBtn);

    m_autoLoadCheckBox = new QCheckBox(tr("自動読込"), this);
    m_autoLoadCheckBox->setToolTip(tr("定跡ウィンドウ表示時に前回のファイルを自動で読み込む"));
    m_autoLoadCheckBox->setChecked(true);
    displayGroupLayout->addWidget(m_autoLoadCheckBox);
    toolbarLayout->addWidget(displayGroup, 0, Qt::AlignLeft);
    toolbarLayout->addStretch();

    QGroupBox *operationGroup = new QGroupBox(tr("操作"), this);
    QHBoxLayout *operationGroupLayout = new QHBoxLayout(operationGroup);
    operationGroupLayout->setContentsMargins(8, 4, 8, 4);
    operationGroupLayout->setSpacing(4);

    m_addMoveButton = new QPushButton(tr("＋追加"), this);
    m_addMoveButton->setToolTip(tr("現在の局面に定跡手を追加"));
    m_addMoveButton->setStyleSheet(editBtnStyle);
    operationGroupLayout->addWidget(m_addMoveButton);

    m_mergeButton = new QToolButton(this);
    m_mergeButton->setText(tr("マージ ▼"));
    m_mergeButton->setToolTip(tr("棋譜から定跡をマージ"));
    m_mergeButton->setPopupMode(QToolButton::InstantPopup);
    m_mergeButton->setStyleSheet(editBtnStyle);

    m_mergeMenu = new QMenu(this);
    m_mergeMenu->setObjectName(QStringLiteral("josekiMergeMenu"));
    m_mergeMenu->setProperty("automationMenu", true);
    m_mergeMenu->addAction(tr("現在の棋譜から"), this, &JosekiWindow::onMergeFromCurrentKifu);
    m_mergeMenu->addAction(tr("棋譜ファイルから"), this, &JosekiWindow::onMergeFromKifuFile);
    m_mergeButton->setMenu(m_mergeMenu);
    operationGroupLayout->addWidget(m_mergeButton);

    m_stopButton = new QPushButton(tr("■ 停止"), this);
    m_stopButton->setToolTip(tr("定跡表示を停止/再開"));
    m_stopButton->setCheckable(true);
    m_stopButton->setStyleSheet(stopBtnStyle);
    operationGroupLayout->addWidget(m_stopButton);
    toolbarLayout->addWidget(operationGroup, 0, Qt::AlignRight);
    mainLayout->addLayout(toolbarLayout);

    QHBoxLayout *fileInfoLayout = new QHBoxLayout();
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

    QHBoxLayout *positionInfoLayout = new QHBoxLayout();
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
    QVBoxLayout *sfenDetailLayout = new QVBoxLayout(m_sfenDetailWidget);
    sfenDetailLayout->setContentsMargins(20, 0, 0, 0);
    sfenDetailLayout->setSpacing(2);
    m_currentSfenLabel = new QLabel(this);
    m_currentSfenLabel->setStyleSheet(QStringLiteral("color: #0066cc; font-size: 9pt;"));
    m_currentSfenLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_currentSfenLabel->setWordWrap(true);
    sfenDetailLayout->addWidget(m_currentSfenLabel);
    m_sfenLineLabel = new QLabel(this);
    m_sfenLineLabel->setStyleSheet(QStringLiteral("color: #228b22; font-size: 9pt;"));
    m_sfenLineLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_sfenLineLabel->setWordWrap(true);
    sfenDetailLayout->addWidget(m_sfenLineLabel);
    m_sfenDetailWidget->setVisible(false);
    mainLayout->addWidget(m_sfenDetailWidget);

    m_tableWidget = new QTableWidget(this);
    m_tableWidget->setObjectName(QStringLiteral("josekiTable"));
    m_tableWidget->setColumnCount(10);
    QStringList headers;
    headers << tr("No.") << tr("着手") << tr("定跡手") << tr("予想応手") << tr("編集")
            << tr("削除") << tr("評価値") << tr("深さ") << tr("出現頻度") << tr("コメント");
    m_tableWidget->setHorizontalHeaderLabels(headers);
    m_tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tableWidget->setAlternatingRowColors(true);
    m_tableWidget->setShowGrid(false);
    m_tableWidget->setWordWrap(false);
    m_tableWidget->verticalHeader()->setVisible(false);
    m_tableWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    m_tableWidget->setColumnWidth(0, 40);
    m_tableWidget->setColumnWidth(1, 50);
    m_tableWidget->setColumnWidth(2, 120);
    m_tableWidget->setColumnWidth(3, 120);
    m_tableWidget->setColumnWidth(4, 50);
    m_tableWidget->setColumnWidth(5, 50);
    m_tableWidget->setColumnWidth(6, 70);
    m_tableWidget->setColumnWidth(7, 50);
    m_tableWidget->setColumnWidth(8, 80);
    m_tableWidget->horizontalHeader()->setStretchLastSection(true);
    mainLayout->addWidget(m_tableWidget, 1);

    m_emptyGuideLabel = new QLabel(m_tableWidget->viewport());
    m_emptyGuideLabel->setWordWrap(true);
    m_emptyGuideLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    auto *emptyLayout = new QVBoxLayout(m_tableWidget->viewport());
    emptyLayout->addWidget(m_emptyGuideLabel);
    m_emptyGuideLabel->setAlignment(Qt::AlignCenter);
    m_emptyGuideLabel->setVisible(false);

    QFrame *statusFrame = new QFrame(this);
    statusFrame->setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
    QHBoxLayout *statusLayout = new QHBoxLayout(statusFrame);
    statusLayout->setContentsMargins(8, 2, 8, 2);
    m_statusLabel = new QLabel(this);
    statusLayout->addWidget(m_statusLabel, 1);
    mainLayout->addWidget(statusFrame);

    m_noticeLabel = new QLabel(this);
    m_noticeLabel->setText(tr("※ 追加・編集・削除後は「保存」ボタンで定跡ファイルに保存してください"));
    m_noticeLabel->setStyleSheet(QStringLiteral("color: #cc6600; font-size: 9pt;"));
    mainLayout->addWidget(m_noticeLabel);

    m_tableContextMenu = new QMenu(this);
    m_actionPlay = m_tableContextMenu->addAction(tr("着手"));
    m_tableContextMenu->addSeparator();
    m_actionEdit = m_tableContextMenu->addAction(tr("編集..."));
    m_actionDelete = m_tableContextMenu->addAction(tr("削除"));
    m_tableContextMenu->addSeparator();
    m_actionCopyMove = m_tableContextMenu->addAction(tr("指し手をコピー"));

    connect(m_newButton, &QPushButton::clicked, this, &JosekiWindow::onNewButtonClicked);
    connect(m_openButton, &QPushButton::clicked, this, &JosekiWindow::onOpenButtonClicked);
    connect(m_saveButton, &QPushButton::clicked, this, &JosekiWindow::onSaveButtonClicked);
    connect(m_saveAsButton, &QPushButton::clicked, this, &JosekiWindow::onSaveAsButtonClicked);
    connect(m_addMoveButton, &QPushButton::clicked, this, &JosekiWindow::onAddMoveButtonClicked);
    connect(m_fontIncreaseBtn, &QPushButton::clicked, this, &JosekiWindow::onFontSizeIncrease);
    connect(m_fontDecreaseBtn, &QPushButton::clicked, this, &JosekiWindow::onFontSizeDecrease);
    connect(m_autoLoadCheckBox, &QCheckBox::checkStateChanged, this, &JosekiWindow::onAutoLoadCheckBoxChanged);
    connect(m_stopButton, &QPushButton::clicked, this, &JosekiWindow::onStopButtonClicked);
    connect(m_showSfenDetailBtn, &QPushButton::toggled, this, &JosekiWindow::onSfenDetailToggled);
    connect(m_tableWidget, &QTableWidget::cellDoubleClicked, this, &JosekiWindow::onTableDoubleClicked);
    connect(m_tableWidget, &QTableWidget::customContextMenuRequested, this, &JosekiWindow::onTableContextMenu);
    connect(m_actionPlay, &QAction::triggered, this, &JosekiWindow::onContextMenuPlay);
    connect(m_actionEdit, &QAction::triggered, this, &JosekiWindow::onContextMenuEdit);
    connect(m_actionDelete, &QAction::triggered, this, &JosekiWindow::onContextMenuDelete);
    connect(m_actionCopyMove, &QAction::triggered, this, &JosekiWindow::onContextMenuCopyMove);

    updateStatusDisplay();
    updateWindowTitle();
}

void JosekiWindow::applyFontSize()
{
    DialogUtils::standardizeDialog(this);
    const int fontSize = m_fontHelper.fontSize();
    QFont font = this->font();
    font.setPointSize(fontSize);
    DialogUtils::applyFontToAllChildren(this, font);

    if (m_tableWidget) {
        QHeaderView *header = m_tableWidget->horizontalHeader();
        header->setFont(font);
        QFontMetrics fm(font);
        header->setFixedHeight(fm.height() + 12);
        m_tableWidget->verticalHeader()->setDefaultSectionSize(fm.height() + 12);
        m_tableWidget->setStyleSheet(TableStyles::thinking(fontSize) + QStringLiteral(
            "QTableView { alternate-background-color: #f7f9fb; }"
            "QTableView::item { padding: 3px 6px; }"));
        for (int col = 0; col < m_tableWidget->columnCount() - 1; ++col) {
            QString sample = m_tableWidget->horizontalHeaderItem(col)->text();
            if (col == 2 || col == 3) sample = QStringLiteral("▲７六歩(77)");
            if (col == 8) sample = QStringLiteral("999,999,999");
            m_tableWidget->setColumnWidth(col, qMax(m_tableWidget->columnWidth(col),
                                                     fm.horizontalAdvance(sample) + 24));
        }
        header->setStretchLastSection(true);
    }
    if (m_showSfenDetailBtn) {
        QFontMetrics fm(font);
        m_showSfenDetailBtn->setFixedWidth(fm.horizontalAdvance(m_showSfenDetailBtn->text()) + 20);
    }
    if (m_noticeLabel) {
        int noticeFontSize = qMax(fontSize - 1, 6);
        m_noticeLabel->setStyleSheet(QStringLiteral("color: #cc6600; font-size: %1pt;").arg(noticeFontSize));
    }
    if (m_mergeMenu) m_mergeMenu->setFont(font);
    if (m_recentFilesMenu) m_recentFilesMenu->setFont(font);
    if (m_tableContextMenu) m_tableContextMenu->setFont(font);
    int sfenFontSize = qMax(fontSize - 1, 6);
    if (m_currentSfenLabel)
        m_currentSfenLabel->setStyleSheet(QStringLiteral("color: #0066cc; font-size: %1pt;").arg(sfenFontSize));
    if (m_sfenLineLabel)
        m_sfenLineLabel->setStyleSheet(QStringLiteral("color: #228b22; font-size: %1pt;").arg(sfenFontSize));
    updateJosekiDisplay();
    DialogUtils::updateFontButtons(this, fontSize, 6, 24);
}

void JosekiWindow::loadSettings()
{
    applyFontSize();
    DialogUtils::restoreDialogSize(this, JosekiSettings::josekiWindowSize());
    const auto widths = JosekiSettings::josekiWindowColumnWidths();
    for (int col = 0; col < widths.size() && col < m_tableWidget->columnCount() - 1; ++col) {
        if (widths.at(col) > 0) m_tableWidget->setColumnWidth(col, widths.at(col));
    }
    m_autoLoadEnabled = JosekiSettings::josekiWindowAutoLoadEnabled();
    m_autoLoadCheckBox->setChecked(m_autoLoadEnabled);
    m_recentFiles = JosekiSettings::josekiWindowRecentFiles();
    updateRecentFilesMenu();
    m_displayEnabled = JosekiSettings::josekiWindowDisplayEnabled();
    if (!m_displayEnabled) {
        m_stopButton->setChecked(true);
        m_stopButton->setText(tr("▶ 再開"));
    }
    bool sfenDetailVisible = JosekiSettings::josekiWindowSfenDetailVisible();
    m_showSfenDetailBtn->setChecked(sfenDetailVisible);
    m_sfenDetailWidget->setVisible(sfenDetailVisible);
    QString lastFilePath = JosekiSettings::josekiWindowLastFilePath();
    if (m_autoLoadEnabled && !lastFilePath.isEmpty() && QFileInfo::exists(lastFilePath)) {
        m_pendingAutoLoad = true;
        m_pendingAutoLoadPath = lastFilePath;
        m_filePathLabel->setText(lastFilePath + tr(" (未読込)"));
    }
}

void JosekiWindow::saveSettings()
{
    DialogUtils::saveDialogSize(this, JosekiSettings::setJosekiWindowSize);
    JosekiSettings::setJosekiWindowLastFilePath(m_currentFilePath);
    JosekiSettings::setJosekiWindowAutoLoadEnabled(m_autoLoadEnabled);
    JosekiSettings::setJosekiWindowRecentFiles(m_recentFiles);
    if (m_tableWidget) {
        QList<int> widths;
        for (int i = 0; i < m_tableWidget->columnCount(); ++i)
            widths.append(m_tableWidget->columnWidth(i));
        JosekiSettings::setJosekiWindowColumnWidths(widths);
    }
}

void JosekiWindow::updateJosekiDisplay()
{
    qCDebug(lcUi) << "updateJosekiDisplay() called";

    if (m_pendingAutoLoad && !m_pendingAutoLoadPath.isEmpty()) {
        m_pendingAutoLoad = false;
        QString pathToLoad = m_pendingAutoLoadPath;
        m_pendingAutoLoadPath.clear();
        qCDebug(lcUi) << "Performing deferred auto-load:" << pathToLoad;
        loadAndApplyFileAsync(pathToLoad);
        return;
    }

    clearTable();
    updatePositionSummary();
    m_sfenLineLabel->clear();
    if (!m_displayEnabled) return;
    if (m_currentSfen.isEmpty()) return;

    QString normalizedSfen = JosekiPresenter::normalizeSfen(m_currentSfen);
    qCDebug(lcUi) << "Looking for:" << normalizedSfen;

    if (!m_repository->containsPosition(normalizedSfen)) {
        m_currentMoves.clear();
        m_sfenLineLabel->setText(tr("定跡: (該当なし)"));
        updateStatusDisplay();
        return;
    }

    const QList<JosekiMove> &moves = m_repository->movesForPosition(normalizedSfen);
    m_currentMoves = moves;

    QString sPly = m_repository->sfenWithPly(normalizedSfen);
    m_sfenLineLabel->setText(sPly.isEmpty()
        ? tr("定跡SFEN: %1").arg(normalizedSfen)
        : tr("定跡SFEN: %1").arg(sPly));

    SfenPositionTracer tracer;
    (void)tracer.setFromSfen(m_currentSfen);
    m_tableWidget->setRowCount(static_cast<int>(moves.size()));

    QLocale locale = QLocale::system();
    locale.setNumberOptions(QLocale::DefaultNumberOptions);

    for (int i = 0; i < moves.size(); ++i) {
        const JosekiMove &move = moves[i];

        auto *noItem = new QTableWidgetItem(QString::number(i + 1));
        noItem->setTextAlignment(Qt::AlignCenter);
        m_tableWidget->setItem(i, 0, noItem);

        if (m_humanCanPlay)
            addRowButton(m_tableWidget, i, 1, tr("着手"), ButtonStyles::panelToolButton(),
                         this, &JosekiWindow::onPlayButtonClicked);

        QString moveJapanese = JosekiPresenter::usiMoveToJapanese(move.move, tracer);
        auto *moveItem = new QTableWidgetItem(moveJapanese);
        moveItem->setTextAlignment(Qt::AlignCenter);
        moveItem->setToolTip(m_humanCanPlay
            ? move.move + QLatin1Char('\n') + tr("ダブルクリックで着手") : move.move);
        m_tableWidget->setItem(i, 2, moveItem);

        SfenPositionTracer nextTracer;
        (void)nextTracer.setFromSfen(m_currentSfen);
        (void)nextTracer.applyUsiMove(move.move);
        auto *nextMoveItem = new QTableWidgetItem(
            JosekiPresenter::usiMoveToJapanese(move.nextMove, nextTracer));
        nextMoveItem->setToolTip(move.nextMove);
        nextMoveItem->setTextAlignment(Qt::AlignCenter);
        m_tableWidget->setItem(i, 3, nextMoveItem);

        addRowButton(m_tableWidget, i, 4, tr("編集"), ButtonStyles::panelToolButton(),
                     this, &JosekiWindow::onEditButtonClicked);
        addRowButton(m_tableWidget, i, 5, tr("削除"), ButtonStyles::panelToolButton(),
                     this, &JosekiWindow::onDeleteButtonClicked);

        m_tableWidget->setItem(i, 6, numericItem(locale, move.value));
        m_tableWidget->setItem(i, 7, numericItem(locale, move.depth));
        m_tableWidget->setItem(i, 8, numericItem(locale, move.frequency));
        auto *commentItem = new QTableWidgetItem(move.comment);
        commentItem->setToolTip(move.comment);
        m_tableWidget->setItem(i, 9, commentItem);
    }

    updateStatusDisplay();
}
