/// @file shogiview_draw.cpp
/// @brief ShogiView の描画ヘルパ（盤面・駒・駒台・ラベル描画）

#include "shogiview.h"
#include "shogiviewhighlighting.h"
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

// 段番号（rank）の描画エントリポイント。
// 役割：
//  - 盤が未設定なら何もしない安全弁
//  - 共通の描画状態（ここでは文字色＝QPalette::WindowText）を一度だけ設定
//  - 1..ranks() を走査して各段の描画を drawRank() に委譲
// 前提：drawRank() は QPainter の永続状態（ペン/ブラシ/変換/クリップ等）を汚さないこと。
//       もし一時的に変更する場合は drawRank() 内部で局所 save()/restore() を使う。
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

// 筋番号（file）の描画エントリポイント。
// 役割：
//  - 盤が未設定なら何もしない安全弁
//  - 共通の描画状態（文字色など）を一度だけ設定
//  - 1..files() を走査して各筋の描画を drawFile() に委譲
// 備考：フォントサイズ等を変更する場合は、ここでまとめて setFont しておくと効率的。
// 前提：drawFile() は QPainter の永続状態を汚さない（必要時のみ局所 save()/restore()）。
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

// E1: 背景を描画する。
// 役割：ウィジェット全体に畳をイメージした背景色を描画し、将棋盤や駒台との調和を図る。
void ShogiView::drawBackground(QPainter* painter)
{
    if (!m_board) return;

    painter->save();

    // まずウィジェット全体をデフォルトの背景色でクリア
    painter->fillRect(rect(), palette().color(QPalette::Window));

    // 畳の基本色（薄い黄緑〜緑がかったベージュ）
    const QColor tatamiBase(200, 190, 130);

    // 将棋盤と駒台を含む領域のみを畳色で描画
    const QSize fs = fieldSize();
    const int boardWidth  = fs.width()  * m_board->files();
    const int boardHeight = fs.height() * m_board->ranks();

    // 駒台の幅（2マス分）
    const int standWidth = fs.width() * 2;

    // 筋番号の帯の高さ
    const int fileLabelHeight = std::max(8, int(m_layout.squareSize() * 0.35));

    // 畳領域の計算（将棋盤 + 駒台 + ギャップ + 余白 + 筋番号領域）
    // 左端: 後手駒台の左端
    const int tatamiLeft = m_layout.offsetX() - m_layout.standGapPx() - standWidth - m_layout.boardMarginPx();
    // 右端: 先手駒台の右端
    const int tatamiRight = m_layout.offsetX() + boardWidth + m_layout.boardMarginPx() + m_layout.standGapPx() + standWidth;
    // 上端: 筋番号の領域を含める（盤の上端 - 余白 - 筋番号帯）
    const int tatamiTop = m_layout.offsetY() - m_layout.boardMarginPx() - fileLabelHeight;
    // 下端: 盤の下端 + 余白 + 筋番号帯（反転時用）
    const int tatamiBottom = m_layout.offsetY() + boardHeight + m_layout.boardMarginPx() + fileLabelHeight;

    // 畳領域の矩形
    const QRect tatamiRect(tatamiLeft, tatamiTop,
                           tatamiRight - tatamiLeft, tatamiBottom - tatamiTop);

    painter->fillRect(tatamiRect, tatamiBase);

    painter->restore();
}

