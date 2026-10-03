/// @file kifupastedialog.cpp
/// @brief 棋譜貼り付けダイアログクラスの実装

#include "kifupastedialog.h"
#include "buttonstyles.h"
#include "gamesettings.h"
#include "dialogutils.h"
#include "applicationfonts.h"
#include <QApplication>
#include <QClipboard>
#include <QFont>
#include <QFontMetrics>
#include <QShortcut>
#include <QTextDocument>
#include <utility>

namespace {
constexpr QSize kDefaultSize{600, 500};
constexpr QSize kMinimumSize{500, 400};
constexpr int kMinimumFontSize = 7;
constexpr int kMaximumFontSize = 20;
} // namespace

KifuPasteDialog::KifuPasteDialog(QWidget* parent)
    : QDialog(parent)
    , m_fontHelper({GameSettings::kifuPasteDialogFontSize(), kMinimumFontSize, kMaximumFontSize, 1,
                    GameSettings::setKifuPasteDialogFontSize})
{
    setupUi();
    applyFontSize();
    updateInputActions();
    updateClipboardAction();

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
    setObjectName(QStringLiteral("kifuPasteDialog"));
    setMinimumSize(kMinimumSize);
    resize(kDefaultSize);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    // 説明ラベル
    auto* info = new QLabel(tr("棋譜や局面のテキストを貼り付けてください。形式は自動判定されます。"), this);
    info->setWordWrap(true);
    mainLayout->addWidget(info);

    // テキスト入力エリア
    m_textEdit = new QPlainTextEdit(this);
    m_textEdit->setObjectName(QStringLiteral("kifuPasteText"));
    m_textEdit->setAccessibleName(tr("棋譜・局面テキスト"));
    m_textEdit->setPlaceholderText(tr("ここに棋譜や局面を貼り付け（Ctrl+V）"));
    m_textEdit->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_textEdit->document()->setDocumentMargin(10);

    // 等幅フォントを設定
    QFont monoFont = ApplicationFonts::monospaceFont();
    monoFont.setPointSize(10);
    m_textEdit->setFont(monoFont);
    ApplicationFonts::useJapaneseFont(m_textEdit);

    // 入力欄の操作を一か所にまとめる。
    QHBoxLayout* toolLayout = new QHBoxLayout();
    toolLayout->setSpacing(8);

    m_btnPaste = new QPushButton(tr("クリップボードから貼り付け"), this);
    m_btnPaste->setObjectName(QStringLiteral("pasteFromClipboard"));
    m_btnPaste->setStyleSheet(ButtonStyles::panelToolButton());
    m_btnClear = new QPushButton(tr("クリア"), this);
    m_btnClear->setObjectName(QStringLiteral("clearText"));
    m_btnClear->setStyleSheet(ButtonStyles::panelToolButton());

    toolLayout->addWidget(m_btnPaste);
    toolLayout->addWidget(m_btnClear);
    toolLayout->addStretch();

    m_btnFontSizeDown = new QPushButton(tr("A-"), this);
    m_btnFontSizeDown->setObjectName(QStringLiteral("fontDecrease"));
    m_btnFontSizeDown->setStyleSheet(ButtonStyles::panelToolButton());
    m_btnFontSizeDown->setToolTip(tr("文字を小さくする"));
    m_btnFontSizeDown->setAccessibleName(m_btnFontSizeDown->toolTip());
    m_btnFontSizeUp = new QPushButton(tr("A+"), this);
    m_btnFontSizeUp->setObjectName(QStringLiteral("fontIncrease"));
    m_btnFontSizeUp->setStyleSheet(ButtonStyles::panelToolButton());
    m_btnFontSizeUp->setToolTip(tr("文字を大きくする"));
    m_btnFontSizeUp->setAccessibleName(m_btnFontSizeUp->toolTip());
    m_fontSizeLabel = new QLabel(this);
    m_fontSizeLabel->setAlignment(Qt::AlignCenter);
    m_fontSizeLabel->setToolTip(tr("文字サイズ"));
    toolLayout->addWidget(m_btnFontSizeDown);
    toolLayout->addWidget(m_fontSizeLabel);
    toolLayout->addWidget(m_btnFontSizeUp);
    mainLayout->addLayout(toolLayout);
    mainLayout->addWidget(m_textEdit, 1);

    auto* formats = new QLabel(tr("棋譜: KIF / KI2 / CSA / JKF / USI / USEN\n局面: SFEN / BOD（局面図）"), this);
    formats->setWordWrap(true);
    mainLayout->addWidget(formats);

    // 確定・キャンセルは入力欄の操作と分離する。
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(8);

    m_btnImport = new QPushButton(tr("取り込む"), this);
    m_btnImport->setObjectName(QStringLiteral("importKifu"));
    m_btnImport->setStyleSheet(ButtonStyles::primaryAction());
    m_btnImport->setToolTip(tr("取り込む（Ctrl+Enter）"));
    m_btnCancel = new QPushButton(tr("キャンセル"), this);
    m_btnCancel->setObjectName(QStringLiteral("cancelPaste"));
    m_btnCancel->setStyleSheet(ButtonStyles::panelToolButton());

    m_btnImport->setDefault(true);

    buttonLayout->addStretch();
    buttonLayout->addWidget(m_btnImport);
    buttonLayout->addWidget(m_btnCancel);

    mainLayout->addLayout(buttonLayout);

    for (auto* button : {m_btnPaste, m_btnClear, m_btnFontSizeDown, m_btnFontSizeUp, m_btnCancel}) {
        button->setAutoDefault(false);
    }
    setTabOrder(m_textEdit, m_btnImport);
    setTabOrder(m_btnImport, m_btnCancel);
    setTabOrder(m_btnCancel, m_btnPaste);
    setTabOrder(m_btnPaste, m_btnClear);
    setTabOrder(m_btnClear, m_btnFontSizeDown);
    setTabOrder(m_btnFontSizeDown, m_btnFontSizeUp);
    m_textEdit->setFocus();

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
    connect(m_textEdit, &QPlainTextEdit::textChanged,
            this, &KifuPasteDialog::updateInputActions);
    connect(QApplication::clipboard(), &QClipboard::dataChanged,
            this, &KifuPasteDialog::updateClipboardAction);
    for (const auto key : {Qt::Key_Return, Qt::Key_Enter}) {
        auto* shortcut = new QShortcut(QKeySequence(Qt::CTRL | key), this);
        connect(shortcut, &QShortcut::activated, this, &KifuPasteDialog::onImportClicked);
    }
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
    // 取り込み側が成功時に閉じる。確認キャンセルや解析失敗なら入力を保持する。
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
            m_textEdit->setFocus();
        }
    }
}

