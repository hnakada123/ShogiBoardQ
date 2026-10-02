/// @file shogiview_stand.cpp
/// @brief ShogiView の駒台描画ヘルパ（駒台セル・駒アイコン・駒文字マッピング）

#include "shogiview.h"
#include "shogiboard.h"
#include "boardsurfacepainter.h"
#include "piecepainter.h"

#include <QColor>
#include <QPainter>
#include <QFont>
#include <QFontMetrics>
#include <QtMath>

// 駒台セル（1マス）の描画矩形を算出するユーティリティ。
// 役割：盤上の基準マス矩形（fieldRect）から、先手/後手の駒台側に水平オフセットした矩形を返す。
static inline QRect makeStandCellRect(bool flip, int param, int offsetX, int offsetY, const QRect& fieldRect, bool leftSide)
{
    QRect adjustedRect;

    if (flip) {
        // 【反転時】先手は左、後手は右に配置。
        adjustedRect.setRect(fieldRect.left() + (leftSide ? -param : +param) + offsetX,
                             fieldRect.top()  + offsetY,
                             fieldRect.width(),
                             fieldRect.height());
    } else {
        // 【通常時】先手は右、後手は左に配置。
        adjustedRect.setRect(fieldRect.left() + (leftSide ? +param : -param) + offsetX,
                             fieldRect.top()  + offsetY,
                             fieldRect.width(),
                             fieldRect.height());
    }

    return adjustedRect;
}

// 持駒の配置・当たり判定はセルのまま、木肌は駒台全体を通して描く。
void ShogiView::drawNormalModeStand(QPainter* painter)
{
    if (!m_board) return;
    for (const auto& stand : {blackStandBoundingRect(), whiteStandBoundingRect()}) {
        BoardSurfacePainter::draw(*painter, m_layout.standSurfaceRect(stand),
                                  m_boardColors.stand, m_boardVisuals.standWoodGrain, fieldSize().width());
    }
}

// 【通常対局モード：先手（左側）駒台のアイコンを 4×2 で描画】
void ShogiView::drawPiecesBlackStandInNormalMode(QPainter* painter)
{
    const int ranks [4] = { 6, 7, 8, 9 };
    for (const int rank : ranks) {
        for (int file = 1; file <= 2; ++file) {
            drawBlackStandPiece(painter, file, rank);
        }
    }
}

// 【通常対局モード：後手（右側）駒台のアイコンを 4×2 で描画】
void ShogiView::drawPiecesWhiteStandInNormalMode(QPainter* painter)
{
    const int ranks [4] = { 4, 3, 2, 1 };
    for (const int rank : ranks) {
        for (int file = 1; file <= 2; ++file) {
            drawWhiteStandPiece(painter, file, rank);
        }
    }
}

void ShogiView::drawPiecesStandFeatures(QPainter* painter)
{
    // 先手/後手の駒台にある「駒」と「枚数」を描画
    drawPiecesBlackStandInNormalMode(painter);
    drawPiecesWhiteStandInNormalMode(painter);
}

/**
 * @brief 駒台の内側に駒・重なり・枚数表示を収める（最大3枚表示）。
 */