// 将棋盤の影を描画する（立体感を出すため）。
// 役割：将棋盤が畳の上に置かれているような立体感を表現する。
void ShogiView::drawBoardShadow(QPainter* painter)
{
    if (!m_board) return;

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    // 9×9マス部分 + 余白を含めた将棋盤全体の矩形
    const QSize fs = fieldSize();
    const int boardWidth  = fs.width()  * m_board->files();
    const int boardHeight = fs.height() * m_board->ranks();
    const int boardLeft   = m_layout.offsetX() - m_layout.boardMarginPx();
    const int boardTop    = m_layout.offsetY() - m_layout.boardMarginPx();
    const int totalWidth  = boardWidth  + m_layout.boardMarginPx() * 2;
    const int totalHeight = boardHeight + m_layout.boardMarginPx() * 2;

    // 影のオフセットとぼかし幅
    const int shadowOffsetX = 3;
    const int shadowOffsetY = 3;
    const int shadowBlur = 3;

    // 複数の半透明レイヤーで影のぼかし効果を表現（効果控えめ）
    for (int i = shadowBlur; i >= 0; --i) {
        const int alpha = 8 - (i * 2);  // 外側ほど薄く（効果控えめ）
        if (alpha <= 0) continue;

        QColor shadowColor(0, 0, 0, alpha);
        painter->setPen(Qt::NoPen);
        painter->setBrush(shadowColor);

        QRect shadowRect(
            boardLeft + shadowOffsetX - i,
            boardTop + shadowOffsetY - i,
            totalWidth + i * 2,
            totalHeight + i * 2
        );
        painter->drawRect(shadowRect);
    }

    painter->restore();
}

// 駒台の影を描画する（立体感を出すため）。
// 役割：駒台が畳の上に置かれているような立体感を表現する。
void ShogiView::drawStandShadow(QPainter* painter)
{
    if (!m_board) return;

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    // 影のオフセットとぼかし幅
    const int shadowOffsetX = 2;
    const int shadowOffsetY = 2;
    const int shadowBlur = 3;

    // 先手（黒）側駒台の矩形を取得
    const QRect blackStand = blackStandBoundingRect();
    // 後手（白）側駒台の矩形を取得
    const QRect whiteStand = whiteStandBoundingRect();

    // 駒台の影を描画するラムダ（効果控えめ）
    auto drawShadow = [&](const QRect& standRect) {
        if (!standRect.isValid()) return;

        for (int i = shadowBlur; i >= 0; --i) {
            const int alpha = 6 - (i * 2);  // 外側ほど薄く（効果控えめ）
            if (alpha <= 0) continue;

            QColor shadowColor(0, 0, 0, alpha);
            painter->setPen(Qt::NoPen);
            painter->setBrush(shadowColor);

            QRect shadowRect(
                standRect.left() + shadowOffsetX - i,
                standRect.top() + shadowOffsetY - i,
                standRect.width() + i * 2,
                standRect.height() + i * 2
            );
            painter->drawRect(shadowRect);
        }
    };

    drawShadow(blackStand);
    drawShadow(whiteStand);

    painter->restore();
}

// 将棋盤の余白部分（9×9マスの外側の枠）を描画する。
// 役割：実際の将棋盤に近い見た目にするため、盤の周囲に余白を追加する。
// 実際の将棋盤: 縦36.4cm×横33.3cm, 余白各約0.8cm（比率約2.4%）
void ShogiView::drawBoardMargin(QPainter* painter)
{
    if (!m_board) return;
    if (m_layout.boardMarginPx() <= 0) return;

    painter->save();

    // 将棋盤の木目色（盤のマスと同じ色）
    const QColor boardColor(228, 203, 115, 255);

    // 9×9マス部分の矩形
    const QSize fs = fieldSize();
    const int boardWidth  = fs.width()  * m_board->files();
    const int boardHeight = fs.height() * m_board->ranks();
    const int boardLeft   = m_layout.offsetX();
    const int boardTop    = m_layout.offsetY();

    // 余白を含めた将棋盤全体の矩形
    const int marginLeft   = boardLeft   - m_layout.boardMarginPx();
    const int marginTop    = boardTop    - m_layout.boardMarginPx();
    const int marginWidth  = boardWidth  + m_layout.boardMarginPx() * 2;

    // 余白部分を塗りつぶす（4辺）
    painter->setPen(Qt::NoPen);
    painter->setBrush(boardColor);

    // 上辺の余白
    painter->drawRect(QRect(marginLeft, marginTop, marginWidth, m_layout.boardMarginPx()));
    // 下辺の余白
    painter->drawRect(QRect(marginLeft, boardTop + boardHeight, marginWidth, m_layout.boardMarginPx()));
    // 左辺の余白
    painter->drawRect(QRect(marginLeft, boardTop, m_layout.boardMarginPx(), boardHeight));
    // 右辺の余白
    painter->drawRect(QRect(boardLeft + boardWidth, boardTop, m_layout.boardMarginPx(), boardHeight));

    painter->restore();
}

