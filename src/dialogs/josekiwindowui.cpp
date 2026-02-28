/// @file josekiwindowui.cpp
/// @brief JosekiWindow の UI構築・表示更新メソッド群

#include "josekiwindow.h"
#include "josekirepository.h"
#include "josekipresenter.h"
#include "josekisettings.h"
#include "sfenpositiontracer.h"
#include "buttonstyles.h"
#include "dialogutils.h"
#include "logcategories.h"

#include <QVBoxLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QFrame>
#include <QToolButton>
#include <QMessageBox>
#include <QFileInfo>
#include <QLocale>
#include <QDockWidget>

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

    const QString fileBtnStyle = ButtonStyles::fileOperation();
    const QString fontBtnStyle = ButtonStyles::fontButton();
    const QString editBtnStyle = ButtonStyles::editOperation();
    const QString stopBtnStyle = ButtonStyles::dangerStop();

    QGroupBox *fileGroup = new QGroupBox(tr("ファイル"), this);
    QHBoxLayout *fileGroupLayout = new QHBoxLayout(fileGroup);
    fileGroupLayout->setContentsMargins(8, 4, 8, 4);
    fileGroupLayout->setSpacing(4);

    m_newButton = new QPushButton(tr("新規"), this);
    m_newButton->setToolTip(tr("新しい空の定跡ファイルを作成"));
    m_newButton->setIcon(style()->standardIcon(QStyle::SP_FileIcon));
    m_newButton->setStyleSheet(fileBtnStyle);
    fileGroupLayout->addWidget(m_newButton);

    m_openButton = new QPushButton(tr("開く"), this);
    m_openButton->setToolTip(tr("定跡ファイル(.db)を開く"));
    m_openButton->setIcon(style()->standardIcon(QStyle::SP_DialogOpenButton));
    m_openButton->setStyleSheet(fileBtnStyle);
    fileGroupLayout->addWidget(m_openButton);

    m_saveButton = new QPushButton(tr("保存"), this);
    m_saveButton->setToolTip(tr("現在のファイルに上書き保存"));
    m_saveButton->setIcon(style()->standardIcon(QStyle::SP_DialogSaveButton));
    m_saveButton->setStyleSheet(fileBtnStyle);
    m_saveButton->setEnabled(false);
    fileGroupLayout->addWidget(m_saveButton);

    m_saveAsButton = new QPushButton(tr("別名保存"), this);
    m_saveAsButton->setToolTip(tr("別の名前で保存"));
    m_saveAsButton->setIcon(style()->standardIcon(QStyle::SP_DialogSaveButton));
    m_saveAsButton->setStyleSheet(fileBtnStyle);
    fileGroupLayout->addWidget(m_saveAsButton);

    m_recentButton = new QPushButton(tr("履歴"), this);
    m_recentButton->setToolTip(tr("最近使ったファイルを開く"));
    m_recentButton->setStyleSheet(fileBtnStyle);
    m_recentFilesMenu = new QMenu(this);
    m_recentButton->setMenu(m_recentFilesMenu);
    fileGroupLayout->addWidget(m_recentButton);
    toolbarLayout->addWidget(fileGroup);

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
    toolbarLayout->addWidget(displayGroup);

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
    m_mergeMenu->addAction(tr("現在の棋譜から"), this, &JosekiWindow::onMergeFromCurrentKifu);
    m_mergeMenu->addAction(tr("棋譜ファイルから"), this, &JosekiWindow::onMergeFromKifuFile);
    m_mergeButton->setMenu(m_mergeMenu);
    operationGroupLayout->addWidget(m_mergeButton);

    m_stopButton = new QPushButton(tr("■ 停止"), this);
    m_stopButton->setToolTip(tr("定跡表示を停止/再開"));
    m_stopButton->setCheckable(true);
    m_stopButton->setStyleSheet(stopBtnStyle);
    operationGroupLayout->addWidget(m_stopButton);
    toolbarLayout->addWidget(operationGroup);
    mainLayout->addLayout(toolbarLayout);

    QHBoxLayout *fileInfoLayout = new QHBoxLayout();
    fileInfoLayout->addWidget(new QLabel(tr("ファイル:"), this));
    m_filePathLabel = new QLabel(tr("未選択"), this);
    m_filePathLabel->setStyleSheet(QStringLiteral("color: gray;"));
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
    m_currentSfenLabel->setStyleSheet(QStringLiteral("color: #0066cc; font-family: monospace; font-size: 9pt;"));
    m_currentSfenLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_currentSfenLabel->setWordWrap(true);
    sfenDetailLayout->addWidget(m_currentSfenLabel);
    m_sfenLineLabel = new QLabel(this);
    m_sfenLineLabel->setStyleSheet(QStringLiteral("color: #228b22; font-family: monospace; font-size: 9pt;"));
    m_sfenLineLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_sfenLineLabel->setWordWrap(true);
    sfenDetailLayout->addWidget(m_sfenLineLabel);
    m_sfenDetailWidget->setVisible(false);
    mainLayout->addWidget(m_sfenDetailWidget);

    m_tableWidget = new QTableWidget(this);
    m_tableWidget->setColumnCount(10);
    QStringList headers;
    headers << tr("No.") << tr("着手") << tr("定跡手") << tr("予想応手") << tr("編集")
            << tr("削除") << tr("評価値") << tr("深さ") << tr("出現頻度") << tr("コメント");
    m_tableWidget->setHorizontalHeaderLabels(headers);
    m_tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tableWidget->setAlternatingRowColors(true);
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

    m_emptyGuideLabel = new QLabel(this);
    m_emptyGuideLabel->setText(
        tr("<div style='text-align: center; color: #888;'>"
           "<p style='font-size: 14pt; margin-bottom: 10px;'>定跡が登録されていません</p>"
           "<p>「＋追加」ボタンで手動追加、または<br>"
           "「マージ」メニューから棋譜を取り込めます</p>"
           "</div>"));
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
    m_noticeLabel->setText(tr("※ 編集・削除後は「保存」ボタンで定跡ファイルに保存してください"));
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
        m_tableWidget->setStyleSheet(QStringLiteral(
            "QHeaderView::section {"
            "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
            "    stop:0 #40acff, stop:1 #209cee);"
            "  color: white;"
            "  font-weight: normal;"
            "  padding: 4px;"
            "  border: none;"
            "  border-bottom: 1px solid #209cee;"
            "  font-size: %1pt;"
            "}")
            .arg(fontSize));
        for (int col = 0; col < m_tableWidget->columnCount(); ++col) {
            QString headerText = m_tableWidget->horizontalHeaderItem(col)
                                     ? m_tableWidget->horizontalHeaderItem(col)->text()
                                     : QString();
            m_tableWidget->setColumnWidth(col, fm.horizontalAdvance(headerText) + 24);
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
        m_currentSfenLabel->setStyleSheet(QStringLiteral("color: #0066cc; font-family: monospace; font-size: %1pt;").arg(sfenFontSize));
    if (m_sfenLineLabel)
        m_sfenLineLabel->setStyleSheet(QStringLiteral("color: #228b22; font-family: monospace; font-size: %1pt;").arg(sfenFontSize));
    updateJosekiDisplay();
}

