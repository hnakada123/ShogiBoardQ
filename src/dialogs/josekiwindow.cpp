/// @file josekiwindow.cpp
/// @brief 定跡ウィンドウクラスの実装

#include "josekiwindow.h"
#include "josekimovedialog.h"
#include "josekimergedialog.h"
#include "settingsservice.h"
#include "sfenpositiontracer.h"
#include "kiftosfenconverter.h"
#include "kifdisplayitem.h"

#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QFile>
#include <QTextStream>
#include <QMessageBox>
#include <QDebug>
#include <QFileInfo>
#include <QDir>
#include <QLocale>
#include <QToolButton>
#include <QClipboard>
#include <QApplication>
#include <QTimer>

JosekiWindow::JosekiWindow(QWidget *parent)
    : QWidget(parent, Qt::Window)  // Qt::Window フラグで独立ウィンドウとして表示
    , m_openButton(nullptr)
    , m_newButton(nullptr)
    , m_saveButton(nullptr)
    , m_saveAsButton(nullptr)
    , m_recentButton(nullptr)
    , m_recentFilesMenu(nullptr)
    , m_filePathLabel(nullptr)
    , m_fileStatusLabel(nullptr)
    , m_fontIncreaseBtn(nullptr)
    , m_fontDecreaseBtn(nullptr)
    , m_autoLoadCheckBox(nullptr)
    , m_stopButton(nullptr)
    , m_addMoveButton(nullptr)
    , m_mergeButton(nullptr)
    , m_mergeMenu(nullptr)
    , m_currentSfenLabel(nullptr)
    , m_sfenLineLabel(nullptr)
    , m_statusLabel(nullptr)
    , m_positionSummaryLabel(nullptr)
    , m_emptyGuideLabel(nullptr)
    , m_noticeLabel(nullptr)
    , m_showSfenDetailBtn(nullptr)
    , m_sfenDetailWidget(nullptr)
    , m_tableContextMenu(nullptr)
    , m_actionPlay(nullptr)
    , m_actionEdit(nullptr)
    , m_actionDelete(nullptr)
    , m_actionCopyMove(nullptr)
    , m_tableWidget(nullptr)
    , m_fontSize(10)
    , m_humanCanPlay(true)  // デフォルトは着手可能
    , m_autoLoadEnabled(true)
    , m_displayEnabled(true)
    , m_modified(false)
{
    setupUi();
    loadSettings();
}

void JosekiWindow::setupUi()
{
    // ウィンドウタイトルとサイズの設定
    setWindowTitle(tr("定跡ウィンドウ"));
    resize(950, 600);

    // メインレイアウト
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(6);
    mainLayout->setContentsMargins(8, 8, 8, 8);

    // ============================================================
    // ツールバー行: 4つのグループを横並び
    // ============================================================
    QHBoxLayout *toolbarLayout = new QHBoxLayout();
    toolbarLayout->setSpacing(12);
    
    // ボタンスタイル定義
    const QString fileBtnStyle = QStringLiteral(
        "QPushButton { background-color: #e3f2fd; border: 1px solid #90caf9; border-radius: 3px; padding: 4px 8px; }"
        "QPushButton:hover { background-color: #bbdefb; }"
        "QPushButton:pressed { background-color: #90caf9; }"
        "QPushButton:disabled { background-color: #f5f5f5; color: #9e9e9e; border: 1px solid #e0e0e0; }");
    const QString fontBtnStyle = QStringLiteral(
        "QPushButton { background-color: #e3f2fd; border: 1px solid #90caf9; border-radius: 3px; }"
        "QPushButton:hover { background-color: #bbdefb; }"
        "QPushButton:pressed { background-color: #90caf9; }");
    const QString editBtnStyle = QStringLiteral(
        "QPushButton { background-color: #e8f5e9; border: 1px solid #a5d6a7; border-radius: 3px; padding: 4px 8px; }"
        "QPushButton:hover { background-color: #c8e6c9; }"
        "QPushButton:pressed { background-color: #a5d6a7; }");
    const QString editToolBtnStyle = QStringLiteral(
        "QToolButton { background-color: #e8f5e9; border: 1px solid #a5d6a7; border-radius: 3px; padding: 4px 8px; }"
        "QToolButton:hover { background-color: #c8e6c9; }"
        "QToolButton:pressed { background-color: #a5d6a7; }"
        "QToolButton::menu-indicator { image: none; }");
    const QString stopBtnStyle = QStringLiteral(
        "QPushButton { background-color: #fff3e0; border: 1px solid #ffcc80; border-radius: 3px; padding: 4px 8px; }"
        "QPushButton:hover { background-color: #ffe0b2; }"
        "QPushButton:pressed { background-color: #ffcc80; }"
        "QPushButton:checked { background-color: #ffcc80; }");

    // --- ファイル操作グループ ---
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

    // --- 表示設定グループ ---
    QGroupBox *displayGroup = new QGroupBox(tr("表示"), this);
    QHBoxLayout *displayGroupLayout = new QHBoxLayout(displayGroup);
    displayGroupLayout->setContentsMargins(8, 4, 8, 4);
    displayGroupLayout->setSpacing(4);

    m_fontIncreaseBtn = new QPushButton(tr("A+"), this);
    m_fontIncreaseBtn->setToolTip(tr("フォントサイズを拡大"));
    m_fontIncreaseBtn->setFixedWidth(36);
    m_fontIncreaseBtn->setStyleSheet(fontBtnStyle);
    displayGroupLayout->addWidget(m_fontIncreaseBtn);

    m_fontDecreaseBtn = new QPushButton(tr("A-"), this);
    m_fontDecreaseBtn->setToolTip(tr("フォントサイズを縮小"));
    m_fontDecreaseBtn->setFixedWidth(36);
    m_fontDecreaseBtn->setStyleSheet(fontBtnStyle);
    displayGroupLayout->addWidget(m_fontDecreaseBtn);

    m_autoLoadCheckBox = new QCheckBox(tr("自動読込"), this);
    m_autoLoadCheckBox->setToolTip(tr("定跡ウィンドウ表示時に前回のファイルを自動で読み込む"));
    m_autoLoadCheckBox->setChecked(true);
    displayGroupLayout->addWidget(m_autoLoadCheckBox);

    toolbarLayout->addWidget(displayGroup);

    // --- 操作グループ ---
    QGroupBox *operationGroup = new QGroupBox(tr("操作"), this);
    QHBoxLayout *operationGroupLayout = new QHBoxLayout(operationGroup);
    operationGroupLayout->setContentsMargins(8, 4, 8, 4);
    operationGroupLayout->setSpacing(4);

    m_addMoveButton = new QPushButton(tr("＋追加"), this);
    m_addMoveButton->setToolTip(tr("現在の局面に定跡手を追加"));
    m_addMoveButton->setStyleSheet(editBtnStyle);
    operationGroupLayout->addWidget(m_addMoveButton);

    // マージボタン（ドロップダウンメニュー付き）
    m_mergeButton = new QToolButton(this);
    m_mergeButton->setText(tr("マージ ▼"));
    m_mergeButton->setToolTip(tr("棋譜から定跡をマージ"));
    m_mergeButton->setPopupMode(QToolButton::InstantPopup);
    m_mergeButton->setStyleSheet(editToolBtnStyle);

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

    // ============================================================
    // ファイル情報行
    // ============================================================
    QHBoxLayout *fileInfoLayout = new QHBoxLayout();
    
    QLabel *fileLabel = new QLabel(tr("ファイル:"), this);
    fileInfoLayout->addWidget(fileLabel);
    
    m_filePathLabel = new QLabel(tr("未選択"), this);
    m_filePathLabel->setStyleSheet(QStringLiteral("color: gray;"));
    m_filePathLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    fileInfoLayout->addWidget(m_filePathLabel, 1);
    
    m_fileStatusLabel = new QLabel(this);
    m_fileStatusLabel->setFixedWidth(80);
    fileInfoLayout->addWidget(m_fileStatusLabel);
    
    mainLayout->addLayout(fileInfoLayout);

    // ============================================================
    // 局面情報行（コンパクト表示）
    // ============================================================
    QHBoxLayout *positionInfoLayout = new QHBoxLayout();
    
    QLabel *positionLabel = new QLabel(tr("局面:"), this);
    positionInfoLayout->addWidget(positionLabel);
    
    m_positionSummaryLabel = new QLabel(tr("(未設定)"), this);
    m_positionSummaryLabel->setStyleSheet(QStringLiteral("color: #0066cc; font-weight: bold;"));
    positionInfoLayout->addWidget(m_positionSummaryLabel);
    
    // 詳細表示ボタン
    m_showSfenDetailBtn = new QPushButton(tr("詳細"), this);
    m_showSfenDetailBtn->setToolTip(tr("SFENの詳細を表示/非表示"));
    m_showSfenDetailBtn->setCheckable(true);
    m_showSfenDetailBtn->setFixedWidth(50);
    positionInfoLayout->addWidget(m_showSfenDetailBtn);
    
    positionInfoLayout->addStretch();
    
    mainLayout->addLayout(positionInfoLayout);
    
    // SFEN詳細表示（初期状態は非表示）
    m_sfenDetailWidget = new QWidget(this);
    QVBoxLayout *sfenDetailLayout = new QVBoxLayout(m_sfenDetailWidget);
    sfenDetailLayout->setContentsMargins(20, 0, 0, 0);
    sfenDetailLayout->setSpacing(2);
    
    // 局面SFEN（青系）
    m_currentSfenLabel = new QLabel(this);
    m_currentSfenLabel->setStyleSheet(QStringLiteral("color: #0066cc; font-family: monospace; font-size: 9pt;"));
    m_currentSfenLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_currentSfenLabel->setWordWrap(true);
    sfenDetailLayout->addWidget(m_currentSfenLabel);
    
    // 定跡SFEN（緑系）
    m_sfenLineLabel = new QLabel(this);
    m_sfenLineLabel->setStyleSheet(QStringLiteral("color: #228b22; font-family: monospace; font-size: 9pt;"));
    m_sfenLineLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_sfenLineLabel->setWordWrap(true);
    sfenDetailLayout->addWidget(m_sfenLineLabel);
    
    m_sfenDetailWidget->setVisible(false);
    mainLayout->addWidget(m_sfenDetailWidget);

    // ============================================================
    // 定跡表示用テーブル
    // ============================================================
    m_tableWidget = new QTableWidget(this);
    m_tableWidget->setColumnCount(10);
    
    QStringList headers;
    headers << tr("No.") << tr("着手") << tr("定跡手") << tr("予想応手") << tr("編集") 
            << tr("削除") << tr("評価値") << tr("深さ") << tr("出現頻度") << tr("コメント");
    m_tableWidget->setHorizontalHeaderLabels(headers);
    
    // テーブルの設定
    m_tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tableWidget->setAlternatingRowColors(true);
    m_tableWidget->verticalHeader()->setVisible(false);
    
    // コンテキストメニューを有効化
    m_tableWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    
    // カラム幅の設定
    m_tableWidget->setColumnWidth(0, 40);   // No.
    m_tableWidget->setColumnWidth(1, 50);   // 着手ボタン
    m_tableWidget->setColumnWidth(2, 120);  // 定跡手
    m_tableWidget->setColumnWidth(3, 120);  // 予想応手
    m_tableWidget->setColumnWidth(4, 50);   // 編集
    m_tableWidget->setColumnWidth(5, 50);   // 削除
    m_tableWidget->setColumnWidth(6, 70);   // 評価値
    m_tableWidget->setColumnWidth(7, 50);   // 深さ
    m_tableWidget->setColumnWidth(8, 80);   // 出現頻度
    m_tableWidget->horizontalHeader()->setStretchLastSection(true);  // コメント列を伸縮
    
    mainLayout->addWidget(m_tableWidget, 1);
    
    // ============================================================
    // 空状態ガイダンス（テーブルの上に重ねて表示）
    // ============================================================
    m_emptyGuideLabel = new QLabel(this);
    m_emptyGuideLabel->setText(
        tr("<div style='text-align: center; color: #888;'>"
           "<p style='font-size: 14pt; margin-bottom: 10px;'>定跡が登録されていません</p>"
           "<p>「＋追加」ボタンで手動追加、または<br>"
           "「マージ」メニューから棋譜を取り込めます</p>"
           "</div>"));
    m_emptyGuideLabel->setAlignment(Qt::AlignCenter);
    m_emptyGuideLabel->setVisible(false);

    // ============================================================
    // ステータスバー
    // ============================================================
    QFrame *statusFrame = new QFrame(this);
    statusFrame->setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
    QHBoxLayout *statusLayout = new QHBoxLayout(statusFrame);
    statusLayout->setContentsMargins(8, 2, 8, 2);
    
    m_statusLabel = new QLabel(this);
    statusLayout->addWidget(m_statusLabel, 1);
    
    mainLayout->addWidget(statusFrame);
    
    // ============================================================
    // 注意書きラベル
    // ============================================================
    m_noticeLabel = new QLabel(this);
    m_noticeLabel->setText(tr("※ 編集・削除後は「保存」ボタンで定跡ファイルに保存してください"));
    m_noticeLabel->setStyleSheet(QStringLiteral("color: #cc6600; font-size: 9pt;"));
    mainLayout->addWidget(m_noticeLabel);

    // ============================================================
    // コンテキストメニュー作成
    // ============================================================
    m_tableContextMenu = new QMenu(this);
    m_actionPlay = m_tableContextMenu->addAction(tr("着手"));
    m_tableContextMenu->addSeparator();
    m_actionEdit = m_tableContextMenu->addAction(tr("編集..."));
    m_actionDelete = m_tableContextMenu->addAction(tr("削除"));
    m_tableContextMenu->addSeparator();
    m_actionCopyMove = m_tableContextMenu->addAction(tr("指し手をコピー"));

    // ============================================================
    // シグナル・スロット接続
    // ============================================================
    connect(m_newButton, &QPushButton::clicked,
            this, &JosekiWindow::onNewButtonClicked);
    connect(m_openButton, &QPushButton::clicked,
            this, &JosekiWindow::onOpenButtonClicked);
    connect(m_saveButton, &QPushButton::clicked,
            this, &JosekiWindow::onSaveButtonClicked);
    connect(m_saveAsButton, &QPushButton::clicked,
            this, &JosekiWindow::onSaveAsButtonClicked);
    connect(m_addMoveButton, &QPushButton::clicked,
            this, &JosekiWindow::onAddMoveButtonClicked);
    connect(m_fontIncreaseBtn, &QPushButton::clicked,
            this, &JosekiWindow::onFontSizeIncrease);
    connect(m_fontDecreaseBtn, &QPushButton::clicked,
            this, &JosekiWindow::onFontSizeDecrease);
    connect(m_autoLoadCheckBox, &QCheckBox::checkStateChanged,
            this, &JosekiWindow::onAutoLoadCheckBoxChanged);
    connect(m_stopButton, &QPushButton::clicked,
            this, &JosekiWindow::onStopButtonClicked);
    
    // SFEN詳細表示トグル
    connect(m_showSfenDetailBtn, &QPushButton::toggled,
            m_sfenDetailWidget, &QWidget::setVisible);
    
    // テーブルのダブルクリックで着手
    connect(m_tableWidget, &QTableWidget::cellDoubleClicked,
            this, &JosekiWindow::onTableDoubleClicked);
    
    // コンテキストメニュー
    connect(m_tableWidget, &QTableWidget::customContextMenuRequested,
            this, &JosekiWindow::onTableContextMenu);
    connect(m_actionPlay, &QAction::triggered,
            this, &JosekiWindow::onContextMenuPlay);
    connect(m_actionEdit, &QAction::triggered,
            this, &JosekiWindow::onContextMenuEdit);
    connect(m_actionDelete, &QAction::triggered,
            this, &JosekiWindow::onContextMenuDelete);
    connect(m_actionCopyMove, &QAction::triggered,
            this, &JosekiWindow::onContextMenuCopyMove);
    
    // 初期状態表示を更新
    updateStatusDisplay();
    updateWindowTitle();
}

