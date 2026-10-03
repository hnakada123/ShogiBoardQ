#include "fontsettingsdialog.h"

#include "applicationfonts.h"
#include "appsettings.h"
#include "dialogfontscale.h"

#include <QApplication>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFontComboBox>
#include <QFontDatabase>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

FontSettingsDialog::FontSettingsDialog(QWidget* parent)
    : QDialog(parent)
    , m_useDefault(new QCheckBox(tr("標準フォントを使用する"), this))
    , m_family(new QFontComboBox(this))
    , m_preview(new QLabel(tr("将棋盤・棋譜・検討\n先手 ▲７六歩　後手 △３四歩\nABC abc 0123456789"), this))
{
    setWindowTitle(tr("GUI全体のフォント"));
    setObjectName(QStringLiteral("fontSettingsDialog"));
    m_useDefault->setObjectName(QStringLiteral("useDefaultFont"));
    m_family->setObjectName(QStringLiteral("fontFamilyComboBox"));
    m_preview->setObjectName(QStringLiteral("fontPreview"));

    auto* layout = new QVBoxLayout(this);
    auto* description = new QLabel(tr("メニュー、棋譜、ログ、各ダイアログに共通の書体を設定します。\n文字サイズは各画面の設定を維持します。"), this);
    description->setWordWrap(true);
    layout->addWidget(description);
    layout->addWidget(m_useDefault);
    auto* familyLabel = new QLabel(tr("書体:"), this);
    familyLabel->setBuddy(m_family);
    layout->addWidget(familyLabel);
    layout->addWidget(m_family);
    m_preview->setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
    m_preview->setAlignment(Qt::AlignCenter);
    m_preview->setMargin(16);
    m_preview->setWordWrap(true);
    layout->addWidget(m_preview, 1);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    auto* reset = buttons->addButton(tr("標準に戻す"), QDialogButtonBox::ResetRole);
    layout->addWidget(buttons);

    const QString saved = AppSettings::uiFontFamily();
    const bool available = QFontDatabase::families().contains(saved, Qt::CaseInsensitive);
    m_useDefault->setChecked(!available);
    m_family->setCurrentFont(QFont(available ? saved : ApplicationFonts::defaultFamily()));
    updatePreview();
    resize(AppSettings::fontSettingsDialogSize());

    connect(m_useDefault, &QCheckBox::toggled, this, &FontSettingsDialog::updatePreview);
    connect(m_family, &QFontComboBox::currentFontChanged, this, &FontSettingsDialog::updatePreview);
    connect(reset, &QPushButton::clicked, this, &FontSettingsDialog::restoreDefaults);
    connect(buttons, &QDialogButtonBox::accepted, this, &FontSettingsDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &FontSettingsDialog::reject);
    DialogFontScale::install(this, QStringLiteral("fontSettings"));
}

void FontSettingsDialog::updatePreview()
{
    m_family->setEnabled(!m_useDefault->isChecked());
    QFont preview = QApplication::font();
    preview.setFamily(m_useDefault->isChecked() ? ApplicationFonts::defaultFamily()
                                               : m_family->currentFont().family());
    preview.setPointSize(font().pointSize() + 6);
    m_preview->setFont(preview);
}

void FontSettingsDialog::restoreDefaults()
{
    m_family->setCurrentFont(QFont(ApplicationFonts::defaultFamily()));
    m_useDefault->setChecked(true);
}

void FontSettingsDialog::accept()
{
    const QString family = m_useDefault->isChecked() ? QString() : m_family->currentFont().family();
    AppSettings::setUiFontFamily(family);
    ApplicationFonts::applyFamily(family);
    QDialog::accept();
}

void FontSettingsDialog::done(int result)
{
    AppSettings::setFontSettingsDialogSize(size());
    QDialog::done(result);
}
