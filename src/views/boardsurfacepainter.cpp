#include "boardsurfacepainter.h"

#include <QCache>
#include <QPainter>
#include <QPainterPath>
#include <QImage>
#include <QRandomGenerator>
#include <QtMath>

namespace {
constexpr int kShadowSteps = 5;
constexpr qreal kShadowSpread = 0.015;

qreal surfaceDepth(qreal squareSize)
{
    return qMax(1.0, squareSize * 0.07);
}

QImage woodSurface(const QSize& size, qreal dpr, const QColor& color)
{
    // 高DPIの大きい盤を保持しても再描画時に木目を生成し直さない。
    // QImageならQApplication終了後の破棄も描画基盤に依存しない。
    static QCache<QString, QImage> cache(128 * 1024);
    const QString key = QStringLiteral("%1/%2/%3/%4")
        .arg(size.width()).arg(size.height()).arg(dpr).arg(color.rgba());
    if (const auto* cached = cache.object(key)) return *cached;

    QImage result(QSize(qCeil(size.width() * dpr), qCeil(size.height() * dpr)), QImage::Format_RGB32);
    result.setDevicePixelRatio(dpr);
    // 地色は全域で均一にし、木目の細かな濃淡だけを重ねる。
    result.fill(color);
    QPainter painter(&result);
    painter.setRenderHint(QPainter::Antialiasing);
    // 毎回同じ種を使い、局面更新や拡大縮小で木目が揺れないようにする。
    QRandomGenerator random(20261001U);
    const qreal width = size.width();
    const qreal height = size.height();
    for (int i = 0; i < 420; ++i) {
        const qreal x = random.generateDouble() * width;
        const qreal drift = (random.generateDouble() - 0.5) * width * 0.008;
        QColor grain = color.darker(190);
        grain.setAlphaF(static_cast<float>(0.025 + random.generateDouble() * 0.06));
        painter.setPen(QPen(grain, (0.3 + random.generateDouble() * 0.8) * width / 600.0));
        QPainterPath path;
        path.moveTo(x, 0);
        path.cubicTo(x + drift, height * 0.25, x - drift, height * 0.7, x + drift * 0.5, height);
        painter.drawPath(path);
    }
    painter.end();
    const int cost = qMax(1, static_cast<int>(result.width() * qint64(result.height()) * 4 / 1024));
    cache.insert(key, new QImage(result), cost);
    return result;
}
}

QMarginsF BoardSurfacePainter::shadowMargins(qreal squareSize)
{
    const qreal depth = surfaceDepth(squareSize);
    const qreal spread = squareSize * kShadowSteps * kShadowSpread;
    return {qMax(0.0, spread - depth * 0.2), qMax(0.0, spread - depth),
            spread + depth * 0.2, spread + depth};
}

QRectF BoardSurfacePainter::paintedRect(const QRectF& surface, qreal squareSize)
{
    // 影の角丸のアンチエイリアス分として1ピクセル広げる。
    return surface.marginsAdded(shadowMargins(squareSize)).adjusted(-1, -1, 1, 1);
}

void BoardSurfacePainter::draw(QPainter& painter, const QRectF& surface, const QColor& color,
                              bool woodGrain, qreal squareSize, const QRectF& exposed)
{
    if (surface.isEmpty()) return;
    // ドラッグ中の駒の周囲だけを描き直すときなど、範囲が縁の光・線より内側なら
    // 影の角丸（表面全体の大きさで塗りつぶしを計算する）を省いて木肌だけを描く。
    const qreal edge = 2 + squareSize / 75.0;
    const bool interiorOnly = !exposed.isEmpty()
        && surface.adjusted(edge, edge, -edge, -edge).contains(exposed);
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    const qreal depth = surfaceDepth(squareSize);
    painter.setPen(Qt::NoPen);
    if (!interiorOnly) {
        for (int i = kShadowSteps; i >= 1; --i) {
            const qreal spread = squareSize * i * kShadowSpread;
            painter.setBrush(QColor(25, 30, 20, 7));
            painter.drawRoundedRect(surface.translated(depth * 0.2, depth)
                                        .adjusted(-spread, -spread, spread, spread), 2, 2);
        }
        painter.setBrush(color.darker(140));
        painter.drawRoundedRect(surface.translated(0, depth), 2, 2);
    }
    if (woodGrain) {
        const qreal dpr = painter.device()->devicePixelRatioF();
        const QImage texture = woodSurface(surface.size().toSize(), dpr, color);
        painter.drawImage(surface, texture, QRectF(texture.rect()));
    } else {
        painter.fillRect(surface, color);
    }
    if (!interiorOnly) {
        // 上辺・左辺に光、下辺に薄い縁を付ける。先後の反転で光源は動かさない。
        painter.setPen(QPen(QColor(255, 250, 223, 110), qMax(0.6, squareSize / 75.0)));
        painter.drawLine(surface.topLeft() + QPointF(1, 1), surface.topRight() + QPointF(-1, 1));
        painter.drawLine(surface.topLeft() + QPointF(1, 1), surface.bottomLeft() + QPointF(1, -1));
        painter.setPen(QPen(color.darker(120), qMax(0.6, squareSize / 90.0)));
        painter.drawLine(surface.bottomLeft(), surface.bottomRight());
    }
    painter.restore();
}