void JosekiWindow::loadSettings()
{
    // フォントサイズを読み込み
    m_fontSize = SettingsService::josekiWindowFontSize();
    applyFontSize();
    
    // ウィンドウサイズを読み込み
    QSize savedSize = SettingsService::josekiWindowSize();
    if (savedSize.isValid() && savedSize.width() > 100 && savedSize.height() > 100) {
        resize(savedSize);
    }
    
    // 自動読込設定を読み込み
    m_autoLoadEnabled = SettingsService::josekiWindowAutoLoadEnabled();
    m_autoLoadCheckBox->setChecked(m_autoLoadEnabled);
    
    // 最近使ったファイルリストを読み込み
    m_recentFiles = SettingsService::josekiWindowRecentFiles();
    updateRecentFilesMenu();

    // 最後に開いた定跡ファイルのパスを保存（遅延読込のため）
    // 実際の読み込みはshowEvent()で行う
    if (m_autoLoadEnabled) {
        QString lastFilePath = SettingsService::josekiWindowLastFilePath();
        if (!lastFilePath.isEmpty() && QFileInfo::exists(lastFilePath)) {
            m_pendingAutoLoad = true;
            m_pendingAutoLoadPath = lastFilePath;
            // ファイルパスは表示しておく（読込中であることがわかるように）
            m_filePathLabel->setText(lastFilePath + tr(" (未読込)"));
        }
    }
}

void JosekiWindow::saveSettings()
{
    // フォントサイズを保存
    SettingsService::setJosekiWindowFontSize(m_fontSize);
    
    // ウィンドウサイズを保存
    SettingsService::setJosekiWindowSize(size());
    
    // 最後に開いた定跡ファイルを保存
    SettingsService::setJosekiWindowLastFilePath(m_currentFilePath);
    
    // 自動読込設定を保存
    SettingsService::setJosekiWindowAutoLoadEnabled(m_autoLoadEnabled);
    
    // 最近使ったファイルリストを保存
    SettingsService::setJosekiWindowRecentFiles(m_recentFiles);
}

void JosekiWindow::closeEvent(QCloseEvent *event)
{
    // 未保存の変更がある場合は確認
    if (!confirmDiscardChanges()) {
        event->ignore();
        return;
    }

    saveSettings();
    QWidget::closeEvent(event);
}

void JosekiWindow::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    // 注意: 遅延読込はupdateJosekiDisplay()内で行う
}

void JosekiWindow::applyFontSize()
{
    // ウィンドウ全体のフォントサイズを更新
    QFont font = this->font();
    font.setPointSize(m_fontSize);
    setFont(font);
    
    // テーブルのフォントも更新
    if (m_tableWidget) {
        m_tableWidget->setFont(font);
        
        // ヘッダーのフォントも更新
        m_tableWidget->horizontalHeader()->setFont(font);
        
        // 行の高さを調整
        m_tableWidget->verticalHeader()->setDefaultSectionSize(m_fontSize + 16);
    }
    
    // ファイルパスラベルのフォントサイズを更新
    if (m_filePathLabel) {
        QFont filePathFont = m_filePathLabel->font();
        filePathFont.setPointSize(m_fontSize);
        m_filePathLabel->setFont(filePathFont);
    }
    
    // ファイルステータスラベルのフォントサイズを更新
    if (m_fileStatusLabel) {
        QFont statusFont = m_fileStatusLabel->font();
        statusFont.setPointSize(m_fontSize);
        m_fileStatusLabel->setFont(statusFont);
    }
    
    // 局面サマリーラベルのフォントサイズを更新
    if (m_positionSummaryLabel) {
        QFont summaryFont = m_positionSummaryLabel->font();
        summaryFont.setPointSize(m_fontSize);
        m_positionSummaryLabel->setFont(summaryFont);
    }
    
    // ステータスバーのフォントサイズを更新
    if (m_statusLabel) {
        QFont statusBarFont = m_statusLabel->font();
        statusBarFont.setPointSize(m_fontSize);
        m_statusLabel->setFont(statusBarFont);
    }
    
    // 空状態ガイダンスのフォントサイズを更新
    if (m_emptyGuideLabel) {
        // HTMLスタイルで指定しているので、全体のフォントを設定
        QFont guideFont = m_emptyGuideLabel->font();
        guideFont.setPointSize(m_fontSize);
        m_emptyGuideLabel->setFont(guideFont);
    }
    
    // 注意書きラベルのフォントサイズを更新
    if (m_noticeLabel) {
        int noticeFontSize = qMax(m_fontSize - 1, 6);
        m_noticeLabel->setStyleSheet(QStringLiteral("color: #cc6600; font-size: %1pt;").arg(noticeFontSize));
    }
    
    // マージメニューのフォントサイズを更新
    if (m_mergeMenu) {
        QFont menuFont = m_mergeMenu->font();
        menuFont.setPointSize(m_fontSize);
        m_mergeMenu->setFont(menuFont);
    }
    
    // 最近使ったファイルメニューのフォントサイズを更新
    if (m_recentFilesMenu) {
        QFont recentMenuFont = m_recentFilesMenu->font();
        recentMenuFont.setPointSize(m_fontSize);
        m_recentFilesMenu->setFont(recentMenuFont);
    }
    
    // コンテキストメニューのフォントサイズを更新
    if (m_tableContextMenu) {
        QFont contextMenuFont = m_tableContextMenu->font();
        contextMenuFont.setPointSize(m_fontSize);
        m_tableContextMenu->setFont(contextMenuFont);
    }
    
    // SFEN詳細表示ラベルのフォントサイズも更新（monospace維持、少し小さめ）
    int sfenFontSize = qMax(m_fontSize - 1, 6);
    
    // 局面SFEN（青系）
    if (m_currentSfenLabel) {
        QString currentSfenStyleSheet = QStringLiteral("color: #0066cc; font-family: monospace; font-size: %1pt;").arg(sfenFontSize);
        m_currentSfenLabel->setStyleSheet(currentSfenStyleSheet);
    }
    
    // 定跡SFEN（緑系）
    if (m_sfenLineLabel) {
        QString sfenLineStyleSheet = QStringLiteral("color: #228b22; font-family: monospace; font-size: %1pt;").arg(sfenFontSize);
        m_sfenLineLabel->setStyleSheet(sfenLineStyleSheet);
    }
    
    // 表示を更新
    updateJosekiDisplay();
}

