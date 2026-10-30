/// @file boardappearancedialog.cpp
/// @brief 対局画面の外観ダイアログの実装

#include "boardappearancedialog.h"
#include "appearancesettings.h"
#include "boardappearance.h"
#include "boardappearancepreview.h"
#include "boardappearancesamples.h"
#include "dialogfontscale.h"
#include "piecestylecatalog.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QEvent>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTabBar>
#include <QTabWidget>
#include <QVBoxLayout>
#include <optional>

namespace Samples = BoardAppearanceSamples;

BoardAppearanceDialog::BoardAppearanceDialog(QWidget* parent)
    : QDialog(parent), m_picker(new QColorDialog(this))
{
    setWindowTitle(tr("対局画面の外観"));
    m_opening = BoardAppearance::instance().state();
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
    connect(m_presetCombo, &QComboBox::activated, this, &BoardAppearanceDialog::applyPreset);
    connect(&BoardAppearance::instance(), &BoardAppearance::pieceStyleChanged,
            this, &BoardAppearanceDialog::rebuildPresets);
    m_tabs = new QTabWidget(this);
    m_tabs->setObjectName(QStringLiteral("boardColorTabs"));
    m_tabs->setTabPosition(QTabWidget::West);
    createColorPages();
    createAppearancePage();
    createWorkspace(layout);
    m_tabs->setCurrentIndex(qBound(0, AppearanceSettings::boardColorDialogTab(), m_tabs->count() - 1));
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    auto* undo = buttons->addButton(tr("開いたときの外観に戻す"), QDialogButtonBox::ResetRole);
    undo->setObjectName(QStringLiteral("restoreOpeningAppearanceButton"));
    connect(undo, &QPushButton::clicked, this, &BoardAppearanceDialog::restoreOpeningAppearance);
    auto* reset = buttons->addButton(tr("標準色に戻す"), QDialogButtonBox::ResetRole);
    reset->setObjectName(QStringLiteral("resetBoardColorsButton"));
    connect(reset, &QPushButton::clicked, this, &BoardAppearanceDialog::restoreDefaults);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    m_picker->setObjectName(QStringLiteral("boardColorPicker"));
    m_picker->setOption(QColorDialog::DontUseNativeDialog);
    m_picker->setWindowModality(Qt::WindowModal);
    connect(m_picker, &QColorDialog::colorSelected, this, &BoardAppearanceDialog::applyColor);
    connect(&BoardAppearance::instance(), &BoardAppearance::colorsChanged,
            this, &BoardAppearanceDialog::refreshButtons);
    rebuildPresets();
    refreshButtons();
    // 文字の大きさと、閉じたときに保存する大きさ（色の選択も同じ）
    DialogFontScale::Options dialogOptions;
    dialogOptions.size = &AppearanceSettings::boardColorDialogSize;
    DialogFontScale::install(this, AppearanceSettings::boardColorsDialogFontSize, dialogOptions);
    DialogFontScale::Options pickerOptions;
    pickerOptions.size = &AppearanceSettings::boardColorPickerSize;
    DialogFontScale::install(m_picker, AppearanceSettings::colorPickerDialogFontSize, pickerOptions);
}

BoardAppearanceDialog::~BoardAppearanceDialog()
{
    AppearanceSettings::boardColorDialogTab.set(m_tabs->currentIndex());
    const int section = m_sections->currentIndex();
    if (section >= 0 && section < m_sectionIds.size()) AppearanceSettings::setAppearanceSection(m_sectionIds.at(section));
    AppearanceSettings::appearancePieceFilter.set(m_pieceFilter->currentIndex());
}

