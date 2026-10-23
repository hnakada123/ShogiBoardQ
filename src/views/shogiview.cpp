/// @file shogiview.cpp
/// @brief 将棋盤ビュー（盤・駒台・駒箱の描画と入力、マスの位置）

#include "shogiview.h"

#include "movefont.h"
#include "appsettings.h"
#include "boardappearance.h"
#include "boardconstants.h"
#include "boardcoordinates.h"
#include "boardsurfacepainter.h"
#include "kifupresentation.h"
#include "logcategories.h"
#include "pieceimageprovider.h"
#include "piecepainter.h"
#include "shogiboard.h"
#include "shogiviewhighlighting.h"

#include <QCursor>
#include <QFont>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QSizePolicy>
#include <QWheelEvent>
#include <QtMath>

#include <algorithm>

// コンストラクタ
ShogiView::ShogiView(QWidget *parent)
    : QWidget(parent),
    m_highlighting(new ShogiViewHighlighting(this, this)),
    m_chrome(new BoardChromeWidget(this))
{
    m_boardColors = BoardAppearance::instance().colors();
    m_boardVisuals = BoardAppearance::instance().visuals();
    connect(&BoardAppearance::instance(), &BoardAppearance::colorsChanged,
            this, &ShogiView::refreshBoardColors);
    connect(&BoardAppearance::instance(), &BoardAppearance::visualsChanged,
            this, &ShogiView::refreshBoardVisuals);

    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    int sq = AppSettings::squareSize();
    if (sq < BoardGeometry::kMinSquareSize || sq > BoardGeometry::kMaxSquareSize)
        sq = AppSettings::squareSize.defaultValue();
    rebuildGeometry(sq, false);

    setMouseTracking(true);

    // 盤面は背景の画像で自分の範囲をすべて塗るため、描き直しのたびに親の下地を塗らせない。
    setAttribute(Qt::WA_OpaquePaintEvent);

    // 対局者カード・時計・手番の札は盤に重ねた BoardChromeWidget が表示する
    connect(m_chrome, &BoardChromeWidget::pieceBoxSideClicked, this, &ShogiView::setPieceBoxSide);
    relayoutChrome();

    connect(&PieceImageProvider::instance(), &PieceImageProvider::styleChanged,
            this, &ShogiView::refreshPieceImages);
    refreshPieceImages();
}

// ─────────────────────────────────────────────────────────────────────────────
// ボード接続
// ─────────────────────────────────────────────────────────────────────────────

void ShogiView::setBoard(ShogiBoard* board)
{
    qCDebug(lcView) << "setBoard called, old:" << m_board << "new:" << board;

    if (m_board == board) {
        qCDebug(lcView) << "setBoard: same board, skipping";
        return;
    }

    if (m_board) {
        QObject::disconnect(m_board, nullptr, this, nullptr);
    }

    m_board = board;
    rebuildGeometry(squareSize(), flipMode());
    // 新しい盤は、まだ画面に出ていないものとして扱う
    m_shownSquares = QList<std::optional<Piece>>(board ? board->files() * board->ranks() : 0);
    m_shownStandCounts.clear();

    if (board) {
        connect(board, &ShogiBoard::dataChanged, this, &ShogiView::onBoardSquareChanged);
        connect(board, &ShogiBoard::standChanged, this, &ShogiView::onBoardStandChanged);
        connect(board, &ShogiBoard::boardReset,  this, &ShogiView::onBoardReset);
        connect(board, &ShogiBoard::dataChanged, this, &ShogiView::positionChanged);
        connect(board, &ShogiBoard::boardReset, this, &ShogiView::positionChanged);
        // 対局者カードの手番の印は、盤の手番をそのまま示す
        connect(board, &ShogiBoard::turnChanged, m_chrome, &BoardChromeWidget::setActiveSide);
        m_chrome->setActiveSide(board->currentPlayer());
    }

    updateGeometry();

    qCDebug(lcView) << "setBoard complete, m_board now:" << m_board;
    relayoutChrome();
    emit positionChanged();
}

// ─────────────────────────────────────────────────────────────────────────────
// ゲッター
// ─────────────────────────────────────────────────────────────────────────────

ShogiBoard* ShogiView::board() const
{
    return m_board;
}

