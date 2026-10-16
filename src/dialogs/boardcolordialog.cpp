#include "boardcolordialog.h"
#include "appsettings.h"
#include "appearancestate.h"
#include "piecestylecatalog.h"
#include "boardappearance.h"
#include "dialogutils.h"
#include "dialogfontscale.h"
#include "pieceimageprovider.h"
#include "boardsurfacepainter.h"
#include "piecepainter.h"

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
#include <QCheckBox>
#include <QSpinBox>
#include "boardappearancecatalog.h"
#include "boardappearancepreview.h"
#include <QApplication>
#include <QEvent>
#include <QHBoxLayout>
#include <QHash>
#include <QListWidget>
#include <QScrollArea>
#include <QStyledItemDelegate>
#include <QTabBar>
#include <functional>

BoardColorDialog::BoardColorDialog(QWidget* parent)
    : QDialog(parent), m_picker(new QColorDialog(this))
{
    setWindowTitle(tr("対局画面の外観"));
    m_opening = AppearanceState::current();
    m_presets = BoardColorPresets::palettes();
    setMinimumSize(960, 640);
    auto* layout = new QVBoxLayout(this);
    auto* description = new QLabel(tr("駒・将棋盤・背景・駒台・対局者情報を自由に組み合わせられます。選択はすべての盤面に反映・保存されます。"), this);
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
    m_tabs->setTabPosition(QTabWidget::West);
    createColorPages();
    createAppearancePage();
    createWorkspace(layout);
    m_tabs->setCurrentIndex(qBound(0, AppSettings::boardColorDialogTab(), m_tabs->count() - 1));
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    auto* undo = buttons->addButton(tr("開いたときの外観に戻す"), QDialogButtonBox::ResetRole);
    undo->setObjectName(QStringLiteral("restoreOpeningAppearanceButton"));
    connect(undo, &QPushButton::clicked, this, &BoardColorDialog::restoreOpeningAppearance);
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
    DialogFontScale::install(this, AppSettings::boardColorsDialogFontSize);
    DialogFontScale::install(m_picker, AppSettings::colorPickerDialogFontSize);
}

