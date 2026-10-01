#include "piecepainter.h"

#include <QIcon>
#include <QPainter>
#include <QPixmapCache>
#include <QtMath>

namespace {
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

void PiecePainter::draw(QPainter& painter, const QIcon& icon, const QRectF& cell, const BoardVisuals& visuals)
{
    if (icon.isNull() || cell.isEmpty()) return;
    const int size = qMax(1, qRound(qMin(cell.width(), cell.height()) * visuals.normalized().pieceScale / 100.0));
    const qreal dpr = painter.device()->devicePixelRatioF();
    const auto pixmap = renderedPiece(icon, size, dpr, visuals.pieceShadow);
    const QSizeF renderedSize = pixmap.deviceIndependentSize();
    const QPointF pos = cell.center() - QPointF(renderedSize.width() / 2, renderedSize.height() / 2);
    painter.save();
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.drawPixmap(pos, pixmap);
    painter.restore();
}