bool ShogiView::positionEditMode() const { return m_interaction.positionEditMode(); }

ShogiViewHighlighting* ShogiView::highlighting() const { return m_highlighting; }

// ─────────────────────────────────────────────────────────────────────────────
// サイズ系
// ─────────────────────────────────────────────────────────────────────────────

QSize ShogiView::sizeHint() const
{
    if (!m_board) {
        return QSize(100, 100);
    }
    return m_geometry.viewSize();
}

QSize ShogiView::minimumSizeHint() const
{
    BoardGeometry::Params params = m_geometry.params();
    params.squareSize = BoardGeometry::kMinSquareSize;
    return BoardGeometry(params).viewSize();
}

// ─────────────────────────────────────────────────────────────────────────────
// 盤の位置
// ─────────────────────────────────────────────────────────────────────────────

void ShogiView::rebuildGeometry(int squareSize, bool flipped)
{
    BoardGeometry::Params params;
    params.squareSize = squareSize;
    params.flipped = flipped;
    params.files = m_board ? m_board->files() : 9;
    params.ranks = m_board ? m_board->ranks() : 9;
    params.font = font();
    m_geometry = BoardGeometry(params);
    relayoutChrome();
}

void ShogiView::applyBoardScaleChange(int squareSize)
{
    rebuildGeometry(squareSize, flipMode());
    updateGeometry();
    update();
}

void ShogiView::zoomTo(int size)
{
    if (size < BoardGeometry::kMinSquareSize || size > BoardGeometry::kMaxSquareSize || size == squareSize()) return;
    // ドラッグ中の駒の位置はマスの大きさで決まるので放す
    endDrag();
    applyBoardScaleChange(size);
    emit fieldSizeChanged(fieldSize());
}

// ─────────────────────────────────────────────────────────────────────────────
// 操作/状態切替
// ─────────────────────────────────────────────────────────────────────────────

void ShogiView::setSquareSize(int size)
{
    if (size < BoardGeometry::kMinSquareSize || size > BoardGeometry::kMaxSquareSize) {
        return;
    }
    applyBoardScaleChange(size);
}

void ShogiView::enlargeBoard()
{
    zoomTo(squareSize() + 1);
}

void ShogiView::reduceBoard()
{
    zoomTo(squareSize() - 1);
}

void ShogiView::setPositionEditMode(bool positionEditMode)
{
    if (m_interaction.positionEditMode() == positionEditMode) return;

    m_interaction.setPositionEditMode(positionEditMode);
    relayoutChrome();
    update();
}

void ShogiView::setFlipMode(bool newFlipMode)
{
    if (flipMode() == newFlipMode) return;

    // 名前の向きのマークとカードの上下も入れ替わる
    rebuildGeometry(squareSize(), newFlipMode);
    refreshPieceImages();
    update();
}

QImage ShogiView::toImage(qreal scale)
{
    QPixmap pm = this->grab(rect());
    QImage img = pm.toImage();
    if (!qFuzzyCompare(scale, 1.0)) {
        img = img.scaled(img.size() * scale,
                         Qt::IgnoreAspectRatio,
                         Qt::SmoothTransformation);
    }
    return img;
}

void ShogiView::applyBoardAndRender(ShogiBoard* board)
{
    if (!board) return;

    // 棋譜をたどるたび・エンジンが指すたびに呼ばれる。局面の変化は ShogiBoard の通知で
    // 変わったマスだけを描き直すので、盤そのもの・マスの大きさが変わったときだけ全体を描き直す
    // （駒の画像が変わったときは loadPieceImages が描き直す）。
    loadPieceImages(flipMode());

    const bool boardChanged = m_board != board;
    setBoard(board);
    if (boardChanged) update();
}

void ShogiView::configureFixedSizing(int squarePx)
{
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    const int s = (squarePx > 0) ? squarePx : squareSize();
    applyBoardScaleChange(std::clamp(s, BoardGeometry::kMinSquareSize, BoardGeometry::kMaxSquareSize));
}

// ============================================================
// 表示状態（矢印・盤の色・対局者名と時計の表示）
// ============================================================

void ShogiView::refreshBoardVisuals()
{
    m_boardVisuals = BoardAppearance::instance().visuals();
    update();
}

