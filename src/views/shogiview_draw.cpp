/// @file shogiview_draw.cpp
/// @brief ShogiView の描画（盤の木肌・罫線・駒・座標・駒台・駒箱）

#include "shogiview.h"
#include "kifupresentation.h"
#include "applicationfonts.h"
#include "shogiviewhighlighting.h"
#include "shogiboard.h"
#include "boardsurfacepainter.h"
#include "piecepainter.h"

#include <QColor>
#include <QPixmap>
#include <QtMath>
#include <utility>
#include <cmath>
#include <QPainter>
#include <QPaintEvent>
#include <QFont>
#include "boardconstants.h"
#include <QFontMetrics>
#include <array>

void ShogiView::drawRanks(QPainter* painter)
{
    // 【安全弁】盤が無ければ段ラベルは描けない
    if (!m_board) return;

    // 【共通状態設定】段ラベル用の文字色（パレットに従う）
    painter->setPen(palette().color(QPalette::WindowText));

    // 【描画ループ】1段目から最終段まで順に描画
    for (int r = 1; r <= m_board->ranks(); ++r) {
        drawRank(painter, r);
    }
}

void ShogiView::drawFiles(QPainter* painter)
{
    // 【安全弁】盤が無ければ筋ラベルは描けない
    if (!m_board) return;

    // 【共通状態設定】筋ラベル用の文字色（パレットに従う）
    painter->setPen(palette().color(QPalette::WindowText));

    // 【描画ループ】1筋目から最終筋まで順に描画
    for (int c = 1; c <= m_board->files(); ++c) {
        drawFile(painter, c);
    }
}

void ShogiView::drawBackground(QPainter* painter)
{
    painter->fillRect(rect(), m_boardColors.background);
}

void ShogiView::drawBoardSurface(QPainter* painter)
{
    if (!m_board) return;
    const QSize fs = fieldSize();
    const QRectF surface = m_layout.boardSurfaceRect(m_board->files(), m_board->ranks());
    BoardSurfacePainter::draw(*painter, surface, m_boardColors.board, m_boardVisuals.woodGrain, fs.width());
}

void ShogiView::drawBoardFields(QPainter* painter)
{
    if (!m_board) return;
    const QSize fs = fieldSize();
    const QRectF grid(m_layout.offsetX(), m_layout.offsetY(),
                      fs.width() * m_board->files(), fs.height() * m_board->ranks());
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    const qreal dpr = painter->device()->devicePixelRatioF();
    const int physicalWidth = qMax(1, qRound(qMax(1.0, fs.width() / 60.0) * dpr));
    const qreal alignment = physicalWidth % 2 ? 0.5 / dpr : 0;
    painter->translate(alignment, alignment);
    painter->setPen(QPen(m_boardColors.grid, physicalWidth / dpr));
    for (int c = 1; c < m_board->files(); ++c) {
        const qreal x = grid.left() + c * fs.width();
        painter->drawLine(QPointF(x, grid.top()), QPointF(x, grid.bottom()));
    }
    for (int r = 1; r < m_board->ranks(); ++r) {
        const qreal y = grid.top() + r * fs.height();
        painter->drawLine(QPointF(grid.left(), y), QPointF(grid.right(), y));
    }
    painter->setPen(QPen(m_boardColors.grid, qMax(1.0, fs.width() / 40.0)));
    painter->setBrush(Qt::NoBrush);
    painter->drawRect(grid);
    painter->restore();
}

void ShogiView::drawPieces(QPainter* painter)
{
    // 【安全弁】盤が未設定なら何もしない
    if (!m_board) return;

    // 【描画ループ】段（r）を降順、筋（c）を昇順に走査し、各マスの駒を描画
    for (int r = m_board->ranks(); r > 0; --r) {
        for (int c = 1; c <= m_board->files(); ++c) {
            drawPiece(painter, c, r);
            if (m_errorOccurred) return;
        }
    }
}

bool ShogiView::paintRegionCovers(const QRect& rect) const
{
    if (m_paintRegion.rectCount() == 1) return m_paintRegion.boundingRect().contains(rect);
    return QRegion(rect).subtracted(m_paintRegion).isEmpty();
}