void ShogiView::drawStandPieceIcon(QPainter* painter, const QRect& adjustedRect, QChar value) const
{
    const Piece pieceKey = charToPiece(value);
    const int count = (m_interaction.dragging() && m_interaction.tempPieceStandCounts().contains(pieceKey))
    ? m_interaction.tempPieceStandCounts()[pieceKey]
    : m_board->pieceStandCount(pieceKey);
    if (count <= 0 || value == QLatin1Char(' ')) return;

    const QIcon icon = piece(value);
    if (icon.isNull()) return;

    const int cellW = adjustedRect.width();
    const auto rendered = PiecePainter::image(icon, cellW, m_boardVisuals,
                                               painter->device()->devicePixelRatioF());
    if (rendered.bounds.isEmpty()) return;
    const QRectF inner = m_layout.standPieceArea(adjustedRect);
    const QSizeF canvas = rendered.pixmap.deviceIndependentSize();
    const QPointF center(canvas.width() / 2, canvas.height() / 2);
    // 影の右下への張り出しも左右・上下に同じ幅を予約し、駒本体は中央に保つ。
    const qreal width = 2 * qMax(center.x() - rendered.bounds.left(), rendered.bounds.right() - center.x());
    const qreal height = 2 * qMax(center.y() - rendered.bounds.top(), rendered.bounds.bottom() - center.y());
    const int visible = qMin(count, 3);
    const int steps = visible - 1;
    const qreal minSpread = cellW * 0.02 * steps;
    const qreal fit = qMin(1.0, qMin((inner.width() - minSpread) / width, inner.height() / height));
    const qreal spread = qMin(cellW * 0.05 * steps, qMax(0.0, inner.width() - width * fit));
    QRectF base(QPointF(), canvas * fit);
    base.moveCenter(inner.center() + QPointF(spread / 2, 0));

    painter->save();
    painter->setClipRect(inner, Qt::IntersectClip);
    painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter->setRenderHint(QPainter::Antialiasing, true);

    // 奥(左)→手前(右)。表示は最大 visible 枚に限定
    for (int i = 0; i < visible; ++i) {
        const qreal shift = steps > 0 ? spread * (steps - i) / steps : 0;
        painter->drawPixmap(base.translated(-shift, 0), rendered.pixmap, QRectF(rendered.pixmap.rect()));
    }

    // 右下バッジ（総数表示）
    if (count >= 2) {
        const QRectF topRect(base.topLeft() + rendered.bounds.topLeft() * fit,
                             rendered.bounds.size() * fit);
        const qreal margin = qMax(1.0, cellW * 0.02);
        QFont f = painter->font();
        int px = qBound(6, int(cellW * 0.34), 48);
        f.setPixelSize(px);
        f.setBold(true);
        painter->setFont(f);

        const QString text = QString::number(count);
        QFontMetrics fm(f);
        const int padX = qMax(1, cellW / 18);
        const int padY = qMax(1, cellW / 22);
        QSize tsz = fm.size(Qt::TextSingleLine, text);

        const int maxBadgeW = qFloor(inner.width() - 2 * margin);
        const int maxBadgeH = qFloor(inner.height() - 2 * margin);
        while (px > 1 && (tsz.width() + padX * 2 > maxBadgeW || tsz.height() + padY * 2 > maxBadgeH)) {
            f.setPixelSize(--px);
            fm = QFontMetrics(f);
            tsz = fm.size(Qt::TextSingleLine, text);
        }
        painter->setFont(f);

        QRectF badge(0, 0, tsz.width() + padX * 2, tsz.height() + padY * 2);
        badge.moveBottomRight(QPointF(
            qBound(inner.left() + margin + badge.width(), topRect.right() - margin, inner.right() - margin),
            qBound(inner.top() + margin + badge.height(), topRect.bottom() - margin, inner.bottom() - margin)));

        const qreal radius = qMin(badge.width(), badge.height()) * 0.35;
        QPen outline(m_boardColors.cardBorder, qMax(1.0, cellW / 60.0));
        painter->setPen(Qt::NoPen);
        painter->setBrush(m_boardColors.cardBackground);
        painter->drawRoundedRect(badge, radius, radius);

        painter->setPen(outline);
        painter->setBrush(Qt::NoBrush);
        painter->drawRoundedRect(badge, radius, radius);

        painter->setPen(m_boardColors.nameText);
        painter->drawText(badge, Qt::AlignCenter, text);
    }

    painter->restore();
}

// 先手（黒）側の駒台セルに対応する「駒アイコン」を描画する。
void ShogiView::drawBlackStandPiece(QPainter* painter, const int file, const int rank) const
{
    const QRect fieldRect = cachedFieldRect(file, rank);
    QRect adjustedRect = makeStandCellRect(
        m_layout.flipMode(), m_layout.param1(),
        m_layout.offsetX(), m_layout.offsetY(),
        fieldRect, /*leftSide=*/true);

    QChar value = rankToBlackShogiPiece(file, rank);
    drawStandPieceIcon(painter, adjustedRect, value);
}

// 後手（白）側の駒台セルに対応する「駒アイコン」を描画する。
void ShogiView::drawWhiteStandPiece(QPainter* painter, const int file, const int rank) const
{
    const QRect fieldRect = cachedFieldRect(file, rank);
    QRect adjustedRect = makeStandCellRect(
        m_layout.flipMode(), m_layout.param2(),
        m_layout.offsetX(), m_layout.offsetY(),
        fieldRect, /*leftSide=*/false);

    QChar value = rankToWhiteShogiPiece(file, rank);
    drawStandPieceIcon(painter, adjustedRect, value);
}

// 駒台の段→駒文字マッピング
QChar ShogiView::rankToBlackShogiPiece(const int file, const int rank) const
{
    // 右列(file=1): K, B, S, L
    if (file == 1) {
        switch (rank) {
        case 6: return 'K';
        case 7: return 'B';
        case 8: return 'S';
        case 9: return 'L';
        default: return ' ';
        }
    }
    // 左列(file=2): R, G, N, P
    else if (file == 2) {
        switch (rank) {
        case 6: return 'R';
        case 7: return 'G';
        case 8: return 'N';
        case 9: return 'P';
        default: return ' ';
        }
    }
    else {
        return ' ';
    }
}

QChar ShogiView::rankToWhiteShogiPiece(const int file, const int rank) const
{
    // 左列(file=1): r, g, n, p
    if (file == 1) {
        switch (rank) {
        case 4: return 'r';
        case 3: return 'g';
        case 2: return 'n';
        case 1: return 'p';
        default: return ' ';
        }
    }
    // 右列(file=2): k, b, s, l
    else if (file == 2) {
        switch (rank) {
        case 4: return 'k';
        case 3: return 'b';
        case 2: return 's';
        case 1: return 'l';
        default: return ' ';
        }
    }
    else {
        return ' ';
    }
}
