/// @file kifupastedialog.cpp
/// @brief 棋譜貼り付けダイアログクラスの実装

#include "kifupastedialog.h"
#include "buttonstyles.h"
#include "gamesettings.h"
#include "dialogutils.h"
#include "applicationfonts.h"
#include "dialogfontscale.h"
#include "fontsizehelper.h"
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
{
    setupUi();

    // 文字の大きさ（A-/A+ は入力欄の操作の列に置く）
    DialogFontScale::Options fontOptions;
    fontOptions.minSize = kMinimumFontSize;
    fontOptions.maxSize = kMaximumFontSize;
    fontOptions.decrease = m_btnFontSizeDown;
    fontOptions.increase = m_btnFontSizeUp;
    fontOptions.sizeLabel = m_fontSizeLabel;
    auto* fontScale = DialogFontScale::install(this, GameSettings::kifuPasteDialogFontSize, fontOptions);
    connect(fontScale->fontSize(), &FontSizeHelper::fontSizeChanged, this, &KifuPasteDialog::fitToFont);
    fitToFont();
    updateInputActions();
    updateClipboardAction();

    // ウィンドウサイズを復元
    DialogUtils::restoreDialogSize(this, GameSettings::kifuPasteDialogSize());
}

KifuPasteDialog::~KifuPasteDialog()
{
    DialogUtils::saveDialogSize(this, GameSettings::kifuPasteDialogSize);
}

void KifuPasteDialog::closeEvent(QCloseEvent* event)
{
    DialogUtils::saveDialogSize(this, GameSettings::kifuPasteDialogSize);
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

    // 等幅フォントを設定（大きさはダイアログの文字に合わせ、A-/A+ で一緒に変わる）
    QFont monoFont = ApplicationFonts::monospaceFont();
    monoFont.setPointSize(font().pointSize());
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

    m_btnFontSizeDown = new QPushButton(this);
    m_btnFontSizeDown->setObjectName(QStringLiteral("fontDecrease"));
    m_btnFontSizeUp = new QPushButton(this);
    m_btnFontSizeUp->setObjectName(QStringLiteral("fontIncrease"));
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

    for (auto* button : {m_btnPaste, m_btnClear, m_btnCancel}) {
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

void KifuPasteDialog::fitToFont()
{
    // 文字の大きさは DialogFontScale が変える（入力欄の等幅の文字も）。ここでは寸法を文字に合わせる
    m_textEdit->setMinimumHeight(qMax(120, QFontMetrics(m_textEdit->font()).lineSpacing() * 4 + 20));
    const QFontMetrics metrics(font());
    for (auto* button : {m_btnPaste, m_btnClear, m_btnImport, m_btnCancel}) {
        button->setMinimumHeight(qMax(32, metrics.height() + 12));
        button->setMinimumWidth(qMax(36, metrics.horizontalAdvance(button->text()) + 24));
    }

    // 明示した固定の最小サイズだけでは、大きな文字でレイアウトが重なる。
    // 折り返す説明文も含め、その文字サイズで収まる寸法を下限にする。
    layout()->invalidate();
    layout()->activate();
    const QSize contentsMinimum = layout()->totalMinimumSize();
    const int minimumWidth = qMax(kMinimumSize.width(), contentsMinimum.width());
    const int minimumHeight = qMax(contentsMinimum.height(), layout()->totalHeightForWidth(minimumWidth));
    setMinimumSize(minimumWidth, qMax(kMinimumSize.height(), minimumHeight));
}