void JosekiWindow::loadSettings()
{
    applyFontSize();
    DialogUtils::restoreDialogSize(this, JosekiSettings::josekiWindowSize());
    m_autoLoadEnabled = JosekiSettings::josekiWindowAutoLoadEnabled();
    m_autoLoadCheckBox->setChecked(m_autoLoadEnabled);
    m_recentFiles = JosekiSettings::josekiWindowRecentFiles();
    updateRecentFilesMenu();
    m_displayEnabled = JosekiSettings::josekiWindowDisplayEnabled();
    if (!m_displayEnabled) {
        m_stopButton->setChecked(true);
        m_stopButton->setText(tr("▶ 再開"));
        m_stopButton->setStyleSheet(QStringLiteral("color: #cc0000; font-weight: bold;"));
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
        if (!loadAndApplyFile(pathToLoad))
            m_filePathLabel->setText(QString());
    }

    clearTable();
    if (!m_displayEnabled) return;
    if (m_currentSfen.isEmpty()) return;

    QString normalizedSfen = JosekiPresenter::normalizeSfen(m_currentSfen);
    qCDebug(lcUi) << "Looking for:" << normalizedSfen;
    updatePositionSummary();

    if (!m_repository->containsPosition(normalizedSfen)) {
        m_currentMoves.clear();
        m_sfenLineLabel->setText(tr("定跡: (該当なし)"));
        updateStatusDisplay();
        return;
    }

    const QVector<JosekiMove> &moves = m_repository->movesForPosition(normalizedSfen);
    m_currentMoves = moves;

    QString sPly = m_repository->sfenWithPly(normalizedSfen);
    m_sfenLineLabel->setText(sPly.isEmpty()
        ? tr("定跡SFEN: %1").arg(normalizedSfen)
        : tr("定跡SFEN: %1").arg(sPly));

    int plyNumber = currentPlyNumber();
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
            addRowButton(m_tableWidget, i, 1, tr("着手"), ButtonStyles::tablePlayButton(),
                         this, &JosekiWindow::onPlayButtonClicked);

        QString moveJapanese = JosekiPresenter::usiMoveToJapanese(move.move, plyNumber, tracer);
        auto *moveItem = new QTableWidgetItem(moveJapanese);
        moveItem->setTextAlignment(Qt::AlignCenter);
        moveItem->setToolTip(tr("ダブルクリックで着手"));
        m_tableWidget->setItem(i, 2, moveItem);

        SfenPositionTracer nextTracer;
        (void)nextTracer.setFromSfen(m_currentSfen);
        (void)nextTracer.applyUsiMove(move.move);
        auto *nextMoveItem = new QTableWidgetItem(
            JosekiPresenter::usiMoveToJapanese(move.nextMove, plyNumber + 1, nextTracer));
        nextMoveItem->setTextAlignment(Qt::AlignCenter);
        m_tableWidget->setItem(i, 3, nextMoveItem);

        addRowButton(m_tableWidget, i, 4, tr("編集"), ButtonStyles::tableEditButton(),
                     this, &JosekiWindow::onEditButtonClicked);
        addRowButton(m_tableWidget, i, 5, tr("削除"), ButtonStyles::tableDeleteButton(),
                     this, &JosekiWindow::onDeleteButtonClicked);

        m_tableWidget->setItem(i, 6, numericItem(locale, move.value));
        m_tableWidget->setItem(i, 7, numericItem(locale, move.depth));
        m_tableWidget->setItem(i, 8, numericItem(locale, move.frequency));
        m_tableWidget->setItem(i, 9, new QTableWidgetItem(move.comment));
    }

    m_tableWidget->resizeColumnToContents(2);
    m_tableWidget->resizeColumnToContents(3);
    updateStatusDisplay();
}

