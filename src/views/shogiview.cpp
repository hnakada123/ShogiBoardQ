/// @file shogiview.cpp
/// @brief 将棋盤ビュー（盤・駒台・駒箱の描画と入力、マスの位置）

#include "shogiview.h"

#include "applicationfonts.h"
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
    if (sq < 20 || sq > 150) sq = AppSettings::squareSize.defaultValue();
    m_layout.setSquareSize(sq);

    m_layout.setStandGapCols(0.7);
    recalcLayoutParams();

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
    invalidateFieldRectCache();
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

QSize ShogiView::fieldSize() const
{
    return m_layout.fieldSize();
}

bool ShogiView::positionEditMode() const { return m_interaction.positionEditMode(); }
int  ShogiView::squareSize() const { return m_layout.squareSize(); }
bool ShogiView::flipMode() const { return m_layout.flipMode(); }

ShogiViewHighlighting* ShogiView::highlighting() const { return m_highlighting; }

// ─────────────────────────────────────────────────────────────────────────────
// サイズ系
// ─────────────────────────────────────────────────────────────────────────────

void ShogiView::setFieldSize(QSize fieldSize)
{
    if (m_layout.fieldSize() == fieldSize) {
        return;
    }

    m_layout.setFieldSize(fieldSize);
    invalidateFieldRectCache();

    emit fieldSizeChanged(fieldSize);

    updateGeometry();

    relayoutChrome();
}

QSize ShogiView::sizeHint() const
{
    if (!m_board) {
        return QSize(100, 100);
    }

    return m_layout.viewSize(m_board->files(), m_board->ranks());
}

QSize ShogiView::minimumSizeHint() const
{
    ShogiViewLayout minimumLayout = m_layout;
    minimumLayout.setSquareSize(20);
    minimumLayout.recalcLayoutParams(font());
    return minimumLayout.viewSize(m_board ? m_board->files() : 9, m_board ? m_board->ranks() : 9);
}

// ─────────────────────────────────────────────────────────────────────────────
// 座標・矩形計算
// ─────────────────────────────────────────────────────────────────────────────

QRect ShogiView::calculateSquareRectangleBasedOnBoardState(const int file, const int rank) const
{
    if (!m_board) return QRect();
    return m_layout.calculateSquareRectangleBasedOnBoardState(
        file, rank, m_board->files(), m_board->ranks());
}

void ShogiView::invalidateFieldRectCache()
{
    m_fieldRectCacheValid = false;
}

QRect ShogiView::cachedFieldRect(const int file, const int rank) const
{
    if (!m_board) return QRect();

    if (!m_fieldRectCacheValid) {
        m_fieldRectCache.clear();
        const int files = m_board->files();
        const int ranks = m_board->ranks();
        for (int f = 1; f <= files; ++f) {
            for (int r = 1; r <= ranks; ++r) {
                const auto key = static_cast<quint64>(f) << 8 | static_cast<quint64>(r);
                m_fieldRectCache.insert(key, m_layout.calculateSquareRectangleBasedOnBoardState(
                                            f, r, files, ranks));
            }
        }
        m_fieldRectCacheValid = true;
    }

    const auto key = static_cast<quint64>(file) << 8 | static_cast<quint64>(rank);
    return m_fieldRectCache.value(key);
}

int ShogiView::boardRightPx() const {
    const int files = m_board ? m_board->files() : 9;
    return m_layout.boardRightPx(files);
}

QRect ShogiView::blackStandBoundingRect() const
{
    if (!m_board) return {};
    return m_layout.blackStandBoundingRect(m_board->files(), m_board->ranks());
}

QRect ShogiView::whiteStandBoundingRect() const
{
    if (!m_board) return {};
    return m_layout.whiteStandBoundingRect(m_board->files(), m_board->ranks());
}

void ShogiView::recalcLayoutParams()
{
    m_layout.recalcLayoutParams(font());
    invalidateFieldRectCache();
    relayoutChrome();
}

void ShogiView::applyBoardScaleChange(bool emitSignal)
{
    invalidateFieldRectCache();
    recalcLayoutParams();
    updateGeometry();

    if (emitSignal) {
        emit fieldSizeChanged(m_layout.fieldSize());
    }
    update();
}

// ─────────────────────────────────────────────────────────────────────────────
// 操作/状態切替
// ─────────────────────────────────────────────────────────────────────────────

