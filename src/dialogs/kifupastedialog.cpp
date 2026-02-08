/// @file kifupastedialog.cpp
/// @brief 棋譜貼り付けダイアログクラスの実装

#include "kifupastedialog.h"
#include <QApplication>
#include <QClipboard>
#include <QFont>

KifuPasteDialog::KifuPasteDialog(QWidget* parent)
    : QDialog(parent)
{
    setupUi();
}

void KifuPasteDialog::setupUi()
{
    setWindowTitle(tr("棋譜貼り付け"));
    setMinimumSize(500, 400);
    resize(600, 500);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(8);

    // 説明ラベル
    m_lblInfo = new QLabel(tr("棋譜または局面テキストを下のエリアに貼り付けてください。\n"
                              "対応形式:\n"
                              "  棋譜: KIF、KI2、CSA、USI、JSON棋譜フォーマット(JKF)、USEN\n"
                              "  局面: SFEN、BOD（局面図）\n"
                              "形式は自動判定されます。"), this);
    m_lblInfo->setWordWrap(true);
    mainLayout->addWidget(m_lblInfo);

    // テキスト入力エリア
    m_textEdit = new QPlainTextEdit(this);
    m_textEdit->setPlaceholderText(tr("ここに棋譜を貼り付けてください..."));
    
    // 等幅フォントを設定
    QFont monoFont(QStringLiteral("Monospace"));
    monoFont.setStyleHint(QFont::Monospace);
    monoFont.setPointSize(10);
    m_textEdit->setFont(monoFont);
    
    mainLayout->addWidget(m_textEdit, 1);  // stretch factor = 1

    // ボタンレイアウト（上段：貼り付け・クリア）
    QHBoxLayout* toolLayout = new QHBoxLayout();
    toolLayout->setSpacing(6);
    
    m_btnPaste = new QPushButton(tr("クリップボードから貼り付け"), this);
    m_btnClear = new QPushButton(tr("クリア"), this);
    
    toolLayout->addWidget(m_btnPaste);
    toolLayout->addWidget(m_btnClear);
    toolLayout->addStretch();
    
    mainLayout->addLayout(toolLayout);

    // ボタンレイアウト（下段：取り込む・キャンセル）
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(6);
    
    m_btnImport = new QPushButton(tr("取り込む"), this);
    m_btnCancel = new QPushButton(tr("キャンセル"), this);
    
    m_btnImport->setDefault(true);
    
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_btnImport);
    buttonLayout->addWidget(m_btnCancel);
    
    mainLayout->addLayout(buttonLayout);

    // シグナル・スロット接続
    connect(m_btnImport, &QPushButton::clicked,
            this, &KifuPasteDialog::onImportClicked);
    connect(m_btnCancel, &QPushButton::clicked,
            this, &KifuPasteDialog::onCancelClicked);
    connect(m_btnPaste, &QPushButton::clicked,
            this, &KifuPasteDialog::onPasteClicked);
    connect(m_btnClear, &QPushButton::clicked,
            this, &KifuPasteDialog::onClearClicked);
}

QString KifuPasteDialog::text() const
{
    return m_textEdit ? m_textEdit->toPlainText() : QString();
}

void KifuPasteDialog::onImportClicked()
{
    const QString content = text();
    if (content.trimmed().isEmpty()) {
        // 空の場合は何もしない（またはメッセージを表示）
        return;
    }
    
    emit importRequested(content);
    accept();
}

void KifuPasteDialog::onCancelClicked()
{
    reject();
}

void KifuPasteDialog::onPasteClicked()
{
    QClipboard* clipboard = QApplication::clipboard();
    if (clipboard) {
        const QString clipText = clipboard->text();
        if (!clipText.isEmpty()) {
            m_textEdit->setPlainText(clipText);
        }
    }
}

void KifuPasteDialog::onClearClicked()
{
    if (m_textEdit) {
        m_textEdit->clear();
    }
}