void JosekiWindow::onFontSizeIncrease()
{
    if (m_fontSize < 24) {
        m_fontSize++;
        applyFontSize();
    }
}

void JosekiWindow::onFontSizeDecrease()
{
    if (m_fontSize > 6) {
        m_fontSize--;
        applyFontSize();
    }
}

void JosekiWindow::onOpenButtonClicked()
{
    // 未保存の変更がある場合は確認
    if (!confirmDiscardChanges()) {
        return;
    }
    
    // 最後に開いたディレクトリを取得
    QString startDir;
    if (!m_currentFilePath.isEmpty()) {
        QFileInfo fi(m_currentFilePath);
        startDir = fi.absolutePath();
    }
    
    // ファイル選択ダイアログを表示
    QString filePath = QFileDialog::getOpenFileName(
        this,
        tr("定跡ファイルを開く"),
        startDir,
        tr("定跡ファイル (*.db);;すべてのファイル (*)")
    );

    if (!filePath.isEmpty()) {
        if (loadJosekiFile(filePath)) {
            m_currentFilePath = filePath;
            m_filePathLabel->setText(filePath);
            m_filePathLabel->setStyleSheet(QString());  // デフォルトスタイルに戻す
            setModified(false);
            
            // 最近使ったファイルリストに追加
            addToRecentFiles(filePath);
            
            // 設定を保存
            saveSettings();
            
            // 現在の局面で定跡を検索・表示
            updateJosekiDisplay();
        }
    }
}

bool JosekiWindow::loadJosekiFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("エラー"), 
                             tr("ファイルを開けませんでした: %1").arg(filePath));
        return false;
    }

    m_josekiData.clear();
    m_sfenWithPlyMap.clear();
    m_mergeRegisteredMoves.clear();  // マージ登録済みセットもクリア

    QTextStream in(&file);
    QString line;
    QString currentSfen;
    QString normalizedSfen;
    
    // フォーマット検証用フラグ
    bool hasValidHeader = false;
    bool hasSfenLine = false;
    bool hasMoveLine = false;
    int lineNumber = 0;
    int invalidMoveLineCount = 0;

    while (!in.atEnd()) {
        line = in.readLine().trimmed();
        lineNumber++;
        
        // Windows改行コード(\r)を除去
        line.remove(QLatin1Char('\r'));
        
        // 空行をスキップ
        if (line.isEmpty()) {
            continue;
        }
        
        // コメント行（#で始まる行）の処理
        if (line.startsWith(QLatin1Char('#'))) {
            // やねうら王定跡フォーマットのヘッダーを確認
            if (line.contains(QStringLiteral("YANEURAOU")) || 
                line.contains(QStringLiteral("yaneuraou"))) {
                hasValidHeader = true;
                continue;
            }
            
            // ヘッダー以外の#行は直前の指し手のコメントとして扱う
            if (!normalizedSfen.isEmpty() && m_josekiData.contains(normalizedSfen)) {
                QVector<JosekiMove> &moves = m_josekiData[normalizedSfen];
                if (!moves.isEmpty()) {
                    // #を除いた部分をコメントとして追加
                    QString commentText = line.mid(1);  // #を除去
                    if (!moves.last().comment.isEmpty()) {
                        // 既にコメントがある場合は追加
                        moves.last().comment += QStringLiteral(" ") + commentText;
                    } else {
                        moves.last().comment = commentText;
                    }
                }
            }
            continue;
        }
        
        // sfen行の処理
        if (line.startsWith(QStringLiteral("sfen "))) {
            currentSfen = line.mid(5).trimmed();
            currentSfen.remove(QLatin1Char('\r'));  // 念のため再度除去
            normalizedSfen = normalizeSfen(currentSfen);
            // 元のSFEN（手数付き）を保持
            if (!m_sfenWithPlyMap.contains(normalizedSfen)) {
                m_sfenWithPlyMap[normalizedSfen] = currentSfen;
            }
            hasSfenLine = true;
            continue;
        }
        
        // 指し手行の処理（次のsfen行が来るまで同じ局面の指し手として追加）
        if (!normalizedSfen.isEmpty()) {
            // 指し手行をパース
            const QStringList parts = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
            if (parts.size() >= 5) {
                // 指し手の形式を簡易チェック（USI形式: 例 7g7f, P*5e, 8h2b+）
                const QString &moveStr = parts[0];
                bool validMove = false;
                
                // 駒打ち形式: X*YZ (例: P*5e)
                if (moveStr.size() >= 4 && moveStr.at(1) == QLatin1Char('*')) {
                    QChar piece = moveStr.at(0).toUpper();
                    if (QString("PLNSGBR").contains(piece)) {
                        validMove = true;
                    }
                }
                // 通常移動形式: XYZW または XYZW+ (例: 7g7f, 8h2b+)
                else if (moveStr.size() >= 4) {
                    bool validFormat = true;
                    // 筋は1-9の数字
                    if (moveStr.at(0) < QLatin1Char('1') || moveStr.at(0) > QLatin1Char('9')) validFormat = false;
                    // 段はa-iのアルファベット
                    if (moveStr.at(1) < QLatin1Char('a') || moveStr.at(1) > QLatin1Char('i')) validFormat = false;
                    if (moveStr.at(2) < QLatin1Char('1') || moveStr.at(2) > QLatin1Char('9')) validFormat = false;
                    if (moveStr.at(3) < QLatin1Char('a') || moveStr.at(3) > QLatin1Char('i')) validFormat = false;
                    validMove = validFormat;
                }
                
                if (!validMove) {
                    invalidMoveLineCount++;
                    qDebug() << "[JosekiWindow] Invalid move format at line" << lineNumber << ":" << moveStr;
                }
                
                JosekiMove move;
                move.move = parts[0];           // 指し手
                move.nextMove = parts[1];       // 次の指し手
                move.value = parts[2].toInt();  // 評価値
                move.depth = parts[3].toInt();  // 深さ
                move.frequency = parts[4].toInt(); // 出現頻度
                
                // コメントがあれば取得（6番目以降を結合）
                if (parts.size() > 5) {
                    QStringList commentParts;
                    for (int i = 5; i < parts.size(); ++i) {
                        commentParts.append(parts[i]);
                    }
                    move.comment = commentParts.join(QLatin1Char(' '));
                }
                
                // 同じ局面のエントリに追加
                m_josekiData[normalizedSfen].append(move);
                hasMoveLine = true;
            } else if (parts.size() > 0) {
                // 5フィールド未満の行は不正
                invalidMoveLineCount++;
                qDebug() << "[JosekiWindow] Invalid line format at line" << lineNumber 
                         << ": expected 5+ fields, got" << parts.size();
            }
        }
    }

    file.close();
    
    // フォーマット検証
    if (!hasValidHeader) {
        QMessageBox::warning(this, tr("フォーマットエラー"), 
            tr("このファイルはやねうら王定跡フォーマット(YANEURAOU-DB2016)ではありません。\n"
               "ヘッダー行（#YANEURAOU-DB2016 等）が見つかりませんでした。\n\n"
               "ファイル: %1").arg(filePath));
        m_josekiData.clear();
        return false;
    }
    
    if (!hasSfenLine) {
        QMessageBox::warning(this, tr("フォーマットエラー"), 
            tr("定跡ファイルにSFEN行が見つかりませんでした。\n\n"
               "やねうら王定跡フォーマットでは「sfen 」で始まる局面行が必要です。\n\n"
               "ファイル: %1").arg(filePath));
        m_josekiData.clear();
        return false;
    }
    
    if (!hasMoveLine) {
        QMessageBox::warning(this, tr("フォーマットエラー"), 
            tr("定跡ファイルに有効な指し手行が見つかりませんでした。\n\n"
               "やねうら王定跡フォーマットでは指し手行に少なくとも5つのフィールド\n"
               "（指し手 予想応手 評価値 深さ 出現頻度）が必要です。\n\n"
               "ファイル: %1").arg(filePath));
        m_josekiData.clear();
        return false;
    }
    
    qDebug() << "[JosekiWindow] Loaded" << m_josekiData.size() << "positions from" << filePath;
    if (invalidMoveLineCount > 0) {
        qDebug() << "[JosekiWindow] Warning:" << invalidMoveLineCount << "lines had invalid format";
    }
    
    // デバッグ：平手初期局面があるか確認
    QString hirate = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b -");
    if (m_josekiData.contains(hirate)) {
        qDebug() << "[JosekiWindow] Hirate position has" << m_josekiData[hirate].size() << "moves";
    }
    
    // ステータス表示を更新
    updateStatusDisplay();
    
    return true;
}

QString JosekiWindow::normalizeSfen(const QString &sfen) const
{
    // SFEN文字列から手数部分を除去して正規化
    // 形式: "盤面 手番 持ち駒 手数"
    const QStringList parts = sfen.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (parts.size() >= 3) {
        // 手数を除いた部分を返す（盤面 手番 持ち駒）
        return parts[0] + QLatin1Char(' ') + parts[1] + QLatin1Char(' ') + parts[2];
    }
    return sfen;
}

void JosekiWindow::setCurrentSfen(const QString &sfen)
{
    m_currentSfen = sfen;
    m_currentSfen.remove(QLatin1Char('\r'));  // Windows改行コードを除去
    
    // "startpos"の場合は平手初期局面に変換
    if (m_currentSfen == QStringLiteral("startpos")) {
        m_currentSfen = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");
    }
    
    // SFENラベルを更新（手数を含めた元のSFENを表示）
    if (m_currentSfenLabel) {
        if (m_currentSfen.isEmpty()) {
            m_currentSfenLabel->setText(tr("(未設定)"));
        } else {
            m_currentSfenLabel->setText(m_currentSfen);
        }
    }
    
    qDebug() << "[JosekiWindow] setCurrentSfen:" << m_currentSfen;
    qDebug() << "[JosekiWindow] Normalized:" << normalizeSfen(m_currentSfen);
    
    updateJosekiDisplay();
}

void JosekiWindow::setHumanCanPlay(bool canPlay)
{
    if (m_humanCanPlay != canPlay) {
        m_humanCanPlay = canPlay;
        // 表示を更新して着手ボタンの表示/非表示を反映
        updateJosekiDisplay();
    }
}

