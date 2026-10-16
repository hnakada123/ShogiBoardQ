/// @file boardstaticlayer.cpp
/// @brief 盤面のうち局面によらない部分を画像にして貼るクラスの実装

#include "boardstaticlayer.h"

#include "boardsurfacepainter.h"
#include "boardgeometry.h"

#include <QPaintDevice>
#include <QPainter>
#include <QPen>
#include <QtMath>

#include <cmath>
#include <tuple>
#include <utility>

bool BoardStaticLayer::Key::operator==(const Key& other) const
{
    const auto fields = [](const Key& k) {
        return std::tie(k.size, k.dpr, k.phase, k.surface, k.blackStand, k.whiteStand, k.fieldSize,
                        k.cells, k.background, k.board, k.stand, k.grid, k.woodGrain, k.standWoodGrain);
    };
    return fields(*this) == fields(other);
}

void BoardStaticLayer::draw(QPainter& painter, QSize widgetSize, const BoardGeometry& geometry,
                            const BoardColors& colors, const BoardVisuals& visuals)
{
    // 端数倍率（125% など）では盤の左上が物理画素の途中に来る。画像もその端数だけずらして描き、
    // 物理画素の境目に合わせて貼ることで、直接描いたときと同じ画素にする。
    const QTransform device = painter.deviceTransform();
    const qreal dpr = painter.device()->devicePixelRatioF();
    const QPointF phase(device.dx() - std::floor(device.dx()), device.dy() - std::floor(device.dy()));
    const Key key{widgetSize, dpr, phase, geometry.surface(),
                  geometry.stand(Turn::Black), geometry.stand(Turn::White),
                  geometry.fieldSize(), geometry.board(),
                  colors.background, colors.board, colors.stand, colors.grid,
                  visuals.woodGrain, visuals.standWoodGrain};
    if (!m_image.isNull() && !(m_key == key)) {
        // 今の画像を1つ前の画像として残し、1つ前の画像が条件に合えば今の画像に戻す
        std::swap(m_image, m_spareImage);
        std::swap(m_key, m_spareKey);
    }
    if (m_image.isNull() || !(m_key == key)) {
        // 端数の分だけ右下へずらすため、1物理画素の余白を持たせる。
        QPixmap image(qCeil(widgetSize.width() * dpr) + 1, qCeil(widgetSize.height() * dpr) + 1);
        image.setDevicePixelRatio(dpr);
        image.fill(colors.background);
        QPainter imagePainter(&image);
        imagePainter.translate(phase / dpr);
        imagePainter.setRenderHint(QPainter::Antialiasing, false);
        imagePainter.setRenderHint(QPainter::TextAntialiasing, true);
        imagePainter.setRenderHint(QPainter::SmoothPixmapTransform, true);
        paint(imagePainter, key, geometry);
        imagePainter.end();
        m_image = image;
        m_key = key;
    }
    painter.save();
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
    painter.drawPixmap(-phase / dpr, m_image);
    painter.restore();
}

void BoardStaticLayer::paint(QPainter& painter, const Key& key, const BoardGeometry& geometry)
{
    const QSize fs = key.fieldSize;
    const int files = geometry.files();
    const int ranks = geometry.ranks();
    painter.fillRect(QRect(QPoint(), key.size), key.background);
    BoardSurfacePainter::draw(painter, key.surface, key.board, key.woodGrain, fs.width());

    // 罫線
    const QRectF grid(key.cells);
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    const qreal dpr = painter.device()->devicePixelRatioF();
    const int physicalWidth = qMax(1, qRound(qMax(1.0, fs.width() / 60.0) * dpr));
    const qreal alignment = physicalWidth % 2 ? 0.5 / dpr : 0;
    painter.translate(alignment, alignment);
    painter.setPen(QPen(key.grid, physicalWidth / dpr));
    for (int c = 1; c < files; ++c) {
        const qreal x = grid.left() + c * fs.width();
        painter.drawLine(QPointF(x, grid.top()), QPointF(x, grid.bottom()));
    }
    for (int r = 1; r < ranks; ++r) {
        const qreal y = grid.top() + r * fs.height();
        painter.drawLine(QPointF(grid.left(), y), QPointF(grid.right(), y));
    }
    painter.setPen(QPen(key.grid, qMax(1.0, fs.width() / 40.0)));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(grid);
    painter.restore();

    // 駒台。持駒の配置・当たり判定はセルのまま、木肌は駒台全体を通して描く
    for (const QRect& stand : {key.blackStand, key.whiteStand}) {
        BoardSurfacePainter::draw(painter, geometry.standSurface(stand), key.stand, key.standWoodGrain,
                                  fs.width());
    }

    // 星（罫線と同じ色）
    painter.save();
    painter.setBrush(key.grid);
    painter.setPen(Qt::NoPen);
    painter.setRenderHint(QPainter::Antialiasing);
    const qreal starRadius = qMax(1.5, fs.width() * 0.05);
    for (const int column : {3, 6}) {
        for (const int row : {3, 6}) {
            painter.drawEllipse(QPointF(key.cells.left() + fs.width() * column, key.cells.top() + fs.height() * row),
                                starRadius, starRadius);
        }
    }
    painter.restore();
}
