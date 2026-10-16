/// @file josekimergedialog.cpp
/// @brief 定跡統合ダイアログクラスの実装

#include "josekimergedialog.h"
#include "josekibook.h"
#include "dialogfontscale.h"
#include "fontsizehelper.h"
#include <QSet>
#include "kifumovedelegate.h"
#include "buttonstyles.h"
#include "dialogutils.h"
#include "josekisettings.h"

#include <QHeaderView>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QFileInfo>

namespace {
constexpr QSize kDefaultSize{600, 500};
constexpr QSize kMinimumSize{500, 400};
} // namespace

JosekiMergeDialog::JosekiMergeDialog(QWidget *parent)
    : QDialog(parent)
{
    setupUi();

    // ウィンドウサイズを復元
    DialogUtils::restoreDialogSize(this, JosekiSettings::josekiMergeDialogSize());
}

JosekiMergeDialog::~JosekiMergeDialog()
{
    DialogUtils::saveDialogSize(this, JosekiSettings::josekiMergeDialogSize);
}

void JosekiMergeDialog::closeEvent(QCloseEvent *event)
{
    DialogUtils::saveDialogSize(this, JosekiSettings::josekiMergeDialogSize);
    QDialog::closeEvent(event);
}

void JosekiMergeDialog::setupUi()
{
    setWindowTitle(tr("棋譜から定跡にマージ"));
    setMinimumSize(kMinimumSize);
    resize(kDefaultSize);
    
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    
    m_fontDecreaseBtn = new QPushButton(this);
    m_fontIncreaseBtn = new QPushButton(this);
    m_statusLabel = new QLabel(tr("登録する指し手がありません。"), this);
    m_statusLabel->setWordWrap(true);

    // === マージ先ファイル表示 ===
    m_targetFileLabel = new QLabel(tr("マージ先: (未設定)"), this);
    m_targetFileLabel->setWordWrap(true);
    m_targetFileLabel->setTextFormat(Qt::PlainText);
    m_targetFileLabel->setStyleSheet(QStringLiteral("QLabel { font-weight: bold; }"));
    mainLayout->addWidget(m_targetFileLabel);
    
    // === 説明ラベル ===
    QLabel *descLabel = new QLabel(tr("棋譜の指し手を定跡に登録します。「登録」ボタンで個別に、「全て登録」で一括登録できます。"), this);
    descLabel->setWordWrap(true);
    mainLayout->addWidget(descLabel);
    
    // === 自動保存説明ラベル ===
    m_autoSaveLabel = new QLabel(tr("※ 登録時に定跡ファイルへ自動保存されます"), this);
    m_autoSaveLabel->setWordWrap(true);
    mainLayout->addWidget(m_autoSaveLabel);
    
    // === テーブル ===
    m_tableWidget = new QTableWidget(this);
    m_tableWidget->setItemDelegateForColumn(1, new KifuMoveDelegate(m_tableWidget));
    m_registerButtons = new QButtonGroup(this);
    connect(m_registerButtons, &QButtonGroup::idClicked, this, &JosekiMergeDialog::onRegisterButtonClicked);
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
    
    mainLayout->addWidget(m_statusLabel);
    mainLayout->addWidget(m_tableWidget, 1);
    
    // === ボタン行 ===
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->addWidget(m_fontDecreaseBtn);
    buttonLayout->addWidget(m_fontIncreaseBtn);
    buttonLayout->addStretch();
    
    m_registerAllButton = new QPushButton(tr("全て登録"), this);
    m_registerAllButton->setObjectName(QStringLiteral("registerAllMoves"));
    m_registerAllButton->setEnabled(false);
    m_registerAllButton->setAutoDefault(false);
    m_registerAllButton->setToolTip(tr("全ての指し手を定跡に登録"));
    m_registerAllButton->setStyleSheet(ButtonStyles::primaryAction());
    buttonLayout->addWidget(m_registerAllButton);

    m_closeButton = new QPushButton(tr("閉じる"), this);
    m_closeButton->setStyleSheet(ButtonStyles::secondaryNeutral());
    buttonLayout->addWidget(m_closeButton);
    
    mainLayout->addLayout(buttonLayout);
    
    // === シグナル・スロット接続 ===
    connect(m_registerAllButton, &QPushButton::clicked, this, &JosekiMergeDialog::onRegisterAllButtonClicked);
    connect(m_closeButton, &QPushButton::clicked, this, &QDialog::accept);

    DialogFontScale::Options fontOptions;
    fontOptions.maxSize = 20;
    fontOptions.decrease = m_fontDecreaseBtn;
    fontOptions.increase = m_fontIncreaseBtn;
    const DialogFontScale* scale = DialogFontScale::install(this, JosekiSettings::josekiMergeDialogFontSize, fontOptions);
    connect(scale->fontSize(), &FontSizeHelper::fontSizeChanged, this, &JosekiMergeDialog::fitRowsToFont);
    fitRowsToFont();
}

