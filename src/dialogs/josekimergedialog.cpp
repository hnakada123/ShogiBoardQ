/// @file josekimergedialog.cpp
/// @brief 定跡統合ダイアログクラスの実装
/// @todo remove コメントスタイルガイド適用済み

#include "josekimergedialog.h"

#include <QHeaderView>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QFileInfo>
#include <QDebug>

JosekiMergeDialog::JosekiMergeDialog(QWidget *parent)
    : QDialog(parent)
    , m_tableWidget(nullptr)
    , m_registerAllButton(nullptr)
    , m_closeButton(nullptr)
    , m_fontIncreaseBtn(nullptr)
    , m_fontDecreaseBtn(nullptr)
    , m_statusLabel(nullptr)
    , m_targetFileLabel(nullptr)
    , m_autoSaveLabel(nullptr)
    , m_currentPly(-1)
    , m_fontSize(10)
{
    setupUi();
}

void JosekiMergeDialog::setupUi()
{
    setWindowTitle(tr("棋譜から定跡にマージ"));
    setMinimumSize(500, 400);
    resize(600, 500);
    
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    
    // === ツールバー行 ===
    QHBoxLayout *toolbarLayout = new QHBoxLayout();
    
    // フォントサイズボタン
    m_fontDecreaseBtn = new QPushButton(tr("A-"), this);
    m_fontDecreaseBtn->setToolTip(tr("フォントサイズを縮小"));
    m_fontDecreaseBtn->setFixedWidth(36);
    toolbarLayout->addWidget(m_fontDecreaseBtn);
    
    m_fontIncreaseBtn = new QPushButton(tr("A+"), this);
    m_fontIncreaseBtn->setToolTip(tr("フォントサイズを拡大"));
    m_fontIncreaseBtn->setFixedWidth(36);
    toolbarLayout->addWidget(m_fontIncreaseBtn);
    
    toolbarLayout->addStretch();
    
    // 状態ラベル
    m_statusLabel = new QLabel(this);
    toolbarLayout->addWidget(m_statusLabel);
    
    mainLayout->addLayout(toolbarLayout);
    
    // === マージ先ファイル表示 ===
    m_targetFileLabel = new QLabel(this);
    m_targetFileLabel->setStyleSheet(QStringLiteral("QLabel { color: #0066cc; font-weight: bold; }"));
    mainLayout->addWidget(m_targetFileLabel);
    
    // === 説明ラベル ===
    QLabel *descLabel = new QLabel(tr("棋譜の指し手を定跡に登録します。「登録」ボタンで個別に、「全て登録」で一括登録できます。"), this);
    descLabel->setWordWrap(true);
    mainLayout->addWidget(descLabel);
    
    // === 自動保存説明ラベル ===
    m_autoSaveLabel = new QLabel(tr("※ 登録時に定跡ファイルへ自動保存されます"), this);
    m_autoSaveLabel->setStyleSheet(QStringLiteral("QLabel { color: #228b22; font-weight: bold; }"));
    mainLayout->addWidget(m_autoSaveLabel);
    
    // === テーブル ===
    m_tableWidget = new QTableWidget(this);
    m_tableWidget->setColumnCount(4);
    
    QStringList headers;
    headers << tr("手数") << tr("指し手") << tr("登録") << tr("状態");
    m_tableWidget->setHorizontalHeaderLabels(headers);
    
    m_tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tableWidget->setAlternatingRowColors(true);
    m_tableWidget->verticalHeader()->setVisible(false);
    
    // カラム幅
    m_tableWidget->setColumnWidth(0, 60);   // 手数
    m_tableWidget->setColumnWidth(1, 200);  // 指し手
    m_tableWidget->setColumnWidth(2, 80);   // 登録ボタン
    m_tableWidget->setColumnWidth(3, 120);  // 状態
    
    m_tableWidget->horizontalHeader()->setStretchLastSection(true);
    
    mainLayout->addWidget(m_tableWidget);
    
    // === ボタン行 ===
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    
    m_registerAllButton = new QPushButton(tr("全て登録"), this);
    m_registerAllButton->setToolTip(tr("全ての指し手を定跡に登録"));
    buttonLayout->addWidget(m_registerAllButton);
    
    buttonLayout->addStretch();
    
    m_closeButton = new QPushButton(tr("閉じる"), this);
    buttonLayout->addWidget(m_closeButton);
    
    mainLayout->addLayout(buttonLayout);
    
    // === シグナル・スロット接続 ===
    connect(m_fontIncreaseBtn, &QPushButton::clicked, this, &JosekiMergeDialog::onFontSizeIncrease);
    connect(m_fontDecreaseBtn, &QPushButton::clicked, this, &JosekiMergeDialog::onFontSizeDecrease);
    connect(m_registerAllButton, &QPushButton::clicked, this, &JosekiMergeDialog::onRegisterAllButtonClicked);
    connect(m_closeButton, &QPushButton::clicked, this, &QDialog::accept);
    
    applyFontSize();
}

