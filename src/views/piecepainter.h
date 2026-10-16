#ifndef PIECEPAINTER_H
#define PIECEPAINTER_H

#include "boardcolors.h"
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
/// draw() が影も含めて塗る範囲（論理ピクセル）。部分再描画の範囲に使う。
QRectF paintedRect(const QRectF& cell, const BoardVisuals& visuals);
/// 持駒を最大3枚まで少しずつずらして重ね、2枚以上なら右下に枚数の札を添える。
/// cellWidth は駒の大きさを決めるマスの幅、area は駒と札を収める範囲（その外は描かない）
void drawStack(QPainter& painter, const QIcon& icon, int count, int cellWidth, const QRectF& area,
               const BoardVisuals& visuals, const BoardColors& colors);
}

#endif // PIECEPAINTER_H