BoardColorDialog::~BoardColorDialog()
{
    DialogUtils::saveDialogSize(this, AppSettings::boardColorDialogSize);
    AppSettings::boardColorDialogTab.set(m_tabs->currentIndex());
    const int section = m_sections->currentIndex();
    if (section >= 0 && section < m_sectionIds.size()) AppSettings::setAppearanceSection(m_sectionIds.at(section));
    AppSettings::setAppearancePieceFilter(m_pieceFilter->currentIndex());
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
    for (const auto& field : std::as_const(m_fields)) {
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
    AppearanceState state = AppearanceState::current();
    state.colors.*m_selectedMember = color;
    state.apply();
}

void BoardColorDialog::refreshButtons()
{
    const auto colors = BoardAppearance::instance().colors();
    for (const auto& field : std::as_const(m_fields)) {
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
    syncThemeSelection();
}

void BoardColorDialog::restoreDefaults()
{
    BoardAppearance::instance().setColors(BoardColors{});
}

void BoardColorDialog::savePickerSize()
{
    DialogUtils::saveDialogSize(m_picker, AppSettings::boardColorPickerSize);
}

void BoardColorDialog::rebuildPresets()
{
    // 配色は駒のセットによらない。見本に描く駒と見出しの駒の名前だけを描き直す
    const QString style = PieceImageProvider::instance().style();
    m_presetLabel->setText(tr("%1に合うおすすめ配色").arg(PieceStyleCatalog::displayName(style)));
    const QSignalBlocker blocker(m_presetCombo);
    if (m_presetCombo->count() != m_presets.size()) {
        m_presetCombo->clear();
        for (const auto& preset : std::as_const(m_presets)) m_presetCombo->addItem(preset.name);
    }
    for (int i = 0; i < m_presetCombo->count(); ++i)
        m_presetCombo->setItemIcon(i, presetIcon(m_presets.at(i).colors));
    syncPresetSelection();
    syncThemeSelection();
}

void BoardColorDialog::applyPreset(int index)
{
    if (index < 0 || index >= m_presets.size()) return;
    AppearanceState state = AppearanceState::current();
    const auto& selected = m_presets.at(index).colors;
    state.colors.background = selected.background;
    state.colors.board = selected.board;
    state.colors.stand = selected.stand;
    state.colors.grid = selected.grid;
    state.apply();
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

QIcon BoardColorDialog::presetIcon(const BoardColors& colors, const QString& style,
                                  const BoardVisuals& visuals) const
{
    // 高DPIでも輪郭が崩れないよう2倍で描画する。
    QPixmap preview(240, 144);
    preview.setDevicePixelRatio(2);
    preview.fill(colors.background);
    QPainter painter(&preview);
    BoardSurfacePainter::draw(painter, QRect(4, 5, 22, 29), colors.stand, visuals.standWoodGrain, 20);
    BoardSurfacePainter::draw(painter, QRect(94, 38, 22, 29), colors.stand, visuals.standWoodGrain, 20);
    BoardSurfacePainter::draw(painter, QRect(28, 1, 64, 70), colors.board, visuals.woodGrain, 20);
    painter.setBrush(Qt::NoBrush);
    painter.setPen(colors.grid);
    for (int row = 0; row < 3; ++row)
        for (int col = 0; col < 3; ++col)
            painter.drawRect(QRect(30 + col * 20, 3 + row * 22, 20, 22));
    auto& pieces = PieceImageProvider::instance();
    const QString selected = style.isEmpty() ? pieces.style() : style;
    PiecePainter::draw(painter, pieces.iconForStyle(Piece::WhiteDragon, selected), QRect(50, 3, 20, 22), visuals);
    PiecePainter::draw(painter, pieces.iconForStyle(Piece::BlackPawn, selected), QRect(70, 25, 20, 22), visuals);
    PiecePainter::draw(painter, pieces.iconForStyle(Piece::BlackKing, selected), QRect(50, 47, 20, 22), visuals);
    painter.end();
    return QIcon(preview);
}

// ============================================================
// 外観のページ（木目・影・駒の倍率）
// ============================================================

void BoardColorDialog::createAppearancePage()
{
    auto* page = new QWidget(m_tabs);
    auto* layout = new QVBoxLayout(page);
    auto* description = new QLabel(tr("盤・駒台・駒・対局者情報をまとめて整える外観です。選択するとすべての将棋盤に反映・保存されます。"), page);
    description->setWordWrap(true);
    layout->addWidget(description);
    auto* form = new QFormLayout;
    m_themeCombo = new QComboBox(page);
    m_themeCombo->setObjectName(QStringLiteral("boardThemeCombo"));
    m_themeCombo->setPlaceholderText(tr("カスタム（個別に指定）"));
    m_themeCombo->setIconSize(QSize(120, 72));
    m_themes = BoardColorPresets::themes();
    for (const auto& theme : std::as_const(m_themes))
        m_themeCombo->addItem(presetIcon(theme.colors, PieceStyleCatalog::standardId()), theme.name, theme.id);
    form->addRow(tr("外観"), m_themeCombo);
    m_pieceScale = new QSpinBox(page);
    m_pieceScale->setObjectName(QStringLiteral("boardPieceScale"));
    m_pieceScale->setRange(90, 112);
    m_pieceScale->setSuffix(QStringLiteral(" %"));
    form->addRow(tr("駒の大きさ"), m_pieceScale);
    layout->addLayout(form);
    m_woodGrain = new QCheckBox(tr("将棋盤の木目を表示する"), page);
    m_woodGrain->setObjectName(QStringLiteral("boardWoodGrain"));
    m_pieceShadow = new QCheckBox(tr("駒の影を表示する"), page);
    m_pieceShadow->setObjectName(QStringLiteral("boardPieceShadow"));
    layout->addWidget(m_woodGrain);
    layout->addWidget(m_pieceShadow);
    m_standWoodGrain = new QCheckBox(tr("駒台の木目を表示する"), page);
    m_standWoodGrain->setObjectName(QStringLiteral("standWoodGrain"));
    layout->addWidget(m_standWoodGrain);
    auto* note = new QLabel(tr("外観を選ぶと「標準の駒」と標準の大きさ・影に切り替わります。各部品はこのウィンドウの見本から個別に選べます。"), page);
    note->setWordWrap(true);
    layout->addWidget(note);
    layout->addStretch();
    // 既存タブの保存済み番号を維持するため末尾に追加する。
    m_tabs->addTab(page, tr("外観"));
    refreshVisuals();
    connect(m_themeCombo, &QComboBox::activated, this, &BoardColorDialog::applyTheme);
    connect(m_pieceScale, &QSpinBox::valueChanged, this, &BoardColorDialog::applyVisuals);
    connect(m_woodGrain, &QCheckBox::toggled, this, &BoardColorDialog::applyVisuals);
    connect(m_pieceShadow, &QCheckBox::toggled, this, &BoardColorDialog::applyVisuals);
    connect(m_standWoodGrain, &QCheckBox::toggled, this, &BoardColorDialog::applyVisuals);
    connect(&BoardAppearance::instance(), &BoardAppearance::visualsChanged,
            this, &BoardColorDialog::refreshVisuals);
}

void BoardColorDialog::applyTheme(int index)
{
    if (index < 0 || index >= m_themes.size()) return;
    AppearanceState state;
    state.colors = m_themes.at(index).colors;
    state.apply();
    syncThemeSelection();
}

void BoardColorDialog::applyVisuals()
{
    BoardAppearance::instance().setVisuals({m_woodGrain->isChecked(), m_pieceShadow->isChecked(),
                                          m_pieceScale->value(), m_standWoodGrain->isChecked()});
}

void BoardColorDialog::refreshVisuals()
{
    const auto visuals = BoardAppearance::instance().visuals();
    const QSignalBlocker blockGrain(m_woodGrain), blockShadow(m_pieceShadow), blockScale(m_pieceScale), blockStand(m_standWoodGrain);
    m_woodGrain->setChecked(visuals.woodGrain);
    m_pieceShadow->setChecked(visuals.pieceShadow);
    m_pieceScale->setValue(visuals.pieceScale);
    m_standWoodGrain->setChecked(visuals.standWoodGrain);
    syncThemeSelection();
}

void BoardColorDialog::syncThemeSelection()
{
    if (!m_themeCombo) return;
    // 外観（テーマ）は標準の駒・標準の見た目と、テーマの配色の組
    int selected = -1;
    const AppearanceState current = AppearanceState::current();
    for (qsizetype i = 0; i < m_themes.size(); ++i) {
        AppearanceState theme;
        theme.colors = m_themes.at(i).colors;
        if (theme == current) selected = static_cast<int>(i);
    }
    const QSignalBlocker blocker(m_themeCombo);
    m_themeCombo->setCurrentIndex(selected);
}

// ============================================================
// 盤と駒の組み合わせの一覧
// ============================================================

namespace {
using Component = BoardAppearanceCatalog::Component;
QListWidget* gallery(QWidget* parent, const QString& name)
{
    auto* list = new QListWidget(parent);
    list->setObjectName(name);
    list->setViewMode(QListView::IconMode);
    list->setResizeMode(QListView::Adjust);
    list->setMovement(QListView::Static);
    list->setSelectionMode(QAbstractItemView::SingleSelection);
    list->setIconSize(QSize(142, 86));
    list->setGridSize(QSize(164, 120));
    list->setSpacing(4);
    list->setWordWrap(true);
    list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    list->setStyleSheet(QStringLiteral(
        "QListWidget { border: 0; background: palette(base); }"
        "QListWidget::item { border: 1px solid palette(midlight); border-radius: 6px; padding: 5px; }"
        "QListWidget::item:selected { border: 2px solid palette(highlight); background: palette(alternate-base); color: palette(text); }"
        "QListWidget::item:hover { border-color: palette(highlight); }"));
    return list;
}

QIcon sampleIconWithoutTint(const QPixmap& image)
{
    QIcon icon(image);
    // 選択中にも Qt の選択色で見本の駒や配色を着色しない。
    icon.addPixmap(image, QIcon::Selected);
    icon.addPixmap(image, QIcon::Active);
    return icon;
}

QPixmap blankSample()
{
    QPixmap image(284, 172);
    image.setDevicePixelRatio(2);
    image.fill(Qt::transparent);
    return image;
}

// 見本は、一覧で画面に見えた項目だけを描く。開くたびに全項目を描くと、駒のセットや
// 盤・駒台などの見本を増やすほどウィンドウを開くのが遅くなり、メモリも増えるため。
class LazySampleDelegate : public QStyledItemDelegate
{
public:
    using Render = std::function<QIcon(const QModelIndex&)>;

    LazySampleDelegate(Render render, QObject* parent)
        : QStyledItemDelegate(parent), m_render(std::move(render))
    {
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);
        const auto it = m_samples.constFind(index.row());
        if (it != m_samples.cend()) {
            opt.icon = *it;
        } else {
            opt.icon = m_render(index);
            m_samples.insert(index.row(), opt.icon);
        }
        const QWidget* widget = opt.widget;
        QStyle* style = widget ? widget->style() : QApplication::style();
        style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, widget);
    }

    // 描いた見本を捨てて、次に見えたときに描き直させる。
    void clear() { m_samples.clear(); }

private:
    Render m_render;
    mutable QHash<int, QIcon> m_samples;
};

QIcon pieceSampleIcon(const QString& style)
{
    QPixmap image = blankSample();
    QPainter painter(&image);
    static constexpr Piece glyphs[] = {Piece::BlackKing, Piece::BlackPawn, Piece::BlackDragon};
    for (int i = 0; i < 3; ++i)
        PiecePainter::draw(painter, PieceImageProvider::instance().iconForStyle(glyphs[i], style),
                           QRect(4 + i * 46, 16, 43, 48), {});
    painter.end();
    return sampleIconWithoutTint(image);
}

QIcon sampleIcon(Component component, const BoardAppearanceSample& sample, const QString& style)
{
    QPixmap image = blankSample();
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    const auto& c = sample.colors;
    auto& pieces = PieceImageProvider::instance();
    if (component == Component::Information) {
        painter.setPen(QPen(c.activeCardBorder, 2));
        painter.setBrush(c.cardBackground);
        painter.drawRoundedRect(QRectF(6, 3, 130, 78), 5, 5);
        painter.setPen(Qt::NoPen);
        painter.setBrush(c.turnBackground);
        painter.drawRoundedRect(QRectF(14, 10, 36, 18), 3, 3);
        QFont font = painter.font();
        font.setPixelSize(11);
        font.setBold(true);
        painter.setFont(font);
        painter.setPen(c.turnText);
        painter.drawText(QRect(14, 10, 36, 18), Qt::AlignCenter, BoardColorDialog::tr("手番"));
        font.setPixelSize(14);
        font.setBold(false);
        painter.setFont(font);
        painter.setPen(c.nameText);
        painter.drawText(QRect(14, 32, 114, 18), Qt::AlignLeft, BoardColorDialog::tr("▲先手"));
        font.setPixelSize(22);
        font.setBold(true);
        painter.setFont(font);
        painter.setPen(c.clockText);
        painter.drawText(QRect(14, 52, 114, 27), Qt::AlignLeft, QStringLiteral("09:58"));
    } else if (component == Component::Stand) {
        BoardSurfacePainter::draw(painter, QRectF(14, 3, 112, 76), c.stand, sample.woodGrain, 30);
        PiecePainter::draw(painter, pieces.iconForStyle(Piece::BlackBishop, style), QRect(23, 19, 42, 46), {});
        PiecePainter::draw(painter, pieces.iconForStyle(Piece::BlackPawn, style), QRect(73, 22, 39, 43), {});
    } else if (component == Component::Background) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(c.background);
        painter.drawRoundedRect(QRectF(6, 3, 130, 78), 5, 5);
    } else {
        BoardSurfacePainter::draw(painter, QRectF(16, 3, 110, 77), c.board, sample.woodGrain, 25);
        painter.setPen(QPen(c.grid, .6));
        for (int i = 0; i <= 3; ++i) {
            painter.drawLine(QPointF(22 + i * 32, 8), QPointF(22 + i * 32, 74));
            painter.drawLine(QPointF(22, 8 + i * 22), QPointF(118, 8 + i * 22));
        }
        PiecePainter::draw(painter, pieces.iconForStyle(Piece::WhitePawn, style), QRect(54, 8, 24, 25), {});
        PiecePainter::draw(painter, pieces.iconForStyle(Piece::BlackKing, style), QRect(84, 48, 29, 29), {});
    }
    painter.end();
    return sampleIconWithoutTint(image);
}
}

