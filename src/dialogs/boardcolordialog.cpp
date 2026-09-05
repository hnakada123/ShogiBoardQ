#include "boardcolordialog.h"
#include "appsettings.h"
#include "boardappearance.h"
#include "dialogutils.h"
#include "pieceimageprovider.h"

#include <QColorDialog>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

BoardColorDialog::BoardColorDialog(QWidget* parent)
    : QDialog(parent), m_picker(new QColorDialog(this))
{
    setWindowTitle(tr("盤面の配色"));
    setMinimumSize(440, 380);
    auto* layout = new QVBoxLayout(this);
    auto* description = new QLabel(tr("色を選択すると、すべての将棋盤に反映・保存されます。"), this);
    description->setWordWrap(true);
    layout->addWidget(description);
    m_presetLabel = new QLabel(this);
    m_presetLabel->setObjectName(QStringLiteral("boardColorPresetLabel"));
    layout->addWidget(m_presetLabel);
    m_presetCombo = new QComboBox(this);
    m_presetCombo->setObjectName(QStringLiteral("boardColorPresetCombo"));
    m_presetCombo->setPlaceholderText(tr("カスタム（個別に指定）"));
    m_presetCombo->setIconSize(QSize(120, 72));
    m_presetCombo->setMaxVisibleItems(5);
    m_presetLabel->setBuddy(m_presetCombo);
    layout->addWidget(m_presetCombo);
    connect(m_presetCombo, &QComboBox::activated, this, &BoardColorDialog::applyPreset);
    connect(&PieceImageProvider::instance(), &PieceImageProvider::styleChanged,
            this, &BoardColorDialog::rebuildPresets);
    auto* form = new QFormLayout;
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    layout->addLayout(form);
    m_fields = {{{new QPushButton(this), &BoardColors::background, tr("将棋盤の背景")},
                 {new QPushButton(this), &BoardColors::board, tr("将棋盤")},
                 {new QPushButton(this), &BoardColors::stand, tr("駒台")},
                 {new QPushButton(this), &BoardColors::grid, tr("マス罫線")}}};
    const QStringList names = {QStringLiteral("backgroundColorButton"), QStringLiteral("boardColorButton"),
                               QStringLiteral("standColorButton"), QStringLiteral("gridColorButton")};
    for (size_t i = 0; i < m_fields.size(); ++i) {
        auto& field = m_fields[i];
        field.button->setObjectName(names.at(static_cast<qsizetype>(i)));
        field.button->setAccessibleName(field.label);
        field.button->setMinimumHeight(36);
        field.button->setIconSize(QSize(36, 22));
        field.button->setAutoDefault(false);
        form->addRow(field.label, field.button);
        connect(field.button, &QPushButton::clicked, this, &BoardColorDialog::chooseColor);
    }
    layout->addStretch();
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    auto* reset = buttons->addButton(tr("標準色に戻す"), QDialogButtonBox::ResetRole);
    reset->setObjectName(QStringLiteral("resetBoardColorsButton"));
    connect(reset, &QPushButton::clicked, this, &BoardColorDialog::restoreDefaults);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    m_picker->setObjectName(QStringLiteral("boardColorPicker"));
    m_picker->setOption(QColorDialog::DontUseNativeDialog);
    m_picker->setWindowModality(Qt::WindowModal);
    connect(m_picker, &QColorDialog::colorSelected, this, &BoardColorDialog::applyColor);
    connect(m_picker, &QDialog::finished, this, &BoardColorDialog::savePickerSize);
    connect(&BoardAppearance::instance(), &BoardAppearance::colorsChanged,
            this, &BoardColorDialog::refreshButtons);
    rebuildPresets();
    refreshButtons();
    DialogUtils::restoreDialogSize(this, AppSettings::boardColorDialogSize());
}

BoardColorDialog::~BoardColorDialog()
{
    DialogUtils::saveDialogSize(this, AppSettings::setBoardColorDialogSize);
}

