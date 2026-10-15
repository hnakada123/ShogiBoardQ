/// @file shogiview.cpp
/// @brief 将棋盤ビューの状態・入力・マスの位置（描画は shogiview_draw.cpp）

#include "shogiview.h"
#include "shogiviewhighlighting.h"
#include "shogiboard.h"
#include "appsettings.h"
#include "shogigamecontroller.h"
#include "logcategories.h"
#include "sfenutils.h"
#include "pieceimageprovider.h"
#include "boardappearance.h"

#include <QColor>
#include <QPainter>
#include <QSizePolicy>
#include <QLabel>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QCursor>
#include "boardconstants.h"
#include "boardsurfacepainter.h"
#include "piecepainter.h"
#include <QFont>
#include <QFontMetrics>
#include <QtMath>
#include <array>

// Highlight基底クラスのデストラクタ（out-of-line定義でweak-vtables警告を回避）
ShogiView::Highlight::~Highlight() {}

// FieldHighlightクラスのデストラクタ
ShogiView::FieldHighlight::~FieldHighlight() {}

// コンストラクタ
ShogiView::ShogiView(QWidget *parent)
    : QWidget(parent),
    m_board(nullptr),
    m_errorOccurred(false),
    m_highlighting(new ShogiViewHighlighting(this, this)),
    m_chrome(new BoardChromeWidget(this))
{
    m_boardColors = BoardAppearance::instance().colors();
    m_boardVisuals = BoardAppearance::instance().visuals();
    connect(&BoardAppearance::instance(), &BoardAppearance::colorsChanged,
            this, &ShogiView::refreshBoardColors);
    connect(&BoardAppearance::instance(), &BoardAppearance::visualsChanged,
            this, &ShogiView::refreshBoardVisuals);
    connect(m_highlighting, &ShogiViewHighlighting::highlightsCleared,
            this, &ShogiView::highlightsCleared);

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

QRect ShogiView::calculateRectangleForRankOrFileLabel(const int file, const int rank) const
{
    if (!m_board) return QRect();
    return m_layout.calculateRectangleForRankOrFileLabel(file, rank, m_board->ranks());
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

int ShogiView::boardLeftPx() const { return m_layout.offsetX(); }

int ShogiView::boardRightPx() const {
    const int files = m_board ? m_board->files() : 9;
    return m_layout.boardRightPx(files);
}

int ShogiView::standInnerEdgePx(bool rightSide) const
{
    const int files = m_board ? m_board->files() : 9;
    return m_layout.standInnerEdgePx(rightSide, files);
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
// ハイライト管理
// ─────────────────────────────────────────────────────────────────────────────

void ShogiView::addHighlight(Highlight* hl)    { m_highlighting->addHighlight(hl); }
void ShogiView::removeHighlight(Highlight* hl) { m_highlighting->removeHighlight(hl); }
void ShogiView::removeHighlightAllData()        { m_highlighting->removeHighlightAllData(); }
ShogiView::Highlight* ShogiView::highlight(int index) const { return m_highlighting->highlight(index); }
int ShogiView::highlightCount() const           { return m_highlighting->highlightCount(); }

// ─────────────────────────────────────────────────────────────────────────────
// 操作/状態切替
// ─────────────────────────────────────────────────────────────────────────────

void ShogiView::setMouseClickMode(bool mouseClickMode)
{
    m_interaction.setMouseClickMode(mouseClickMode);
}

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

void ShogiView::setErrorOccurred(bool newErrorOccurred)
{
    m_errorOccurred = newErrorOccurred;
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

void ShogiView::returnAllPiecesToBox()
{
    removeHighlightAllData();
    board()->resetGameBoard();
    update();
}

void ShogiView::initializeToFlatStartingPosition()
{
    removeHighlightAllData();
    board()->setSfen(SfenUtils::hirateSfen());

    board()->initStand();
    update();
}

void ShogiView::shogiProblemInitialPosition()
{
    removeHighlightAllData();

    QString shogiProblemInitialSFENStr =
        "5+r1kl/6p2/6Bpn/9/7P1/9/9/9/9 b RSb4g3s3n3l15p 1";
    board()->setSfen(shogiProblemInitialSFENStr);

    update();
}

void ShogiView::setRankFontScale(double scale)
{
    m_layout.setRankFontScale(scale);
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

void ShogiView::relayoutEditExitButton()
{
    relayoutChrome();
    m_chrome->relayoutEditExitButton();
}

void ShogiView::setNameFontScale(double scale)
{
    m_layout.setNameFontScale(scale);
    relayoutChrome();
}

void ShogiView::updateTurnIndicator(Turn now)
{
    relayoutChrome();
    m_chrome->updateTurnIndicator(now);
}

// ─────────────────────────────────────────────────────────────────────────────
// 名前・時計・手番（BoardChromeWidget へ転送。新しいコードは chrome() を直接使う）
// ─────────────────────────────────────────────────────────────────────────────

void ShogiView::setBlackClockText(const QString& text) { m_chrome->setBlackClockText(text); }
void ShogiView::setWhiteClockText(const QString& text) { m_chrome->setWhiteClockText(text); }
QLabel* ShogiView::blackClockLabel() const { return m_chrome->blackClockLabel(); }
QLabel* ShogiView::whiteClockLabel() const { return m_chrome->whiteClockLabel(); }
void ShogiView::setClockEnabled(bool enabled) { m_chrome->setClockEnabled(enabled); }
void ShogiView::setBlackPlayerName(const QString& name) { m_chrome->setBlackPlayerName(name); }
void ShogiView::setWhitePlayerName(const QString& name) { m_chrome->setWhitePlayerName(name); }
ElideLabel* ShogiView::blackNameLabel() const { return m_chrome->blackNameLabel(); }
ElideLabel* ShogiView::whiteNameLabel() const { return m_chrome->whiteNameLabel(); }
void ShogiView::setActiveSide(bool blackTurn) { m_chrome->setActiveSide(blackTurn); }
void ShogiView::setHighlightStyle(const QColor& bgOn, const QColor& fgOn, const QColor& fgOff)
{
    m_chrome->setHighlightStyle(bgOn, fgOn, fgOff);
}
void ShogiView::clearTurnHighlight() { m_chrome->clearTurnHighlight(); }
void ShogiView::setActiveIsBlack(bool activeIsBlack) { m_chrome->setActiveIsBlack(activeIsBlack); }
void ShogiView::setGameOverStyleLock(bool locked) { m_chrome->setGameOverStyleLock(locked); }
bool ShogiView::gameOverStyleLock() const { return m_chrome->gameOverStyleLock(); }
void ShogiView::setUrgencyVisuals(Urgency u) { m_chrome->setUrgencyVisuals(u); }
void ShogiView::applyClockUrgency(qint64 activeRemainMs) { m_chrome->applyClockUrgency(activeRemainMs); }

// ============================================================
// マウス・キーの入力
// ============================================================

// ─────────────────────────────────────────────────────────────────────────────
// 入力座標変換
// ─────────────────────────────────────────────────────────────────────────────

QPoint ShogiView::clickedSquare(const QPoint &clickPosition) const
{
    return m_interaction.clickedSquare(clickPosition, m_layout, m_board, pieceBoxRect());
}

QPoint ShogiView::getClickedSquareInDefaultState(const QPoint& pos) const
{
    return m_interaction.getClickedSquareInDefaultState(pos, m_layout, m_board);
}

QPoint ShogiView::getClickedSquareInFlippedState(const QPoint& pos) const
{
    return m_interaction.getClickedSquareInFlippedState(pos, m_layout, m_board);
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
        if (m_errorOccurred) return;
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

void ShogiView::fitBoardToWidget()
{
    // Fixed SizePolicy方式では使用しない
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

QIcon ShogiView::piece(QChar type) const
{
    return m_pieces.value(type, QIcon());
}

void ShogiView::setPieces()
{
    loadPieceImages(false);
}

void ShogiView::setPiecesFlip()
{
    loadPieceImages(true);
}

void ShogiView::refreshPieceImages()
{
    loadPieceImages(flipMode());
}

void ShogiView::loadPieceImages(bool flipped)
{
    auto& provider = PieceImageProvider::instance();
    const QString types = QStringLiteral("PLNSGBRKQMOTCUplnsgbrkqmotcu");
    bool changed = false;
    for (const QChar type : types) {
        const QIcon icon = provider.icon(type, flipped);
        const auto current = m_pieces.constFind(type);
        if (current != m_pieces.cend() && current->cacheKey() == icon.cacheKey()) continue;
        m_pieces.insert(type, icon);
        changed = true;
    }
    // 画像が同じなら描き直さない（盤を読み込み直すたびに呼ばれるため）
    if (changed) update();
}

// ============================================================
// マスと駒台・駒箱のセルの位置
// ============================================================

QRect ShogiView::standPieceRect(QChar pieceChar) const
{
    if (!m_board || pieceChar == QLatin1Char(' ')) return {};

    const bool black = pieceChar.isUpper();
    const int firstRank = black ? 6 : 1;
    for (int rank = firstRank; rank < firstRank + 4; ++rank) {
        for (int file = 1; file <= 2; ++file) {
            if (ShogiViewLayout::standPieceAt(black, file, rank) == pieceChar)
                return m_layout.standCellRect(black, cachedFieldRect(file, rank));
        }
    }
    return {};
}

QRect ShogiView::fieldRect(const QPoint& field) const
{
    if (!m_board) return {};
    const int file = field.x();
    const int rank = field.y();
    if (file == BoardConstants::kPieceBoxFile) return pieceBoxCellRect(rank);

    if (file == BoardConstants::kBlackStandFile || file == BoardConstants::kWhiteStandFile) {
        // 駒台の疑似座標 (10/11, 1..9) → 駒台描画で使う基準盤マス (file=1/2, rank)
        const bool black = file == BoardConstants::kBlackStandFile;
        const QPoint base = ShogiViewLayout::standBaseCell(black, rank);
        if (base.x() == 0) return {};
        return m_layout.standCellRect(black, cachedFieldRect(base.x(), base.y()));
    }

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
    const QRect cell = standPieceRect(pieceToChar(piece));
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
    const QString standPieces = QStringLiteral("PLNSGBRKplnsgbrk");
    for (const QChar pieceChar : standPieces) {
        const Piece piece = charToPiece(pieceChar);
        const auto shown = m_shownStandCounts.constFind(piece);
        if (shown == m_shownStandCounts.cend() || shown.value() != m_board->pieceStandCount(piece))
            update(standPieceRect(pieceChar));
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

