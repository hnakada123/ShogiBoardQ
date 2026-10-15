/// @file josekiwindowui.cpp
/// @brief JosekiWindow の画面: 構築・文字サイズ・設定の読み書き・表と状態表示の更新

#include "josekiwindow.h"
#include "buttonstyles.h"
#include "dialogutils.h"
#include "josekibooksession.h"
#include "josekipresenter.h"
#include "josekirepository.h"
#include "josekisettings.h"
#include "kifumovedelegate.h"
#include "kifupresentation.h"
#include "logcategories.h"
#include "sfenreplay.h"
#include "shogiboard.h"
#include "tablestyles.h"

#include <QAction>
#include <QCheckBox>
#include <QFileInfo>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QLabel>
#include <QLocale>
#include <QMenu>
#include <QPushButton>
#include <QTableWidget>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <utility>

namespace {
constexpr QSize kDefaultSize{950, 600};

QTableWidgetItem *numericItem(const QLocale &locale, int value)
{
    auto *item = new QTableWidgetItem(locale.toString(value));
    item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    return item;
}

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

// 行のボタンを用意する。列ごとに文字と接続先は決まっているので、既にある行のボタンは
// 使い回す（ボタンの生成とスタイルシートの適用が、表の作り直しで最も重い）。
void ensureRowButton(QTableWidget *table, int row, int col, const QString &text, const QString &style,
                     JosekiWindow *win, void (JosekiWindow::*slot)())
{
    if (auto *existing = qobject_cast<QPushButton*>(table->cellWidget(row, col))) {
        existing->setProperty("row", row);
        existing->setFont(win->font());
        return;
    }
    auto *btn = new QPushButton(text, win);
    btn->setProperty("row", row);
    btn->setFont(win->font());
    btn->setStyleSheet(style);
    btn->setCursor(Qt::PointingHandCursor);
    QObject::connect(btn, &QPushButton::clicked, win, slot);
    table->setCellWidget(row, col, btn);
}
} // namespace

// ============================================================
// 構築
// ============================================================

void JosekiWindow::setupUi()
{
    setWindowTitle(tr("定跡ウィンドウ"));
    resize(kDefaultSize);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(6);
    mainLayout->setContentsMargins(8, 8, 8, 8);

    const QString buttonStyle = ButtonStyles::panelToolButton() + QStringLiteral("QPushButton { padding: 4px 8px; }");

    // --- ツールバー: ファイル・表示・操作 ---
    auto *toolbarLayout = new QHBoxLayout();
    toolbarLayout->setSpacing(12);

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
    m_recentFilesMenu = new QMenu(this);
    m_recentFilesMenu->setObjectName(QStringLiteral("josekiRecentMenu"));
    m_recentFilesMenu->setProperty("automationMenu", true);
    m_recentButton->setMenu(m_recentFilesMenu);
    for (auto *button : {m_newButton, m_openButton, m_saveButton, m_saveAsButton, m_recentButton})
        fileGroupLayout->addWidget(button);
    toolbarLayout->addWidget(fileGroup, 0, Qt::AlignLeft);

    QHBoxLayout *displayGroupLayout = nullptr;
    QGroupBox *displayGroup = makeGroup(tr("表示"), this, &displayGroupLayout);
    auto *fontDecrease = makeToolButton(tr("A-"), tr("フォントサイズを縮小"), ButtonStyles::panelToolButton(), this);
    auto *fontIncrease = makeToolButton(tr("A+"), tr("フォントサイズを拡大"), ButtonStyles::panelToolButton(), this);
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
    mainLayout->addLayout(toolbarLayout);

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

    // --- 定跡手の表 ---
    m_tableWidget = new QTableWidget(this);
    auto *moveDelegate = new KifuMoveDelegate(m_tableWidget);
    m_tableWidget->setItemDelegateForColumn(2, moveDelegate);
    m_tableWidget->setItemDelegateForColumn(3, moveDelegate);
    m_tableWidget->setObjectName(QStringLiteral("josekiTable"));
    m_tableWidget->setColumnCount(10);
    m_tableWidget->setHorizontalHeaderLabels({tr("No."), tr("着手"), tr("定跡手"), tr("予想応手"), tr("編集"),
                                              tr("削除"), tr("評価値"), tr("深さ"), tr("出現頻度"), tr("コメント")});
    m_tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tableWidget->setAlternatingRowColors(true);
    m_tableWidget->setShowGrid(false);
    m_tableWidget->setWordWrap(false);
    m_tableWidget->verticalHeader()->setVisible(false);
    m_tableWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    const int defaultWidths[] = {40, 50, 120, 120, 50, 50, 70, 50, 80};
    for (int col = 0; col < 9; ++col) m_tableWidget->setColumnWidth(col, defaultWidths[col]);
    m_tableWidget->horizontalHeader()->setStretchLastSection(true);
    mainLayout->addWidget(m_tableWidget, 1);

    m_emptyGuideLabel = new QLabel(m_tableWidget->viewport());
    m_emptyGuideLabel->setWordWrap(true);
    m_emptyGuideLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    auto *emptyLayout = new QVBoxLayout(m_tableWidget->viewport());
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

    connect(m_newButton, &QPushButton::clicked, this, &JosekiWindow::onNewButtonClicked);
    connect(m_openButton, &QPushButton::clicked, this, &JosekiWindow::onOpenButtonClicked);
    connect(m_saveButton, &QPushButton::clicked, this, &JosekiWindow::onSaveButtonClicked);
    connect(m_saveAsButton, &QPushButton::clicked, this, &JosekiWindow::onSaveAsButtonClicked);
    connect(m_addMoveButton, &QPushButton::clicked, this, &JosekiWindow::onAddMoveButtonClicked);
    connect(fontIncrease, &QPushButton::clicked, this, &JosekiWindow::onFontSizeIncrease);
    connect(fontDecrease, &QPushButton::clicked, this, &JosekiWindow::onFontSizeDecrease);
    connect(m_autoLoadCheckBox, &QCheckBox::checkStateChanged, this, &JosekiWindow::onAutoLoadCheckBoxChanged);
    connect(m_stopButton, &QPushButton::clicked, this, &JosekiWindow::onStopButtonClicked);
    connect(m_showSfenDetailBtn, &QPushButton::toggled, this, &JosekiWindow::onSfenDetailToggled);
    connect(m_tableWidget, &QTableWidget::cellDoubleClicked, this, &JosekiWindow::onTableDoubleClicked);
    connect(m_tableWidget, &QTableWidget::customContextMenuRequested, this, &JosekiWindow::onTableContextMenu);
    connect(m_actionPlay, &QAction::triggered, this, &JosekiWindow::onContextMenuPlay);
    connect(m_actionEdit, &QAction::triggered, this, &JosekiWindow::onContextMenuEdit);
    connect(m_actionDelete, &QAction::triggered, this, &JosekiWindow::onContextMenuDelete);
    connect(actionCopyMove, &QAction::triggered, this, &JosekiWindow::onContextMenuCopyMove);

    updateStatusDisplay();
    updateWindowTitle();
}