bool ShogiView::StaticLayerKey::operator==(const StaticLayerKey& other) const
{
    return size == other.size && dpr == other.dpr && phase == other.phase && surface == other.surface
        && blackStand == other.blackStand && whiteStand == other.whiteStand && fieldSize == other.fieldSize
        && offset == other.offset && background == other.background && board == other.board
        && stand == other.stand && grid == other.grid && woodGrain == other.woodGrain
        && standWoodGrain == other.standWoodGrain;
}

// 背景・木肌と縁・影・罫線・駒台・星は局面によらず変わらないため、画像にしておいて貼る。
// 影の角丸の塗りや罫線のアンチエイリアスは盤全体の大きさで計算されるので、
// 駒を動かすたびに描き直すと全面の再描画が重くなる。
// 寸法・配色・質感・物理画素に対する位置が変わったときだけ作り直す。
// 盤を回転すると駒台の位置が入れ替わる。回転を戻したときや、拡大して縮小し戻したときに
// 作り直さないよう、1つ前の条件の画像も残しておく。
void ShogiView::drawStaticLayer(QPainter* painter)
{
    // 端数倍率（125% など）では盤の左上が物理画素の途中に来る。画像もその端数だけずらして描き、
    // 物理画素の境目に合わせて貼ることで、直接描いたときと同じ画素にする。
    const QTransform device = painter->deviceTransform();
    const qreal dpr = painter->device()->devicePixelRatioF();
    const QPointF phase(device.dx() - std::floor(device.dx()), device.dy() - std::floor(device.dy()));
    const QRect blackStand = blackStandBoundingRect();
    const QRect whiteStand = whiteStandBoundingRect();
    const StaticLayerKey key{size(), dpr, phase, m_layout.boardSurfaceRect(m_board->files(), m_board->ranks()),
                             blackStand, whiteStand, fieldSize(), QPoint(m_layout.offsetX(), m_layout.offsetY()),
                             m_boardColors.background, m_boardColors.board, m_boardColors.stand,
                             m_boardColors.grid, m_boardVisuals.woodGrain, m_boardVisuals.standWoodGrain};
    if (!m_staticLayer.isNull() && !(m_staticLayerKey == key)) {
        // 今の画像を1つ前の画像として残し、1つ前の画像が条件に合えば今の画像に戻す
        std::swap(m_staticLayer, m_spareStaticLayer);
        std::swap(m_staticLayerKey, m_spareStaticLayerKey);
    }
    if (m_staticLayer.isNull() || !(m_staticLayerKey == key)) {
        // 端数の分だけ右下へずらすため、1物理画素の余白を持たせる。
        QPixmap layer(qCeil(width() * dpr) + 1, qCeil(height() * dpr) + 1);
        layer.setDevicePixelRatio(dpr);
        layer.fill(m_boardColors.background);
        QPainter layerPainter(&layer);
        layerPainter.translate(phase / dpr);
        layerPainter.setRenderHint(QPainter::Antialiasing, false);
        layerPainter.setRenderHint(QPainter::TextAntialiasing, true);
        layerPainter.setRenderHint(QPainter::SmoothPixmapTransform, true);
        drawBackground(&layerPainter);
        drawBoardSurface(&layerPainter);
        drawBoardFields(&layerPainter);
        drawNormalModeStand(&layerPainter);
        drawFourStars(&layerPainter);
        layerPainter.end();
        m_staticLayer = layer;
        m_staticLayerKey = key;
    }
    painter->save();
    painter->setRenderHint(QPainter::SmoothPixmapTransform, false);
    painter->drawPixmap(-phase / dpr, m_staticLayer);
    painter->restore();
}