void KifuPasteDialog::onClearClicked()
{
    if (m_textEdit) {
        m_textEdit->clear();
        m_textEdit->setFocus();
    }
}

void KifuPasteDialog::increaseFontSize()
{
    if (m_fontHelper.increase()) applyFontSize();
}

void KifuPasteDialog::decreaseFontSize()
{
    if (m_fontHelper.decrease()) applyFontSize();
}

void KifuPasteDialog::updateInputActions()
{
    const QString content = text();
    m_btnImport->setEnabled(!content.trimmed().isEmpty());
    m_btnClear->setEnabled(!content.isEmpty());
}

void KifuPasteDialog::updateClipboardAction()
{
    m_btnPaste->setEnabled(!QApplication::clipboard()->text().trimmed().isEmpty());
}

void KifuPasteDialog::applyFontSize()
{
    DialogUtils::standardizeDialog(this);
    const int size = m_fontHelper.fontSize();
    QFont font = this->font();
    font.setPointSize(size);
    setFont(font);

    // テキストエディタには等幅フォントを維持
    QFont monoFont = m_textEdit->font();
    monoFont.setPointSize(size);
    m_textEdit->setFont(monoFont);
    m_textEdit->setMinimumHeight(qMax(120, QFontMetrics(monoFont).lineSpacing() * 4 + 20));

    // 全子ウィジェットにフォントを適用
    const auto widgets = findChildren<QWidget*>();
    for (QWidget* widget : std::as_const(widgets)) {
        if (widget && widget != m_textEdit) {
            widget->setFont(font);
        }
    }
    m_fontSizeLabel->setText(tr("%1 pt").arg(size));
    m_btnFontSizeDown->setEnabled(size > kMinimumFontSize);
    m_btnFontSizeUp->setEnabled(size < kMaximumFontSize);
    const int buttonHeight = qMax(32, QFontMetrics(font).height() + 12);
    for (auto* button : {m_btnPaste, m_btnClear, m_btnFontSizeDown, m_btnFontSizeUp, m_btnImport, m_btnCancel}) {
        button->setMinimumHeight(buttonHeight);
        button->setMinimumWidth(qMax(36, QFontMetrics(font).horizontalAdvance(button->text()) + 24));
    }

    // 明示した固定の最小サイズだけでは、大きな文字でレイアウトが重なる。
    // 折り返す説明文も含め、その文字サイズで収まる寸法を下限にする。
    layout()->invalidate();
    layout()->activate();
    const QSize contentsMinimum = layout()->totalMinimumSize();
    const int minimumWidth = qMax(kMinimumSize.width(), contentsMinimum.width());
    const int minimumHeight = qMax(contentsMinimum.height(), layout()->totalHeightForWidth(minimumWidth));
    setMinimumSize(minimumWidth, qMax(kMinimumSize.height(), minimumHeight));
    DialogUtils::updateFontButtons(this, size, kMinimumFontSize, kMaximumFontSize);
}
