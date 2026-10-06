#include "boardcolordialog.h"
#include "appsettings.h"
#include "boardappearance.h"
#include "boardappearancecatalog.h"
#include "boardappearancepreview.h"
#include "boardsurfacepainter.h"
#include "pieceimageprovider.h"
#include "piecepainter.h"

#include <QComboBox>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QTabBar>
#include <QTabWidget>
#include <QVBoxLayout>
#include <iterator>

namespace {
using Component = BoardAppearanceCatalog::Component;
struct Combination {
    const char* name;
    const char* pieces;
    int board;
    int stand;
    int information;
    int background;
};
const Combination combinations[] = {
    {QT_TRANSLATE_NOOP("BoardColorDialog", "標準・榧と畳"), "standard", 0, 0, 0, 0},
    {QT_TRANSLATE_NOOP("BoardColorDialog", "淡虎斑・白榧"), "torafu_light", 1, 1, 1, 1},
    {QT_TRANSLATE_NOOP("BoardColorDialog", "黒檀・月白"), "deep_ebony", 8, 8, 16, 8},
    {QT_TRANSLATE_NOOP("BoardColorDialog", "白木・墨夜"), "wood_pale", 11, 11, 10, 11},
    {QT_TRANSLATE_NOOP("BoardColorDialog", "青磁・白磁"), "tint_celadon", 6, 6, 2, 6},
    {QT_TRANSLATE_NOOP("BoardColorDialog", "葡萄・藤鼠"), "deep_grape", 17, 17, 4, 17},
    {QT_TRANSLATE_NOOP("BoardColorDialog", "Facet（木肌）"), "chess_facet_wood", 20, 20, 1, 20},
    {QT_TRANSLATE_NOOP("BoardColorDialog", "Facet（白）"), "chess_facet_paper", 21, 21, 2, 21},
    {QT_TRANSLATE_NOOP("BoardColorDialog", "Facet（墨）"), "chess_facet_slate", 22, 22, 10, 22},
    {QT_TRANSLATE_NOOP("BoardColorDialog", "Atelier（木肌）"), "chess_atelier_wood", 20, 20, 1, 20},
    {QT_TRANSLATE_NOOP("BoardColorDialog", "Atelier（白）"), "chess_atelier_paper", 21, 21, 2, 21},
    {QT_TRANSLATE_NOOP("BoardColorDialog", "Atelier（墨）"), "chess_atelier_slate", 22, 22, 10, 22},
    {QT_TRANSLATE_NOOP("BoardColorDialog", "Ribbon（木肌）"), "chess_ribbon_wood", 20, 20, 1, 20},
    {QT_TRANSLATE_NOOP("BoardColorDialog", "Ribbon（白）"), "chess_ribbon_paper", 21, 21, 2, 21},
    {QT_TRANSLATE_NOOP("BoardColorDialog", "Ribbon（墨）"), "chess_ribbon_slate", 22, 22, 10, 22},
    {QT_TRANSLATE_NOOP("BoardColorDialog", "端正（木肌）"), "alphabet_sei_wood", 23, 23, 1, 23},
    {QT_TRANSLATE_NOOP("BoardColorDialog", "端正（白）"), "alphabet_sei_paper", 24, 24, 2, 24},
    {QT_TRANSLATE_NOOP("BoardColorDialog", "端正（墨）"), "alphabet_sei_slate", 25, 25, 10, 25},
    {QT_TRANSLATE_NOOP("BoardColorDialog", "凛（木肌）"), "alphabet_rin_wood", 23, 23, 1, 23},
    {QT_TRANSLATE_NOOP("BoardColorDialog", "凛（白）"), "alphabet_rin_paper", 24, 24, 2, 24},
    {QT_TRANSLATE_NOOP("BoardColorDialog", "凛（墨）"), "alphabet_rin_slate", 25, 25, 10, 25},
    {QT_TRANSLATE_NOOP("BoardColorDialog", "墨（木肌）"), "alphabet_sumi_wood", 23, 23, 1, 23},
    {QT_TRANSLATE_NOOP("BoardColorDialog", "墨（白）"), "alphabet_sumi_paper", 24, 24, 2, 24},
    {QT_TRANSLATE_NOOP("BoardColorDialog", "墨（墨）"), "alphabet_sumi_slate", 25, 25, 10, 25}
};

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

QIcon sampleIcon(Component component, const BoardAppearanceSample& sample, const QString& style)
{
    QPixmap image(284, 172);
    image.setDevicePixelRatio(2);
    image.fill(Qt::transparent);
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
        PiecePainter::draw(painter, pieces.iconForStyle('B', style), QRect(23, 19, 42, 46), {});
        PiecePainter::draw(painter, pieces.iconForStyle('P', style), QRect(73, 22, 39, 43), {});
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
        PiecePainter::draw(painter, pieces.iconForStyle('p', style), QRect(54, 8, 24, 25), {});
        PiecePainter::draw(painter, pieces.iconForStyle('K', style), QRect(84, 48, 29, 29), {});
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
    m_pieceFilter->addItems({tr("すべての駒（%1種類）").arg(AppSettings::availablePieceStyles().size()),
                            tr("虎斑"), tr("木肌"), tr("淡色"), tr("深色"), tr("意匠"), tr("チェス"), tr("アルファベット")});
    pieceLayout->addWidget(m_pieceFilter);
    m_pieceList = gallery(piecePage, QStringLiteral("appearancePieces"));
    m_pieceList->viewport()->installEventFilter(this);
    pieceLayout->addWidget(m_pieceList);
    for (const auto& style : AppSettings::availablePieceStyles()) {
        QPixmap image(284, 172);
        image.setDevicePixelRatio(2);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        const QString glyphs = QStringLiteral("KPU");
        for (int i = 0; i < glyphs.size(); ++i)
            PiecePainter::draw(painter, PieceImageProvider::instance().iconForStyle(glyphs.at(i), style),
                               QRect(4 + i * 46, 16, 43, 48), {});
        painter.end();
        auto* item = new QListWidgetItem(sampleIconWithoutTint(image), BoardColorPresets::pieceStyleName(style), m_pieceList);
        item->setData(Qt::UserRole, style);
        item->setToolTip(item->text());
    }
    m_sections->addTab(piecePage, tr("駒"));
    const QStringList titles{tr("将棋盤"), tr("駒台"), tr("対局者情報"), tr("背景")};
    const QStringList names{QStringLiteral("appearanceBoards"), QStringLiteral("appearanceStands"),
                            QStringLiteral("appearanceInformation"), QStringLiteral("appearanceBackgrounds")};
    for (int i = 0; i < titles.size(); ++i) {
        const auto component = static_cast<Component>(i);
        auto* page = new QWidget(m_sections);
        auto* column = new QVBoxLayout(page);
        auto* note = new QLabel(i == 2 ? tr("手番・対局者名・持ち時間の見本（20種類）")
                                      : tr("%1の見本（%2種類）").arg(titles.at(i)).arg(BoardAppearanceCatalog::samples(component).size()), page);
        note->setWordWrap(true);
        column->addWidget(note);
        auto* list = gallery(page, names.at(i));
        list->viewport()->installEventFilter(this);
        list->setProperty("component", i);
        for (const auto& sample : BoardAppearanceCatalog::samples(component))
            new QListWidgetItem(sample.name, list);
        m_componentLists.append(list);
        column->addWidget(list);
        if (component == Component::Background) m_sections->insertTab(2, page, titles.at(i));
        else m_sections->addTab(page, titles.at(i));
        connect(list, &QListWidget::currentRowChanged, this, &BoardColorDialog::selectComponent);
    }
    auto* details = new QScrollArea(m_sections);
    details->setWidgetResizable(true);
    details->setFrameShape(QFrame::NoFrame);
    details->setWidget(m_tabs);
    m_sections->addTab(details, tr("詳細"));
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
    for (const auto& combination : combinations) m_combinations->addItem(tr(combination.name));
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
    m_sections->setCurrentIndex(AppSettings::appearanceSection());
    m_pieceFilter->setCurrentIndex(AppSettings::appearancePieceFilter());
    refreshCatalogIcons();
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
    auto colors = BoardAppearance::instance().colors();
    auto visuals = BoardAppearance::instance().visuals();
    BoardAppearanceCatalog::apply(component, samples.at(row), colors, visuals);
    BoardAppearance::instance().setColors(colors);
    BoardAppearance::instance().setVisuals(visuals);
}

void BoardColorDialog::filterPieces(int index)
{
    const QStringList prefixes{QString(), QStringLiteral("torafu_"), QStringLiteral("wood_"),
                               QStringLiteral("tint_"), QStringLiteral("deep_"), QStringLiteral("sengoku"), QStringLiteral("chess_"), QStringLiteral("alphabet_")};
    if (index < 0 || index >= prefixes.size()) return;
    for (int row = 0; row < m_pieceList->count(); ++row) {
        auto* item = m_pieceList->item(row);
        item->setHidden(index != 0 && !item->data(Qt::UserRole).toString().startsWith(prefixes.at(index)));
    }
}

void BoardColorDialog::refreshCatalogIcons()
{
    const auto style = PieceImageProvider::instance().style();
    for (int i = 0; i < m_componentLists.size(); ++i) {
        const auto component = static_cast<Component>(i);
        const auto samples = BoardAppearanceCatalog::samples(component);
        for (int row = 0; row < samples.size(); ++row)
            m_componentLists.at(i)->item(row)->setIcon(sampleIcon(component, samples.at(row), style));
    }
    syncCatalog();
}

void BoardColorDialog::syncCatalog()
{
    const auto colors = BoardAppearance::instance().colors();
    const auto visuals = BoardAppearance::instance().visuals();
    const auto style = PieceImageProvider::instance().style();
    const QSignalBlocker blockPieces(m_pieceList), blockCombinations(m_combinations);
    m_pieceList->setCurrentRow(static_cast<int>(AppSettings::availablePieceStyles().indexOf(style)));
    QStringList selection{BoardColorPresets::pieceStyleName(style)};
    for (int i = 0; i < m_componentLists.size(); ++i) {
        auto* list = m_componentLists.at(i);
        const QSignalBlocker blocker(list);
        const auto component = static_cast<Component>(i);
        const auto samples = BoardAppearanceCatalog::samples(component);
        int selected = -1;
        for (int row = 0; row < samples.size(); ++row)
            if (BoardAppearanceCatalog::matches(component, samples.at(row), colors, visuals)) selected = row;
        list->setCurrentRow(selected);
        selection.append(selected < 0 ? tr("カスタム") : samples.at(selected).name);
    }
    m_selectionSummary->setText(tr("駒：%1\n将棋盤：%2　／　背景：%5\n駒台：%3　／　対局者情報：%4")
                                  .arg(selection.at(0), selection.at(1), selection.at(2), selection.at(3), selection.at(4)));
    int selected = -1;
    for (int i = 0; i < static_cast<int>(std::size(combinations)); ++i) {
        const auto& combination = combinations[i];
        if (style == QLatin1String(combination.pieces)
            && m_componentLists.at(0)->currentRow() == combination.board
            && m_componentLists.at(1)->currentRow() == combination.stand
            && m_componentLists.at(2)->currentRow() == combination.information
            && m_componentLists.at(3)->currentRow() == combination.background) selected = i;
    }
    m_combinations->setCurrentIndex(selected);
}

void BoardColorDialog::applyCombination(int index)
{
    if (index < 0 || index >= static_cast<int>(std::size(combinations))) return;
    const auto& combination = combinations[index];
    auto colors = BoardAppearance::instance().colors();
    auto visuals = BoardAppearance::instance().visuals();
    const int rows[] = {combination.board, combination.stand, combination.information, combination.background};
    for (int i = 0; i < static_cast<int>(std::size(rows)); ++i) {
        const auto component = static_cast<Component>(i);
        BoardAppearanceCatalog::apply(component, BoardAppearanceCatalog::samples(component).at(rows[i]), colors, visuals);
    }
    PieceImageProvider::instance().setStyle(QLatin1String(combination.pieces));
    BoardAppearance::instance().setColors(colors);
    BoardAppearance::instance().setVisuals(visuals);
}

void BoardColorDialog::restoreOpeningAppearance()
{
    PieceImageProvider::instance().setStyle(m_openingStyle);
    BoardAppearance::instance().setColors(m_openingColors);
    BoardAppearance::instance().setVisuals(m_openingVisuals);
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