void BoardColorDialog::createWorkspace(QVBoxLayout* layout)
{
    auto* content = new QHBoxLayout;
    m_sections = new QTabWidget(this);
    m_sections->setObjectName(QStringLiteral("appearanceSections"));
    m_sections->setMinimumWidth(382);
    m_sections->setMaximumWidth(490);
    auto* piecePage = new QWidget(m_sections);
    auto* pieceLayout = new QVBoxLayout(piecePage);
    m_pieceFilter = new QComboBox(piecePage);
    m_pieceFilter->setObjectName(QStringLiteral("appearancePieceFilter"));
    // 項目の位置は PieceStyleCatalog::Family の値（0 は「すべて」）
    m_pieceFilter->addItem(tr("すべての駒（%1種類）").arg(PieceStyleCatalog::entries().size()));
    for (int family = 1; family < static_cast<int>(PieceStyleCatalog::Family::Count); ++family)
        m_pieceFilter->addItem(PieceStyleCatalog::familyName(static_cast<PieceStyleCatalog::Family>(family)));
    pieceLayout->addWidget(m_pieceFilter);
    m_pieceList = gallery(piecePage, QStringLiteral("appearancePieces"));
    m_pieceList->viewport()->installEventFilter(this);
    m_pieceList->setItemDelegate(new LazySampleDelegate([](const QModelIndex& index) {
        return pieceSampleIcon(index.data(Qt::UserRole).toString());
    }, m_pieceList));
    pieceLayout->addWidget(m_pieceList);
    // 見本は描くときに作る。項目の大きさを揃えるため、同じ大きさの空の画像を全項目で共有する。
    const QIcon placeholder = sampleIconWithoutTint(blankSample());
    for (const auto& entry : PieceStyleCatalog::entries()) {
        const QString style = QString::fromLatin1(entry.id);
        auto* item = new QListWidgetItem(placeholder, PieceStyleCatalog::displayName(style), m_pieceList);
        item->setData(Qt::UserRole, style);
        item->setToolTip(item->text());
    }
    addSection(QStringLiteral("pieces"), piecePage, tr("駒"));
    // 部品の見本のタブ（タブの並び。id は開いていたタブの保存に使う）
    struct ComponentSection {
        const char* id;
        Component component;
        QString title;
        QString note;
        const char* listName;
    };
    const auto count = [](Component component) { return BoardAppearanceCatalog::samples(component).size(); };
    const ComponentSection componentSections[] = {
        {"board", Component::Board, tr("将棋盤"), tr("%1の見本（%2種類）").arg(tr("将棋盤")).arg(count(Component::Board)),
         "appearanceBoards"},
        {"background", Component::Background, tr("背景"),
         tr("%1の見本（%2種類）").arg(tr("背景")).arg(count(Component::Background)), "appearanceBackgrounds"},
        {"stand", Component::Stand, tr("駒台"), tr("%1の見本（%2種類）").arg(tr("駒台")).arg(count(Component::Stand)),
         "appearanceStands"},
        {"information", Component::Information, tr("対局者情報"),
         tr("手番・対局者名・持ち時間の見本（%1種類）").arg(count(Component::Information)), "appearanceInformation"},
    };
    for (const auto& section : componentSections) {
        const Component component = section.component;
        auto* page = new QWidget(m_sections);
        auto* column = new QVBoxLayout(page);
        auto* note = new QLabel(section.note, page);
        note->setWordWrap(true);
        column->addWidget(note);
        auto* list = gallery(page, QString::fromLatin1(section.listName));
        list->viewport()->installEventFilter(this);
        list->setProperty("component", static_cast<int>(component));
        list->setItemDelegate(new LazySampleDelegate([component](const QModelIndex& index) {
            return sampleIcon(component, BoardAppearanceCatalog::samples(component).at(index.row()),
                              PieceImageProvider::instance().style());
        }, list));
        const auto samples = BoardAppearanceCatalog::samples(component);
        for (const auto& sample : samples)
            new QListWidgetItem(placeholder, sample.name, list);
        m_componentLists.at(static_cast<std::size_t>(component)) = list;
        column->addWidget(list);
        addSection(QString::fromLatin1(section.id), page, section.title);
        connect(list, &QListWidget::currentRowChanged, this, &BoardColorDialog::selectComponent);
    }
    auto* details = new QScrollArea(m_sections);
    details->setWidgetResizable(true);
    details->setFrameShape(QFrame::NoFrame);
    details->setWidget(m_tabs);
    addSection(QStringLiteral("details"), details, tr("詳細"));
    // 英語など見出しが長い言語でも、既定の文字サイズでは6つのタブを切らずに並べる
    // （文字を大きくしたときは、従来どおりスクロールボタンで切り替える）
    const int tabsWidth = m_sections->tabBar()->sizeHint().width() + 4;
    m_sections->setMinimumWidth(qMax(m_sections->minimumWidth(), tabsWidth));
    m_sections->setMaximumWidth(qMax(m_sections->maximumWidth(), tabsWidth));
    content->addWidget(m_sections, 2);

    auto* right = new QVBoxLayout;
    auto* recommendations = new QHBoxLayout;
    recommendations->addWidget(new QLabel(tr("おすすめの組み合わせ"), this));
    m_combinations = new QComboBox(this);
    m_combinations->setObjectName(QStringLiteral("appearanceCombination"));
    m_combinations->setPlaceholderText(tr("自由な組み合わせ"));
    for (const auto& combination : BoardAppearanceCatalog::combinations())
        m_combinations->addItem(BoardAppearanceCatalog::combinationName(combination));
    recommendations->addWidget(m_combinations, 1);
    right->addLayout(recommendations);
    auto* toolbar = new QHBoxLayout;
    toolbar->addWidget(new QLabel(tr("プレビュー"), this));
    toolbar->addStretch();
    auto* position = new QComboBox(this);
    position->setObjectName(QStringLiteral("appearancePreviewPosition"));
    position->addItems({tr("初期局面"), tr("成駒・持駒の見本")});
    toolbar->addWidget(position);
    auto* flip = new QPushButton(tr("盤面反転"), this);
    flip->setObjectName(QStringLiteral("appearancePreviewFlip"));
    flip->setCheckable(true);
    flip->setAutoDefault(false);
    toolbar->addWidget(flip);
    right->addLayout(toolbar);
    m_preview = new BoardAppearancePreview(this);
    m_preview->setProperty("dialogFontScaleExcluded", true);
    right->addWidget(m_preview, 1);
    m_selectionSummary = new QLabel(this);
    m_selectionSummary->setObjectName(QStringLiteral("appearanceSelectionSummary"));
    m_selectionSummary->setWordWrap(true);
    m_selectionSummary->setMinimumHeight(64);
    right->addWidget(m_selectionSummary);
    content->addLayout(right, 3);
    layout->addLayout(content, 1);

    connect(position, &QComboBox::currentIndexChanged, m_preview, &BoardAppearancePreview::setPosition);
    connect(flip, &QPushButton::toggled, m_preview, &BoardAppearancePreview::setFlipped);
    connect(m_pieceList, &QListWidget::currentRowChanged, this, &BoardColorDialog::selectPiece);
    connect(m_pieceFilter, &QComboBox::currentIndexChanged, this, &BoardColorDialog::filterPieces);
    connect(m_combinations, &QComboBox::activated, this, &BoardColorDialog::applyCombination);
    connect(&BoardAppearance::instance(), &BoardAppearance::colorsChanged, this, &BoardColorDialog::syncCatalog);
    connect(&BoardAppearance::instance(), &BoardAppearance::visualsChanged, this, &BoardColorDialog::syncCatalog);
    connect(&PieceImageProvider::instance(), &PieceImageProvider::styleChanged,
            this, &BoardColorDialog::refreshCatalogIcons);
    m_sections->setCurrentIndex(qMax(0, static_cast<int>(m_sectionIds.indexOf(AppSettings::appearanceSection()))));
    m_pieceFilter->setCurrentIndex(AppSettings::appearancePieceFilter());
    syncCatalog();
}

