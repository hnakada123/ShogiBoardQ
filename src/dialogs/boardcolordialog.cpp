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
#include <QTabWidget>
#include <QVBoxLayout>
#include <utility>

BoardColorDialog::BoardColorDialog(QWidget* parent)
    : QDialog(parent), m_picker(new QColorDialog(this))
{
    setWindowTitle(tr("盤面の配色"));
    setMinimumSize(560, 460);
    auto* layout = new QVBoxLayout(this);
    auto* description = new QLabel(tr("色を選択すると、すべての将棋盤に反映・保存されます。"), this);
    description->setWordWrap(true);
    layout->addWidget(description);
    m_presetLabel = new QLabel(this);
    m_presetLabel->setObjectName(QStringLiteral("boardColorPresetLabel"));
    m_presetCombo = new QComboBox(this);
    m_presetCombo->setObjectName(QStringLiteral("boardColorPresetCombo"));
    m_presetCombo->setPlaceholderText(tr("カスタム（個別に指定）"));
    m_presetCombo->setIconSize(QSize(120, 72));
    m_presetCombo->setMaxVisibleItems(5);
    m_presetLabel->setBuddy(m_presetCombo);
    connect(m_presetCombo, &QComboBox::activated, this, &BoardColorDialog::applyPreset);
    connect(&PieceImageProvider::instance(), &PieceImageProvider::styleChanged,
            this, &BoardColorDialog::rebuildPresets);
    m_tabs = new QTabWidget(this);
    m_tabs->setObjectName(QStringLiteral("boardColorTabs"));
    layout->addWidget(m_tabs);
    createColorPages();
    m_tabs->setCurrentIndex(qBound(0, AppSettings::boardColorDialogTab(), m_tabs->count() - 1));
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
    AppSettings::setBoardColorDialogTab(m_tabs->currentIndex());
}

void BoardColorDialog::createColorPages()
{
    auto page = [this](const QString& title, const QString& note = QString()) {
        auto* widget = new QWidget(m_tabs);
        auto* layout = new QVBoxLayout(widget);
        if (m_tabs->count() == 0) {
            layout->addWidget(m_presetLabel);
            layout->addWidget(m_presetCombo);
        }
        auto* form = new QFormLayout;
        form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        layout->addLayout(form);
        if (!note.isEmpty()) {
            auto* label = new QLabel(note, widget);
            label->setWordWrap(true);
            layout->addWidget(label);
        }
        layout->addStretch();
        m_tabs->addTab(widget, title);
        return form;
    };
    const QString transparentNote = tr("背景や枠線のアルファ値を0にすると透明になります。");
    auto* form = page(tr("盤・駒台"));
    addColorField(form, &BoardColors::background, tr("将棋盤の背景"), QStringLiteral("backgroundColorButton"));
    addColorField(form, &BoardColors::board, tr("将棋盤"), QStringLiteral("boardColorButton"));
    addColorField(form, &BoardColors::stand, tr("駒台"), QStringLiteral("standColorButton"));
    addColorField(form, &BoardColors::grid, tr("マス罫線"), QStringLiteral("gridColorButton"));

    form = page(tr("カード全体"), tr("対局者名と持ち時間を囲むカードの色を設定します。"));
    addColorField(form, &BoardColors::cardBackground, tr("カードの背景"), QStringLiteral("cardBackgroundColorButton"));
    addColorField(form, &BoardColors::cardBorder, tr("通常の枠線"), QStringLiteral("cardBorderColorButton"));
    addColorField(form, &BoardColors::activeCardBorder, tr("手番側の枠線"), QStringLiteral("activeCardBorderColorButton"));

    form = page(tr("手番"), tr("「手番」バッジの色を設定します。") + QChar(0x0a) + transparentNote);
    addColorField(form, &BoardColors::turnBackground, tr("手番の背景"), QStringLiteral("turnBackgroundColorButton"));
    addColorField(form, &BoardColors::turnBorder, tr("手番の枠線"), QStringLiteral("turnBorderColorButton"));
    addColorField(form, &BoardColors::turnText, tr("手番の文字"), QStringLiteral("turnTextColorButton"));

    form = page(tr("対局者名"), transparentNote);
    addColorField(form, &BoardColors::nameBackground, tr("対局者名の背景"), QStringLiteral("nameBackgroundColorButton"));
    addColorField(form, &BoardColors::nameBorder, tr("対局者名の枠線"), QStringLiteral("nameBorderColorButton"));
    addColorField(form, &BoardColors::nameText, tr("対局者名の文字"), QStringLiteral("nameTextColorButton"));

    form = page(tr("持ち時間"), transparentNote);
    addColorField(form, &BoardColors::clockBackground, tr("持ち時間の背景"), QStringLiteral("clockBackgroundColorButton"));
    addColorField(form, &BoardColors::clockBorder, tr("持ち時間の枠線"), QStringLiteral("clockBorderColorButton"));
    addColorField(form, &BoardColors::clockText, tr("持ち時間の文字"), QStringLiteral("clockTextColorButton"));
    addColorField(form, &BoardColors::clockWarningText, tr("残り10秒以下の文字"), QStringLiteral("clockWarningTextColorButton"));
    addColorField(form, &BoardColors::clockCriticalText, tr("秒読み・残り5秒以下の文字"), QStringLiteral("clockCriticalTextColorButton"));
}

