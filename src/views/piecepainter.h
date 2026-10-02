#ifndef PIECEPAINTER_H
#define PIECEPAINTER_H

#include "boardvisuals.h"
#include <QPixmap>
#include <QRectF>

class QPainter;
class QIcon;

/// 盤上・持駒・ドラッグ・プレビューで駒の倍率と影を統一する。
namespace PiecePainter {
struct Image {
    QPixmap pixmap;
    QRectF bounds; ///< 透明部分を除いた、影を含む描画範囲（論理ピクセル）。
};
Image image(const QIcon& icon, qreal cellSize, const BoardVisuals& visuals, qreal dpr);
void draw(QPainter& painter, const QIcon& icon, const QRectF& cell, const BoardVisuals& visuals);
}

#endif // PIECEPAINTER_H
