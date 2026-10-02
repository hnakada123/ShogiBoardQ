#include "piecepainter.h"

#include <QCache>
#include <QIcon>
#include <QImage>
#include <QPainter>
#include <QPixmapCache>
#include <QtMath>

namespace {
int pieceSize(qreal cellSize, const BoardVisuals& visuals)
{
    return qMax(1, qRound(cellSize * visuals.normalized().pieceScale / 100.0));
}

QPixmap renderedPiece(const QIcon& icon, int size, qreal dpr, bool shadow)
{
    const QString key = QStringLiteral("shogi-piece/%1/%2/%3/%4")
        .arg(icon.cacheKey()).arg(size).arg(dpr).arg(shadow);
    QPixmap result;
    if (QPixmapCache::find(key, &result)) return result;
    const QPixmap face = icon.pixmap(QSize(size, size), dpr, QIcon::Normal, QIcon::On);
    const int pad = qMax(2, qCeil(size * 0.07));
    result = QPixmap(qCeil((size + 2 * pad) * dpr), qCeil((size + 2 * pad) * dpr));
    result.setDevicePixelRatio(dpr);
    result.fill(Qt::transparent);
    QPainter painter(&result);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    const QRectF target(pad, pad, size, size);
    if (shadow) {
        QPixmap mask(face.size());
        mask.setDevicePixelRatio(dpr);
        mask.fill(Qt::transparent);
        QPainter maskPainter(&mask);
        maskPainter.drawPixmap(QPointF(), face);
        maskPainter.setCompositionMode(QPainter::CompositionMode_SourceIn);
        maskPainter.fillRect(QRectF(QPointF(), mask.deviceIndependentSize()), QColor(45, 30, 14, 130));
        maskPainter.end();
        // 透過輪郭を周囲に重ねてぼかす。駒画像の回転とは独立した画面方向の影。
        const qreal blur = qMax(0.45, size * 0.013);
        painter.setOpacity(0.10);
        for (int y = -1; y <= 1; ++y)
            for (int x = -1; x <= 1; ++x)
                painter.drawPixmap(target.translated(size * 0.014 + x * blur, size * 0.032 + y * blur),
                                   mask, QRectF(mask.rect()));
        painter.setOpacity(1);
    }
    painter.drawPixmap(target, face, QRectF(face.rect()));
    painter.end();
    QPixmapCache::insert(key, result);
    return result;
}
}

PiecePainter::Image PiecePainter::image(const QIcon& icon, qreal cellSize,
                                        const BoardVisuals& visuals, qreal dpr)
{
    if (icon.isNull() || cellSize <= 0) return {};
    const auto pixmap = renderedPiece(icon, pieceSize(cellSize, visuals), dpr, visuals.pieceShadow);
    // 画像そのものはQPixmapCacheに任せ、GUI終了後も安全な矩形だけを保持する。
    static QCache<qint64, QRectF> boundsCache(256);
    const qint64 key = pixmap.cacheKey();
    if (const auto* bounds = boundsCache.object(key)) return {pixmap, *bounds};

    const auto pixels = pixmap.toImage().convertToFormat(QImage::Format_ARGB32);
    int left = pixels.width(), top = pixels.height(), right = -1, bottom = -1;
    for (int y = 0; y < pixels.height(); ++y) {
        const auto* row = reinterpret_cast<const QRgb*>(pixels.constScanLine(y));
        for (int x = 0; x < pixels.width(); ++x) {
            if (qAlpha(row[x]) == 0) continue;
            left = qMin(left, x);
            top = qMin(top, y);
            right = qMax(right, x);
            bottom = qMax(bottom, y);
        }
    }
    const QRectF bounds = right < left ? QRectF()
        : QRectF(left / dpr, top / dpr, (right - left + 1) / dpr, (bottom - top + 1) / dpr);
    boundsCache.insert(key, new QRectF(bounds));
    return {pixmap, bounds};
}

void PiecePainter::draw(QPainter& painter, const QIcon& icon, const QRectF& cell, const BoardVisuals& visuals)
{
    if (icon.isNull() || cell.isEmpty()) return;
    const int size = pieceSize(qMin(cell.width(), cell.height()), visuals);
    const qreal dpr = painter.device()->devicePixelRatioF();
    const auto pixmap = renderedPiece(icon, size, dpr, visuals.pieceShadow);
    const QSizeF renderedSize = pixmap.deviceIndependentSize();
    const QPointF pos = cell.center() - QPointF(renderedSize.width() / 2, renderedSize.height() / 2);
    painter.save();
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.drawPixmap(pos, pixmap);
    painter.restore();
}
