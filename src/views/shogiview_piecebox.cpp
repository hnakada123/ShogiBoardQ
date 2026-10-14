/// @file shogiview_piecebox.cpp
/// @brief 局面編集用の駒箱（未使用駒）の配置と描画

#include "shogiview.h"
#include "shogiboard.h"
#include "boardconstants.h"
#include "boardsurfacepainter.h"
#include "piecepainter.h"

#include <QFontMetrics>
#include <QPainter>

QRect ShogiView::pieceBoxRect() const
{
    if (!positionEditMode() || !m_board) return {};
    const QRect stand = flipMode() ? blackStandBoundingRect() : whiteStandBoundingRect();
    // 駒箱は駒台の下側の対局者カードのさらに下、盤の下端に揃えて置く
    const QRect card = m_chrome->visibleCardRect(flipMode());
    const int gap = qMax(4, qRound(fieldSize().height() * 0.08));
    const int top = (card.isValid() ? card.bottom() : stand.bottom()) + gap + 1;
    const int bottom = m_layout.offsetY() + fieldSize().height() * m_board->ranks();
    const int height = qMin(bottom - top, qRound(fieldSize().height() * 3.2));
    if (height <= 0) return {};
    return QRect(stand.left(), bottom - height, stand.width(), height);
}

QRect ShogiView::pieceBoxCellRect(int rank) const
{
    return ShogiViewLayout::pieceBoxCellRect(pieceBoxRect(), rank);
}

void ShogiView::setPieceBoxSide(Turn side)
{
    m_interaction.setPieceBoxSide(side);
    m_chrome->setPieceBoxSide(side);
    update();
}

void ShogiView::relayoutPieceBoxSideSelector()
{
    m_chrome->placePieceBoxSelector(ShogiViewLayout::pieceBoxSideSelectorRect(pieceBoxRect()));
    m_chrome->setPieceBoxSide(pieceBoxSide());
}

void ShogiView::drawPieceBoxBackground(QPainter* painter)
{
    const QRect box = pieceBoxRect();
    if (box.isEmpty()) return;
    // 駒箱は局面編集中だけ表示し、ラベルの文字も含むため、変わらない部分の画像には含めない。
    const QRectF surface = m_layout.standSurfaceRect(box);
    const QMarginsF shadow = BoardSurfacePainter::shadowMargins(fieldSize().width());
    if (!intersectsPaintRegion(surface.marginsAdded(shadow).adjusted(-1, -1, 1, 1))) return;
    BoardSurfacePainter::draw(*painter, surface, m_boardColors.stand, m_boardVisuals.standWoodGrain,
                              fieldSize().width());
    painter->save();
    const QRect label = ShogiViewLayout::pieceBoxLabelRect(box);
    QFont labelFont = painter->font();
    labelFont.setPixelSize(qMax(1, label.height() - 2));
    labelFont.setBold(true);
    painter->setFont(labelFont);
    painter->setPen(m_boardColors.grid);
    painter->drawText(label, Qt::AlignCenter, tr("駒箱"));
    painter->restore();
}

void ShogiView::drawPieceBoxPieces(QPainter* painter)
{
    if (pieceBoxRect().isEmpty()) return;
    const auto counts = m_board->pieceBox();
    for (int rank = 1; rank <= 8; ++rank) {
        const Piece boxPiece = m_board->pieceCharacter(BoardConstants::kPieceBoxFile, rank);
        int count = counts.value(boxPiece);
        if (m_interaction.dragging() && m_interaction.dragFrom() == QPoint(BoardConstants::kPieceBoxFile, rank)) --count;
        if (count <= 0) continue;
        const Piece displayPiece = pieceBoxSide() == Turn::Black ? boxPiece : toWhite(boxPiece);
        const QIcon icon = piece(pieceToChar(displayPiece));
        const QRect cell = pieceBoxCellRect(rank);
        if (!intersectsPaintRegion(cell)) continue;
        const int numberWidth = qMax(1, cell.width() * 3 / 10);
        painter->save();
        painter->setClipRect(cell, Qt::IntersectClip);
        PiecePainter::draw(*painter, icon, cell.adjusted(1, 1, -numberWidth, -1), m_boardVisuals);
        if (count >= 2) {
            // 小さい駒箱では駒を重ねず、文字に重ならない専用欄へ枚数を表示する。
            const QRect numberRect(cell.right() - numberWidth, cell.top(), numberWidth, cell.height() / 2);
            const QString number = QString::number(count);
            QFont countFont = painter->font();
            int pixels = qMax(1, cell.height() / 3);
            countFont.setBold(true);
            countFont.setPixelSize(pixels);
            while (pixels > 1 && QFontMetrics(countFont).horizontalAdvance(number) > numberWidth)
                countFont.setPixelSize(--pixels);
            painter->setFont(countFont);
            painter->setPen(m_boardColors.grid);
            painter->drawText(numberRect, Qt::AlignCenter, number);
        }
        painter->restore();
    }
}