void JosekiWindow::updateJosekiDisplay()
{
    qDebug() << "[JosekiWindow] updateJosekiDisplay() called";

    // 遅延読込: 定跡データが未読込の場合、ここで読み込む
    if (m_pendingAutoLoad && !m_pendingAutoLoadPath.isEmpty()) {
        m_pendingAutoLoad = false;  // 二重読込防止
        QString pathToLoad = m_pendingAutoLoadPath;
        m_pendingAutoLoadPath.clear();

        qDebug() << "[JosekiWindow] Performing deferred auto-load:" << pathToLoad;

        if (loadJosekiFile(pathToLoad)) {
            m_currentFilePath = pathToLoad;
            m_filePathLabel->setText(pathToLoad);
            m_filePathLabel->setStyleSheet(QString());
            setModified(false);
        } else {
            m_filePathLabel->setText(QString());
        }
    }

    clearTable();

    // 表示が停止中の場合は何もしない
    if (!m_displayEnabled) {
        qDebug() << "[JosekiWindow] updateJosekiDisplay: display is disabled";
        return;
    }

    if (m_currentSfen.isEmpty()) {
        qDebug() << "[JosekiWindow] updateJosekiDisplay: currentSfen is empty";
        return;
    }
    
    // 現在の局面を正規化
    QString normalizedSfen = normalizeSfen(m_currentSfen);
    
    qDebug() << "[JosekiWindow] Looking for:" << normalizedSfen;
    qDebug() << "[JosekiWindow] Joseki data has" << m_josekiData.size() << "entries";
    
    // 局面サマリーを更新
    updatePositionSummary();
    
    // 定跡データを検索
    if (!m_josekiData.contains(normalizedSfen)) {
        // 一致する定跡がない場合は空のテーブルを表示
        qDebug() << "[JosekiWindow] No match found for current position";
        m_currentMoves.clear();
        m_sfenLineLabel->setText(tr("定跡: (該当なし)"));
        updateStatusDisplay();
        return;
    }
    
    const QVector<JosekiMove> &moves = m_josekiData[normalizedSfen];
    
    // 現在表示中の定跡手リストを保存
    m_currentMoves = moves;
    
    // 定跡ファイルのSFEN行を表示
    if (m_sfenWithPlyMap.contains(normalizedSfen)) {
        m_sfenLineLabel->setText(tr("定跡SFEN: %1").arg(m_sfenWithPlyMap[normalizedSfen]));
    } else {
        m_sfenLineLabel->setText(tr("定跡SFEN: %1").arg(normalizedSfen));
    }
    
    qDebug() << "[JosekiWindow] Found" << moves.size() << "moves for this position";
    
    // SFENから手数を取得（手番の次の手が何手目か）
    int plyNumber = 1;  // デフォルト
    const QStringList sfenParts = m_currentSfen.split(QChar(' '));
    if (sfenParts.size() >= 4) {
        bool ok;
        plyNumber = sfenParts.at(3).toInt(&ok);
        if (!ok) plyNumber = 1;
    }
    
    // SfenPositionTracerを初期化して現在の局面をセット
    SfenPositionTracer tracer;
    tracer.setFromSfen(m_currentSfen);
    
    m_tableWidget->setRowCount(static_cast<int>(moves.size()));
    
    for (int i = 0; i < moves.size(); ++i) {
        const JosekiMove &move = moves[i];
        
        // No.
        QTableWidgetItem *noItem = new QTableWidgetItem(QString::number(i + 1));
        noItem->setTextAlignment(Qt::AlignCenter);
        m_tableWidget->setItem(i, 0, noItem);
        
        // 着手ボタン（青系の配色）- 人間の手番の時のみ表示
        if (m_humanCanPlay) {
            QPushButton *playButton = new QPushButton(tr("着手"), this);
            playButton->setProperty("row", i);
            playButton->setStyleSheet(QStringLiteral(
                "QPushButton {"
                "  background-color: #4a90d9;"
                "  color: white;"
                "  border: none;"
                "  border-radius: 3px;"
                "  padding: 2px 8px;"
                "}"
                "QPushButton:hover {"
                "  background-color: #357abd;"
                "}"
                "QPushButton:pressed {"
                "  background-color: #2a5f8f;"
                "}"
            ));
            connect(playButton, &QPushButton::clicked, this, &JosekiWindow::onPlayButtonClicked);
            m_tableWidget->setCellWidget(i, 1, playButton);
        }
        
        // 定跡手（日本語表記に変換）
        QString moveJapanese = usiMoveToJapanese(move.move, plyNumber, tracer);
        QTableWidgetItem *moveItem = new QTableWidgetItem(moveJapanese);
        moveItem->setTextAlignment(Qt::AlignCenter);
        moveItem->setToolTip(tr("ダブルクリックで着手"));
        m_tableWidget->setItem(i, 2, moveItem);
        
        // 予想応手（次の指し手）を日本語表記に変換
        SfenPositionTracer nextTracer;
        nextTracer.setFromSfen(m_currentSfen);
        nextTracer.applyUsiMove(move.move);  // 定跡手を適用
        QString nextMoveJapanese = usiMoveToJapanese(move.nextMove, plyNumber + 1, nextTracer);
        QTableWidgetItem *nextMoveItem = new QTableWidgetItem(nextMoveJapanese);
        nextMoveItem->setTextAlignment(Qt::AlignCenter);
        m_tableWidget->setItem(i, 3, nextMoveItem);
        
        // 編集ボタン（緑系の配色）
        QPushButton *editButton = new QPushButton(tr("編集"), this);
        editButton->setProperty("row", i);
        editButton->setStyleSheet(QStringLiteral(
            "QPushButton {"
            "  background-color: #5cb85c;"
            "  color: white;"
            "  border: none;"
            "  border-radius: 3px;"
            "  padding: 2px 8px;"
            "}"
            "QPushButton:hover {"
            "  background-color: #449d44;"
            "}"
            "QPushButton:pressed {"
            "  background-color: #398439;"
            "}"
        ));
        connect(editButton, &QPushButton::clicked, this, &JosekiWindow::onEditButtonClicked);
        m_tableWidget->setCellWidget(i, 4, editButton);
        
        // 削除ボタン（赤系の配色）
        QPushButton *deleteButton = new QPushButton(tr("削除"), this);
        deleteButton->setProperty("row", i);
        deleteButton->setStyleSheet(QStringLiteral(
            "QPushButton {"
            "  background-color: #d9534f;"
            "  color: white;"
            "  border: none;"
            "  border-radius: 3px;"
            "  padding: 2px 8px;"
            "}"
            "QPushButton:hover {"
            "  background-color: #c9302c;"
            "}"
            "QPushButton:pressed {"
            "  background-color: #ac2925;"
            "}"
        ));
        connect(deleteButton, &QPushButton::clicked, this, &JosekiWindow::onDeleteButtonClicked);
        m_tableWidget->setCellWidget(i, 5, deleteButton);
        
        // 評価値（3桁区切り）
        QLocale locale = QLocale::system();
        locale.setNumberOptions(QLocale::DefaultNumberOptions);
        QString valueStr = locale.toString(move.value);
        QTableWidgetItem *valueItem = new QTableWidgetItem(valueStr);
        valueItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_tableWidget->setItem(i, 6, valueItem);
        
        // 深さ（3桁区切り、右寄せ）
        QString depthStr = locale.toString(move.depth);
        QTableWidgetItem *depthItem = new QTableWidgetItem(depthStr);
        depthItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_tableWidget->setItem(i, 7, depthItem);
        
        // 出現頻度（3桁区切り）
        QString freqStr = locale.toString(move.frequency);
        QTableWidgetItem *freqItem = new QTableWidgetItem(freqStr);
        freqItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_tableWidget->setItem(i, 8, freqItem);
        
        // コメント
        QTableWidgetItem *commentItem = new QTableWidgetItem(move.comment);
        m_tableWidget->setItem(i, 9, commentItem);
    }
    
    // ステータス表示を更新
    updateStatusDisplay();
}

void JosekiWindow::clearTable()
{
    m_tableWidget->setRowCount(0);
    m_currentMoves.clear();
    updateStatusDisplay();
}

void JosekiWindow::onPlayButtonClicked()
{
    // クリックされたボタンを取得
    QPushButton *button = qobject_cast<QPushButton*>(sender());
    if (!button) {
        qDebug() << "[JosekiWindow] onPlayButtonClicked: sender is not a QPushButton";
        return;
    }
    
    // ボタンに設定された行番号を取得
    bool ok;
    int row = button->property("row").toInt(&ok);
    if (!ok || row < 0 || row >= m_currentMoves.size()) {
        qDebug() << "[JosekiWindow] onPlayButtonClicked: invalid row" << row;
        return;
    }
    
    // 該当する定跡手を取得
    const JosekiMove &move = m_currentMoves[row];
    
    qDebug() << "[JosekiWindow] onPlayButtonClicked: row=" << row << "usiMove=" << move.move;
    
    // シグナルを発行して指し手をMainWindowに通知
    emit josekiMoveSelected(move.move);
}

void JosekiWindow::onAutoLoadCheckBoxChanged(Qt::CheckState state)
{
    m_autoLoadEnabled = (state == Qt::Checked);
    qDebug() << "[JosekiWindow] Auto load enabled:" << m_autoLoadEnabled;
}

void JosekiWindow::onStopButtonClicked()
{
    m_displayEnabled = !m_stopButton->isChecked();
    
    if (m_displayEnabled) {
        m_stopButton->setText(tr("■ 停止"));
        m_stopButton->setStyleSheet(QString());
        // 表示を再開した場合は現在の局面を再表示
        updateJosekiDisplay();
    } else {
        m_stopButton->setText(tr("▶ 再開"));
        m_stopButton->setStyleSheet(QStringLiteral("color: #cc0000; font-weight: bold;"));
        // 停止した場合はテーブルをクリア
        clearTable();
    }
    
    updateStatusDisplay();
    qDebug() << "[JosekiWindow] Display enabled:" << m_displayEnabled;
}

void JosekiWindow::updateStatusDisplay()
{
    // ファイル読込状態を更新
    if (m_fileStatusLabel) {
        if (!m_josekiData.isEmpty()) {
            m_fileStatusLabel->setText(tr("✓読込済"));
            m_fileStatusLabel->setStyleSheet(QStringLiteral("color: green; font-weight: bold;"));
        } else {
            m_fileStatusLabel->setText(tr(""));
            m_fileStatusLabel->setStyleSheet(QString());
        }
    }
    
    // ステータスバーを更新
    if (m_statusLabel) {
        QStringList statusParts;
        
        // ファイル情報
        if (!m_currentFilePath.isEmpty()) {
            QFileInfo fi(m_currentFilePath);
            statusParts << tr("ファイル: %1").arg(fi.fileName());
        } else {
            statusParts << tr("ファイル: 未選択");
        }
        
        // 局面数
        statusParts << tr("局面数: %1").arg(m_josekiData.size());
        
        // 表示状態
        if (!m_displayEnabled) {
            statusParts << tr("【停止中】");
        } else if (!m_currentMoves.isEmpty()) {
            statusParts << tr("定跡: %1件").arg(m_currentMoves.size());
        }
        
        m_statusLabel->setText(statusParts.join(QStringLiteral("  |  ")));
    }
    
    // 空状態ガイダンスの表示制御
    if (m_emptyGuideLabel && m_tableWidget) {
        bool showGuide = m_displayEnabled && m_currentMoves.isEmpty() && !m_currentSfen.isEmpty();
        m_emptyGuideLabel->setVisible(showGuide);
        
        // ガイダンスラベルの位置をテーブルの中央に配置
        if (showGuide) {
            m_emptyGuideLabel->setGeometry(m_tableWidget->geometry());
            m_emptyGuideLabel->raise();
        }
    }
}