void ShogiView::paintEvent(QPaintEvent* event)
{
    // 【安全弁】盤未設定、またはエラーフラグが立っている場合は盤を描かず、下地だけを塗る
    // （親の下地を塗らない設定のため、何も描かないと前の内容が残る）。
    if (!m_board || m_errorOccurred) {
        QPainter(this).fillRect(event->rect(), palette().color(QPalette::Window));
        return;
    }

    // 描き直す範囲。各描画ヘルパはこの範囲に掛からない要素を省く。
    m_paintRegion = event->region();

    // 【ペインタ開始】このスコープでのみ QPainter を有効化。
    QPainter painter(this);

    // 【共通描画状態の一括設定】
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    // 【描画順序：背面 → 前面】
    // 0)〜3) 背景・木肌と縁・影・罫線・駒台・星（変わらない部分は画像を貼る）
    drawStaticLayer(&painter);

    // 局面編集中の駒箱
    drawPieceBoxBackground(&painter);

    // 4) ハイライト（選択/移動可能マスなど）
    m_highlighting->drawHighlights(painter, m_paintRegion);

    // 5) 盤上の駒
    drawPieces(&painter);

    // 描画中に致命的な異常が検知された場合はここで打ち切る。
    if (m_errorOccurred) return;

    // 6) 先手/後手の駒台にある「駒」と「枚数」を描画
    drawPiecesStandFeatures(&painter);

    // 6.5) 候補手の矢印（駒台からの始点も駒の上に重ねる）
    m_highlighting->drawArrows(painter, m_layout);

    // 7) 段・筋ラベル（最前面に近いレイヤに載せる）
    drawRanks(&painter);
    drawFiles(&painter);

    // 8) 最前面：ドラッグ中の駒（マウス追従）。盤やラベルより上に重ねる。
    m_interaction.drawDraggingPiece(painter, m_layout, m_pieces, m_boardVisuals);
}

void ShogiView::drawFourStars(QPainter* painter)
{
    // 【状態の局所保護】このブロックでのみ描画状態を変更し、外へ影響させない
    painter->save();

    // 星も罫線と同じ色で描画する。
    painter->setBrush(m_boardColors.grid);
    painter->setPen(Qt::NoPen);  // 縁取りなし

    // 【サイズ/基準点】
    const qreal starRadius = qMax(1.5, fieldSize().width() * 0.05);
    painter->setRenderHint(QPainter::Antialiasing);
    const QSize fs = fieldSize();
    const int basePointX3 = fs.width()  * 3;
    const int basePointX6 = fs.width()  * 6;
    const int basePointY3 = fs.height() * 3;
    const int basePointY6 = fs.height() * 6;

    // 【描画】
    painter->drawEllipse(QPointF(basePointX3 + m_layout.offsetX(), basePointY3 + m_layout.offsetY()), starRadius, starRadius);
    painter->drawEllipse(QPointF(basePointX6 + m_layout.offsetX(), basePointY3 + m_layout.offsetY()), starRadius, starRadius);
    painter->drawEllipse(QPointF(basePointX3 + m_layout.offsetX(), basePointY6 + m_layout.offsetY()), starRadius, starRadius);
    painter->drawEllipse(QPointF(basePointX6 + m_layout.offsetX(), basePointY6 + m_layout.offsetY()), starRadius, starRadius);

    // 【状態復元】
    painter->restore();
}

void ShogiView::drawPiece(QPainter* painter, const int file, const int rank)
{
    // 【盤座標 → ウィジェット座標】（キャッシュ済み矩形を使用）
    const QRect fieldRect = cachedFieldRect(file, rank);
    QRect adjustedRect(fieldRect.left() + m_layout.offsetX(),
                       fieldRect.top()  + m_layout.offsetY(),
                       fieldRect.width(),
                       fieldRect.height());

    // 再描画範囲に掛からないマスは描かない（駒の影の張り出しを含めて判定する）
    const QRect painted = fieldRect.isEmpty() ? QRect()
        : adjustedRect | PiecePainter::paintedRect(adjustedRect, m_boardVisuals).toAlignedRect();
    if (!intersectsPaintRegion(painted)) return;

    // 【盤から駒種を取得】
    const Piece pieceValue = m_board->pieceCharacter(file, rank);

    // 【ドラッグ中の元マスは描かない】
    const bool lifted = m_interaction.dragging() && QPoint(file, rank) == m_interaction.dragFrom();

    // 画面に出た駒を記録する（局面を丸ごと替えたときに、違うマスだけを描き直すため）。
    // 隣のマスを描き直すと、はみ出した影の分だけこのマスも一部が描き直される。記録と同じ駒なら
    // 画面はそのままなので記録を保ち、違う駒のときは一部だけ新しくなったので不明にする。
    const qsizetype index = qsizetype(rank - 1) * m_board->files() + (file - 1);
    if (index < m_shownSquares.size()) {
        std::optional<Piece>& shown = m_shownSquares[index];
        if (lifted) shown.reset();
        else if (paintRegionCovers(painted)) shown = pieceValue;
        else if (shown != pieceValue) shown.reset();
    }

    // 【アイコン描画】
    if (!lifted && pieceValue != Piece::None) {
        const QIcon icon = piece(pieceToChar(pieceValue));
        if (!icon.isNull()) {
            PiecePainter::draw(*painter, icon, adjustedRect, m_boardVisuals);
        }
    }
}

