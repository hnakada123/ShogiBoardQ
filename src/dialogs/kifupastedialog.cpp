/// @file kifupastedialog.cpp
/// @brief 棋譜貼り付けダイアログクラスの実装

#include "kifupastedialog.h"
#include "buttonstyles.h"
#include "gamesettings.h"
#include "dialogutils.h"
#include <QApplication>
#include <QClipboard>
#include <QFont>

namespace {
constexpr QSize kDefaultSize{600, 500};
constexpr QSize kMinimumSize{500, 400};
} // namespace

KifuPasteDialog::KifuPasteDialog(QWidget* parent)
    : QDialog(parent)
{
    setupUi();
    loadFontSizeSettings();

    // ウィンドウサイズを復元
    DialogUtils::restoreDialogSize(this, GameSettings::kifuPasteDialogSize());
}

KifuPasteDialog::~KifuPasteDialog()
{
    DialogUtils::saveDialogSize(this, GameSettings::setKifuPasteDialogSize);
}

void KifuPasteDialog::closeEvent(QCloseEvent* event)
{
    DialogUtils::saveDialogSize(this, GameSettings::setKifuPasteDialogSize);
    QDialog::closeEvent(event);
}

void KifuPasteDialog::setupUi()
{
    setWindowTitle(tr("棋譜貼り付け"));
    setMinimumSize(kMinimumSize);
    resize(kDefaultSize);

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
    m_btnPaste->setStyleSheet(ButtonStyles::editOperation());
    m_btnClear = new QPushButton(tr("クリア"), this);
    m_btnClear->setStyleSheet(ButtonStyles::secondaryNeutral());

    toolLayout->addWidget(m_btnPaste);
    toolLayout->addWidget(m_btnClear);
    toolLayout->addStretch();

    mainLayout->addLayout(toolLayout);

    // ボタンレイアウト（下段：フォントサイズ・取り込む・キャンセル）
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(6);

    m_btnFontSizeDown = new QPushButton(tr("A-"), this);
    m_btnFontSizeDown->setStyleSheet(ButtonStyles::fontButton());
    m_btnFontSizeUp = new QPushButton(tr("A+"), this);
    m_btnFontSizeUp->setStyleSheet(ButtonStyles::fontButton());

    m_btnImport = new QPushButton(tr("取り込む"), this);
    m_btnImport->setStyleSheet(ButtonStyles::primaryAction());
    m_btnCancel = new QPushButton(tr("キャンセル"), this);
    m_btnCancel->setStyleSheet(ButtonStyles::secondaryNeutral());

    m_btnImport->setDefault(true);

    buttonLayout->addWidget(m_btnFontSizeDown);
    buttonLayout->addWidget(m_btnFontSizeUp);
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
    connect(m_btnFontSizeDown, &QPushButton::clicked,
            this, &KifuPasteDialog::decreaseFontSize);
    connect(m_btnFontSizeUp, &QPushButton::clicked,
            this, &KifuPasteDialog::increaseFontSize);
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

void KifuPasteDialog::increaseFontSize()
{
    if (m_fontSize < MaxFontSize) {
        m_fontSize++;
        applyFontSize(m_fontSize);
        saveFontSizeSettings();
    }
}

void KifuPasteDialog::decreaseFontSize()
{
    if (m_fontSize > MinFontSize) {
        m_fontSize--;
        applyFontSize(m_fontSize);
        saveFontSizeSettings();
    }
}

void KifuPasteDialog::applyFontSize(int size)
{
    QFont font = this->font();
    font.setPointSize(size);
    setFont(font);

    // テキストエディタには等幅フォントを維持
    QFont monoFont = m_textEdit->font();
    monoFont.setPointSize(size);
    m_textEdit->setFont(monoFont);

    // 全子ウィジェットにフォントを適用
    const QList<QWidget*> widgets = findChildren<QWidget*>();
    for (QWidget* widget : std::as_const(widgets)) {
        if (widget && widget != m_textEdit) {
            widget->setFont(font);
        }
    }
}

void KifuPasteDialog::loadFontSizeSettings()
{
    m_fontSize = GameSettings::kifuPasteDialogFontSize();
    if (m_fontSize < MinFontSize) m_fontSize = MinFontSize;
    if (m_fontSize > MaxFontSize) m_fontSize = MaxFontSize;
    applyFontSize(m_fontSize);
}

void KifuPasteDialog::saveFontSizeSettings()
{
    GameSettings::setKifuPasteDialogFontSize(m_fontSize);
}