// 盤の各マス（field）を描画するエントリポイント。
// 最適化方針を適用：セルごとの save()/restore() を撤去し、共通状態は外側で一度だけ設定。
void ShogiView::drawBoardFields(QPainter* painter)
{
    // 【安全弁】盤が未設定なら何もしない
    if (!m_board) return;

    // 【共通状態の一括設定】
    painter->setPen(palette().color(QPalette::Dark));

    // 【描画ループ】段（r）× 筋（c）で全マスを走査し、個々の描画は drawField() に委譲
    for (int r = 1; r <= m_board->ranks(); ++r) {
        for (int c = 1; c <= m_board->files(); ++c) {
            drawField(painter, c, r);
        }
    }
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

// 盤上の全ての駒を描画するエントリポイント。
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

// 画面全体の描画エントリポイント（paintEvent）。
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
    // 0) E1: 背景グラデーション（最背面）
    drawBackground(&painter);

    // 0.3) 将棋盤と駒台の影（立体感）を描画
    drawBoardShadow(&painter);
    drawStandShadow(&painter);

    // 0.5) 将棋盤の余白部分（9×9マスの外側）を描画
    drawBoardMargin(&painter);

    // 1) 盤面（マスの背景・枠など）
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
    m_interaction.drawDraggingPiece(painter, m_layout, m_pieces);
}

// 将棋盤の「四隅の星（3,3）（6,3）（3,6）（6,6）」を描画する。
void ShogiView::drawFourStars(QPainter* painter)
{
    // 【状態の局所保護】このブロックでのみ描画状態を変更し、外へ影響させない
    painter->save();

    // 【描画スタイル】星は塗りつぶし円で表現（濃い茶色で視認性確保）
    painter->setBrush(QColor(50, 30, 10));  // 濃い茶色
    painter->setPen(Qt::NoPen);  // 縁取りなし

    // 【サイズ/基準点】
    const int starRadius = 3;
    const QSize fs = fieldSize();
    const int basePointX3 = fs.width()  * 3;
    const int basePointX6 = fs.width()  * 6;
    const int basePointY3 = fs.height() * 3;
    const int basePointY6 = fs.height() * 6;

    // 【描画】
    painter->drawEllipse(QPoint(basePointX3 + m_layout.offsetX(), basePointY3 + m_layout.offsetY()), starRadius, starRadius);
    painter->drawEllipse(QPoint(basePointX6 + m_layout.offsetX(), basePointY3 + m_layout.offsetY()), starRadius, starRadius);
    painter->drawEllipse(QPoint(basePointX3 + m_layout.offsetX(), basePointY6 + m_layout.offsetY()), starRadius, starRadius);
    painter->drawEllipse(QPoint(basePointX6 + m_layout.offsetX(), basePointY6 + m_layout.offsetY()), starRadius, starRadius);

    // 【状態復元】
    painter->restore();
}