void JosekiMergeDialog::setKifuData(const QVector<KifuMergeEntry> &entries, int currentPly)
{
    m_entries = entries;
    m_currentPly = currentPly;
    
    m_statusLabel->setText(tr("%1手の棋譜").arg(entries.size()));
    
    updateTable();
}

void JosekiMergeDialog::setRegisteredMoves(const QSet<QString> &registeredMoves)
{
    m_registeredMoves = registeredMoves;
}

void JosekiMergeDialog::setTargetJosekiFile(const QString &filePath)
{
    if (filePath.isEmpty()) {
        m_targetFileLabel->setText(tr("マージ先: (未設定)"));
    } else {
        // ファイル名のみを表示
        QFileInfo fi(filePath);
        m_targetFileLabel->setText(tr("マージ先: %1").arg(fi.fileName()));
        m_targetFileLabel->setToolTip(filePath);
    }
}

void JosekiMergeDialog::updateTable()
{
    const int rowCount = static_cast<int>(m_entries.size());
    m_tableWidget->setRowCount(rowCount);
    
    for (int i = 0; i < rowCount; ++i) {
        const KifuMergeEntry &entry = m_entries[i];
        
        // 手数
        QString plyText = QString::number(entry.ply);
        QTableWidgetItem *plyItem = new QTableWidgetItem(plyText);
        plyItem->setTextAlignment(Qt::AlignCenter);
        m_tableWidget->setItem(i, 0, plyItem);
        
        // 指し手（「現在の指し手」の文字列は表示しない）
        QString moveText = entry.japaneseMove;
        QTableWidgetItem *moveItem = new QTableWidgetItem(moveText);
        m_tableWidget->setItem(i, 1, moveItem);
        
        // 登録済みかどうかをチェック
        bool alreadyRegistered = isRegistered(entry.sfen, entry.usiMove);
        
        // 登録ボタン（青系の配色）
        QPushButton *registerBtn = new QPushButton(alreadyRegistered ? tr("登録済") : tr("登録"), this);
        registerBtn->setProperty("row", i);
        registerBtn->setStyleSheet(QStringLiteral(
            "QPushButton {"
            "  background-color: #4a90d9;"
            "  color: white;"
            "  border: none;"
            "  border-radius: 3px;"
            "  padding: 4px 12px;"
            "}"
            "QPushButton:hover {"
            "  background-color: #357abd;"
            "}"
            "QPushButton:pressed {"
            "  background-color: #2a5f8f;"
            "}"
            "QPushButton:disabled {"
            "  background-color: #aaa;"
            "  color: #666;"
            "}"
        ));
        
        if (alreadyRegistered) {
            registerBtn->setEnabled(false);
        } else {
            connect(registerBtn, &QPushButton::clicked, this, &JosekiMergeDialog::onRegisterButtonClicked);
        }
        m_tableWidget->setCellWidget(i, 2, registerBtn);
        
        // 状態
        QTableWidgetItem *statusItem = new QTableWidgetItem();
        statusItem->setTextAlignment(Qt::AlignCenter);
        if (alreadyRegistered) {
            statusItem->setText(tr("✓登録済"));
            statusItem->setForeground(QColor(0, 128, 0));  // 緑色
        }
        m_tableWidget->setItem(i, 3, statusItem);
        
        // 現在の指し手の行をハイライト
        if (entry.isCurrentMove) {
            QColor highlightColor(255, 255, 200);  // 薄い黄色
            plyItem->setBackground(highlightColor);
            moveItem->setBackground(highlightColor);
            statusItem->setBackground(highlightColor);
        }
    }
}

