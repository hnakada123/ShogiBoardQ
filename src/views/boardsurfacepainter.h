#ifndef BOARDSURFACEPAINTER_H
#define BOARDSURFACEPAINTER_H

#include <QColor>
#include <QMarginsF>
#include <QRectF>

class QPainter;

/// 盤・駒台・小型プレビューで共有する木肌と縁の描画。
namespace BoardSurfacePainter {
/// 表面の外側に描く縁・影の幅（論理ピクセル）。
QMarginsF shadowMargins(qreal squareSize);
void draw(QPainter& painter, const QRectF& surface, const QColor& color,
          bool woodGrain, qreal squareSize);
}

#endif // BOARDSURFACEPAINTER_H