void JosekiWindow::onMoveResult(bool success, const QString &usiMove)
{
    if (!success) {
        QMessageBox::warning(this, tr("着手エラー"), 
            tr("定跡手「%1」を指すことができませんでした。\n\n"
               "この定跡手は現在の局面では合法手ではない可能性があります。\n"
               "定跡ファイルのデータに誤りがある可能性があります。").arg(usiMove));
    }
}

// 全角数字と漢数字のテーブル
static const QString kZenkakuDigits = QStringLiteral(" １２３４５６７８９");  // インデックス1-9
static const QString kKanjiRanks = QStringLiteral(" 一二三四五六七八九");    // インデックス1-9

QString JosekiWindow::pieceToKanji(QChar pieceChar, bool promoted)
{
    switch (pieceChar.toUpper().toLatin1()) {
    case 'P': return promoted ? QStringLiteral("と") : QStringLiteral("歩");
    case 'L': return promoted ? QStringLiteral("杏") : QStringLiteral("香");
    case 'N': return promoted ? QStringLiteral("圭") : QStringLiteral("桂");
    case 'S': return promoted ? QStringLiteral("全") : QStringLiteral("銀");
    case 'G': return QStringLiteral("金");
    case 'B': return promoted ? QStringLiteral("馬") : QStringLiteral("角");
    case 'R': return promoted ? QStringLiteral("龍") : QStringLiteral("飛");
    case 'K': return QStringLiteral("玉");
    default: return QStringLiteral("?");
    }
}

QString JosekiWindow::usiMoveToJapanese(const QString &usiMove, int plyNumber, SfenPositionTracer &tracer) const
{
    if (usiMove.isEmpty()) return QString();

    // 手番記号（奇数手=先手▲、偶数手=後手△）
    QString teban = (plyNumber % 2 != 0) ? QStringLiteral("▲") : QStringLiteral("△");

    // 駒打ちのパターン: P*7f
    if (usiMove.size() >= 4 && usiMove.at(1) == QChar('*')) {
        QChar pieceChar = usiMove.at(0);
        int toFile = usiMove.at(2).toLatin1() - '0';
        int toRank = usiMove.at(3).toLatin1() - 'a' + 1;

        QString kanji = pieceToKanji(pieceChar);

        QString result = teban;
        if (toFile >= 1 && toFile <= 9) result += kZenkakuDigits.at(toFile);
        if (toRank >= 1 && toRank <= 9) result += kKanjiRanks.at(toRank);
        result += kanji + QStringLiteral("打");

        return result;
    }

    // 通常移動のパターン: 7g7f, 7g7f+
    if (usiMove.size() >= 4) {
        int fromFile = usiMove.at(0).toLatin1() - '0';
        QChar fromRankLetter = usiMove.at(1);
        int fromRank = fromRankLetter.toLatin1() - 'a' + 1;
        int toFile = usiMove.at(2).toLatin1() - '0';
        int toRank = usiMove.at(3).toLatin1() - 'a' + 1;
        bool promotes = (usiMove.size() >= 5 && usiMove.at(4) == QChar('+'));

        // 範囲チェック
        if (fromFile < 1 || fromFile > 9 || fromRank < 1 || fromRank > 9 ||
            toFile < 1 || toFile > 9 || toRank < 1 || toRank > 9) {
            return teban + usiMove;  // 変換できない場合はそのまま
        }

        // 移動元の駒を取得
        QString pieceToken = tracer.tokenAtFileRank(fromFile, fromRankLetter);
        bool isPromoted = pieceToken.startsWith(QChar('+'));
        QChar pieceChar = isPromoted ? pieceToken.at(1) : (pieceToken.isEmpty() ? QChar('?') : pieceToken.at(0));

        QString result = teban;

        // 移動先
        if (toFile >= 1 && toFile <= 9) result += kZenkakuDigits.at(toFile);
        if (toRank >= 1 && toRank <= 9) result += kKanjiRanks.at(toRank);

        // 駒種
        QString kanji = pieceToKanji(pieceChar, isPromoted);
        result += kanji;

        // 成り
        if (promotes) {
            result += QStringLiteral("成");
        }

        // 移動元
        result += QStringLiteral("(") + QString::number(fromFile) + QString::number(fromRank) + QStringLiteral(")");

        return result;
    }

    return teban + usiMove;  // 変換できない場合はそのまま
}

void JosekiWindow::onNewButtonClicked()
{
    // 未保存の変更がある場合は確認
    if (!confirmDiscardChanges()) {
        return;
    }
    
    // 定跡データをクリア
    m_josekiData.clear();
    m_sfenWithPlyMap.clear();
    m_mergeRegisteredMoves.clear();  // マージ登録済みセットもクリア
    m_currentFilePath.clear();
    m_filePathLabel->setText(tr("新規ファイル（未保存）"));
    m_filePathLabel->setStyleSheet(QStringLiteral("color: blue;"));
    
    setModified(false);
    updateStatusDisplay();
    updateJosekiDisplay();
    
    qDebug() << "[JosekiWindow] Created new empty joseki file";
}

void JosekiWindow::onSaveButtonClicked()
{
    if (m_currentFilePath.isEmpty()) {
        // ファイルパスがない場合は「名前を付けて保存」を呼び出す
        onSaveAsButtonClicked();
        return;
    }
    
    if (saveJosekiFile(m_currentFilePath)) {
        setModified(false);
        qDebug() << "[JosekiWindow] Saved to" << m_currentFilePath;
    }
}

void JosekiWindow::onSaveAsButtonClicked()
{
    // 保存先ディレクトリを決定
    QString startDir;
    if (!m_currentFilePath.isEmpty()) {
        QFileInfo fi(m_currentFilePath);
        startDir = fi.absolutePath();
    }
    
    // ファイル保存ダイアログを表示
    QString filePath = QFileDialog::getSaveFileName(
        this,
        tr("定跡ファイルを保存"),
        startDir,
        tr("定跡ファイル (*.db);;すべてのファイル (*)")
    );
    
    if (filePath.isEmpty()) {
        return;
    }
    
    // 拡張子がない場合は.dbを追加
    if (!filePath.endsWith(QStringLiteral(".db"), Qt::CaseInsensitive)) {
        filePath += QStringLiteral(".db");
    }
    
    if (saveJosekiFile(filePath)) {
        m_currentFilePath = filePath;
        m_filePathLabel->setText(filePath);
        m_filePathLabel->setStyleSheet(QString());
        setModified(false);
        
        // 最近使ったファイルリストに追加
        addToRecentFiles(filePath);
        
        // 設定を保存
        saveSettings();
        
        qDebug() << "[JosekiWindow] Saved as" << filePath;
    }
}

bool JosekiWindow::saveJosekiFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("エラー"),
                             tr("ファイルを保存できませんでした: %1").arg(filePath));
        return false;
    }
    
    QTextStream out(&file);
    
    // ヘッダーを書き込み
    out << QStringLiteral("#YANEURAOU-DB2016 1.00\n");
    
    // 各局面と定跡手を書き込み
    QMapIterator<QString, QVector<JosekiMove>> it(m_josekiData);
    while (it.hasNext()) {
        it.next();
        const QString &normalizedSfen = it.key();
        const QVector<JosekiMove> &moves = it.value();
        
        // 元のSFEN（手数付き）を取得、なければ正規化SFENを使用
        QString sfenToWrite;
        if (m_sfenWithPlyMap.contains(normalizedSfen)) {
            sfenToWrite = m_sfenWithPlyMap[normalizedSfen];
        } else {
            sfenToWrite = normalizedSfen;
        }
        
        // 手数が含まれているか確認し、なければデフォルト値1を追加
        const QStringList parts = sfenToWrite.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (parts.size() == 3) {
            // 手数が含まれていない場合はデフォルト値1を追加
            sfenToWrite += QStringLiteral(" 1");
        }
        
        // SFEN行を書き込み
        out << QStringLiteral("sfen ") << sfenToWrite << QStringLiteral("\n");
        
        // 各指し手を書き込み
        for (const JosekiMove &move : moves) {
            out << move.move << QStringLiteral(" ")
                << move.nextMove << QStringLiteral(" ")
                << move.value << QStringLiteral(" ")
                << move.depth << QStringLiteral(" ")
                << move.frequency
                << QStringLiteral("\n");
            
            // コメントがあれば次の行に#プレフィックス付きで追加
            if (!move.comment.isEmpty()) {
                out << QStringLiteral("#") << move.comment << QStringLiteral("\n");
            }
        }
    }

    // 書き込みステータスを確認
    out.flush();
    if (out.status() != QTextStream::Ok) {
        QMessageBox::warning(this, tr("エラー"),
                             tr("ファイル書き込み中にエラーが発生しました: %1").arg(filePath));
        file.close();
        return false;
    }

    file.close();
    return true;
}

void JosekiWindow::updateWindowTitle()
{
    QString title = tr("定跡ウィンドウ");
    
    if (!m_currentFilePath.isEmpty()) {
        QFileInfo fi(m_currentFilePath);
        title = fi.fileName() + QStringLiteral(" - ") + title;
    }
    
    if (m_modified) {
        title = QStringLiteral("* ") + title;
    }
    
    setWindowTitle(title);
}

void JosekiWindow::setModified(bool modified)
{
    m_modified = modified;
    // 上書保存ボタン: ファイルパスがあり、変更がある場合のみ有効
    m_saveButton->setEnabled(!m_currentFilePath.isEmpty() && modified);
    updateWindowTitle();
}

bool JosekiWindow::confirmDiscardChanges()
{
    if (!m_modified) {
        return true;  // 変更がなければそのまま続行
    }
    
    QMessageBox msgBox(this);
    msgBox.setWindowTitle(tr("確認"));
    msgBox.setText(tr("定跡データに未保存の変更があります。\n変更を破棄しますか？"));
    msgBox.setIcon(QMessageBox::Question);
    
    QPushButton *saveButton = msgBox.addButton(tr("保存"), QMessageBox::AcceptRole);
    QPushButton *discardButton = msgBox.addButton(tr("破棄"), QMessageBox::DestructiveRole);
    msgBox.addButton(tr("キャンセル"), QMessageBox::RejectRole);
    
    msgBox.setDefaultButton(saveButton);
    msgBox.exec();
    
    if (msgBox.clickedButton() == saveButton) {
        onSaveButtonClicked();
        return !m_modified;  // 保存が成功したら続行
    } else if (msgBox.clickedButton() == discardButton) {
        return true;  // 変更を破棄して続行
    } else {
        return false;  // キャンセル
    }
}