void JosekiMergeDialog::setKifuData(const QList<KifuMergeEntry> &entries)
{
    m_entries = entries;
    
    m_statusLabel->setText(entries.isEmpty() ? tr("登録する指し手がありません。")
                                           : tr("%1手の棋譜").arg(entries.size()));
    
    updateTable();
}

void JosekiMergeDialog::setRegisteredMoves(const QSet<JosekiMoveKey> &registeredMoves)
{
    m_registeredMoves = registeredMoves;
    updateTable();
}

void JosekiMergeDialog::setTargetJosekiFile(const QString &filePath)
{
    if (filePath.isEmpty()) {
        m_targetFileLabel->setText(tr("マージ先: (未設定)"));
        m_targetFileLabel->setToolTip(QString());
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
    bool hasUnregistered = false;
    
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
        hasUnregistered = hasUnregistered || !alreadyRegistered;
        
        // 登録ボタン（青系の配色）
        QPushButton *registerBtn = new QPushButton(alreadyRegistered ? tr("登録済") : tr("登録"), this);
        registerBtn->setAutoDefault(false);
        registerBtn->setStyleSheet(ButtonStyles::tableRegisterButton());
        registerBtn->setEnabled(!alreadyRegistered);
        m_registerButtons->addButton(registerBtn, i);
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
    m_registerAllButton->setEnabled(hasUnregistered);
    // 行を入れてから列幅を合わせる（空の表で合わせると指し手が縦に折り返し、登録ボタンの文字が切れる）
    for (int column = 0; column < 3; ++column) m_tableWidget->resizeColumnToContents(column);
    m_tableWidget->resizeRowsToContents();
}

void JosekiMergeDialog::setRegistrar(Registrar registrar)
{
    m_registrar = std::move(registrar);
}

void JosekiMergeDialog::onRegisterButtonClicked(int row)
{
    if (row < 0 || row >= static_cast<int>(m_entries.size())) return;
    const KifuMergeEntry &entry = m_entries.at(row);
    if (isRegistered(entry.sfen, entry.usiMove)) return;
    if (registerEntries({entry})) updateTable();
}

bool JosekiMergeDialog::registerEntries(const QList<KifuMergeEntry> &entries)
{
    if (!isEnabled() || !m_registrar || entries.isEmpty() || !m_registrar(entries)) return false;
    for (const KifuMergeEntry &entry : entries)
        m_registeredMoves.insert(moveKey(entry));
    return true;
}

void JosekiMergeDialog::onRegisterAllButtonClicked()
{
    if (m_entries.isEmpty()) {
        QMessageBox::information(this, tr("情報"), tr("登録する指し手がありません。"));
        return;
    }
    if (!isEnabled()) return;

    QList<KifuMergeEntry> pending;
    QSet<JosekiMoveKey> pendingKeys;
    for (const KifuMergeEntry &entry : std::as_const(m_entries)) {
        const JosekiMoveKey key = moveKey(entry);
        if (isRegistered(entry.sfen, entry.usiMove) || pendingKeys.contains(key)) continue;
        pendingKeys.insert(key);
        pending.append(entry);
    }
    if (pending.isEmpty()) return;

    const bool registered = registerEntries(pending);
    updateTable();
    if (!registered) return;

    QMessageBox::information(this, tr("一括登録完了"),
        tr("%1件の指し手を定跡に登録しました。").arg(pending.size()));
}

void JosekiMergeDialog::fitRowsToFont()
{
    m_tableWidget->verticalHeader()->setDefaultSectionSize(QFontMetrics(m_tableWidget->font()).height() + 16);
    m_tableWidget->resizeRowsToContents();
    m_tableWidget->resizeColumnsToContents();
}

JosekiMoveKey JosekiMergeDialog::moveKey(const KifuMergeEntry &entry)
{
    return {JosekiBook::positionKey(entry.sfen), entry.usiMove};
}

bool JosekiMergeDialog::isRegistered(const QString &sfen, const QString &usiMove) const
{
    return m_registeredMoves.contains({JosekiBook::positionKey(sfen), usiMove});
}
