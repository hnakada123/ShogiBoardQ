/// @file shogiview_piecebox.cpp
/// @brief 局面編集用の駒箱（未使用駒）の配置と描画

#include "shogiview.h"
#include "shogiboard.h"
#include "boardconstants.h"
#include "boardsurfacepainter.h"
#include "piecepainter.h"

#include <QButtonGroup>
#include <QFrame>
#include <QFontMetrics>
#include <QPainter>
#include <QToolButton>

QRect ShogiView::pieceBoxRect() const
{
    if (!positionEditMode() || !m_board) return {};
    const QRect stand = flipMode() ? blackStandBoundingRect() : whiteStandBoundingRect();
    const auto* card = flipMode() ? m_blackPlayerCard : m_whitePlayerCard;
    const int gap = qMax(4, qRound(fieldSize().height() * 0.08));
    const int top = (card && !card->isHidden() ? card->geometry().bottom() : stand.bottom()) + gap + 1;
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
    if (m_pieceBoxBlackButton) m_pieceBoxBlackButton->setChecked(side == Turn::Black);
    if (m_pieceBoxWhiteButton) m_pieceBoxWhiteButton->setChecked(side == Turn::White);
    update();
}

void ShogiView::onPieceBoxSideClicked(int id)
{
    setPieceBoxSide(id == 0 ? Turn::Black : Turn::White);
}

void ShogiView::relayoutPieceBoxSideSelector()
{
    const QRect selector = ShogiViewLayout::pieceBoxSideSelectorRect(pieceBoxRect());
    if (selector.isEmpty()) {
        if (m_pieceBoxBlackButton) m_pieceBoxBlackButton->hide();
        if (m_pieceBoxWhiteButton) m_pieceBoxWhiteButton->hide();
        return;
    }
    if (!m_pieceBoxBlackButton) {
        auto* group = new QButtonGroup(this);
        m_pieceBoxBlackButton = new QToolButton(this);
        m_pieceBoxWhiteButton = new QToolButton(this);
        m_pieceBoxBlackButton->setObjectName(QStringLiteral("pieceBoxBlackButton"));
        m_pieceBoxWhiteButton->setObjectName(QStringLiteral("pieceBoxWhiteButton"));
        m_pieceBoxBlackButton->setText(tr("先手"));
        m_pieceBoxWhiteButton->setText(tr("後手"));
        m_pieceBoxBlackButton->setToolTip(tr("先手の駒を配置（手番は変更しません）"));
        m_pieceBoxWhiteButton->setToolTip(tr("後手の駒を配置（手番は変更しません）"));
        group->addButton(m_pieceBoxBlackButton, 0);
        group->addButton(m_pieceBoxWhiteButton, 1);
        for (auto* button : {m_pieceBoxBlackButton, m_pieceBoxWhiteButton}) {
            button->setCheckable(true);
            button->setCursor(Qt::PointingHandCursor);
            button->setStyleSheet(QStringLiteral(
                "QToolButton { border: 1px solid palette(mid); border-radius: 3px;"
                " padding: 0px; background: palette(button); color: palette(button-text); }"
                "QToolButton:checked { background: palette(highlight); color: palette(highlighted-text); }"
                "QToolButton:focus { border: 1px solid palette(text); }"));
        }
        connect(group, &QButtonGroup::idClicked, this, &ShogiView::onPieceBoxSideClicked);
        setPieceBoxSide(pieceBoxSide());
    }
    const int halfWidth = selector.width() / 2;
    m_pieceBoxBlackButton->setGeometry(selector.left() + 1, selector.top(), halfWidth - 2, selector.height() - 1);
    m_pieceBoxWhiteButton->setGeometry(selector.left() + halfWidth + 1, selector.top(),
                                     selector.width() - halfWidth - 2, selector.height() - 1);
    for (auto* button : {m_pieceBoxBlackButton, m_pieceBoxWhiteButton}) {
        QFont buttonFont = font();
        buttonFont.setBold(true);
        int pixels = qMax(1, button->height() * 2 / 3);
        buttonFont.setPixelSize(pixels);
        while (pixels > 1 && (QFontMetrics(buttonFont).horizontalAdvance(button->text()) > button->width() - 4
                              || QFontMetrics(buttonFont).height() > button->height() - 2))
            buttonFont.setPixelSize(--pixels);
        button->setFont(buttonFont);
        button->show();
        button->raise();
    }
}

void ShogiView::drawPieceBoxBackground(QPainter* painter)
{
    const QRect box = pieceBoxRect();
    if (box.isEmpty()) return;
    BoardSurfacePainter::draw(*painter, m_layout.standSurfaceRect(box),
                              m_boardColors.stand, m_boardVisuals.standWoodGrain, fieldSize().width());
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