void JosekiWindow::addToRecentFiles(const QString &filePath)
{
    // 既に存在する場合は削除（先頭に移動するため）
    m_recentFiles.removeAll(filePath);
    
    // 先頭に追加
    m_recentFiles.prepend(filePath);
    
    // 最大5件に制限
    while (m_recentFiles.size() > 5) {
        m_recentFiles.removeLast();
    }
    
    // メニューを更新
    updateRecentFilesMenu();
}

void JosekiWindow::updateRecentFilesMenu()
{
    m_recentFilesMenu->clear();
    
    if (m_recentFiles.isEmpty()) {
        QAction *emptyAction = m_recentFilesMenu->addAction(tr("（履歴なし）"));
        emptyAction->setEnabled(false);
        return;
    }
    
    for (const QString &filePath : std::as_const(m_recentFiles)) {
        QFileInfo fi(filePath);
        QString displayName = fi.fileName();
        
        QAction *action = m_recentFilesMenu->addAction(displayName);
        action->setData(filePath);
        action->setToolTip(filePath);
        connect(action, &QAction::triggered, this, &JosekiWindow::onRecentFileClicked);
    }
    
    m_recentFilesMenu->addSeparator();
    
    // 履歴をクリアするアクション
    QAction *clearAction = m_recentFilesMenu->addAction(tr("履歴をクリア"));
    connect(clearAction, &QAction::triggered, this, &JosekiWindow::onClearRecentFilesClicked);
}

void JosekiWindow::onClearRecentFilesClicked()
{
    m_recentFiles.clear();
    updateRecentFilesMenu();
    saveSettings();
}

void JosekiWindow::onRecentFileClicked()
{
    QAction *action = qobject_cast<QAction*>(sender());
    if (!action) {
        return;
    }
    
    QString filePath = action->data().toString();
    if (filePath.isEmpty()) {
        return;
    }
    
    // 未保存の変更がある場合は確認
    if (!confirmDiscardChanges()) {
        return;
    }
    
    // ファイルが存在するか確認
    if (!QFileInfo::exists(filePath)) {
        QMessageBox::warning(this, tr("エラー"),
                             tr("ファイルが見つかりません: %1").arg(filePath));
        // リストから削除
        m_recentFiles.removeAll(filePath);
        updateRecentFilesMenu();
        return;
    }
    
    // ファイルを開く
    if (loadJosekiFile(filePath)) {
        m_currentFilePath = filePath;
        m_filePathLabel->setText(filePath);
        m_filePathLabel->setStyleSheet(QString());
        setModified(false);
        
        // 先頭に移動
        addToRecentFiles(filePath);
        
        // 設定を保存
        saveSettings();
        
        // 現在の局面で定跡を検索・表示
        updateJosekiDisplay();
    }
}

void JosekiWindow::onAddMoveButtonClicked()
{
    // 現在の局面が設定されているか確認
    if (m_currentSfen.isEmpty()) {
        QMessageBox::warning(this, tr("定跡手追加"),
                             tr("局面が設定されていません。\n"
                                "将棋盤で局面を表示してから定跡手を追加してください。"));
        return;
    }
    
    // 定跡手追加ダイアログを表示
    JosekiMoveDialog dialog(this, false);
    
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    
    // 入力された定跡手を取得
    JosekiMove newMove;
    newMove.move = dialog.move();
    newMove.nextMove = dialog.nextMove();
    newMove.value = dialog.value();
    newMove.depth = dialog.depth();
    newMove.frequency = dialog.frequency();
    newMove.comment = dialog.comment();
    
    // 正規化されたSFENを取得
    QString normalizedSfen = normalizeSfen(m_currentSfen);
    
    // 元のSFEN（手数付き）を保持（まだ登録されていない場合）
    if (!m_sfenWithPlyMap.contains(normalizedSfen)) {
        // m_currentSfenに手数が含まれているか確認
        const QStringList parts = m_currentSfen.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (parts.size() >= 4) {
            // 手数が含まれている場合はそのまま使用
            m_sfenWithPlyMap[normalizedSfen] = m_currentSfen;
        } else if (parts.size() == 3) {
            // 手数が含まれていない場合はデフォルト値1を追加
            m_sfenWithPlyMap[normalizedSfen] = m_currentSfen + QStringLiteral(" 1");
        } else {
            // それ以外の場合はそのまま使用
            m_sfenWithPlyMap[normalizedSfen] = m_currentSfen;
        }
    }
    
    // 同じ指し手が既に登録されていないかチェック
    if (m_josekiData.contains(normalizedSfen)) {
        const QVector<JosekiMove> &existingMoves = m_josekiData[normalizedSfen];
        for (const JosekiMove &move : existingMoves) {
            if (move.move == newMove.move) {
                QMessageBox::StandardButton result = QMessageBox::question(
                    this, tr("確認"),
                    tr("指し手「%1」は既に登録されています。\n上書きしますか？").arg(newMove.move),
                    QMessageBox::Yes | QMessageBox::No,
                    QMessageBox::No
                );
                
                if (result == QMessageBox::No) {
                    return;
                }
                
                // 既存の指し手を削除
                QVector<JosekiMove> &moves = m_josekiData[normalizedSfen];
                for (int i = 0; i < moves.size(); ++i) {
                    if (moves[i].move == newMove.move) {
                        moves.removeAt(i);
                        break;
                    }
                }
                break;
            }
        }
    }
    
    // 定跡データに追加
    m_josekiData[normalizedSfen].append(newMove);
    
    // 編集状態を更新
    setModified(true);
    
    // 表示を更新
    updateJosekiDisplay();
    
    qDebug() << "[JosekiWindow] Added joseki move:" << newMove.move 
             << "to position:" << normalizedSfen;
}

void JosekiWindow::onEditButtonClicked()
{
    // クリックされたボタンを取得
    QPushButton *button = qobject_cast<QPushButton*>(sender());
    if (!button) {
        qDebug() << "[JosekiWindow] onEditButtonClicked: sender is not a QPushButton";
        return;
    }
    
    // ボタンに設定された行番号を取得
    bool ok;
    int row = button->property("row").toInt(&ok);
    if (!ok || row < 0 || row >= m_currentMoves.size()) {
        qDebug() << "[JosekiWindow] onEditButtonClicked: invalid row" << row;
        return;
    }
    
    // 現在の定跡手を取得
    const JosekiMove &currentMove = m_currentMoves.at(row);
    
    // 定跡手の日本語表記を生成
    int plyNumber = 1;
    const QStringList sfenParts = m_currentSfen.split(QChar(' '));
    if (sfenParts.size() >= 4) {
        bool okPly;
        plyNumber = sfenParts.at(3).toInt(&okPly);
        if (!okPly) plyNumber = 1;
    }
    SfenPositionTracer tracer;
    tracer.setFromSfen(m_currentSfen);
    QString japaneseMoveStr = usiMoveToJapanese(currentMove.move, plyNumber, tracer);
    
    // 編集ダイアログを表示（編集モード）
    JosekiMoveDialog dialog(this, true);
    
    // 現在の値をダイアログに設定（指し手は設定しない＝編集不可）
    dialog.setValue(currentMove.value);
    dialog.setDepth(currentMove.depth);
    dialog.setFrequency(currentMove.frequency);
    dialog.setComment(currentMove.comment);
    
    // 日本語表記の定跡手を表示
    dialog.setEditMoveDisplay(japaneseMoveStr);
    
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    
    // 正規化されたSFENを取得
    QString normalizedSfen = normalizeSfen(m_currentSfen);
    
    // 定跡データを更新
    if (m_josekiData.contains(normalizedSfen)) {
        QVector<JosekiMove> &moves = m_josekiData[normalizedSfen];
        for (int i = 0; i < moves.size(); ++i) {
            if (moves[i].move == currentMove.move) {
                moves[i].value = dialog.value();
                moves[i].depth = dialog.depth();
                moves[i].frequency = dialog.frequency();
                moves[i].comment = dialog.comment();
                
                qDebug() << "[JosekiWindow] Updated joseki move:" << currentMove.move;
                break;
            }
        }
    }
    
    // 編集状態を更新
    setModified(true);
    
    // 表示を更新
    updateJosekiDisplay();
}

void JosekiWindow::onDeleteButtonClicked()
{
    // クリックされたボタンを取得
    QPushButton *button = qobject_cast<QPushButton*>(sender());
    if (!button) {
        qDebug() << "[JosekiWindow] onDeleteButtonClicked: sender is not a QPushButton";
        return;
    }
    
    // ボタンに設定された行番号を取得
    bool ok;
    int row = button->property("row").toInt(&ok);
    if (!ok || row < 0 || row >= m_currentMoves.size()) {
        qDebug() << "[JosekiWindow] onDeleteButtonClicked: invalid row" << row;
        return;
    }
    
    // 現在の定跡手を取得
    const JosekiMove &currentMove = m_currentMoves.at(row);
    
    // 定跡手の日本語表記を生成
    int plyNumber = 1;
    const QStringList sfenParts = m_currentSfen.split(QChar(' '));
    if (sfenParts.size() >= 4) {
        bool okPly;
        plyNumber = sfenParts.at(3).toInt(&okPly);
        if (!okPly) plyNumber = 1;
    }
    SfenPositionTracer tracer;
    tracer.setFromSfen(m_currentSfen);
    QString japaneseMoveStr = usiMoveToJapanese(currentMove.move, plyNumber, tracer);
    
    // 削除確認ダイアログ
    QMessageBox::StandardButton result = QMessageBox::question(
        this, tr("削除確認"),
        tr("定跡手「%1」を削除しますか？").arg(japaneseMoveStr),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No
    );
    
    if (result != QMessageBox::Yes) {
        return;
    }
    
    // 正規化されたSFENを取得
    QString normalizedSfen = normalizeSfen(m_currentSfen);
    
    // 定跡データから削除
    if (m_josekiData.contains(normalizedSfen)) {
        QVector<JosekiMove> &moves = m_josekiData[normalizedSfen];
        for (int i = 0; i < moves.size(); ++i) {
            if (moves[i].move == currentMove.move) {
                moves.removeAt(i);
                qDebug() << "[JosekiWindow] Deleted joseki move:" << currentMove.move;
                
                // この局面の定跡手がなくなった場合はエントリも削除
                if (moves.isEmpty()) {
                    m_josekiData.remove(normalizedSfen);
                    m_sfenWithPlyMap.remove(normalizedSfen);
                    qDebug() << "[JosekiWindow] Removed empty position:" << normalizedSfen;
                }
                break;
            }
        }
    }
    
    // 編集状態を更新
    setModified(true);
    
    // 表示を更新
    updateJosekiDisplay();
}

