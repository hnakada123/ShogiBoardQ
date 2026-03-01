/// @file shogiview_stand.cpp
/// @brief ShogiView の駒台描画ヘルパ（駒台セル・駒アイコン・駒文字マッピング）

#include "shogiview.h"
#include "shogiboard.h"

#include <QColor>
#include <QPainter>
#include <QFont>
#include <QFontMetrics>

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

void ShogiView::drawBlackNormalModeStand(QPainter* painter)
{
    painter->setPen(palette().color(QPalette::Dark));
    for (int r = 6; r <= 9; ++r) {
        for (int c = 1; c <= 2; ++c) {
            drawBlackStandField(painter, c, r);
        }
    }
}

void ShogiView::drawWhiteNormalModeStand(QPainter* painter)
{
    painter->setPen(palette().color(QPalette::Dark));
    for (int r = 1; r <= 4; ++r) {
        for (int c = 1; c <= 2; ++c) {
            drawWhiteStandField(painter, c, r);
        }
    }
}

// 通常対局モードにおける「駒台」描画の統括エントリポイント。
// 役割：先手（黒）→ 後手（白）の順に、通常モード用の駒台マスを描画する関数へ委譲する。
void ShogiView::drawNormalModeStand(QPainter* painter)
{
    // 先手（黒）側の通常モード駒台を描画
    drawBlackNormalModeStand(painter);

    // 後手（白）側の通常モード駒台を描画
    drawWhiteNormalModeStand(painter);
}

// 先手（黒）側の駒台セル（1マス）を描画する。
void ShogiView::drawBlackStandField(QPainter* painter, const int file, const int rank) const
{
    const QRect fieldRect = cachedFieldRect(file, rank);
    QRect adjustedRect = makeStandCellRect(
        m_layout.flipMode(), m_layout.param1(), m_layout.offsetX(), m_layout.offsetY(), fieldRect, true);

    painter->save();
    const QColor fillColor(228, 167, 46, 255);  // 駒台の塗り（木目系）
    painter->setPen(fillColor);
    painter->setBrush(fillColor);
    painter->drawRect(adjustedRect);
    painter->restore();
}

// 後手（白）側の駒台セル（1マス）を描画する。
void ShogiView::drawWhiteStandField(QPainter* painter, const int file, const int rank) const
{
    const QRect fieldRect = cachedFieldRect(file, rank);
    QRect adjustedRect = makeStandCellRect(
        m_layout.flipMode(), m_layout.param2(), m_layout.offsetX(), m_layout.offsetY(), fieldRect, false);

    painter->save();
    const QColor fillColor(228, 167, 46, 255);  // 駒台の塗り（木目系）
    painter->setPen(fillColor);
    painter->setBrush(fillColor);
    painter->drawRect(adjustedRect);
    painter->restore();
}

// 【通常対局モード：先手（左側）駒台のアイコンを 4×2 で描画】
void ShogiView::drawPiecesBlackStandInNormalMode(QPainter* painter)
{
    const int ranks [4] = { 6, 7, 8, 9 };
    for (int row = 0; row < 4; ++row) {
        for (int file = 1; file <= 2; ++file) {
            drawBlackStandPiece(painter, file, ranks[row]);
        }
    }
}