void ShogiView::refreshBoardColors()
{
    // 対局者カードの配色は BoardChromeWidget が自分で配色の変更を受け取る
    m_boardColors = BoardAppearance::instance().colors();
    update();
}

void ShogiView::setArrows(const QList<Arrow>& arrows)
{
    m_highlighting->setArrows(arrows);
}

const QList<ShogiView::Arrow>& ShogiView::arrows() const
{
    return m_highlighting->arrows();
}

void ShogiView::clearArrows()
{
    m_highlighting->clearArrows();
}

// ─────────────────────────────────────────────────────────────────────────────
// 対局者カードの配置
// ─────────────────────────────────────────────────────────────────────────────

void ShogiView::relayoutChrome()
{
    m_chrome->setGeometry(rect());
    m_chrome->setBoardGeometry(m_board ? std::optional<BoardGeometry>(m_geometry) : std::nullopt);
    // 駒箱はカードの下に置くので、カードを置いてから切り替えボタンを置く
    relayoutPieceBoxSideSelector();
}

void ShogiView::setEditExitButtonVisible(bool visible)
{
    if (visible) relayoutChrome();
    m_chrome->setEditExitButtonVisible(visible);
}

void ShogiView::setNameFontScale(double scale)
{
    m_chrome->setNameFontScale(scale);
    relayoutPieceBoxSideSelector();
}

// ============================================================
// マウス・キーの入力
// ============================================================

// ─────────────────────────────────────────────────────────────────────────────
// 入力座標変換
// ─────────────────────────────────────────────────────────────────────────────

QPoint ShogiView::clickedSquare(const QPoint &clickPosition) const
{
    if (!m_board) return {};

    // 駒箱（局面編集中だけ表示）。ドラッグ中はどのセルに落としても駒の種類の位置へ戻す
    const QRect box = pieceBoxRect();
    if (box.contains(clickPosition)) {
        if (BoardGeometry::pieceBoxSideSelector(box).contains(clickPosition)) return {};
        for (int rank = 1; rank <= 8; ++rank) {
            const bool matches = m_interaction.dragging()
                ? m_board->pieceCharacter(BoardConstants::kPieceBoxFile, rank)
                      == toBlack(demote(m_interaction.dragPiece()))
                : pieceBoxCellRect(rank).contains(clickPosition);
            if (matches) return QPoint(BoardConstants::kPieceBoxFile, rank);
        }
        return {};
    }

    const QPoint square = m_geometry.squareAt(clickPosition);
    if (!square.isNull()) return square;

    for (const Turn side : {Turn::Black, Turn::White}) {
        if (!m_geometry.stand(side).contains(clickPosition)) continue;
        // 局面編集中のドラッグは、駒台に落とせば駒の種類で置き場所が決まる
        if (positionEditMode() && m_interaction.dragging()) {
            const Piece dragged = demote(m_interaction.dragPiece());
            if (dragged == Piece::None) return {};
            return BoardCoordinates::standSquareOf(side == Turn::Black ? toBlack(dragged) : toWhite(dragged));
        }
        for (const Piece piece : BoardGeometry::standPieces(side)) {
            if (m_geometry.standCell(piece).contains(clickPosition)) return BoardCoordinates::standSquareOf(piece);
        }
    }
    return {};
}

// ─────────────────────────────────────────────────────────────────────────────
// マウス・キーイベント
// ─────────────────────────────────────────────────────────────────────────────

void ShogiView::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_interaction.dragging() && event->button() == Qt::RightButton) {
        endDrag();
        QPoint pt = clickedSquare(event->pos());
        emit rightClicked(pt);
        return;
    }

    if (event->button() == Qt::LeftButton) {
        QPoint pt = clickedSquare(event->pos());

        if (pt.isNull()) return;
        emit clicked(pt);
    }
    else if (event->button() == Qt::RightButton) {
        QPoint pt = clickedSquare(event->pos());
        if (pt.isNull()) return;
        emit rightClicked(pt);
    }
}