void BoardColorDialog::chooseColor()
{
    for (const auto& field : m_fields) {
        if (field.button != sender()) continue;
        m_selectedMember = field.member;
        m_picker->setWindowTitle(tr("%1の色").arg(field.label));
        m_picker->setCurrentColor(BoardAppearance::instance().colors().*field.member);
        DialogUtils::restoreDialogSize(m_picker, AppSettings::boardColorPickerSize());
        m_picker->open();
        return;
    }
}

void BoardColorDialog::applyColor(const QColor& color)
{
    if (!m_selectedMember || !color.isValid()) return;
    auto colors = BoardAppearance::instance().colors();
    colors.*m_selectedMember = color;
    BoardAppearance::instance().setColors(colors);
}

void BoardColorDialog::refreshButtons()
{
    const auto colors = BoardAppearance::instance().colors();
    for (const auto& field : m_fields) {
        const QColor color = colors.*field.member;
        QPixmap swatch(36, 22);
        swatch.fill(color);
        QPainter painter(&swatch);
        painter.setPen(palette().color(QPalette::Mid));
        painter.drawRect(swatch.rect().adjusted(0, 0, -1, -1));
        painter.end();
        field.button->setIcon(QIcon(swatch));
        field.button->setText(color.name().toUpper());
    }
    syncPresetSelection();
}

void BoardColorDialog::restoreDefaults()
{
    BoardAppearance::instance().setColors(BoardColors{});
}

void BoardColorDialog::savePickerSize()
{
    DialogUtils::saveDialogSize(m_picker, AppSettings::setBoardColorPickerSize);
}

void BoardColorDialog::rebuildPresets()
{
    const QString style = PieceImageProvider::instance().style();
    m_presetLabel->setText(tr("%1に合うおすすめ配色").arg(BoardColorPresets::pieceStyleName(style)));
    m_presets = BoardColorPresets::forPieceStyle(style);
    const QSignalBlocker blocker(m_presetCombo);
    m_presetCombo->clear();
    for (const auto& preset : m_presets) {
        m_presetCombo->addItem(presetIcon(preset.colors), preset.name);
    }
    syncPresetSelection();
}

void BoardColorDialog::applyPreset(int index)
{
    if (index < 0 || index >= m_presets.size()) return;
    BoardAppearance::instance().setColors(m_presets.at(index).colors);
}

void BoardColorDialog::syncPresetSelection()
{
    const auto colors = BoardAppearance::instance().colors();
    int selected = -1;
    for (qsizetype i = 0; i < m_presets.size(); ++i) {
        if (m_presets.at(i).colors == colors) {
            selected = static_cast<int>(i);
            break;
        }
    }
    const QSignalBlocker blocker(m_presetCombo);
    m_presetCombo->setCurrentIndex(selected);
}

QIcon BoardColorDialog::presetIcon(const BoardColors& colors) const
{
    // 高DPIでも輪郭が崩れないよう2倍で描画する。
    QPixmap preview(240, 144);
    preview.setDevicePixelRatio(2);
    preview.fill(colors.background);
    QPainter painter(&preview);
    painter.fillRect(QRect(4, 5, 22, 29), colors.stand);
    painter.fillRect(QRect(94, 38, 22, 29), colors.stand);
    painter.setBrush(colors.board);
    painter.setPen(colors.grid);
    for (int row = 0; row < 3; ++row)
        for (int col = 0; col < 3; ++col)
            painter.drawRect(QRect(30 + col * 20, 3 + row * 22, 20, 22));
    auto& pieces = PieceImageProvider::instance();
    pieces.icon(QLatin1Char('u')).paint(&painter, QRect(50, 3, 20, 22));
    pieces.icon(QLatin1Char('P')).paint(&painter, QRect(70, 25, 20, 22));
    pieces.icon(QLatin1Char('K')).paint(&painter, QRect(50, 47, 20, 22));
    painter.end();
    return QIcon(preview);
}
