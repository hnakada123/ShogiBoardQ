#include "josekiwindow.h"
#include "settingsservice.h"

#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QFile>
#include <QTextStream>
#include <QMessageBox>
#include <QDebug>
#include <QFileInfo>
#include <QLocale>

JosekiWindow::JosekiWindow(QWidget *parent)
    : QWidget(parent, Qt::Window)  // Qt::Window フラグで独立ウィンドウとして表示
    , m_openButton(nullptr)
    , m_filePathLabel(nullptr)
    , m_currentSfenLabel(nullptr)
    , m_fontIncreaseBtn(nullptr)
    , m_fontDecreaseBtn(nullptr)
    , m_tableWidget(nullptr)
    , m_fontSize(10)
{
    setupUi();
    loadSettings();
}

void JosekiWindow::setupUi()
{
    // ウィンドウタイトルとサイズの設定
    setWindowTitle(tr("定跡ウィンドウ"));
    resize(800, 500);

    // メインレイアウト
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // 上部ツールバー用の水平レイアウト
    QHBoxLayout *toolbarLayout = new QHBoxLayout();

    // 「開く」ボタン
    m_openButton = new QPushButton(tr("開く"), this);
    m_openButton->setToolTip(tr("定跡ファイル(.db)を開く"));
    toolbarLayout->addWidget(m_openButton);

    // ファイルパス表示ラベル
    m_filePathLabel = new QLabel(tr("ファイル未選択"), this);
    m_filePathLabel->setStyleSheet(QStringLiteral("color: gray;"));
    toolbarLayout->addWidget(m_filePathLabel, 1);  // ストレッチファクター1で残りの領域を使用

    toolbarLayout->addStretch();
    
    // フォントサイズ調整ボタン
    m_fontDecreaseBtn = new QPushButton(tr("A-"), this);
    m_fontDecreaseBtn->setToolTip(tr("フォントサイズを縮小"));
    m_fontDecreaseBtn->setFixedWidth(40);
    toolbarLayout->addWidget(m_fontDecreaseBtn);
    
    m_fontIncreaseBtn = new QPushButton(tr("A+"), this);
    m_fontIncreaseBtn->setToolTip(tr("フォントサイズを拡大"));
    m_fontIncreaseBtn->setFixedWidth(40);
    toolbarLayout->addWidget(m_fontIncreaseBtn);

    mainLayout->addLayout(toolbarLayout);

    // デバッグ用：現在の局面のSFEN表示
    QHBoxLayout *sfenLayout = new QHBoxLayout();
    QLabel *sfenTitleLabel = new QLabel(tr("現在の局面(SFEN):"), this);
    sfenLayout->addWidget(sfenTitleLabel);
    
    m_currentSfenLabel = new QLabel(tr("(未設定)"), this);
    m_currentSfenLabel->setStyleSheet(QStringLiteral("color: blue; font-family: monospace;"));
    m_currentSfenLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);  // テキスト選択可能に
    m_currentSfenLabel->setWordWrap(true);
    sfenLayout->addWidget(m_currentSfenLabel, 1);
    
    mainLayout->addLayout(sfenLayout);

    // 定跡表示用テーブル
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
    m_tableWidget->setColumnWidth(2, 80);   // 定跡手
    m_tableWidget->setColumnWidth(3, 80);   // 予想応手
    m_tableWidget->setColumnWidth(4, 50);   // 編集
    m_tableWidget->setColumnWidth(5, 50);   // 削除
    m_tableWidget->setColumnWidth(6, 70);   // 評価値
    m_tableWidget->setColumnWidth(7, 50);   // 深さ
    m_tableWidget->setColumnWidth(8, 70);   // 出現頻度
    m_tableWidget->horizontalHeader()->setStretchLastSection(true);  // コメント列を伸縮
    
    mainLayout->addWidget(m_tableWidget, 1);

    // シグナル・スロット接続
    connect(m_openButton, &QPushButton::clicked,
            this, &JosekiWindow::onOpenButtonClicked);
    connect(m_fontIncreaseBtn, &QPushButton::clicked,
            this, &JosekiWindow::onFontSizeIncrease);
    connect(m_fontDecreaseBtn, &QPushButton::clicked,
            this, &JosekiWindow::onFontSizeDecrease);
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
    
    // 最後に開いた定跡ファイルを読み込み
    QString lastFilePath = SettingsService::josekiWindowLastFilePath();
    if (!lastFilePath.isEmpty() && QFileInfo::exists(lastFilePath)) {
        if (loadJosekiFile(lastFilePath)) {
            m_currentFilePath = lastFilePath;
            m_filePathLabel->setText(lastFilePath);
            m_filePathLabel->setStyleSheet(QString());
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
}

void JosekiWindow::closeEvent(QCloseEvent *event)
{
    saveSettings();
    QWidget::closeEvent(event);
}

void JosekiWindow::applyFontSize()
{
    QFont font = m_tableWidget->font();
    font.setPointSize(m_fontSize);
    m_tableWidget->setFont(font);
    
    // ヘッダーのフォントも更新
    m_tableWidget->horizontalHeader()->setFont(font);
    
    // 行の高さを調整
    m_tableWidget->verticalHeader()->setDefaultSectionSize(m_fontSize + 16);
    
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

    QTextStream in(&file);
    QString line;
    QString currentSfen;
    QString normalizedSfen;

    while (!in.atEnd()) {
        line = in.readLine().trimmed();
        
        // Windows改行コード(\r)を除去
        line.remove(QLatin1Char('\r'));
        
        // 空行をスキップ
        if (line.isEmpty()) {
            continue;
        }
        
        // ヘッダー行（#YANEURAOU-DB2016など）をスキップ
        if (line.startsWith(QLatin1Char('#'))) {
            continue;
        }
        
        // sfen行の処理
        if (line.startsWith(QStringLiteral("sfen "))) {
            currentSfen = line.mid(5).trimmed();
            currentSfen.remove(QLatin1Char('\r'));  // 念のため再度除去
            normalizedSfen = normalizeSfen(currentSfen);
            continue;
        }
        
        // 指し手行の処理（次のsfen行が来るまで同じ局面の指し手として追加）
        if (!normalizedSfen.isEmpty()) {
            // 指し手行をパース
            const QStringList parts = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
            if (parts.size() >= 5) {
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
            }
        }
    }

    file.close();
    
    qDebug() << "[JosekiWindow] Loaded" << m_josekiData.size() << "positions from" << filePath;
    
    // デバッグ：平手初期局面があるか確認
    QString hirate = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b -");
    if (m_josekiData.contains(hirate)) {
        qDebug() << "[JosekiWindow] Hirate position has" << m_josekiData[hirate].size() << "moves";
    }
    
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
    
    // デバッグ用：SFENラベルを更新
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

void JosekiWindow::updateJosekiDisplay()
{
    qDebug() << "[JosekiWindow] updateJosekiDisplay() called";
    
    clearTable();
    
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
        return;
    }
    
    const QVector<JosekiMove> &moves = m_josekiData[normalizedSfen];
    
    qDebug() << "[JosekiWindow] Found" << moves.size() << "moves for this position";
    
    m_tableWidget->setRowCount(static_cast<int>(moves.size()));
    
    for (int i = 0; i < moves.size(); ++i) {
        const JosekiMove &move = moves[i];
        
        // No.
        QTableWidgetItem *noItem = new QTableWidgetItem(QString::number(i + 1));
        noItem->setTextAlignment(Qt::AlignCenter);
        m_tableWidget->setItem(i, 0, noItem);
        
        // 着手ボタン（青系の配色）
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
        m_tableWidget->setCellWidget(i, 1, playButton);
        
        // 定跡手
        QTableWidgetItem *moveItem = new QTableWidgetItem(move.move);
        moveItem->setTextAlignment(Qt::AlignCenter);
        m_tableWidget->setItem(i, 2, moveItem);
        
        // 予想応手（次の指し手）
        QTableWidgetItem *nextMoveItem = new QTableWidgetItem(move.nextMove);
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
        m_tableWidget->setCellWidget(i, 5, deleteButton);
        
        // 評価値（3桁区切り）
        QLocale locale = QLocale::system();
        locale.setNumberOptions(QLocale::DefaultNumberOptions);  // グループセパレーターを有効化
        QString valueStr = locale.toString(move.value);
        qDebug() << "[JosekiWindow] value:" << move.value << "-> formatted:" << valueStr << "locale:" << locale.name();
        QTableWidgetItem *valueItem = new QTableWidgetItem(valueStr);
        valueItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_tableWidget->setItem(i, 6, valueItem);
        
        // 深さ（3桁区切り）
        QString depthStr = locale.toString(move.depth);
        qDebug() << "[JosekiWindow] depth:" << move.depth << "-> formatted:" << depthStr;
        QTableWidgetItem *depthItem = new QTableWidgetItem(depthStr);
        depthItem->setTextAlignment(Qt::AlignCenter);
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
}

void JosekiWindow::clearTable()
{
    m_tableWidget->setRowCount(0);
}