void JosekiWindow::onMergeFromCurrentKifu()
{
    // ファイルパスが未設定の場合は先に保存先を決定
    if (m_currentFilePath.isEmpty()) {
        QMessageBox::StandardButton result = QMessageBox::question(
            this,
            tr("保存先の指定"),
            tr("定跡ファイルの保存先が設定されていません。\n"
               "OKを選択すると保存先が指定できます。"),
            QMessageBox::Ok | QMessageBox::Cancel,
            QMessageBox::Ok
        );
        
        if (result != QMessageBox::Ok) {
            return;
        }
        
        // 名前を付けて保存ダイアログを表示
        QString filePath = QFileDialog::getSaveFileName(
            this,
            tr("定跡ファイルの保存先を指定"),
            QDir::homePath(),
            tr("定跡ファイル (*.db);;すべてのファイル (*)")
        );
        
        if (filePath.isEmpty()) {
            return;  // キャンセルされた
        }
        
        // 拡張子がなければ追加
        if (!filePath.endsWith(QStringLiteral(".db"), Qt::CaseInsensitive)) {
            filePath += QStringLiteral(".db");
        }
        
        // ファイルパスを設定
        m_currentFilePath = filePath;
        QFileInfo fi(filePath);
        m_filePathLabel->setText(filePath);
        m_filePathLabel->setStyleSheet(QString());
        
        // 空のファイルとして保存
        if (!saveJosekiFile(m_currentFilePath)) {
            QMessageBox::warning(this, tr("エラー"),
                tr("ファイルの保存に失敗しました。"));
            m_currentFilePath.clear();
            m_filePathLabel->setText(tr("新規ファイル（未保存）"));
            m_filePathLabel->setStyleSheet(QStringLiteral("color: blue;"));
            return;
        }
        
        addToRecentFiles(m_currentFilePath);
        updateStatusDisplay();
    }
    
    // MainWindowに棋譜データを要求
    emit requestKifuDataForMerge();
}

void JosekiWindow::setKifuDataForMerge(const QStringList &sfenList, 
                                        const QStringList &moveList,
                                        const QStringList &japaneseMoveList,
                                        int currentPly)
{
    qDebug() << "[JosekiWindow] setKifuDataForMerge: sfenList.size=" << sfenList.size()
             << "moveList.size=" << moveList.size()
             << "japaneseMoveList.size=" << japaneseMoveList.size()
             << "currentPly=" << currentPly;
    
    // 棋譜データが空の場合
    if (moveList.isEmpty()) {
        QMessageBox::information(this, tr("情報"),
            tr("棋譜に指し手がありません。"));
        return;
    }
    
    // マージエントリを作成（指し手のみ、開始局面は除外）
    QVector<KifuMergeEntry> entries;
    
    // データ構造:
    // sfenList[0] = 初期局面（1手目を指す前）
    // sfenList[1] = 1手目後の局面
    // moveList[0] = 1手目のUSI指し手
    // moveList[1] = 2手目のUSI指し手
    // japaneseMoveList[0] = "=== 開始局面 ===" （除外対象）
    // japaneseMoveList[1] = "1 ▲７六歩(77)" （1手目）
    // japaneseMoveList[2] = "2 △３四歩(33)" （2手目）
    // currentPly: 0=開始局面、1=1手目、2=2手目...（0起点）
    
    for (int i = 0; i < moveList.size(); ++i) {
        if (i >= sfenList.size()) break;
        
        // USI指し手が空の場合はスキップ（終局記号など）
        if (moveList[i].isEmpty()) continue;
        
        KifuMergeEntry entry;
        entry.ply = i + 1;  // 1手目から（表示用、1起点）
        entry.sfen = sfenList[i];  // この手を指す前の局面
        entry.usiMove = moveList[i];
        
        // 日本語表記を取得（japaneseMoveListの先頭は「=== 開始局面 ===」なので+1）
        if (i + 1 < japaneseMoveList.size()) {
            entry.japaneseMove = japaneseMoveList[i + 1];
        } else {
            // フォールバック：USI形式をそのまま使用
            entry.japaneseMove = moveList[i];
        }
        
        // 現在の指し手かどうか（currentPlyは0起点なので、i+1と比較）
        entry.isCurrentMove = (i + 1 == currentPly);
        
        entries.append(entry);
    }
    
    if (entries.isEmpty()) {
        QMessageBox::information(this, tr("情報"),
            tr("登録可能な指し手がありません。"));
        return;
    }
    
    // マージダイアログを表示
    JosekiMergeDialog *dialog = new JosekiMergeDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    
    // マージ先の定跡ファイル名を設定
    dialog->setTargetJosekiFile(m_currentFilePath);
    
    // 登録済みの指し手セットを設定
    dialog->setRegisteredMoves(m_mergeRegisteredMoves);
    
    // シグナル接続
    connect(dialog, &JosekiMergeDialog::registerMove,
            this, &JosekiWindow::onMergeRegisterMove);
    
    dialog->setKifuData(entries, currentPly);
    dialog->show();
}

void JosekiWindow::onMergeRegisterMove(const QString &sfen, const QString &sfenWithPly, const QString &usiMove)
{
    qDebug() << "[JosekiWindow] onMergeRegisterMove: sfen=" << sfen
             << "usiMove=" << usiMove;
    
    // 登録済みセットに追加
    QString key = sfen + QStringLiteral(":") + usiMove;
    m_mergeRegisteredMoves.insert(key);
    
    // 既存の定跡手があるか確認
    if (m_josekiData.contains(sfen)) {
        QVector<JosekiMove> &moves = m_josekiData[sfen];
        
        // 同じ指し手が既にあるか確認
        for (int i = 0; i < moves.size(); ++i) {
            if (moves[i].move == usiMove) {
                // 既に登録済みの場合は出現頻度を増やす
                moves[i].frequency += 1;
                qDebug() << "[JosekiWindow] Updated frequency for existing move:" << usiMove;
                updateJosekiDisplay();
                // 自動保存
                if (!m_currentFilePath.isEmpty()) {
                    if (saveJosekiFile(m_currentFilePath)) {
                        setModified(false);
                    }
                }
                return;
            }
        }
        
        // 新しい指し手として追加
        JosekiMove newMove;
        newMove.move = usiMove;
        newMove.nextMove = QStringLiteral("none");
        newMove.value = 0;
        newMove.depth = 0;
        newMove.frequency = 1;
        moves.append(newMove);
        qDebug() << "[JosekiWindow] Added new move to existing position:" << usiMove;
    } else {
        // 新しい局面として追加
        JosekiMove newMove;
        newMove.move = usiMove;
        newMove.nextMove = QStringLiteral("none");
        newMove.value = 0;
        newMove.depth = 0;
        newMove.frequency = 1;
        
        QVector<JosekiMove> moves;
        moves.append(newMove);
        m_josekiData[sfen] = moves;
        
        // 手数付きSFENも保存
        if (!m_sfenWithPlyMap.contains(sfen)) {
            m_sfenWithPlyMap[sfen] = sfenWithPly;
        }
        
        qDebug() << "[JosekiWindow] Added new position with move:" << usiMove;
    }
    
    updateJosekiDisplay();
    // 自動保存
    if (!m_currentFilePath.isEmpty()) {
        if (saveJosekiFile(m_currentFilePath)) {
            setModified(false);
        }
    }
}