void BoardColorDialog::selectPiece(int row)
{
    if (row < 0) return;
    PieceImageProvider::instance().setStyle(m_pieceList->item(row)->data(Qt::UserRole).toString());
}

void BoardColorDialog::selectComponent(int row)
{
    if (row < 0) return;
    auto* list = qobject_cast<QListWidget*>(sender());
    if (!list) return;
    const auto component = static_cast<Component>(list->property("component").toInt());
    const auto samples = BoardAppearanceCatalog::samples(component);
    if (row >= samples.size()) return;
    AppearanceState state = AppearanceState::current();
    BoardAppearanceCatalog::apply(component, samples.at(row), state.colors, state.visuals);
    state.apply();
}

void BoardColorDialog::filterPieces(int index)
{
    // index は PieceStyleCatalog::Family の値（0 は「すべて」）
    if (index < 0 || index >= static_cast<int>(PieceStyleCatalog::Family::Count)) return;
    for (int row = 0; row < m_pieceList->count(); ++row) {
        auto* item = m_pieceList->item(row);
        const auto family = PieceStyleCatalog::family(item->data(Qt::UserRole).toString());
        item->setHidden(index != 0 && static_cast<int>(family) != index);
    }
}

void BoardColorDialog::refreshCatalogIcons()
{
    // 盤・駒台の見本には選択中の駒を描くので、駒のセットが変わったら描き直す。
    for (auto* list : std::as_const(m_componentLists)) {
        static_cast<LazySampleDelegate*>(list->itemDelegate())->clear();
        list->viewport()->update();
    }
    syncCatalog();
}