// 指定された (file, rank) のマス（1 マス分の矩形）を描画する。
void ShogiView::drawField(QPainter* painter, const int file, const int rank) const
{
    // 【盤座標 → ウィジェット座標】（キャッシュ済み矩形を使用）
    const QRect fieldRect = cachedFieldRect(file, rank);
    QRect adjustedRect(fieldRect.left() + m_layout.offsetX(),
                       fieldRect.top()  + m_layout.offsetY(),
                       fieldRect.width(),
                       fieldRect.height());

    painter->save();

    // マスの塗り（落ち着いた木目色）
    const QColor fillColor(228, 203, 115, 255);
    painter->setBrush(fillColor);

    // マスの枠線色（濃い茶色）
    QPen gridPen(QColor(80, 60, 30));
    gridPen.setWidth(1);
    painter->setPen(gridPen);

    painter->drawRect(adjustedRect);

    painter->restore();
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

// 指定された (file, rank) のマスに存在する「駒」を描画する。
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
    Piece pieceValue = m_board->getPieceCharacter(file, rank);

    // 【アイコン描画】
    if (pieceValue != Piece::None) {
        const QIcon icon = piece(pieceToChar(pieceValue));
        if (!icon.isNull()) {
            icon.paint(painter, adjustedRect, Qt::AlignCenter);
        }
    }
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

// 指定段（rank）に対応する「段ラベル（漢数字）」を描画する。
void ShogiView::drawRank(QPainter* painter, const int rank) const
{
    if (!m_board) return;

    // (1) 指定段の基準セル（file=1）を取り、Y と高さを決める
    const QRect cell = cachedFieldRect(1, rank);
    const int h = cell.height();
    const int y = cell.top() + m_layout.offsetY();

    // (2) 反転していなければ右側に段ラベル、反転時は左側に段ラベル
    const bool rightSide = !m_layout.flipMode();

    const int boardEdge  = rightSide ? boardRightPx() : boardLeftPx();
    const int innerEdge  = standInnerEdgePx(rightSide);

    // (3) その中点を段ラベル帯の中心 X とする
    const int xCenter    = (boardEdge + innerEdge) / 2;

    // (4) ラベル帯の幅を決定（隙間が狭い場合はクリップ）
    int w = m_layout.labelBandPx();
    const int gapPx = std::abs(innerEdge - boardEdge);
    if (w > gapPx - 2) w = std::max(12, gapPx - 2);

    const QRect rankRect(xCenter - w/2, y, w, h);

    // (5) フォントを段ラベル用に調整（局所的に保存/復元）
    painter->save();
    QFont f = painter->font();
    double pt = m_layout.labelFontPt() * m_layout.rankFontScale() * 1.3;
    pt = std::min(pt, h * 0.95);
    f.setPointSizeF(pt);
    f.setBold(true);
    painter->setFont(f);
    painter->setPen(QColor(40, 30, 20));

    // (6) 1..9 段に対応する漢数字を中央揃えで描画
    static const QStringList rankTexts = { "一","二","三","四","五","六","七","八","九" };
    if (rank >= 1 && rank <= rankTexts.size()) {
        painter->drawText(rankRect, Qt::AlignCenter, rankTexts.at(rank - 1));
    }
    painter->restore();
}

// 指定筋（file）に対応する「筋ラベル（全角数字）」を描画する。
void ShogiView::drawFile(QPainter* painter, const int file) const
{
    if (!m_board) return;

    // (1) 指定筋の基準セル（rank=1）から X と幅を算出
    const QRect cell = cachedFieldRect(file, 1);
    const int x = cell.left() + m_layout.offsetX();
    const int w = cell.width();

    const int h = std::max(8, int(m_layout.squareSize() * 0.35));
    int y = 0;

    // (2) 配置先の決定（反転時は下側、非反転時は上側）
    if (m_layout.flipMode()) {
        const QSize fs = fieldSize();
        const int boardBottom = m_layout.offsetY() + fs.height() * m_board->ranks();
        y = boardBottom + m_layout.labelGapPx();
    } else {
        y = m_layout.offsetY() - m_layout.labelGapPx() - h;
        if (y < 0) y = 0;
    }

    const QRect fileRect(x, y, w, h);

    // (3) フォント調整（局所保護）
    painter->save();
    QFont f = painter->font();
    double pt = m_layout.labelFontPt() * m_layout.rankFontScale() * 1.3;
    pt = std::min(pt, w * 0.95);
    f.setPointSizeF(pt);
    f.setBold(true);
    painter->setFont(f);
    painter->setPen(QColor(40, 30, 20));

    // (4) 全角数字 １..９ を中央描画
    static const QStringList fileTexts = { "１","２","３","４","５","６","７","８","９" };
    if (file >= 1 && file <= fileTexts.size())
        painter->drawText(fileRect, Qt::AlignHCenter | Qt::AlignVCenter, fileTexts.at(file - 1));

    painter->restore();
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
