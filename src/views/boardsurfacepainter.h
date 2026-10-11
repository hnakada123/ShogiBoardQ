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
/// exposed は描き直す範囲。表面の内側に収まるときは見えない縁・影を省く（空なら全体を描く）。
void draw(QPainter& painter, const QRectF& surface, const QColor& color,
          bool woodGrain, qreal squareSize, const QRectF& exposed = QRectF());
/// draw() が縁・影も含めて塗る範囲（論理ピクセル）。
QRectF paintedRect(const QRectF& surface, qreal squareSize);
}

#endif // BOARDSURFACEPAINTER_H
