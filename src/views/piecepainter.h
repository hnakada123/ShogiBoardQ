#ifndef PIECEPAINTER_H
#define PIECEPAINTER_H

#include "boardvisuals.h"
#include <QRectF>

class QPainter;
class QIcon;

/// 盤上・持駒・ドラッグ・プレビューで駒の倍率と影を統一する。
namespace PiecePainter {
void draw(QPainter& painter, const QIcon& icon, const QRectF& cell, const BoardVisuals& visuals);
}

#endif // PIECEPAINTER_H