void JosekiMergeDialog::onRegisterButtonClicked()
{
    QPushButton *button = qobject_cast<QPushButton*>(sender());
    if (!button) return;
    
    int row = button->property("row").toInt();
    if (row < 0 || row >= static_cast<int>(m_entries.size())) return;
    
    const KifuMergeEntry &entry = m_entries[row];
    
    // SFENを正規化
    QString normalizedSfen = normalizeSfen(entry.sfen);
    
    qDebug() << "[JosekiMergeDialog] Registering move:" << entry.usiMove 
             << "at ply" << entry.ply
             << "sfen:" << normalizedSfen;
    
    // シグナルを発行
    emit registerMove(normalizedSfen, entry.sfen, entry.usiMove);
    
    // 状態を更新
    QTableWidgetItem *statusItem = m_tableWidget->item(row, 3);
    if (statusItem) {
        statusItem->setText(tr("✓登録済"));
        statusItem->setForeground(QColor(0, 128, 0));  // 緑色
    }
    
    // ボタンを無効化
    button->setEnabled(false);
    button->setText(tr("登録済"));
}

void JosekiMergeDialog::onRegisterAllButtonClicked()
{
    if (m_entries.isEmpty()) {
        QMessageBox::information(this, tr("情報"), tr("登録する指し手がありません。"));
        return;
    }
    
    const int entryCount = static_cast<int>(m_entries.size());
    int count = 0;
    for (int i = 0; i < entryCount; ++i) {
        const KifuMergeEntry &entry = m_entries[i];
        QString normalizedSfen = normalizeSfen(entry.sfen);
        
        emit registerMove(normalizedSfen, entry.sfen, entry.usiMove);
        
        // 状態を更新
        QTableWidgetItem *statusItem = m_tableWidget->item(i, 3);
        if (statusItem) {
            statusItem->setText(tr("✓登録済"));
            statusItem->setForeground(QColor(0, 128, 0));
        }
        
        // ボタンを無効化
        QPushButton *btn = qobject_cast<QPushButton*>(m_tableWidget->cellWidget(i, 2));
        if (btn) {
            btn->setEnabled(false);
            btn->setText(tr("登録済"));
        }
        
        ++count;
    }
    
    QMessageBox::information(this, tr("一括登録完了"), 
        tr("%1件の指し手を定跡に登録しました。").arg(count));
}

void JosekiMergeDialog::onFontSizeIncrease()
{
    if (m_fontSize < 20) {
        m_fontSize += 1;
        applyFontSize();
    }
}

void JosekiMergeDialog::onFontSizeDecrease()
{
    if (m_fontSize > 8) {
        m_fontSize -= 1;
        applyFontSize();
    }
}

void JosekiMergeDialog::applyFontSize()
{
    QFont font = this->font();
    font.setPointSize(m_fontSize);
    setFont(font);
    
    m_tableWidget->setFont(font);
    m_tableWidget->horizontalHeader()->setFont(font);
    m_tableWidget->verticalHeader()->setDefaultSectionSize(m_fontSize + 16);
}

QString JosekiMergeDialog::normalizeSfen(const QString &sfen) const
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

bool JosekiMergeDialog::isRegistered(const QString &sfen, const QString &usiMove) const
{
    QString normalizedSfen = normalizeSfen(sfen);
    QString key = normalizedSfen + QStringLiteral(":") + usiMove;
    return m_registeredMoves.contains(key);
}