void BoardColorDialog::syncCatalog()
{
    const AppearanceState state = AppearanceState::current();
    const QSignalBlocker blockPieces(m_pieceList), blockCombinations(m_combinations);
    m_pieceList->setCurrentRow(static_cast<int>(PieceStyleCatalog::ids().indexOf(state.pieceStyle)));
    QString names[BoardAppearanceCatalog::kComponentCount];
    for (int i = 0; i < BoardAppearanceCatalog::kComponentCount; ++i) {
        const auto component = static_cast<Component>(i);
        QListWidget* list = m_componentLists.at(static_cast<std::size_t>(i));
        const QSignalBlocker blocker(list);
        const int selected = BoardAppearanceCatalog::selectedIndex(component, state);
        list->setCurrentRow(selected);
        names[i] = selected < 0 ? tr("カスタム") : BoardAppearanceCatalog::samples(component).at(selected).name;
    }
    m_selectionSummary->setText(tr("駒：%1\n将棋盤：%2　／　背景：%5\n駒台：%3　／　対局者情報：%4")
                                  .arg(PieceStyleCatalog::displayName(state.pieceStyle),
                                       names[static_cast<int>(Component::Board)],
                                       names[static_cast<int>(Component::Stand)],
                                       names[static_cast<int>(Component::Information)],
                                       names[static_cast<int>(Component::Background)]));
    m_combinations->setCurrentIndex(BoardAppearanceCatalog::combinationIndex(state));
}

