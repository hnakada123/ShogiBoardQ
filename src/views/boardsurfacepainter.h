#ifndef BOARDSURFACEPAINTER_H
#define BOARDSURFACEPAINTER_H

#include <QColor>
#include <QRectF>

class QPainter;

/// 盤・駒台・小型プレビューで共有する木肌と縁の描画。
namespace BoardSurfacePainter {
void draw(QPainter& painter, const QRectF& surface, const QColor& color,
          bool woodGrain, qreal squareSize);
}

#endif // BOARDSURFACEPAINTER_H
