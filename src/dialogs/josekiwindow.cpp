#include "josekiwindow.h"
#include "settingsservice.h"
#include "sfenpositiontracer.h"

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
    , m_autoLoadCheckBox(nullptr)
    , m_stopButton(nullptr)
    , m_closeButton(nullptr)
    , m_tableWidget(nullptr)
    , m_fontSize(10)
    , m_humanCanPlay(true)  // デフォルトは着手可能
    , m_autoLoadEnabled(true)
    , m_displayEnabled(true)
{
    setupUi();
    loadSettings();
}

void JosekiWindow::setupUi()
{
    // ウィンドウタイトルとサイズの設定
    setWindowTitle(tr("定跡ウィンドウ"));
    resize(900, 500);

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

    // 自動読込チェックボックス
    m_autoLoadCheckBox = new QCheckBox(tr("自動読込"), this);
    m_autoLoadCheckBox->setToolTip(tr("定跡ウィンドウ表示時に前回のファイルを自動で読み込む"));
    m_autoLoadCheckBox->setChecked(true);
    toolbarLayout->addWidget(m_autoLoadCheckBox);

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
    
    // 停止ボタン
    m_stopButton = new QPushButton(tr("停止"), this);
    m_stopButton->setToolTip(tr("定跡表示を停止/再開"));
    m_stopButton->setCheckable(true);
    toolbarLayout->addWidget(m_stopButton);
    
    // 閉じるボタン
    m_closeButton = new QPushButton(tr("閉じる"), this);
    m_closeButton->setToolTip(tr("定跡ウィンドウを閉じる"));
    toolbarLayout->addWidget(m_closeButton);

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
    
    // カラム幅の設定（定跡手・予想応手の幅を拡大）
    m_tableWidget->setColumnWidth(0, 40);   // No.
    m_tableWidget->setColumnWidth(1, 50);   // 着手ボタン
    m_tableWidget->setColumnWidth(2, 120);  // 定跡手（拡大）
    m_tableWidget->setColumnWidth(3, 120);  // 予想応手（拡大）
    m_tableWidget->setColumnWidth(4, 50);   // 編集
    m_tableWidget->setColumnWidth(5, 50);   // 削除
    m_tableWidget->setColumnWidth(6, 70);   // 評価値
    m_tableWidget->setColumnWidth(7, 50);   // 深さ
    m_tableWidget->setColumnWidth(8, 80);   // 出現頻度
    m_tableWidget->horizontalHeader()->setStretchLastSection(true);  // コメント列を伸縮
    
    mainLayout->addWidget(m_tableWidget, 1);

    // シグナル・スロット接続
    connect(m_openButton, &QPushButton::clicked,
            this, &JosekiWindow::onOpenButtonClicked);
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
    
    // 最後に開いた定跡ファイルを読み込み（自動読込が有効な場合のみ）
    if (m_autoLoadEnabled) {
        QString lastFilePath = SettingsService::josekiWindowLastFilePath();
        if (!lastFilePath.isEmpty() && QFileInfo::exists(lastFilePath)) {
            if (loadJosekiFile(lastFilePath)) {
                m_currentFilePath = lastFilePath;
                m_filePathLabel->setText(lastFilePath);
                m_filePathLabel->setStyleSheet(QString());
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
        
        // ヘッダー行（#YANEURAOU-DB2016など）をチェック
        if (line.startsWith(QLatin1Char('#'))) {
            // やねうら王定跡フォーマットのヘッダーを確認
            if (line.contains(QStringLiteral("YANEURAOU")) || 
                line.contains(QStringLiteral("yaneuraou"))) {
                hasValidHeader = true;
            }
            continue;
        }
        
        // sfen行の処理
        if (line.startsWith(QStringLiteral("sfen "))) {
            currentSfen = line.mid(5).trimmed();
            currentSfen.remove(QLatin1Char('\r'));  // 念のため再度除去
            normalizedSfen = normalizeSfen(currentSfen);
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
        return;
    }
    
    const QVector<JosekiMove> &moves = m_josekiData[normalizedSfen];
    
    // 現在表示中の定跡手リストを保存
    m_currentMoves = moves;
    
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
}

void JosekiWindow::clearTable()
{
    m_tableWidget->setRowCount(0);
    m_currentMoves.clear();
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
        m_stopButton->setText(tr("停止"));
        // 表示を再開した場合は現在の局面を再表示
        updateJosekiDisplay();
    } else {
        m_stopButton->setText(tr("再開"));
        // 停止した場合はテーブルをクリア
        clearTable();
    }
    
    qDebug() << "[JosekiWindow] Display enabled:" << m_displayEnabled;
}

void JosekiWindow::onCloseButtonClicked()
{
    close();
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