void BoardAppearanceDialog::createColorPages()
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
    // 色の表（BoardColors::fields）の分類ごとに1つのタブ
    const QString transparentNote = tr("背景や枠線のアルファ値を0にすると透明になります。");
    std::optional<BoardColors::Group> group;
    QFormLayout* form = nullptr;
    for (const BoardColors::Field& field : BoardColors::fields()) {
        if (field.group != group) {
            group = field.group;
            switch (field.group) {
            case BoardColors::Group::Board: form = page(tr("盤・駒台")); break;
            case BoardColors::Group::Card:
                form = page(tr("カード全体"), tr("対局者名と持ち時間を囲むカードの色を設定します。"));
                break;
            case BoardColors::Group::Turn:
                form = page(tr("手番"), tr("「手番」バッジの色を設定します。") + QChar(0x0a) + transparentNote);
                break;
            case BoardColors::Group::Name: form = page(tr("対局者名"), transparentNote); break;
            case BoardColors::Group::Clock: form = page(tr("持ち時間"), transparentNote); break;
            }
        }
        addColorField(form, field);
    }
}

void BoardAppearanceDialog::addColorField(QFormLayout* form, const BoardColors::Field& field)
{
    const BoardColors::Member member = field.member;
    const QString label = QCoreApplication::translate("BoardAppearanceDialog", field.label);
    auto* button = new QPushButton(this);
    button->setObjectName(QString::fromLatin1(field.objectName));
    button->setAccessibleName(label);
    button->setMinimumHeight(36);
    button->setIconSize(QSize(36, 22));
    button->setAutoDefault(false);
    form->addRow(label, button);
    m_fields.append({button, member, label});
    connect(button, &QPushButton::clicked, this, &BoardAppearanceDialog::chooseColor);
}

void BoardAppearanceDialog::chooseColor()
{
    for (const auto& field : std::as_const(m_fields)) {
        if (field.button != sender()) continue;
        m_selectedMember = field.member;
        m_picker->setWindowTitle(tr("%1の色").arg(field.label));
        m_picker->setOption(QColorDialog::ShowAlphaChannel, BoardColors::supportsTransparency(field.member));
        m_picker->setCurrentColor(BoardAppearance::instance().colors().*field.member);
        m_picker->open();
        return;
    }
}

void BoardAppearanceDialog::applyColor(const QColor& color)
{
    if (!m_selectedMember || !color.isValid()) return;
    AppearanceState state = BoardAppearance::instance().state();
    state.colors.*m_selectedMember = color;
    BoardAppearance::instance().apply(state);
}

void BoardAppearanceDialog::refreshButtons()
{
    const auto colors = BoardAppearance::instance().colors();
    for (const auto& field : std::as_const(m_fields)) {
        const QColor color = colors.*field.member;
        field.button->setIcon(Samples::colorSwatch(color, palette().color(QPalette::Mid)));
        field.button->setText(color.name(color.alpha() == 255 ? QColor::HexRgb : QColor::HexArgb).toUpper());
    }
    syncPresetSelection();
}

void BoardAppearanceDialog::restoreDefaults()
{
    BoardAppearance::instance().setColors(BoardColors{});
}

void BoardAppearanceDialog::rebuildPresets()
{
    // 配色は駒のセットによらない。見本に描く駒と見出しの駒の名前だけを描き直す
    const QString style = BoardAppearance::instance().pieceStyle();
    m_presetLabel->setText(tr("%1に合うおすすめ配色").arg(PieceStyleCatalog::displayName(style)));
    const QSignalBlocker blocker(m_presetCombo);
    if (m_presetCombo->count() != m_presets.size()) {
        m_presetCombo->clear();
        for (const auto& preset : std::as_const(m_presets)) m_presetCombo->addItem(preset.name);
    }
    for (int i = 0; i < m_presetCombo->count(); ++i)
        m_presetCombo->setItemIcon(i, Samples::paletteSample(m_presets.at(i).colors, style));
    syncPresetSelection();
}