void BoardColorDialog::applyCombination(int index)
{
    const auto& combinations = BoardAppearanceCatalog::combinations();
    if (index < 0 || index >= combinations.size()) return;
    BoardAppearanceCatalog::applied(combinations.at(index), AppearanceState::current()).apply();
}

void BoardColorDialog::restoreOpeningAppearance()
{
    m_opening.apply();
}

void BoardColorDialog::addSection(const QString& id, QWidget* page, const QString& title)
{
    m_sections->addTab(page, title);
    m_sectionIds.append(id);
}

bool BoardColorDialog::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::Resize || event->type() == QEvent::FontChange) {
        auto* viewport = qobject_cast<QWidget*>(watched);
        auto* list = viewport ? qobject_cast<QListWidget*>(viewport->parentWidget()) : nullptr;
        if (list) {
            const int cellWidth = qMax(164, list->fontMetrics().horizontalAdvance(QStringLiteral("対局者情報")) + 32);
            const int columns = qMax(1, viewport->width() / cellWidth);
            const int cellHeight = list->iconSize().height() + list->fontMetrics().lineSpacing() * 2 + 24;
            list->setGridSize(QSize(viewport->width() / columns - 2 * list->spacing(), cellHeight));
        }
    }
    return QDialog::eventFilter(watched, event);
}
