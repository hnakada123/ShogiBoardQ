/// @file boardappearancesamples.cpp
/// @brief 外観ダイアログの見本の描画の実装

#include "boardappearancesamples.h"

#include "boardsurfacepainter.h"
#include "pieceimageprovider.h"
#include "piecepainter.h"

#include <QApplication>
#include <QCoreApplication>
#include <QListWidget>
#include <QPainter>
#include <QPixmap>

namespace BoardAppearanceSamples {

namespace {
using Component = BoardAppearanceCatalog::Component;

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
} // namespace

QListWidget* createGallery(QWidget* parent, const QString& objectName)
{
    auto* list = new QListWidget(parent);
    list->setObjectName(objectName);
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

QIcon placeholder()
{
    return sampleIconWithoutTint(blankSample());
}

QIcon pieceSample(const QString& style)
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

QIcon componentSample(Component component, const BoardAppearanceSample& sample, const QString& style)
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
        painter.drawText(QRect(14, 10, 36, 18), Qt::AlignCenter, QCoreApplication::translate("BoardAppearanceDialog", "手番"));
        font.setPixelSize(14);
        font.setBold(false);
        painter.setFont(font);
        painter.setPen(c.nameText);
        painter.drawText(QRect(14, 32, 114, 18), Qt::AlignLeft, QCoreApplication::translate("BoardAppearanceDialog", "▲先手"));
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

QIcon paletteSample(const BoardColors& colors, const QString& style, const BoardVisuals& visuals)
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
    PiecePainter::draw(painter, pieces.iconForStyle(Piece::WhiteDragon, style), QRect(50, 3, 20, 22), visuals);
    PiecePainter::draw(painter, pieces.iconForStyle(Piece::BlackPawn, style), QRect(70, 25, 20, 22), visuals);
    PiecePainter::draw(painter, pieces.iconForStyle(Piece::BlackKing, style), QRect(50, 47, 20, 22), visuals);
    painter.end();
    return QIcon(preview);
}

QIcon colorSwatch(const QColor& color, const QColor& frame)
{
    QPixmap swatch(36, 22);
    swatch.fill(Qt::white);
    QPainter painter(&swatch);
    for (int y = 0; y < swatch.height(); y += 6)
        for (int x = 0; x < swatch.width(); x += 6)
            if ((x / 6 + y / 6) % 2 == 0) painter.fillRect(x, y, 6, 6, QColor(210, 210, 210));
    painter.fillRect(swatch.rect(), color);
    painter.setPen(frame);
    painter.drawRect(swatch.rect().adjusted(0, 0, -1, -1));
    painter.end();
    return QIcon(swatch);
}

LazySampleDelegate::LazySampleDelegate(Render render, QObject* parent)
    : QStyledItemDelegate(parent)
    , m_render(std::move(render))
{
}

void LazySampleDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
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

} // namespace BoardAppearanceSamples