void JosekiWindow::onMergeFromKifuFile()
{
    // ファイルパスが未設定の場合は先に保存先を決定
    if (m_currentFilePath.isEmpty()) {
        QMessageBox::StandardButton result = QMessageBox::question(
            this,
            tr("保存先の指定"),
            tr("定跡ファイルの保存先が設定されていません。\n"
               "OKを選択すると保存先が指定できます。"),
            QMessageBox::Ok | QMessageBox::Cancel,
            QMessageBox::Ok
        );
        
        if (result != QMessageBox::Ok) {
            return;
        }
        
        // 名前を付けて保存ダイアログを表示
        QString filePath = QFileDialog::getSaveFileName(
            this,
            tr("定跡ファイルの保存先を指定"),
            QDir::homePath(),
            tr("定跡ファイル (*.db);;すべてのファイル (*)")
        );
        
        if (filePath.isEmpty()) {
            return;  // キャンセルされた
        }
        
        // 拡張子がなければ追加
        if (!filePath.endsWith(QStringLiteral(".db"), Qt::CaseInsensitive)) {
            filePath += QStringLiteral(".db");
        }
        
        // ファイルパスを設定
        m_currentFilePath = filePath;
        m_filePathLabel->setText(filePath);
        m_filePathLabel->setStyleSheet(QString());
        
        // 空のファイルとして保存
        if (!saveJosekiFile(m_currentFilePath)) {
            QMessageBox::warning(this, tr("エラー"),
                tr("ファイルの保存に失敗しました。"));
            m_currentFilePath.clear();
            m_filePathLabel->setText(tr("新規ファイル（未保存）"));
            m_filePathLabel->setStyleSheet(QStringLiteral("color: blue;"));
            return;
        }
        
        addToRecentFiles(m_currentFilePath);
        updateStatusDisplay();
    }
    
    // 棋譜ファイルを選択
    QString kifFilePath = QFileDialog::getOpenFileName(
        this,
        tr("棋譜ファイルを選択"),
        QDir::homePath(),
        tr("KIF形式 (*.kif *.kifu);;すべてのファイル (*)")
    );
    
    if (kifFilePath.isEmpty()) {
        return;  // キャンセルされた
    }
    
    qDebug() << "[JosekiWindow] onMergeFromKifuFile: loading" << kifFilePath;
    
    // KIFファイルを解析
    KifParseResult parseResult;
    QString errorMessage;
    
    if (!KifToSfenConverter::parseWithVariations(kifFilePath, parseResult, &errorMessage)) {
        qDebug() << "[JosekiWindow] onMergeFromKifuFile: parse failed:" << errorMessage;
        QMessageBox::warning(this, tr("エラー"),
            tr("棋譜ファイルの読み込みに失敗しました。\n%1").arg(errorMessage));
        return;
    }
    
    // 本譜からエントリを作成
    const KifLine &mainline = parseResult.mainline;
    
    qDebug() << "[JosekiWindow] onMergeFromKifuFile: parse succeeded";
    qDebug() << "[JosekiWindow]   mainline.baseSfen:" << mainline.baseSfen;
    qDebug() << "[JosekiWindow]   mainline.usiMoves.size():" << mainline.usiMoves.size();
    qDebug() << "[JosekiWindow]   mainline.sfenList.size():" << mainline.sfenList.size();
    qDebug() << "[JosekiWindow]   mainline.disp.size():" << mainline.disp.size();
    qDebug() << "[JosekiWindow]   mainline.endsWithTerminal:" << mainline.endsWithTerminal;
    
    if (!mainline.usiMoves.isEmpty()) {
        qDebug() << "[JosekiWindow]   first usiMove:" << mainline.usiMoves.first();
        qDebug() << "[JosekiWindow]   last usiMove:" << mainline.usiMoves.last();
    }
    
    if (!mainline.sfenList.isEmpty()) {
        qDebug() << "[JosekiWindow]   first sfen:" << mainline.sfenList.first();
    }
    
    if (!mainline.disp.isEmpty()) {
        qDebug() << "[JosekiWindow]   first disp.prettyMove:" << mainline.disp.first().prettyMove;
    }
    
    if (mainline.usiMoves.isEmpty()) {
        qDebug() << "[JosekiWindow] onMergeFromKifuFile: usiMoves is empty!";
        QMessageBox::information(this, tr("情報"),
            tr("棋譜に指し手がありません。"));
        return;
    }
    
    // sfenListが空の場合はbaseSfenとusiMovesから生成
    QStringList sfenList = mainline.sfenList;
    if (sfenList.isEmpty() && !mainline.baseSfen.isEmpty()) {
        qDebug() << "[JosekiWindow] sfenList is empty, building from baseSfen and usiMoves";
        sfenList = SfenPositionTracer::buildSfenRecord(mainline.baseSfen, mainline.usiMoves, false);
        qDebug() << "[JosekiWindow] built sfenList.size():" << sfenList.size();
    }
    
    // マージエントリを作成
    QVector<KifuMergeEntry> entries;
    
    // sfenList[0] = 初期局面、sfenList[i] = i手目後の局面
    // usiMoves[i] = i+1手目のUSI指し手
    // disp[i] = i+1手目の表示情報（ただしdisp[0]は開始局面の場合がある）
    for (int i = 0; i < mainline.usiMoves.size(); ++i) {
        if (i >= sfenList.size()) {
            qDebug() << "[JosekiWindow]   i=" << i << " exceeds sfenList.size(), breaking";
            break;
        }
        
        // USI指し手が空の場合はスキップ
        if (mainline.usiMoves[i].isEmpty()) {
            qDebug() << "[JosekiWindow]   i=" << i << " usiMove is empty, skipping";
            continue;
        }
        
        KifuMergeEntry entry;
        entry.ply = i + 1;  // 1手目から
        entry.sfen = sfenList[i];  // この手を指す前の局面
        entry.usiMove = mainline.usiMoves[i];
        
        // 日本語表記を取得
        // disp配列の構造を確認（先頭が開始局面かどうか）
        int dispIndex = i;
        if (mainline.disp.size() > mainline.usiMoves.size()) {
            // disp[0]が開始局面の場合、指し手はdisp[i+1]
            dispIndex = i + 1;
        }
        
        if (dispIndex < mainline.disp.size() && !mainline.disp[dispIndex].prettyMove.isEmpty()) {
            entry.japaneseMove = mainline.disp[dispIndex].prettyMove;
        } else {
            entry.japaneseMove = mainline.usiMoves[i];
        }
        
        entry.isCurrentMove = false;  // ファイルからの読み込みなので現在位置はなし
        
        qDebug() << "[JosekiWindow]   entry[" << i << "] ply=" << entry.ply
                 << "sfen=" << entry.sfen.left(30) << "..."
                 << "usiMove=" << entry.usiMove
                 << "japaneseMove=" << entry.japaneseMove;
        
        entries.append(entry);
    }
    
    qDebug() << "[JosekiWindow] onMergeFromKifuFile: entries.size()=" << entries.size();
    
    if (entries.isEmpty()) {
        qDebug() << "[JosekiWindow] onMergeFromKifuFile: entries is empty!";
        QMessageBox::information(this, tr("情報"),
            tr("登録可能な指し手がありません。"));
        return;
    }
    
    // マージダイアログを表示
    JosekiMergeDialog *dialog = new JosekiMergeDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    
    // マージ先の定跡ファイル名を設定
    dialog->setTargetJosekiFile(m_currentFilePath);
    
    // 登録済みの指し手セットを設定
    dialog->setRegisteredMoves(m_mergeRegisteredMoves);
    
    // ダイアログタイトルに読み込んだファイル名を追加
    QFileInfo fi(kifFilePath);
    dialog->setWindowTitle(tr("棋譜から定跡にマージ - %1").arg(fi.fileName()));
    
    // シグナル接続
    connect(dialog, &JosekiMergeDialog::registerMove,
            this, &JosekiWindow::onMergeRegisterMove);
    
    dialog->setKifuData(entries, -1);  // currentPlyは-1（選択なし）
    dialog->show();
}

// ============================================================
// 局面サマリー更新
// ============================================================
void JosekiWindow::updatePositionSummary()
{
    if (!m_positionSummaryLabel) return;
    
    if (m_currentSfen.isEmpty()) {
        m_positionSummaryLabel->setText(tr("(未設定)"));
        m_currentSfenLabel->setText(QString());
        return;
    }
    
    // SFENから手数と手番を抽出
    const QStringList parts = m_currentSfen.split(QChar(' '));
    int plyNumber = 1;
    QString turn = tr("先手");
    
    if (parts.size() >= 2) {
        turn = (parts[1] == QStringLiteral("b")) ? tr("先手") : tr("後手");
    }
    if (parts.size() >= 4) {
        bool ok;
        plyNumber = parts[3].toInt(&ok);
        if (!ok) plyNumber = 1;
    }
    
    // 初期配置かどうか判定
    QString positionDesc;
    if (plyNumber == 1 && parts.size() >= 1) {
        if (parts[0] == QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL")) {
            positionDesc = tr("初期配置");
        } else {
            positionDesc = tr("駒落ち");
        }
    } else {
        positionDesc = tr("%1手目").arg(plyNumber);
    }
    
    m_positionSummaryLabel->setText(tr("%1 (%2番)").arg(positionDesc, turn));
    m_positionSummaryLabel->setToolTip(m_currentSfen);
    
    // SFEN詳細を更新
    m_currentSfenLabel->setText(tr("局面SFEN: %1").arg(m_currentSfen));
}

// ============================================================
// テーブルダブルクリック（着手）
// ============================================================
void JosekiWindow::onTableDoubleClicked(int row, int column)
{
    Q_UNUSED(column);
    
    if (!m_humanCanPlay) {
        QMessageBox::information(this, tr("情報"),
            tr("現在はエンジンの手番のため着手できません。"));
        return;
    }
    
    if (row < 0 || row >= m_currentMoves.size()) return;
    
    const JosekiMove &move = m_currentMoves[row];
    qDebug() << "[JosekiWindow] Double-click play:" << move.move;
    emit josekiMoveSelected(move.move);
}

// ============================================================
// コンテキストメニュー表示
// ============================================================
void JosekiWindow::onTableContextMenu(const QPoint &pos)
{
    QModelIndex index = m_tableWidget->indexAt(pos);
    if (!index.isValid()) return;
    
    int row = index.row();
    if (row < 0 || row >= m_currentMoves.size()) return;
    
    // 着手アクションの有効/無効を設定
    m_actionPlay->setEnabled(m_humanCanPlay);
    
    // 現在の行を記憶
    m_tableWidget->selectRow(row);
    
    m_tableContextMenu->exec(m_tableWidget->viewport()->mapToGlobal(pos));
}

// ============================================================
// コンテキストメニュー: 着手
// ============================================================
void JosekiWindow::onContextMenuPlay()
{
    int row = m_tableWidget->currentRow();
    if (row < 0 || row >= m_currentMoves.size()) return;
    
    if (!m_humanCanPlay) {
        QMessageBox::information(this, tr("情報"),
            tr("現在はエンジンの手番のため着手できません。"));
        return;
    }
    
    const JosekiMove &move = m_currentMoves[row];
    emit josekiMoveSelected(move.move);
}

// ============================================================
// コンテキストメニュー: 編集
// ============================================================
void JosekiWindow::onContextMenuEdit()
{
    int row = m_tableWidget->currentRow();
    if (row < 0 || row >= m_currentMoves.size()) return;
    
    QString normalizedSfen = normalizeSfen(m_currentSfen);
    if (!m_josekiData.contains(normalizedSfen)) return;
    
    const JosekiMove &currentMove = m_currentMoves.at(row);
    
    // 定跡手の日本語表記を生成
    int plyNumber = 1;
    const QStringList sfenParts = m_currentSfen.split(QChar(' '));
    if (sfenParts.size() >= 4) {
        bool okPly;
        plyNumber = sfenParts.at(3).toInt(&okPly);
        if (!okPly) plyNumber = 1;
    }
    SfenPositionTracer tracer;
    tracer.setFromSfen(m_currentSfen);
    QString japaneseMoveStr = usiMoveToJapanese(currentMove.move, plyNumber, tracer);
    
    // 編集ダイアログを表示（編集モード）
    JosekiMoveDialog dialog(this, true);
    
    // 現在の値をダイアログに設定
    dialog.setValue(currentMove.value);
    dialog.setDepth(currentMove.depth);
    dialog.setFrequency(currentMove.frequency);
    dialog.setComment(currentMove.comment);
    dialog.setEditMoveDisplay(japaneseMoveStr);
    
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    
    // 定跡データを更新
    QVector<JosekiMove> &moves = m_josekiData[normalizedSfen];
    for (int i = 0; i < moves.size(); ++i) {
        if (moves[i].move == currentMove.move) {
            moves[i].value = dialog.value();
            moves[i].depth = dialog.depth();
            moves[i].frequency = dialog.frequency();
            moves[i].comment = dialog.comment();
            break;
        }
    }
    
    setModified(true);
    updateJosekiDisplay();
}

// ============================================================
// コンテキストメニュー: 削除
// ============================================================
void JosekiWindow::onContextMenuDelete()
{
    int row = m_tableWidget->currentRow();
    if (row < 0 || row >= m_currentMoves.size()) return;
    
    QString normalizedSfen = normalizeSfen(m_currentSfen);
    if (!m_josekiData.contains(normalizedSfen)) return;
    
    const JosekiMove &move = m_currentMoves[row];
    
    QMessageBox::StandardButton result = QMessageBox::question(
        this,
        tr("削除確認"),
        tr("定跡手「%1」を削除しますか？").arg(move.move),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No
    );
    
    if (result == QMessageBox::Yes) {
        m_josekiData[normalizedSfen].remove(row);
        
        if (m_josekiData[normalizedSfen].isEmpty()) {
            m_josekiData.remove(normalizedSfen);
            m_sfenWithPlyMap.remove(normalizedSfen);
        }
        
        setModified(true);
        updateJosekiDisplay();
    }
}

// ============================================================
// コンテキストメニュー: 指し手をコピー
// ============================================================
void JosekiWindow::onContextMenuCopyMove()
{
    int row = m_tableWidget->currentRow();
    if (row < 0 || row >= m_currentMoves.size()) return;
    
    const JosekiMove &move = m_currentMoves[row];
    
    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setText(move.move);
    
    // ステータスバーに通知
    if (m_statusLabel) {
        QString oldText = m_statusLabel->text();
        m_statusLabel->setText(tr("「%1」をコピーしました").arg(move.move));
        
        // 2秒後に元に戻す
        QTimer::singleShot(2000, this, [this, oldText]() {
            if (m_statusLabel) {
                updateStatusDisplay();
            }
        });
    }
}