void ShogiView::setSquareSize(int size)
{
    if (size < 20 || size > 150) {
        return;
    }
    m_layout.setSquareSize(size);
    applyBoardScaleChange(false);
}

void ShogiView::enlargeBoard(bool emitSignal)
{
    if (m_layout.squareSize() >= 150) {
        return;
    }

    m_layout.setSquareSize(m_layout.squareSize() + 1);
    applyBoardScaleChange(emitSignal);
}

void ShogiView::reduceBoard(bool emitSignal)
{
    if (m_layout.squareSize() <= 20) {
        return;
    }

    m_layout.setSquareSize(m_layout.squareSize() - 1);
    applyBoardScaleChange(emitSignal);
}

void ShogiView::setPositionEditMode(bool positionEditMode)
{
    if (m_interaction.positionEditMode() == positionEditMode) return;

    m_interaction.setPositionEditMode(positionEditMode);
    relayoutChrome();
    update();
}

void ShogiView::setStandGapCols(double cols)
{
    m_layout.setStandGapCols(cols);
    recalcLayoutParams();
    updateGeometry();
    update();
}

void ShogiView::setFlipMode(bool newFlipMode)
{
    if (m_layout.flipMode() == newFlipMode) return;

    m_layout.setFlipMode(newFlipMode);
    invalidateFieldRectCache();
    refreshPieceImages();
    // 名前の向きのマークとカードの上下も入れ替わる
    relayoutChrome();
    update();
}

void ShogiView::updateBoardSize()
{
    setFieldSize(QSize(m_layout.squareSize(), qRound(m_layout.squareSize() * ShogiViewLayout::kSquareAspectRatio)));
    updateGeometry();
    relayoutChrome();
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
    loadPieceImages(m_layout.flipMode());

    const bool boardChanged = m_board != board;
    setBoard(board);

    const QSize field(squareSize(), qRound(squareSize() * ShogiViewLayout::kSquareAspectRatio));
    const bool fieldChanged = fieldSize() != field;
    setFieldSize(field);

    if (boardChanged || fieldChanged) update();
}