void ShogiView::mouseMoveEvent(QMouseEvent* event)
{
    if (m_interaction.dragging()) {
        // 盤全体ではなく、駒の移動前と移動後の範囲だけを描き直す。
        const QRect before = m_interaction.draggingPieceRect(fieldSize(), m_boardVisuals);
        m_interaction.updateDragPos(event->pos());
        update(before);
        update(m_interaction.draggingPieceRect(fieldSize(), m_boardVisuals));
    }
    QWidget::mouseMoveEvent(event);
}

void ShogiView::resizeEvent(QResizeEvent* e)
{
    QWidget::resizeEvent(e);
    // 盤に重ねた対局者カードの層を同じ大きさにして置き直す
    relayoutChrome();
}

void ShogiView::wheelEvent(QWheelEvent* e)
{
    if (e->modifiers() & Qt::ControlModifier) {
        const int delta = e->angleDelta().y();
        if (delta > 0) {
            enlargeBoard();
        } else if (delta < 0) {
            reduceBoard();
        }
        e->accept();
        return;
    }

    QWidget::wheelEvent(e);
}

// ─────────────────────────────────────────────────────────────────────────────
// ドラッグ操作
// ─────────────────────────────────────────────────────────────────────────────

void ShogiView::startDrag(const QPoint &from)
{
    m_interaction.startDrag(from, m_board, mapFromGlobal(QCursor::pos()));
    // つまんだマス（駒が消える・持駒の枚数が減る）と持ち上げた駒だけを描き直す。
    update(fieldPaintRect(from));
    update(m_interaction.draggingPieceRect(fieldSize(), m_boardVisuals));
}

void ShogiView::endDrag()
{
    if (!m_interaction.dragging()) return;
    // 持ち上げた駒とつまんだマス（駒台・駒箱の枚数を含む）だけを描き直す。
    // 着手による盤・駒台の変化は ShogiBoard の通知（dataChanged / standChanged）で描き直す。
    const QRect lifted = m_interaction.draggingPieceRect(fieldSize(), m_boardVisuals);
    const QPoint from = m_interaction.dragFrom();
    m_interaction.endDrag();
    update(lifted);
    update(fieldPaintRect(from));
}

// ─────────────────────────────────────────────────────────────────────────────
// 駒画像管理
// ─────────────────────────────────────────────────────────────────────────────

QIcon ShogiView::piece(Piece piece) const
{
    return m_pieces.value(piece, QIcon());
}

void ShogiView::refreshPieceImages()
{
    loadPieceImages(flipMode());
}

void ShogiView::loadPieceImages(bool flipped)
{
    auto& provider = PieceImageProvider::instance();
    bool changed = false;
    for (int code = 0; code < 2 * 16; ++code) {
        if ((code & PieceCode::kKindMask) >= kPieceKindCount) continue;
        const Piece piece = static_cast<Piece>(code);
        const QIcon icon = provider.icon(piece, flipped);
        const auto current = m_pieces.constFind(piece);
        if (current != m_pieces.cend() && current->cacheKey() == icon.cacheKey()) continue;
        m_pieces.insert(piece, icon);
        changed = true;
    }
    // 画像が同じなら描き直さない（盤を読み込み直すたびに呼ばれるため）
    if (changed) update();
}

// ============================================================
// マスと駒台・駒箱のセルの位置
// ============================================================

QRect ShogiView::fieldRect(const QPoint& field) const
{
    if (!m_board) return {};
    const int file = field.x();
    const int rank = field.y();
    if (file == BoardConstants::kPieceBoxFile) return pieceBoxCellRect(rank);

    if (file == BoardConstants::kBlackStandFile || file == BoardConstants::kWhiteStandFile)
        return standPieceRect(BoardCoordinates::standPieceAt(file, rank));

    return m_geometry.square(file, rank);
}

QRect ShogiView::fieldPaintRect(const QPoint& field) const
{
    const QRect cell = fieldRect(field);
    // 駒台・駒箱の駒と枚数はセル内に切り抜いて描く。盤上の駒は影の分だけマスからはみ出す。
    if (cell.isEmpty() || field.x() > m_board->files()) return cell;
    return cell | PiecePainter::paintedRect(cell, m_boardVisuals).toAlignedRect().adjusted(-1, -1, 1, 1);
}