void JosekiWindow::clearTable()
{
    m_tableWidget->setRowCount(0);
    m_currentMoves.clear();
    updateStatusDisplay();
}

void JosekiWindow::updateStatusDisplay()
{
    bool hasData = !m_repository->isEmpty();
    if (m_fileStatusLabel) {
        if (hasData) {
            m_fileStatusLabel->setText(tr("✓読込済"));
            m_fileStatusLabel->setStyleSheet(QStringLiteral("color: green; font-weight: bold;"));
        } else {
            m_fileStatusLabel->setText(QString());
            m_fileStatusLabel->setStyleSheet(QString());
        }
    }
    if (m_stopButton)
        m_stopButton->setVisible(hasData);
    if (m_statusLabel) {
        QStringList statusParts;
        if (!m_currentFilePath.isEmpty())
            statusParts << tr("ファイル: %1").arg(QFileInfo(m_currentFilePath).fileName());
        else
            statusParts << tr("ファイル: 未選択");
        statusParts << tr("局面数: %1").arg(m_repository->positionCount());
        if (!m_displayEnabled)
            statusParts << tr("【停止中】");
        else if (!m_currentMoves.isEmpty())
            statusParts << tr("定跡: %1件").arg(m_currentMoves.size());
        m_statusLabel->setText(statusParts.join(QStringLiteral("  |  ")));
    }
    if (m_emptyGuideLabel && m_tableWidget) {
        bool showGuide = m_displayEnabled && m_currentMoves.isEmpty() && !m_currentSfen.isEmpty();
        m_emptyGuideLabel->setVisible(showGuide);
        if (showGuide) {
            m_emptyGuideLabel->setGeometry(m_tableWidget->geometry());
            m_emptyGuideLabel->raise();
        }
    }
}