void BoardAppearanceDialog::applyPreset(int index)
{
    if (index < 0 || index >= m_presets.size()) return;
    AppearanceState state = BoardAppearance::instance().state();
    const auto& selected = m_presets.at(index).colors;
    state.colors.background = selected.background;
    state.colors.board = selected.board;
    state.colors.stand = selected.stand;
    state.colors.grid = selected.grid;
    BoardAppearance::instance().apply(state);
}

void BoardAppearanceDialog::syncPresetSelection()
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

// ============================================================
// 外観のページ（木目・影・駒の倍率）
// ============================================================

void BoardAppearanceDialog::createAppearancePage()
{
    auto* page = new QWidget(m_tabs);
    auto* layout = new QVBoxLayout(page);
    auto* description = new QLabel(tr("駒の大きさ・駒の影・木目を設定します。すべての将棋盤に反映・保存されます。"), page);
    description->setWordWrap(true);
    layout->addWidget(description);
    auto* form = new QFormLayout;
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
    layout->addStretch();
    // 既存タブの保存済み番号を維持するため末尾に追加する。
    m_tabs->addTab(page, tr("外観"));
    refreshVisuals();
    connect(m_pieceScale, &QSpinBox::valueChanged, this, &BoardAppearanceDialog::applyVisuals);
    connect(m_woodGrain, &QCheckBox::toggled, this, &BoardAppearanceDialog::applyVisuals);
    connect(m_pieceShadow, &QCheckBox::toggled, this, &BoardAppearanceDialog::applyVisuals);
    connect(m_standWoodGrain, &QCheckBox::toggled, this, &BoardAppearanceDialog::applyVisuals);
    connect(&BoardAppearance::instance(), &BoardAppearance::visualsChanged,
            this, &BoardAppearanceDialog::refreshVisuals);
}

void BoardAppearanceDialog::applyVisuals()
{
    BoardAppearance::instance().setVisuals({m_woodGrain->isChecked(), m_pieceShadow->isChecked(),
                                          m_pieceScale->value(), m_standWoodGrain->isChecked()});
}

void BoardAppearanceDialog::refreshVisuals()
{
    const auto visuals = BoardAppearance::instance().visuals();
    const QSignalBlocker blockGrain(m_woodGrain), blockShadow(m_pieceShadow), blockScale(m_pieceScale), blockStand(m_standWoodGrain);
    m_woodGrain->setChecked(visuals.woodGrain);
    m_pieceShadow->setChecked(visuals.pieceShadow);
    m_pieceScale->setValue(visuals.pieceScale);
    m_standWoodGrain->setChecked(visuals.standWoodGrain);
}

// ============================================================
// 盤と駒の組み合わせの一覧
// ============================================================

using Component = BoardAppearanceCatalog::Component;