void ShogiView::configureFixedSizing(int squarePx)
{
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    const int s = (squarePx > 0) ? squarePx : squareSize();
    m_layout.setSquareSize(s);
    applyBoardScaleChange(false);
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
    BoardChromeWidget::BoardLayout layout;
    layout.fieldSize = fieldSize().isValid()
        ? fieldSize()
        : QSize(m_layout.squareSize(), qRound(m_layout.squareSize() * ShogiViewLayout::kSquareAspectRatio));
    layout.nameFontScale = m_layout.nameFontScale();
    layout.blackStand = blackStandBoundingRect();
    layout.whiteStand = whiteStandBoundingRect();
    if (m_board) {
        layout.boardRect = QRect(m_layout.offsetX(), m_layout.offsetY(),
                                 layout.fieldSize.width() * m_board->files(),
                                 layout.fieldSize.height() * m_board->ranks());
    }
    layout.flipped = m_layout.flipMode();
    m_chrome->setBoardLayout(layout);
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
    m_layout.setNameFontScale(scale);
    relayoutChrome();
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
        if (ShogiViewLayout::pieceBoxSideSelectorRect(box).contains(clickPosition)) return {};
        for (int rank = 1; rank <= 8; ++rank) {
            const bool matches = m_interaction.dragging()
                ? m_board->pieceCharacter(BoardConstants::kPieceBoxFile, rank)
                      == toBlack(demote(m_interaction.dragPiece()))
                : pieceBoxCellRect(rank).contains(clickPosition);
            if (matches) return QPoint(BoardConstants::kPieceBoxFile, rank);
        }
        return {};
    }

    const QPoint boardLocal = clickPosition - QPoint(m_layout.offsetX(), m_layout.offsetY());
    const QPoint square = m_layout.squareAt(boardLocal, m_board->files(), m_board->ranks());
    if (!square.isNull()) return square;

    for (const bool black : {true, false}) {
        const QRect stand = black ? blackStandBoundingRect() : whiteStandBoundingRect();
        if (!stand.contains(clickPosition)) continue;
        // 局面編集中のドラッグは、駒台に落とせば駒の種類で置き場所が決まる
        if (positionEditMode() && m_interaction.dragging()) {
            const Piece dragged = demote(m_interaction.dragPiece());
            if (dragged == Piece::None) return {};
            return BoardCoordinates::standSquareOf(black ? toBlack(dragged) : toWhite(dragged));
        }
        for (const QPoint& base : ShogiViewLayout::standBaseCells(black)) {
            if (standCellRect(black, base).contains(clickPosition))
                return BoardCoordinates::standSquareOf(ShogiViewLayout::standPieceAt(black, base.x(), base.y()));
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
        const QRect before = m_interaction.draggingPieceRect(m_layout, m_boardVisuals);
        m_interaction.updateDragPos(event->pos());
        update(before);
        update(m_interaction.draggingPieceRect(m_layout, m_boardVisuals));
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
    update(m_interaction.draggingPieceRect(m_layout, m_boardVisuals));
}

void ShogiView::endDrag()
{
    if (!m_interaction.dragging()) return;
    // 持ち上げた駒とつまんだマス（駒台・駒箱の枚数を含む）だけを描き直す。
    // 着手による盤・駒台の変化は ShogiBoard の通知（dataChanged / standChanged）で描き直す。
    const QRect lifted = m_interaction.draggingPieceRect(m_layout, m_boardVisuals);
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

QRect ShogiView::standCellRect(bool black, const QPoint& base) const
{
    return m_layout.standCellRect(black, cachedFieldRect(base.x(), base.y()));
}

QRect ShogiView::standPieceRect(Piece piece) const
{
    if (!m_board) return {};
    const QPoint base = ShogiViewLayout::standBaseCellOf(piece);
    if (base.isNull()) return {};
    return standCellRect(isBlackPiece(demote(piece)), base);
}

QRect ShogiView::fieldRect(const QPoint& field) const
{
    if (!m_board) return {};
    const int file = field.x();
    const int rank = field.y();
    if (file == BoardConstants::kPieceBoxFile) return pieceBoxCellRect(rank);

    if (file == BoardConstants::kBlackStandFile || file == BoardConstants::kWhiteStandFile)
        return standPieceRect(BoardCoordinates::standPieceAt(file, rank));

    if (file < 1 || file > m_board->files() || rank < 1 || rank > m_board->ranks()) return {};
    return cachedFieldRect(file, rank).translated(m_layout.offsetX(), m_layout.offsetY());
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
        update(blackStandBoundingRect());
        update(whiteStandBoundingRect());
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
    for (const bool black : {true, false}) {
        for (const QPoint& base : ShogiViewLayout::standBaseCells(black)) {
            const Piece piece = ShogiViewLayout::standPieceAt(black, base.x(), base.y());
            const auto shown = m_shownStandCounts.constFind(piece);
            if (shown == m_shownStandCounts.cend() || shown.value() != m_board->pieceStandCount(piece))
                update(standCellRect(black, base));
        }
    }
    update(pieceBoxRect());
}


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

// ============================================================
// 描画
// ============================================================

// 段・筋の番号（罫線と同じ色）
void ShogiView::drawRanksAndFiles(QPainter* painter) const
{
    for (int r = 1; r <= m_board->ranks(); ++r) drawRank(painter, r);
    for (int c = 1; c <= m_board->files(); ++c) drawFile(painter, c);
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
    m_staticLayer.draw(painter, size(), m_layout, m_board->files(), m_board->ranks(), m_boardColors, m_boardVisuals);

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
    m_highlighting->drawArrows(painter, m_layout);

    // 7) 段・筋ラベル（最前面に近いレイヤに載せる）
    drawRanksAndFiles(&painter);

    // 8) 最前面：ドラッグ中の駒（マウス追従）。盤やラベルより上に重ねる。
    m_interaction.drawDraggingPiece(painter, m_layout, piece(m_interaction.dragPiece()), m_boardVisuals);
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
        const QIcon icon = piece(pieceValue);
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
    const int x = flipMode() ? m_layout.offsetX() - band : boardRightPx();
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

// 駒台のセル（盤の基準マスを駒台側へずらした位置）ごとに、持駒と枚数を描く。
// 駒台の木肌は盤面の変わらない部分の画像（BoardStaticLayer）に含める。
void ShogiView::drawStandPieces(QPainter* painter)
{
    for (const bool black : {true, false}) {
        for (const QPoint& base : ShogiViewLayout::standBaseCells(black))
            drawStandPieceIcon(painter, standCellRect(black, base),
                               ShogiViewLayout::standPieceAt(black, base.x(), base.y()));
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
                            m_layout.standPieceArea(adjustedRect), m_boardVisuals, m_boardColors);
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