// 【通常対局モード：後手（右側）駒台のアイコンを 4×2 で描画】
void ShogiView::drawPiecesWhiteStandInNormalMode(QPainter* painter)
{
    const int ranks [4] = { 4, 3, 2, 1 };
    for (int row = 0; row < 4; ++row) {
        for (int file = 1; file <= 2; ++file) {
            drawWhiteStandPiece(painter, file, ranks[row]);
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
 * @brief 駒台セル内に「等倍」の駒アイコンを重ね描きする（最大2段＝最大3枚表示）。
 */
void ShogiView::drawStandPieceIcon(QPainter* painter, const QRect& adjustedRect, QChar value) const
{
    const Piece pieceKey = charToPiece(value);
    const int count = (m_interaction.dragging() && m_interaction.tempPieceStandCounts().contains(pieceKey))
    ? m_interaction.tempPieceStandCounts()[pieceKey]
    : m_board->m_pieceStand.value(pieceKey);
    if (count <= 0 || value == QLatin1Char(' ')) return;

    const QIcon icon = piece(value);
    if (icon.isNull()) return;

    // === 駒画像は正方形のアスペクト比を維持 ===
    const int cellW = adjustedRect.width();
    const int iconW = cellW;
    const int iconH = cellW;  // 幅と同じにして正方形を維持

    // ▼ここで"見せる重なり段数"と"最大表示枚数"をハードキャップ
    const int maxOverlapSteps = 2;
    const int maxVisibleIcons = maxOverlapSteps + 1;
    const int visible = qMin(count, maxVisibleIcons);
    const int stepsEff = qMax(0, visible - 1);

    // 重なり幅（横広がり）
    const qreal perStep = iconW * 0.05;
    const qreal hardMax = iconW * 0.85;
    const qreal totalSpread = qMin(hardMax, perStep * qreal(stepsEff));

    // 最前面ベースを「重ね全体センター=マス中心」に配置
    QRect base(0, 0, iconW, iconH);
    base.moveCenter(QPoint(adjustedRect.center().x() + int(totalSpread / 2.0),
                           adjustedRect.center().y()));

    // バッジ右端のはみ出しを微修正
    int margin = qMax(2, iconW / 40);
    int overflowRight = base.right() - (adjustedRect.right() - margin);
    if (overflowRight > 0) base.translate(-overflowRight, 0);

    painter->save();
    painter->setClipRect(adjustedRect);
    painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter->setRenderHint(QPainter::Antialiasing, true);

    const quint64 cacheKey =
        (static_cast<quint64>(static_cast<quint16>(value.unicode())) << 32U)
        | (static_cast<quint64>(static_cast<quint16>(iconW)) << 16U)
        | static_cast<quint64>(static_cast<quint16>(iconH));
    QPixmap pm;
    const auto it = m_standPiecePixmapCache.constFind(cacheKey);
    if (it != m_standPiecePixmapCache.constEnd()) {
        pm = it.value();
    } else {
        pm = icon.pixmap(QSize(iconW, iconH), QIcon::Normal, QIcon::On);
        if (!pm.isNull()) {
            m_standPiecePixmapCache.insert(cacheKey, pm);
        }
    }
    const bool iconOk = !pm.isNull();

    // 奥(左)→手前(右)。表示は最大 visible 枚に限定
    for (int i = 0; i < visible; ++i) {
        const qreal t = (stepsEff > 0) ? (qreal(i) / qreal(stepsEff)) : 1.0;
        const qreal shiftX = -totalSpread * (1.0 - t);
        const QRect r = base.translated(int(shiftX), 0);

        if (iconOk) painter->drawPixmap(r, pm);
        else {
            QFont f = painter->font();
            f.setPixelSize(qMax(8, int(iconH * 0.7)));
            painter->setFont(f);
            painter->drawText(r, Qt::AlignCenter, QString(value));
        }
    }

    // 右下バッジ（総数表示）
    if (count >= 2) {
        const QRect topRect = base;
        QFont f = painter->font();
        int px = qBound(9, int(iconH * 0.34), 48);
        f.setPixelSize(px);
        f.setBold(true);
        painter->setFont(f);

        const QString text = QString::number(count);
        QFontMetrics fm(f);
        int padX = qMax(3, iconW / 18);
        int padY = qMax(2, iconH / 22);
        QSize tsz = fm.size(Qt::TextSingleLine, text);

        const int maxBadgeW = qMax(8, iconW - 2 * margin);
        const int maxBadgeH = qMax(8, iconH - 2 * margin);
        const int needW = tsz.width() + padX * 2;
        const int needH = tsz.height() + padY * 2;
        const qreal scale = qMin<qreal>(1.0, qMin(qreal(maxBadgeW)/needW, qreal(maxBadgeH)/needH));
        if (scale < 1.0) {
            px   = qMax(8, int(px   * scale));
            padX = qMax(2, int(padX * scale));
            padY = qMax(2, int(padY * scale));
            f.setPixelSize(px);
            painter->setFont(f);
            fm  = QFontMetrics(f);
            tsz = fm.size(Qt::TextSingleLine, text);
        }

        QRect badge(0, 0, qMin(maxBadgeW, tsz.width() + padX * 2),
                    qMin(maxBadgeH, tsz.height() + padY * 2));
        badge.moveBottomRight(QPoint(topRect.right() - margin, topRect.bottom() - margin));

        const qreal radius = qMin(badge.width(), badge.height()) * 0.35;
        QPen outline(QColor(255, 255, 255, 220), qMax(1, iconW / 60));
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(0, 0, 0, 170));
        painter->drawRoundedRect(badge, radius, radius);

        painter->setPen(outline);
        painter->setBrush(Qt::NoBrush);
        painter->drawRoundedRect(badge, radius, radius);

        painter->setPen(Qt::white);
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
