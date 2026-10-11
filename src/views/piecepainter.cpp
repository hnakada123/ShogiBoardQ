#include "piecepainter.h"

#include <QCache>
#include <QHash>
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

// 影の張り出し用に駒画像の周囲へ確保する余白
int piecePadding(int size)
{
    return qMax(2, qCeil(size * 0.07));
}

// 描いた駒画像を QPixmapCache から探す条件
struct RenderKey {
    qint64 icon = 0;
    int size = 0;
    qreal dpr = 0;
    bool shadow = false;
    bool operator==(const RenderKey& other) const
    {
        return icon == other.icon && size == other.size && dpr == other.dpr && shadow == other.shadow;
    }
};

size_t qHash(const RenderKey& key, size_t seed = 0)
{
    return qHashMulti(seed, key.icon, key.size, key.dpr, key.shadow);
}

QPixmap renderedPiece(const QIcon& icon, int size, qreal dpr, bool shadow)
{
    // 駒を描くたびに文字列のキーを組み立てると重いので、条件ごとに QPixmapCache の鍵を覚える。
    // 画像そのものは QPixmapCache に任せる（GUI の終了後に QPixmap を残さない）。
    static QHash<RenderKey, QPixmapCache::Key> keys;
    constexpr qsizetype kMaxKeys = 4096;
    const RenderKey renderKey{icon.cacheKey(), size, dpr, shadow};
    QPixmap result;
    if (const auto it = keys.constFind(renderKey); it != keys.cend() && QPixmapCache::find(*it, &result))
        return result;
    const QPixmap face = icon.pixmap(QSize(size, size), dpr, QIcon::Normal, QIcon::On);
    const int pad = piecePadding(size);
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
    if (keys.size() >= kMaxKeys) keys.clear();
    keys.insert(renderKey, QPixmapCache::insert(result));
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

QRectF PiecePainter::paintedRect(const QRectF& cell, const BoardVisuals& visuals)
{
    if (cell.isEmpty()) return {};
    const int size = pieceSize(qMin(cell.width(), cell.height()), visuals);
    // 画像は物理画素へ切り上げて作るため、その分の1ピクセルを足す。
    const qreal side = size + 2 * piecePadding(size) + 1;
    QRectF rect(0, 0, side, side);
    rect.moveCenter(cell.center());
    return rect;
}