void BoardColorDialog::addColorField(QFormLayout* form, BoardColors::Member member,
                                    const QString& label, const QString& objectName)
{
    auto* button = new QPushButton(this);
    button->setObjectName(objectName);
    button->setAccessibleName(label);
    button->setMinimumHeight(36);
    button->setIconSize(QSize(36, 22));
    button->setAutoDefault(false);
    form->addRow(label, button);
    m_fields.append({button, member, label});
    connect(button, &QPushButton::clicked, this, &BoardColorDialog::chooseColor);
}

void BoardColorDialog::chooseColor()
{
    for (const auto& field : m_fields) {
        if (field.button != sender()) continue;
        m_selectedMember = field.member;
        m_picker->setWindowTitle(tr("%1の色").arg(field.label));
        m_picker->setOption(QColorDialog::ShowAlphaChannel, BoardColors::supportsTransparency(field.member));
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
        swatch.fill(Qt::white);
        QPainter painter(&swatch);
        for (int y = 0; y < swatch.height(); y += 6)
            for (int x = 0; x < swatch.width(); x += 6)
                if ((x / 6 + y / 6) % 2 == 0) painter.fillRect(x, y, 6, 6, QColor(210, 210, 210));
        painter.fillRect(swatch.rect(), color);
        painter.setPen(palette().color(QPalette::Mid));
        painter.drawRect(swatch.rect().adjusted(0, 0, -1, -1));
        painter.end();
        field.button->setIcon(QIcon(swatch));
        field.button->setText(color.name(color.alpha() == 255 ? QColor::HexRgb : QColor::HexArgb).toUpper());
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
    for (const auto& preset : std::as_const(m_presets)) {
        m_presetCombo->addItem(presetIcon(preset.colors), preset.name);
    }
    syncPresetSelection();
}

void BoardColorDialog::applyPreset(int index)
{
    if (index < 0 || index >= m_presets.size()) return;
    auto colors = BoardAppearance::instance().colors();
    const auto& selected = m_presets.at(index).colors;
    colors.background = selected.background;
    colors.board = selected.board;
    colors.stand = selected.stand;
    colors.grid = selected.grid;
    BoardAppearance::instance().setColors(colors);
}

void BoardColorDialog::syncPresetSelection()
{
    const auto colors = BoardAppearance::instance().colors();
    int selected = -1;
    for (qsizetype i = 0; i < m_presets.size(); ++i) {
        if (m_presets.at(i).colors.sameBoardPalette(colors)) {
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