void ShogiView::drawRank(QPainter* painter, const int rank) const
{
    if (!m_board) return;
    const QRect cell = cachedFieldRect(1, rank);
    const int band = m_layout.coordinateBandPx();
    const int x = flipMode() ? boardLeftPx() - band : boardRightPx();
    const QRect label(x, cell.top() + m_layout.offsetY(), band, cell.height());
    if (!intersectsPaintRegion(label)) return;
    painter->save();
    QFont f = painter->font();
    if (KifuPresentation::options().notation == KifuPresentation::Notation::Japanese)
        f = ApplicationFonts::japaneseFont(f);
    f.setPixelSize(qMax(8, qRound(fieldSize().width() * 0.30 * m_layout.rankFontScale())));
    f.setBold(false);
    painter->setFont(f);
    painter->setPen(m_boardColors.grid);
    painter->drawText(label, Qt::AlignCenter, KifuPresentation::rankLabel(rank));
    painter->restore();
}

void ShogiView::drawFile(QPainter* painter, const int file) const
{
    if (!m_board) return;
    const QRect cell = cachedFieldRect(file, 1);
    const int band = m_layout.coordinateBandPx();
    const int y = flipMode() ? m_layout.offsetY() + fieldSize().height() * m_board->ranks()
                            : m_layout.offsetY() - band;
    const QRect label(cell.left() + m_layout.offsetX(), y, cell.width(), band);
    if (!intersectsPaintRegion(label)) return;
    painter->save();
    QFont f = painter->font();
    f.setPixelSize(qMax(8, qRound(fieldSize().width() * 0.30 * m_layout.rankFontScale())));
    f.setBold(false);
    painter->setFont(f);
    painter->setPen(m_boardColors.grid);
    painter->drawText(label, Qt::AlignCenter, QString::number(file));
    painter->restore();
}

// ============================================================
// 駒台の描画
// ============================================================

// 持駒の配置・当たり判定はセルのまま、木肌は駒台全体を通して描く。
// 駒台の木肌は盤面の変わらない部分の画像（drawStaticLayer）に含める。
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
    drawPieceBoxPieces(painter);
}

/**
 * @brief 駒台の内側に駒・重なり・枚数表示を収める（最大3枚表示）。
 */
void ShogiView::drawStandPieceIcon(QPainter* painter, const QRect& adjustedRect, QChar value) const
{
    // 駒・枚数はセル内に切り抜いて描くため、セルが再描画範囲に掛からなければ省く。
    if (!intersectsPaintRegion(adjustedRect)) return;
    const Piece pieceKey = charToPiece(value);
    const bool lifting = m_interaction.dragging() && m_interaction.tempPieceStandCounts().contains(pieceKey);
    const int count = lifting ? m_interaction.tempPieceStandCounts()[pieceKey] : m_board->pieceStandCount(pieceKey);
    // 画面に出た枚数を記録する（持ち上げ中の一時的な枚数は局面と違うので記録しない）
    if (lifting || !paintRegionCovers(adjustedRect)) m_shownStandCounts.remove(pieceKey);
    else m_shownStandCounts.insert(pieceKey, count);
    drawCountedPiece(painter, adjustedRect, piece(value), count);
}

void ShogiView::drawCountedPiece(QPainter* painter, const QRect& adjustedRect,
                                 const QIcon& icon, int count) const
{
    if (count <= 0 || icon.isNull()) return;

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
    const QRect adjustedRect = m_layout.standCellRect(true, fieldRect);
    const QChar value = ShogiViewLayout::standPieceAt(true, file, rank);
    drawStandPieceIcon(painter, adjustedRect, value);
}

// 後手（白）側の駒台セルに対応する「駒アイコン」を描画する。
void ShogiView::drawWhiteStandPiece(QPainter* painter, const int file, const int rank) const
{
    const QRect fieldRect = cachedFieldRect(file, rank);
    const QRect adjustedRect = m_layout.standCellRect(false, fieldRect);
    const QChar value = ShogiViewLayout::standPieceAt(false, file, rank);
    drawStandPieceIcon(painter, adjustedRect, value);
}

// ============================================================
// 駒箱の描画
// ============================================================

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
