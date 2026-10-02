/// @file shogiview_draw.cpp
/// @brief 盤全体の木肌・罫線・駒・座標の描画

#include "shogiview.h"
#include "shogiviewhighlighting.h"
#include "shogiboard.h"
#include "boardsurfacepainter.h"
#include "piecepainter.h"

#include <QColor>
#include <QPainter>
#include <QFont>

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

void ShogiView::paintEvent(QPaintEvent *)
{
    // 【安全弁】盤未設定、またはエラーフラグが立っている場合は描画を行わない。
    if (!m_board || m_errorOccurred) return;

    // 【ペインタ開始】このスコープでのみ QPainter を有効化。
    QPainter painter(this);

    // 【共通描画状態の一括設定】
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    // 【描画順序：背面 → 前面】
    // 0) 背景
    drawBackground(&painter);

    // 1) 余白まで連続する木肌と縁・影
    drawBoardSurface(&painter);

    // 2) 罫線
    drawBoardFields(&painter);

    // 2) 局面編集/通常に応じた周辺（駒台グリッドなどのフィールド）
    drawNormalModeStand(&painter);

    // 3) 盤の星（目印）
    drawFourStars(&painter);

    // 4) ハイライト（選択/移動可能マスなど）
    m_highlighting->drawHighlights(painter, m_layout);

    // 5) 盤上の駒
    drawPieces(&painter);

    // 5.5) 矢印（検討機能の最善手表示）
    m_highlighting->drawArrows(painter, m_layout);

    // 描画中に致命的な異常が検知された場合はここで打ち切る。
    if (m_errorOccurred) return;

    // 6) 先手/後手の駒台にある「駒」と「枚数」を描画
    drawPiecesStandFeatures(&painter);

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
    // 【ドラッグ中の元マスは描かない】
    if (m_interaction.dragging() && file == m_interaction.dragFrom().x() && rank == m_interaction.dragFrom().y()) {
        return;
    }

    // 【盤座標 → ウィジェット座標】（キャッシュ済み矩形を使用）
    const QRect fieldRect = cachedFieldRect(file, rank);
    QRect adjustedRect(fieldRect.left() + m_layout.offsetX(),
                       fieldRect.top()  + m_layout.offsetY(),
                       fieldRect.width(),
                       fieldRect.height());

    // 【盤から駒種を取得】
    Piece pieceValue = m_board->pieceCharacter(file, rank);

    // 【アイコン描画】
    if (pieceValue != Piece::None) {
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
    painter->save();
    QFont f = painter->font();
    f.setPixelSize(qMax(8, qRound(fieldSize().width() * 0.30 * m_layout.rankFontScale())));
    f.setBold(false);
    painter->setFont(f);
    painter->setPen(m_boardColors.grid);
    static const QString ranks = QStringLiteral("一二三四五六七八九");
    if (rank >= 1 && rank <= ranks.size()) painter->drawText(label, Qt::AlignCenter, ranks.mid(rank - 1, 1));
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
    painter->save();
    QFont f = painter->font();
    f.setPixelSize(qMax(8, qRound(fieldSize().width() * 0.30 * m_layout.rankFontScale())));
    f.setBold(false);
    painter->setFont(f);
    painter->setPen(m_boardColors.grid);
    painter->drawText(label, Qt::AlignCenter, QString::number(file));
    painter->restore();
}