void BoardAppearanceDialog::createWorkspace(QVBoxLayout* layout)
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
    m_pieceList = Samples::createGallery(piecePage, QStringLiteral("appearancePieces"));
    m_pieceList->viewport()->installEventFilter(this);
    m_pieceList->setItemDelegate(new Samples::LazySampleDelegate([](const QModelIndex& index) {
        return Samples::pieceSample(index.data(Qt::UserRole).toString());
    }, m_pieceList));
    pieceLayout->addWidget(m_pieceList);
    // 見本は描くときに作る。項目の大きさを揃えるため、同じ大きさの空の画像を全項目で共有する。
    const QIcon placeholder = Samples::placeholder();
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
        auto* list = Samples::createGallery(page, QString::fromLatin1(section.listName));
        list->viewport()->installEventFilter(this);
        list->setProperty("component", static_cast<int>(component));
        list->setItemDelegate(new Samples::LazySampleDelegate([component](const QModelIndex& index) {
            return Samples::componentSample(component, BoardAppearanceCatalog::samples(component).at(index.row()),
                                            BoardAppearance::instance().pieceStyle());
        }, list));
        const auto samples = BoardAppearanceCatalog::samples(component);
        for (const auto& sample : samples)
            new QListWidgetItem(placeholder, sample.name, list);
        m_componentLists.at(static_cast<std::size_t>(component)) = list;
        column->addWidget(list);
        addSection(QString::fromLatin1(section.id), page, section.title);
        connect(list, &QListWidget::currentRowChanged, this, &BoardAppearanceDialog::selectComponent);
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
    connect(m_pieceList, &QListWidget::currentRowChanged, this, &BoardAppearanceDialog::selectPiece);
    connect(m_pieceFilter, &QComboBox::currentIndexChanged, this, &BoardAppearanceDialog::filterPieces);
    connect(m_combinations, &QComboBox::activated, this, &BoardAppearanceDialog::applyCombination);
    connect(&BoardAppearance::instance(), &BoardAppearance::colorsChanged, this, &BoardAppearanceDialog::syncCatalog);
    connect(&BoardAppearance::instance(), &BoardAppearance::visualsChanged, this, &BoardAppearanceDialog::syncCatalog);
    connect(&BoardAppearance::instance(), &BoardAppearance::pieceStyleChanged,
            this, &BoardAppearanceDialog::refreshCatalogIcons);
    m_sections->setCurrentIndex(qMax(0, static_cast<int>(m_sectionIds.indexOf(AppearanceSettings::appearanceSection()))));
    m_pieceFilter->setCurrentIndex(AppearanceSettings::appearancePieceFilter());
    syncCatalog();
}

void BoardAppearanceDialog::selectPiece(int row)
{
    if (row < 0) return;
    BoardAppearance::instance().setPieceStyle(m_pieceList->item(row)->data(Qt::UserRole).toString());
}

void BoardAppearanceDialog::selectComponent(int row)
{
    if (row < 0) return;
    auto* list = qobject_cast<QListWidget*>(sender());
    if (!list) return;
    const auto component = static_cast<Component>(list->property("component").toInt());
    const auto samples = BoardAppearanceCatalog::samples(component);
    if (row >= samples.size()) return;
    AppearanceState state = BoardAppearance::instance().state();
    BoardAppearanceCatalog::apply(component, samples.at(row), state.colors, state.visuals);
    BoardAppearance::instance().apply(state);
}

void BoardAppearanceDialog::filterPieces(int index)
{
    // index は PieceStyleCatalog::Family の値（0 は「すべて」）
    if (index < 0 || index >= static_cast<int>(PieceStyleCatalog::Family::Count)) return;
    for (int row = 0; row < m_pieceList->count(); ++row) {
        auto* item = m_pieceList->item(row);
        const auto family = PieceStyleCatalog::family(item->data(Qt::UserRole).toString());
        item->setHidden(index != 0 && static_cast<int>(family) != index);
    }
}

void BoardAppearanceDialog::refreshCatalogIcons()
{
    // 盤・駒台の見本には選択中の駒を描くので、駒のセットが変わったら描き直す。
    for (auto* list : std::as_const(m_componentLists)) {
        static_cast<Samples::LazySampleDelegate*>(list->itemDelegate())->clear();
        list->viewport()->update();
    }
    syncCatalog();
}

void BoardAppearanceDialog::syncCatalog()
{
    const AppearanceState state = BoardAppearance::instance().state();
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

void BoardAppearanceDialog::applyCombination(int index)
{
    const auto& combinations = BoardAppearanceCatalog::combinations();
    if (index < 0 || index >= combinations.size()) return;
    auto& appearance = BoardAppearance::instance();
    appearance.apply(BoardAppearanceCatalog::applied(combinations.at(index), appearance.state()));
}

void BoardAppearanceDialog::restoreOpeningAppearance()
{
    BoardAppearance::instance().apply(m_opening);
}

void BoardAppearanceDialog::addSection(const QString& id, QWidget* page, const QString& title)
{
    m_sections->addTab(page, title);
    m_sectionIds.append(id);
}

bool BoardAppearanceDialog::eventFilter(QObject* watched, QEvent* event)
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