void JosekiWindow::applyFontSize()
{
    // メニュー（履歴・マージ・表の右クリック）も DialogUtils が同じ文字にする
    const QFont font = m_fontHelper.applyTo(this);
    const int fontSize = m_fontHelper.fontSize();
    const QFontMetrics fm(font);

    QHeaderView *header = m_tableWidget->horizontalHeader();
    header->setFont(font);
    header->setFixedHeight(fm.height() + 12);
    m_tableWidget->verticalHeader()->setDefaultSectionSize(fm.height() + 12);
    m_tableWidget->setStyleSheet(TableStyles::thinking(fontSize) + QStringLiteral(
        "QTableView { alternate-background-color: #f7f9fb; }"
        "QTableView::item { padding: 3px 6px; }"));
    for (int col = 0; col < m_tableWidget->columnCount() - 1; ++col) {
        QString sample = m_tableWidget->horizontalHeaderItem(col)->text();
        if (col == 2 || col == 3) sample = QStringLiteral("▲７六歩(77)");
        if (col == 8) sample = QStringLiteral("999,999,999");
        m_tableWidget->setColumnWidth(col, qMax(m_tableWidget->columnWidth(col), fm.horizontalAdvance(sample) + 24));
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
    for (int col = 0; col < widths.size() && col < m_tableWidget->columnCount() - 1; ++col) {
        if (widths.at(col) > 0) m_tableWidget->setColumnWidth(col, widths.at(col));
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
    for (int i = 0; i < m_tableWidget->columnCount(); ++i) widths.append(m_tableWidget->columnWidth(i));
    JosekiSettings::setJosekiWindowColumnWidths(widths);
}

// ============================================================
// 定跡手の表
// ============================================================

void JosekiWindow::updateJosekiDisplay()
{
    refreshHumanCanPlay();

    if (!m_pendingAutoLoadPath.isEmpty()) {
        const QString pathToLoad = std::exchange(m_pendingAutoLoadPath, QString());
        qCDebug(lcUi) << "Performing deferred auto-load:" << pathToLoad;
        loadAndApplyFileAsync(pathToLoad);
        return;
    }

    // 局面が変わるたび（1手ごと）に呼ばれる。表はいったん空にせず、行数を合わせて中身を
    // 入れ替える。状態表示（ラベルのスタイルと案内の表示）は最後に1回だけ更新する。
    updatePositionSummary();
    m_displayedSfen = m_currentSfen;

    QList<JosekiMove> moves;
    QString sfenLine;
    if (m_displayEnabled && !m_currentSfen.isEmpty()) {
        const JosekiRepository *repository = m_session->repository();
        const QString normalizedSfen = JosekiPresenter::normalizeSfen(m_currentSfen);
        if (repository->containsPosition(normalizedSfen)) {
            moves = repository->movesForPosition(normalizedSfen);
            const QString sPly = repository->sfenWithPly(normalizedSfen);
            sfenLine = tr("定跡SFEN: %1").arg(sPly.isEmpty() ? normalizedSfen : sPly);
        } else {
            sfenLine = tr("定跡: (該当なし)");
        }
    }
    m_currentMoves = moves;
    m_sfenLineLabel->setText(sfenLine);
    rebuildTableRows(moves);
    updateStatusDisplay();
}

void JosekiWindow::rebuildTableRows(const QList<JosekiMove> &moves)
{
    const int rows = static_cast<int>(moves.size());
    // 作り直していたときと同じく、選択と表示位置は初めに戻す
    m_tableWidget->setCurrentItem(nullptr);
    m_tableWidget->clearSelection();
    m_tableWidget->setRowCount(rows);
    if (rows == 0) return;
    m_tableWidget->scrollToTop();

    QLocale locale = QLocale::system();
    locale.setNumberOptions(QLocale::DefaultNumberOptions);
    const QString buttonStyle = ButtonStyles::panelToolButton();
    const QString playText = tr("着手");
    const QString editText = tr("編集");
    const QString deleteText = tr("削除");
    const QString doubleClickTip = tr("ダブルクリックで着手");

    for (int i = 0; i < rows; ++i) {
        const JosekiMove &move = moves[i];

        auto *noItem = new QTableWidgetItem(QString::number(i + 1));
        noItem->setTextAlignment(Qt::AlignCenter);
        m_tableWidget->setItem(i, 0, noItem);

        if (m_humanCanPlay)
            ensureRowButton(m_tableWidget, i, 1, playText, buttonStyle, this, &JosekiWindow::onPlayButtonClicked);
        else
            m_tableWidget->removeCellWidget(i, 1);

        auto *moveItem = new QTableWidgetItem(JosekiPresenter::usiMoveToJapanese(move.move, m_currentSfen));
        moveItem->setTextAlignment(Qt::AlignCenter);
        moveItem->setToolTip(m_humanCanPlay ? move.move + QLatin1Char('\n') + doubleClickTip : move.move);
        m_tableWidget->setItem(i, 2, moveItem);

        const QString nextSfen = SfenReplay::positionAfter(m_currentSfen, move.move);
        auto *nextMoveItem = new QTableWidgetItem(JosekiPresenter::usiMoveToJapanese(move.nextMove, nextSfen));
        nextMoveItem->setToolTip(move.nextMove);
        nextMoveItem->setTextAlignment(Qt::AlignCenter);
        m_tableWidget->setItem(i, 3, nextMoveItem);

        ensureRowButton(m_tableWidget, i, 4, editText, buttonStyle, this, &JosekiWindow::onEditButtonClicked);
        ensureRowButton(m_tableWidget, i, 5, deleteText, buttonStyle, this, &JosekiWindow::onDeleteButtonClicked);

        m_tableWidget->setItem(i, 6, numericItem(locale, move.value));
        m_tableWidget->setItem(i, 7, numericItem(locale, move.depth));
        m_tableWidget->setItem(i, 8, numericItem(locale, move.frequency));
        auto *commentItem = new QTableWidgetItem(move.comment);
        commentItem->setToolTip(move.comment);
        m_tableWidget->setItem(i, 9, commentItem);
    }
}

void JosekiWindow::clearTable()
{
    m_tableWidget->setRowCount(0);
    m_currentMoves.clear();
    m_displayedSfen.clear();
}

// ============================================================
// 状態表示
// ============================================================

void JosekiWindow::updateStatusDisplay()
{
    const QString &filePath = m_session->filePath();
    const bool modified = m_session->isModified();
    const bool hasData = !m_session->repository()->isEmpty();
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

    QStringList statusParts;
    statusParts << (filePath.isEmpty() ? tr("ファイル: 未選択") : tr("ファイル: %1").arg(QFileInfo(filePath).fileName()));
    statusParts << tr("局面数: %1").arg(QLocale().toString(m_session->repository()->positionCount()));
    statusParts << (m_displayEnabled ? tr("定跡: %1件").arg(m_currentMoves.size()) : tr("【停止中】"));
    m_statusLabel->setText(statusParts.join(QStringLiteral("  |  ")));

    const bool showGuide = m_currentMoves.isEmpty();
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
    const JosekiPresenter::PositionInfo info = JosekiPresenter::describePosition(m_currentSfen);
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
    m_filePathLabel->setText(m_session->filePath());
    m_filePathLabel->setStyleSheet(QString());
}

void JosekiWindow::showTransientStatus(const QString &text)
{
    m_statusLabel->setText(text);
    QTimer::singleShot(2000, this, &JosekiWindow::onRestoreStatusDisplay);
}