void ShogiView::onBoardSquareChanged(int file, int rank)
{
    update(fieldPaintRect(QPoint(file, rank)));
    // 駒箱の枚数は盤と駒台の駒から数えるため、局面編集中は一緒に描き直す
    update(pieceBoxRect());
}

void ShogiView::onBoardStandChanged(Piece piece)
{
    const QRect cell = standPieceRect(piece);
    if (cell.isEmpty()) {
        update(m_geometry.stand(Turn::Black));
        update(m_geometry.stand(Turn::White));
    } else {
        update(cell);
    }
    update(pieceBoxRect());
}

void ShogiView::onBoardReset()
{
    if (!m_board) return;
    m_chrome->setActiveSide(m_board->currentPlayer());

    const int files = m_board->files();
    if (m_shownSquares.size() != qsizetype(files) * m_board->ranks()) {
        update();
        return;
    }
    for (int rank = 1; rank <= m_board->ranks(); ++rank) {
        for (int file = 1; file <= files; ++file) {
            const std::optional<Piece>& shown = m_shownSquares.at(qsizetype(rank - 1) * files + (file - 1));
            if (!shown || *shown != m_board->pieceCharacter(file, rank)) update(fieldPaintRect(QPoint(file, rank)));
        }
    }
    for (const Turn side : {Turn::Black, Turn::White}) {
        for (const Piece piece : BoardGeometry::standPieces(side)) {
            const auto shown = m_shownStandCounts.constFind(piece);
            if (shown == m_shownStandCounts.cend() || shown.value() != m_board->pieceStandCount(piece))
                update(standPieceRect(piece));
        }
    }
    update(pieceBoxRect());
}


QRect ShogiView::pieceBoxRect() const
{
    if (!positionEditMode() || !m_board) return {};
    return m_geometry.pieceBox(m_chrome->cardHeight(), height());
}

QRect ShogiView::pieceBoxCellRect(int rank) const
{
    return BoardGeometry::pieceBoxCell(pieceBoxRect(), rank);
}

void ShogiView::setPieceBoxSide(Turn side)
{
    m_interaction.setPieceBoxSide(side);
    m_chrome->setPieceBoxSide(side);
    update();
}

void ShogiView::relayoutPieceBoxSideSelector()
{
    m_chrome->placePieceBoxSelector(BoardGeometry::pieceBoxSideSelector(pieceBoxRect()));
    m_chrome->setPieceBoxSide(pieceBoxSide());
}

// ============================================================
// 描画
// ============================================================

// 段・筋の番号（罫線と同じ色）
void ShogiView::drawRanksAndFiles(QPainter* painter) const
{
    painter->save();
    QFont font = painter->font();
    font.setPixelSize(m_geometry.labelPixelSize());
    font.setBold(false);
    const QFont rankFont = MoveFont::font(font);   // 段の漢数字は指し手と同じ書体
    painter->setPen(m_boardColors.grid);
    painter->setFont(rankFont);
    for (int rank = 1; rank <= m_board->ranks(); ++rank) {
        const QRect label = m_geometry.rankLabel(rank);
        if (intersectsPaintRegion(label)) painter->drawText(label, Qt::AlignCenter, KifuPresentation::rankLabel(rank));
    }
    painter->setFont(font);
    for (int file = 1; file <= m_board->files(); ++file) {
        const QRect label = m_geometry.fileLabel(file);
        if (intersectsPaintRegion(label)) painter->drawText(label, Qt::AlignCenter, QString::number(file));
    }
    painter->restore();
}

void ShogiView::drawPieces(QPainter* painter)
{
    // 段（r）を降順、筋（c）を昇順に走査し、各マスの駒を描画
    for (int r = m_board->ranks(); r > 0; --r) {
        for (int c = 1; c <= m_board->files(); ++c) {
            drawPiece(painter, c, r);
        }
    }
}

bool ShogiView::paintRegionCovers(const QRect& rect) const
{
    if (m_paintRegion.rectCount() == 1) return m_paintRegion.boundingRect().contains(rect);
    return QRegion(rect).subtracted(m_paintRegion).isEmpty();
}

