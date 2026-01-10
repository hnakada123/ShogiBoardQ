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
    , m_closeButton(nullptr)
    , m_currentSfenLabel(nullptr)
    , m_sfenLineLabel(nullptr)
    , m_statusLabel(nullptr)
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
    resize(950, 550);

    // メインレイアウト
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(6);

    // ============================================================
    // ツールバー行1: ファイルグループ
    // ============================================================
    QGroupBox *fileGroup = new QGroupBox(tr("ファイル"), this);
    QVBoxLayout *fileGroupLayout = new QVBoxLayout(fileGroup);
    fileGroupLayout->setContentsMargins(8, 4, 8, 4);
    fileGroupLayout->setSpacing(4);
    
    // ファイル操作ボタン行
    QHBoxLayout *fileButtonLayout = new QHBoxLayout();
    
    // フォントサイズ変更ボタン（新規作成ボタンの左に配置）
    m_fontIncreaseBtn = new QPushButton(tr("A+"), this);
    m_fontIncreaseBtn->setToolTip(tr("フォントサイズを拡大"));
    m_fontIncreaseBtn->setFixedWidth(36);
    fileButtonLayout->addWidget(m_fontIncreaseBtn);
    
    m_fontDecreaseBtn = new QPushButton(tr("A-"), this);
    m_fontDecreaseBtn->setToolTip(tr("フォントサイズを縮小"));
    m_fontDecreaseBtn->setFixedWidth(36);
    fileButtonLayout->addWidget(m_fontDecreaseBtn);
    
    fileButtonLayout->addSpacing(10);
    
    m_newButton = new QPushButton(tr("新規作成"), this);
    m_newButton->setToolTip(tr("新しい空の定跡ファイルを作成"));
    m_newButton->setIcon(style()->standardIcon(QStyle::SP_FileIcon));
    fileButtonLayout->addWidget(m_newButton);
    
    m_openButton = new QPushButton(tr("開く"), this);
    m_openButton->setToolTip(tr("定跡ファイル(.db)を開く"));
    m_openButton->setIcon(style()->standardIcon(QStyle::SP_DialogOpenButton));
    fileButtonLayout->addWidget(m_openButton);
    
    m_saveButton = new QPushButton(tr("上書保存"), this);
    m_saveButton->setToolTip(tr("現在のファイルに上書き保存"));
    m_saveButton->setIcon(style()->standardIcon(QStyle::SP_DialogSaveButton));
    m_saveButton->setEnabled(false);  // 初期状態は無効
    fileButtonLayout->addWidget(m_saveButton);
    
    m_saveAsButton = new QPushButton(tr("名前を付けて保存"), this);
    m_saveAsButton->setToolTip(tr("別の名前で保存"));
    m_saveAsButton->setIcon(style()->standardIcon(QStyle::SP_DialogSaveButton));
    fileButtonLayout->addWidget(m_saveAsButton);
    
    fileButtonLayout->addSpacing(10);
    
    m_recentButton = new QPushButton(tr("履歴"), this);
    m_recentButton->setToolTip(tr("最近使ったファイルを開く"));
    m_recentFilesMenu = new QMenu(this);
    m_recentButton->setMenu(m_recentFilesMenu);
    fileButtonLayout->addWidget(m_recentButton);
    
    fileButtonLayout->addStretch();
    fileGroupLayout->addLayout(fileButtonLayout);
    
    // ファイルパスと状態表示行
    QHBoxLayout *fileInfoLayout = new QHBoxLayout();
    m_filePathLabel = new QLabel(tr("ファイル未選択"), this);
    m_filePathLabel->setStyleSheet(QStringLiteral("color: gray;"));
    m_filePathLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    fileInfoLayout->addWidget(m_filePathLabel, 1);
    
    m_fileStatusLabel = new QLabel(this);
    m_fileStatusLabel->setFixedWidth(80);
    fileInfoLayout->addWidget(m_fileStatusLabel);
    fileGroupLayout->addLayout(fileInfoLayout);

    // ============================================================
    // ツールバー行2: 自動読込チェックボックス + 操作グループ
    // ============================================================
    QHBoxLayout *toolbarRow2 = new QHBoxLayout();
    
    // --- 自動読込チェックボックス ---
    m_autoLoadCheckBox = new QCheckBox(tr("自動読込"), this);
    m_autoLoadCheckBox->setToolTip(tr("定跡ウィンドウ表示時に前回のファイルを自動で読み込む"));
    m_autoLoadCheckBox->setChecked(true);
    toolbarRow2->addWidget(m_autoLoadCheckBox);
    
    toolbarRow2->addStretch();
    
    // --- 操作グループ ---
    QGroupBox *operationGroup = new QGroupBox(tr("操作"), this);
    QHBoxLayout *operationGroupLayout = new QHBoxLayout(operationGroup);
    operationGroupLayout->setContentsMargins(8, 4, 8, 4);
    
    m_addMoveButton = new QPushButton(tr("＋追加"), this);
    m_addMoveButton->setToolTip(tr("現在の局面に定跡手を追加"));
    m_addMoveButton->setFixedWidth(70);
    operationGroupLayout->addWidget(m_addMoveButton);
    
    // マージボタン（ドロップダウンメニュー付き）
    m_mergeButton = new QToolButton(this);
    m_mergeButton->setText(tr("📥マージ"));
    m_mergeButton->setToolTip(tr("棋譜から定跡をマージ"));
    m_mergeButton->setPopupMode(QToolButton::InstantPopup);
    m_mergeButton->setFixedWidth(90);
    
    m_mergeMenu = new QMenu(this);
    m_mergeMenu->addAction(tr("現在の棋譜から"), this, &JosekiWindow::onMergeFromCurrentKifu);
    m_mergeMenu->addAction(tr("棋譜ファイルから"), this, &JosekiWindow::onMergeFromKifuFile);
    m_mergeButton->setMenu(m_mergeMenu);
    operationGroupLayout->addWidget(m_mergeButton);
    
    m_stopButton = new QPushButton(tr("⏸停止"), this);
    m_stopButton->setToolTip(tr("定跡表示を停止/再開"));
    m_stopButton->setCheckable(true);
    m_stopButton->setFixedWidth(70);
    operationGroupLayout->addWidget(m_stopButton);
    
    m_closeButton = new QPushButton(tr("閉じる"), this);
    m_closeButton->setToolTip(tr("定跡ウィンドウを閉じる"));
    m_closeButton->setIcon(style()->standardIcon(QStyle::SP_DialogCloseButton));
    operationGroupLayout->addWidget(m_closeButton);
    
    toolbarRow2->addWidget(operationGroup);

    // レイアウトに追加
    mainLayout->addWidget(fileGroup);
    mainLayout->addLayout(toolbarRow2);

    // ============================================================
    // 状態表示行（表形式で揃える）
    // ============================================================
    QHBoxLayout *sfenAreaLayout = new QHBoxLayout();
    
    // グリッドレイアウトで表形式に
    QGridLayout *sfenGridLayout = new QGridLayout();
    sfenGridLayout->setSpacing(4);
    
    // --- 1行目: 現在の局面 ---
    QLabel *sfenTitleLabel = new QLabel(tr("現在の局面:"), this);
    sfenGridLayout->addWidget(sfenTitleLabel, 0, 0, Qt::AlignLeft);
    
    m_currentSfenLabel = new QLabel(tr("(未設定)"), this);
    m_currentSfenLabel->setStyleSheet(QStringLiteral("color: blue; font-family: monospace;"));
    m_currentSfenLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_currentSfenLabel->setWordWrap(false);
    sfenGridLayout->addWidget(m_currentSfenLabel, 0, 1, Qt::AlignLeft);
    
    // --- 2行目: 定跡 ---
    QLabel *sfenLineTitleLabel = new QLabel(tr("定跡:"), this);
    sfenGridLayout->addWidget(sfenLineTitleLabel, 1, 0, Qt::AlignLeft);
    
    m_sfenLineLabel = new QLabel(tr("(定跡なし)"), this);
    m_sfenLineLabel->setStyleSheet(QStringLiteral("color: green; font-family: monospace;"));
    m_sfenLineLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_sfenLineLabel->setWordWrap(false);
    sfenGridLayout->addWidget(m_sfenLineLabel, 1, 1, Qt::AlignLeft);
    
    // 2列目を伸縮可能に
    sfenGridLayout->setColumnStretch(1, 1);
    
    sfenAreaLayout->addLayout(sfenGridLayout, 1);
    
    sfenAreaLayout->addSpacing(10);
    
    // 状態ラベル（右端に配置）
    m_statusLabel = new QLabel(this);
    m_statusLabel->setFixedWidth(150);
    sfenAreaLayout->addWidget(m_statusLabel);
    
    mainLayout->addLayout(sfenAreaLayout);

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
    connect(m_closeButton, &QPushButton::clicked,
            this, &JosekiWindow::onCloseButtonClicked);
    
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
    
    // 最後に開いた定跡ファイルを読み込み（自動読込が有効な場合のみ）
    if (m_autoLoadEnabled) {
        QString lastFilePath = SettingsService::josekiWindowLastFilePath();
        if (!lastFilePath.isEmpty() && QFileInfo::exists(lastFilePath)) {
            if (loadJosekiFile(lastFilePath)) {
                m_currentFilePath = lastFilePath;
                m_filePathLabel->setText(lastFilePath);
                m_filePathLabel->setStyleSheet(QString());
                setModified(false);
            }
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

void JosekiWindow::applyFontSize()
{
    // ウィンドウ全体のフォントサイズを更新
    QFont font = this->font();
    font.setPointSize(m_fontSize);
    setFont(font);
    
    // テーブルのフォントも更新
    m_tableWidget->setFont(font);
    
    // ヘッダーのフォントも更新
    m_tableWidget->horizontalHeader()->setFont(font);
    
    // 行の高さを調整
    m_tableWidget->verticalHeader()->setDefaultSectionSize(m_fontSize + 16);
    
    // SFEN表示ラベルのフォントサイズも更新（monospace維持）
    QString blueStyleSheet = QStringLiteral("color: blue; font-family: monospace; font-size: %1pt;").arg(m_fontSize);
    m_currentSfenLabel->setStyleSheet(blueStyleSheet);
    
    QString greenStyleSheet = QStringLiteral("color: green; font-family: monospace; font-size: %1pt;").arg(m_fontSize);
    m_sfenLineLabel->setStyleSheet(greenStyleSheet);
    
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
    
    // 定跡データを検索
    if (!m_josekiData.contains(normalizedSfen)) {
        // 一致する定跡がない場合は空のテーブルを表示
        qDebug() << "[JosekiWindow] No match found for current position";
        m_currentMoves.clear();
        m_sfenLineLabel->setText(tr("(定跡なし)"));
        updateStatusDisplay();
        return;
    }
    
    const QVector<JosekiMove> &moves = m_josekiData[normalizedSfen];
    
    // 現在表示中の定跡手リストを保存
    m_currentMoves = moves;
    
    // 定跡ファイルのSFEN行を表示（「sfen 」はラベル側に含まれているので省略）
    if (m_sfenWithPlyMap.contains(normalizedSfen)) {
        m_sfenLineLabel->setText(m_sfenWithPlyMap[normalizedSfen]);
    } else {
        m_sfenLineLabel->setText(normalizedSfen);
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
        // m_humanCanPlayがfalseの場合、セルは空のまま
        
        // 定跡手（日本語表記に変換）
        // tracerは現在局面をセットしているので、そのまま使用
        QString moveJapanese = usiMoveToJapanese(move.move, plyNumber, tracer);
        QTableWidgetItem *moveItem = new QTableWidgetItem(moveJapanese);
        moveItem->setTextAlignment(Qt::AlignCenter);
        m_tableWidget->setItem(i, 2, moveItem);
        
        // 予想応手（次の指し手）を日本語表記に変換
        // 予想応手は定跡手を指した後の局面から変換する必要がある
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
        locale.setNumberOptions(QLocale::DefaultNumberOptions);  // グループセパレーターを有効化
        QString valueStr = locale.toString(move.value);
        qDebug() << "[JosekiWindow] value:" << move.value << "-> formatted:" << valueStr << "locale:" << locale.name();
        QTableWidgetItem *valueItem = new QTableWidgetItem(valueStr);
        valueItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_tableWidget->setItem(i, 6, valueItem);
        
        // 深さ（3桁区切り、右寄せ）
        QString depthStr = locale.toString(move.depth);
        qDebug() << "[JosekiWindow] depth:" << move.depth << "-> formatted:" << depthStr;
        QTableWidgetItem *depthItem = new QTableWidgetItem(depthStr);
        depthItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_tableWidget->setItem(i, 7, depthItem);
        
        // 出現頻度（3桁区切り）
        QString freqStr = locale.toString(move.frequency);
        qDebug() << "[JosekiWindow] frequency:" << move.frequency << "-> formatted:" << freqStr;
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
        m_stopButton->setText(tr("⏸停止"));
        // 表示を再開した場合は現在の局面を再表示
        updateJosekiDisplay();
    } else {
        m_stopButton->setText(tr("▶再開"));
        // 停止した場合はテーブルをクリア
        clearTable();
    }
    
    updateStatusDisplay();
    qDebug() << "[JosekiWindow] Display enabled:" << m_displayEnabled;
}

void JosekiWindow::onCloseButtonClicked()
{
    close();
}

void JosekiWindow::updateStatusDisplay()
{
    // ファイル読込状態を更新
    if (m_fileStatusLabel) {
        if (!m_josekiData.isEmpty()) {
            m_fileStatusLabel->setText(tr("✓読込済"));
            m_fileStatusLabel->setStyleSheet(QStringLiteral("color: green; font-weight: bold;"));
        } else {
            m_fileStatusLabel->setText(tr("✗未読込"));
            m_fileStatusLabel->setStyleSheet(QStringLiteral("color: gray;"));
        }
    }
    
    // 表示状態を更新
    if (m_statusLabel) {
        if (!m_displayEnabled) {
            m_statusLabel->setText(tr("○停止中"));
            m_statusLabel->setStyleSheet(QStringLiteral("color: orange; font-weight: bold;"));
        } else if (m_currentMoves.isEmpty()) {
            if (m_currentSfen.isEmpty()) {
                m_statusLabel->setText(tr("―局面未設定"));
                m_statusLabel->setStyleSheet(QStringLiteral("color: gray;"));
            } else {
                m_statusLabel->setText(tr("―定跡なし"));
                m_statusLabel->setStyleSheet(QStringLiteral("color: gray;"));
            }
        } else {
            m_statusLabel->setText(tr("●表示中 (%1件)").arg(m_currentMoves.size()));
            m_statusLabel->setStyleSheet(QStringLiteral("color: green; font-weight: bold;"));
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
    connect(clearAction, &QAction::triggered, this, [this]() {
        m_recentFiles.clear();
        updateRecentFilesMenu();
        saveSettings();
    });
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
    
    // KIFファイルを解析
    KifParseResult parseResult;
    QString errorMessage;
    
    if (!KifToSfenConverter::parseWithVariations(kifFilePath, parseResult, &errorMessage)) {
        QMessageBox::warning(this, tr("エラー"),
            tr("棋譜ファイルの読み込みに失敗しました。\n%1").arg(errorMessage));
        return;
    }
    
    // 本譜からエントリを作成
    const KifLine &mainline = parseResult.mainline;
    
    if (mainline.usiMoves.isEmpty()) {
        QMessageBox::information(this, tr("情報"),
            tr("棋譜に指し手がありません。"));
        return;
    }
    
    // マージエントリを作成
    QVector<KifuMergeEntry> entries;
    
    // sfenList[0] = 初期局面、sfenList[i] = i手目後の局面
    // usiMoves[i] = i+1手目のUSI指し手
    // disp[i] = i+1手目の表示情報
    for (int i = 0; i < mainline.usiMoves.size(); ++i) {
        if (i >= mainline.sfenList.size()) break;
        
        // USI指し手が空の場合はスキップ
        if (mainline.usiMoves[i].isEmpty()) continue;
        
        KifuMergeEntry entry;
        entry.ply = i + 1;  // 1手目から
        entry.sfen = mainline.sfenList[i];  // この手を指す前の局面
        entry.usiMove = mainline.usiMoves[i];
        
        // 日本語表記を取得
        if (i < mainline.disp.size()) {
            entry.japaneseMove = mainline.disp[i].prettyMove;
        } else {
            entry.japaneseMove = mainline.usiMoves[i];
        }
        
        entry.isCurrentMove = false;  // ファイルからの読み込みなので現在位置はなし
        
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
    
    // ダイアログタイトルに読み込んだファイル名を追加
    QFileInfo fi(kifFilePath);
    dialog->setWindowTitle(tr("棋譜から定跡にマージ - %1").arg(fi.fileName()));
    
    // シグナル接続
    connect(dialog, &JosekiMergeDialog::registerMove,
            this, &JosekiWindow::onMergeRegisterMove);
    
    dialog->setKifuData(entries, -1);  // currentPlyは-1（選択なし）
    dialog->show();
}