void JosekiWindow::updatePositionSummary()
{
    if (!m_positionSummaryLabel) return;
    if (m_currentSfen.isEmpty()) {
        m_positionSummaryLabel->setText(tr("(未設定)"));
        m_currentSfenLabel->setText(QString());
        return;
    }
    const QStringList parts = m_currentSfen.split(QChar(' '));
    int plyNumber = 1;
    QString turn = tr("先手");
    if (parts.size() >= 2)
        turn = (parts[1] == QStringLiteral("b")) ? tr("先手") : tr("後手");
    if (parts.size() >= 4) {
        bool ok;
        plyNumber = parts[3].toInt(&ok);
        if (!ok) plyNumber = 1;
    }
    QString positionDesc;
    if (plyNumber == 1 && !parts.isEmpty()) {
        if (parts[0] == QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL"))
            positionDesc = tr("初期配置");
        else
            positionDesc = tr("駒落ち");
    } else {
        positionDesc = tr("%1手目").arg(plyNumber);
    }
    m_positionSummaryLabel->setText(tr("%1 (%2番)").arg(positionDesc, turn));
    m_positionSummaryLabel->setToolTip(m_currentSfen);
    m_currentSfenLabel->setText(tr("局面SFEN: %1").arg(m_currentSfen));
}

void JosekiWindow::updateWindowTitle()
{
    QString title = tr("定跡ウィンドウ");
    if (!m_currentFilePath.isEmpty())
        title = QFileInfo(m_currentFilePath).fileName() + QStringLiteral(" - ") + title;
    if (m_modified)
        title = QStringLiteral("* ") + title;
    setWindowTitle(title);
}

void JosekiWindow::setModified(bool modified)
{
    m_modified = modified;
    m_saveButton->setEnabled(!m_currentFilePath.isEmpty() && modified);
    updateWindowTitle();
    if (m_dockWidget) {
        QString dockTitle = tr("定跡");
        if (m_modified) dockTitle += QStringLiteral(" *");
        m_dockWidget->setWindowTitle(dockTitle);
    }
    if (m_fileStatusLabel) {
        if (m_modified) {
            m_fileStatusLabel->setText(tr("未保存"));
            m_fileStatusLabel->setStyleSheet(QStringLiteral("color: #cc6600; font-weight: bold;"));
        } else if (!m_repository->isEmpty()) {
            m_fileStatusLabel->setText(tr("✓読込済"));
            m_fileStatusLabel->setStyleSheet(QStringLiteral("color: green; font-weight: bold;"));
        } else {
            m_fileStatusLabel->setText(QString());
            m_fileStatusLabel->setStyleSheet(QString());
        }
    }
}

bool JosekiWindow::confirmDiscardChanges()
{
    if (!m_modified) return true;
    QMessageBox msgBox(this);
    msgBox.setWindowTitle(tr("確認"));
    msgBox.setText(tr("定跡データに未保存の変更があります。\n変更を破棄しますか？"));
    msgBox.setIcon(QMessageBox::Question);
    QPushButton *saveBtn = msgBox.addButton(tr("保存"), QMessageBox::AcceptRole);
    QPushButton *discardBtn = msgBox.addButton(tr("破棄"), QMessageBox::DestructiveRole);
    msgBox.addButton(tr("キャンセル"), QMessageBox::RejectRole);
    msgBox.setDefaultButton(saveBtn);
    msgBox.exec();
    if (msgBox.clickedButton() == saveBtn) {
        onSaveButtonClicked();
        return !m_modified;
    }
    if (msgBox.clickedButton() == discardBtn) return true;
    return false;
}