void ShogiView::paintEvent(QPaintEvent* event)
{
    // 【安全弁】盤未設定の場合は盤を描かず、下地だけを塗る
    // （親の下地を塗らない設定のため、何も描かないと前の内容が残る）。
    if (!m_board) {
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
    m_staticLayer.draw(painter, size(), m_geometry, m_boardColors, m_boardVisuals);

    // 局面編集中の駒箱
    drawPieceBoxBackground(&painter);

    // 4) ハイライト（選択/移動可能マスなど）
    m_highlighting->drawHighlights(painter, m_paintRegion);

    // 5) 盤上の駒
    drawPieces(&painter);

    // 6) 先手/後手の駒台にある「駒」と「枚数」を描画
    drawStandPieces(&painter);
    drawPieceBoxPieces(&painter);

    // 6.5) 候補手の矢印（駒台からの始点も駒の上に重ねる）
    m_highlighting->drawArrows(painter);

    // 7) 段・筋ラベル（最前面に近いレイヤに載せる）
    drawRanksAndFiles(&painter);

    // 8) 最前面：ドラッグ中の駒（マウス追従）。盤やラベルより上に重ねる。
    m_interaction.drawDraggingPiece(painter, fieldSize(), piece(m_interaction.dragPiece()), m_boardVisuals);
}

void ShogiView::drawPiece(QPainter* painter, const int file, const int rank)
{
    const QRect adjustedRect = m_geometry.square(file, rank);

    // 再描画範囲に掛からないマスは描かない（駒の影の張り出しを含めて判定する）
    const QRect painted = adjustedRect.isEmpty() ? QRect()
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
        const QIcon icon = piece(pieceValue);
        if (!icon.isNull()) {
            PiecePainter::draw(*painter, icon, adjustedRect, m_boardVisuals);
        }
    }
}

// ============================================================
// 駒台の描画
// ============================================================

// 駒台のセルごとに、持駒と枚数を描く。
// 駒台の木肌は盤面の変わらない部分の画像（BoardStaticLayer）に含める。
void ShogiView::drawStandPieces(QPainter* painter)
{
    for (const Turn side : {Turn::Black, Turn::White}) {
        for (const Piece piece : BoardGeometry::standPieces(side))
            drawStandPieceIcon(painter, standPieceRect(piece), piece);
    }
}

/**
 * @brief 駒台の内側に駒・重なり・枚数表示を収める（最大3枚表示）。
 */
void ShogiView::drawStandPieceIcon(QPainter* painter, const QRect& adjustedRect, Piece pieceKey) const
{
    // 駒・枚数はセル内に切り抜いて描くため、セルが再描画範囲に掛からなければ省く。
    if (!intersectsPaintRegion(adjustedRect)) return;
    const bool lifting = m_interaction.dragging() && m_interaction.tempPieceStandCounts().contains(pieceKey);
    const int count = lifting ? m_interaction.tempPieceStandCounts()[pieceKey] : m_board->pieceStandCount(pieceKey);
    // 画面に出た枚数を記録する（持ち上げ中の一時的な枚数は局面と違うので記録しない）
    if (lifting || !paintRegionCovers(adjustedRect)) m_shownStandCounts.remove(pieceKey);
    else m_shownStandCounts.insert(pieceKey, count);
    PiecePainter::drawStack(*painter, piece(pieceKey), count, adjustedRect.width(),
                            m_geometry.standPieceArea(adjustedRect), m_boardVisuals, m_boardColors);
}

// ============================================================
// 駒箱の描画
// ============================================================

void ShogiView::drawPieceBoxBackground(QPainter* painter)
{
    const QRect box = pieceBoxRect();
    if (box.isEmpty()) return;
    // 駒箱は局面編集中だけ表示し、ラベルの文字も含むため、変わらない部分の画像には含めない。
    const QRectF surface = m_geometry.standSurface(box);
    const QMarginsF shadow = BoardSurfacePainter::shadowMargins(fieldSize().width());
    if (!intersectsPaintRegion(surface.marginsAdded(shadow).adjusted(-1, -1, 1, 1))) return;
    BoardSurfacePainter::draw(*painter, surface, m_boardColors.stand, m_boardVisuals.standWoodGrain,
                              fieldSize().width());
    painter->save();
    const QRect label = BoardGeometry::pieceBoxLabel(box);
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
        const QIcon icon = piece(displayPiece);
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
