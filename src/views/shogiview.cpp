/// @file shogiview.cpp
/// @brief 将棋盤面描画ビュークラスの実装（コア：コンストラクタ・イベント・状態管理）

#include "shogiview.h"
#include "shogiviewhighlighting.h"
#include "shogiboard.h"
#include "settingscommon.h"
#include "elidelabel.h"
#include "globaltooltip.h"
#include "shogigamecontroller.h"
#include "logcategories.h"

#include <QColor>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QPainter>
#include <QSettings>
#include <QDir>
#include <QApplication>
#include <QHelpEvent>
#include <QFont>
#include <QFontMetrics>
#include <QDebug>
#include <QSizePolicy>
#include <QLayout>
#include <QFontDatabase>
#include <QPushButton>

// Highlight基底クラスのデストラクタ（out-of-line定義でweak-vtables警告を回避）
ShogiView::Highlight::~Highlight() {}

// FieldHighlightクラスのデストラクタ
ShogiView::FieldHighlight::~FieldHighlight() {}

// コンストラクタ
ShogiView::ShogiView(QWidget *parent)
    : QWidget(parent),
    m_board(nullptr),
    m_errorOccurred(false)
{
    // ハイライト/矢印/手番表示の管理クラスを生成
    m_highlighting = new ShogiViewHighlighting(this, this);
    connect(m_highlighting, &ShogiViewHighlighting::highlightsCleared,
            this, &ShogiView::highlightsCleared);

    QDir::setCurrent(QApplication::applicationDirPath());

    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    QSettings settings(SettingsCommon::settingsFilePath(), QSettings::IniFormat);
    int sq = settings.value("SizeRelated/squareSize", 50).toInt();
    if (sq < 20 || sq > 150) sq = 50;
    m_layout.setSquareSize(sq);

    m_layout.setStandGapCols(0.5);
    recalcLayoutParams();

    setMouseTracking(true);

    // ───────────────────────────────── 時計・名前ラベル（先手：黒） ─────────────────────────────────
    m_blackClockLabel = new QLabel(QStringLiteral("00:00:00"), this);
    m_blackClockLabel->setObjectName(QStringLiteral("blackClockLabel"));
    m_blackClockLabel->setAlignment(Qt::AlignCenter);
    m_blackClockLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    m_blackClockLabel->setStyleSheet(QStringLiteral("background: transparent; color: black;"));
    {
        QFont f = font();
        f.setBold(true);
        f.setPointSizeF(qMax(8.0, m_layout.squareSize() * 0.45));
        m_blackClockLabel->setFont(f);
    }

    m_blackNameLabel = new ElideLabel(this);
    m_blackNameLabel->setElideMode(Qt::ElideRight);
    m_blackNameLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_blackNameLabel->setSlideOnHover(true);
    m_blackNameLabel->setManualPanEnabled(true);
    m_blackNameLabel->setSlideSpeed(2);
    m_blackNameLabel->setSlideInterval(16);

    // ───────────────────────────────── 名前ラベル（後手：白） ─────────────────────────────────
    m_whiteNameLabel = new ElideLabel(this);
    m_whiteNameLabel->setElideMode(Qt::ElideRight);
    m_whiteNameLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_whiteNameLabel->setSlideOnHover(true);
    m_whiteNameLabel->setManualPanEnabled(true);
    m_whiteNameLabel->setSlideSpeed(2);
    m_whiteNameLabel->setSlideInterval(16);

    // ───────────────────────────────── 時計ラベル（後手：白） ─────────────────────────────────
    m_whiteClockLabel = new QLabel(QStringLiteral("00:00:00"), this);
    m_whiteClockLabel->setObjectName(QStringLiteral("whiteClockLabel"));
    m_whiteClockLabel->setAlignment(Qt::AlignCenter);
    m_whiteClockLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    m_whiteClockLabel->setStyleSheet(QStringLiteral("background: transparent; color: black;"));
    {
        QFont f = font();
        f.setBold(true);
        f.setPointSizeF(qMax(8.0, m_layout.squareSize() * 0.45));
        m_whiteClockLabel->setFont(f);
    }

    updateBlackClockLabelGeometry();
    updateWhiteClockLabelGeometry();

    // 起動直後の見た目を整える
    m_highlighting->applyStartupTypography();

    m_blackNameLabel->installEventFilter(this);
    m_whiteNameLabel->installEventFilter(this);
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
        m_board->disconnect(this);
    }

    m_board = board;
    invalidateFieldRectCache();

    if (board) {
        connect(board, &ShogiBoard::dataChanged, this, qOverload<>(&ShogiView::update));
        connect(board, &ShogiBoard::boardReset,  this, qOverload<>(&ShogiView::update));
    }

    updateGeometry();

    qCDebug(lcView) << "setBoard complete, m_board now:" << m_board;
}

// ─────────────────────────────────────────────────────────────────────────────
// イベントフィルタ
// ─────────────────────────────────────────────────────────────────────────────

bool ShogiView::eventFilter(QObject* obj, QEvent* ev)
{
    if (!m_tooltip) {
        m_tooltip = new GlobalToolTip(this);
        m_tooltip->setCompact(true);
        m_tooltip->setPointSizeF(12.0);
    }

    if (obj == m_blackNameLabel || obj == m_whiteNameLabel) {
        if (ev->type() == QEvent::ToolTip) {
            auto* he = static_cast<QHelpEvent*>(ev);
            const QString text = (obj == m_blackNameLabel) ? m_blackNameBase : m_whiteNameBase;
            m_tooltip->showText(he->globalPos(), text);
            return true;
        }
        else if (ev->type() == QEvent::Leave) {
            m_tooltip->hideTip();
        }
    }

    return QWidget::eventFilter(obj, ev);
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

ElideLabel* ShogiView::blackNameLabel() const { return m_blackNameLabel; }
QLabel* ShogiView::blackClockLabel() const { return m_blackClockLabel; }
ElideLabel* ShogiView::whiteNameLabel() const { return m_whiteNameLabel; }
QLabel* ShogiView::whiteClockLabel() const { return m_whiteClockLabel; }

bool ShogiView::isClockEnabled() const { return m_clockEnabled; }
bool ShogiView::positionEditMode() const { return m_interaction.positionEditMode(); }
int  ShogiView::squareSize() const { return m_layout.squareSize(); }
bool ShogiView::getFlipMode() const { return m_layout.flipMode(); }

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
    m_standPiecePixmapCache.clear();
    m_highlighting->clearDropPieceCache();
    invalidateFieldRectCache();

    emit fieldSizeChanged(fieldSize);

    updateGeometry();

    updateBlackClockLabelGeometry();
    updateWhiteClockLabelGeometry();
}

QSize ShogiView::sizeHint() const
{
    if (!m_board) {
        return QSize(100, 100);
    }

    const QSize fs = fieldSize().isValid() ? fieldSize()
                                           : QSize(m_layout.squareSize(), qRound(m_layout.squareSize() * ShogiViewLayout::kSquareAspectRatio));

    const int boardWidth  = fs.width()  * m_board->files();
    const int boardHeight = fs.height() * m_board->ranks();

    const int standWidth = fs.width() * 2;

    const int totalWidth = standWidth + m_layout.standGapPx() + boardWidth + m_layout.standGapPx() + standWidth;
    const int totalHeight = boardHeight + m_layout.offsetY() * 2;

    return QSize(totalWidth, totalHeight);
}

QSize ShogiView::minimumSizeHint() const
{
    const int minSquare = 20;
    const int minSquareH = qRound(minSquare * ShogiViewLayout::kSquareAspectRatio);
    const int files = m_board ? m_board->files() : 9;
    const int ranks = m_board ? m_board->ranks() : 9;

    const int minBoardWidth = minSquare * files;
    const int minBoardHeight = minSquareH * ranks;
    const int minStandWidth = minSquare * 2;
    const int minGap = qRound(minSquare * 0.5);

    const int minWidth = minStandWidth + minGap + minBoardWidth + minGap + minStandWidth;
    const int minHeight = minBoardHeight + m_layout.offsetY() * 2;

    return QSize(minWidth, minHeight);
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
    relayoutTurnLabels();
}

// ─────────────────────────────────────────────────────────────────────────────
// 駒画像管理
// ─────────────────────────────────────────────────────────────────────────────

void ShogiView::setPiece(char type, const QIcon &icon)
{
    m_pieces.insert(type, icon);
    m_standPiecePixmapCache.clear();
    m_highlighting->clearDropPieceCache();
    update();
}

QIcon ShogiView::piece(QChar type) const
{
    return m_pieces.value(type, QIcon());
}

void ShogiView::setPieces()
{
    // ── 先手（大文字） ──
    setPiece('P', QIcon(":/pieces/Sente_fu45.svg"));
    setPiece('L', QIcon(":/pieces/Sente_kyou45.svg"));
    setPiece('N', QIcon(":/pieces/Sente_kei45.svg"));
    setPiece('S', QIcon(":/pieces/Sente_gin45.svg"));
    setPiece('G', QIcon(":/pieces/Sente_kin45.svg"));
    setPiece('B', QIcon(":/pieces/Sente_kaku45.svg"));
    setPiece('R', QIcon(":/pieces/Sente_hi45.svg"));
    setPiece('K', QIcon(":/pieces/Sente_ou45.svg"));

    // 昇格駒（先手）
    setPiece('Q', QIcon(":/pieces/Sente_to45.svg"));
    setPiece('M', QIcon(":/pieces/Sente_narikyou45.svg"));
    setPiece('O', QIcon(":/pieces/Sente_narikei45.svg"));
    setPiece('T', QIcon(":/pieces/Sente_narigin45.svg"));
    setPiece('C', QIcon(":/pieces/Sente_uma45.svg"));
    setPiece('U', QIcon(":/pieces/Sente_ryuu45.svg"));

    // ── 後手（小文字） ──
    setPiece('p', QIcon(":/pieces/Gote_fu45.svg"));
    setPiece('l', QIcon(":/pieces/Gote_kyou45.svg"));
    setPiece('n', QIcon(":/pieces/Gote_kei45.svg"));
    setPiece('s', QIcon(":/pieces/Gote_gin45.svg"));
    setPiece('g', QIcon(":/pieces/Gote_kin45.svg"));
    setPiece('b', QIcon(":/pieces/Gote_kaku45.svg"));
    setPiece('r', QIcon(":/pieces/Gote_hi45.svg"));
    setPiece('k', QIcon(":/pieces/Gote_gyoku45.svg"));

    // 昇格駒（後手）
    setPiece('q', QIcon(":/pieces/Gote_to45.svg"));
    setPiece('m', QIcon(":/pieces/Gote_narikyou45.svg"));
    setPiece('o', QIcon(":/pieces/Gote_narikei45.svg"));
    setPiece('t', QIcon(":/pieces/Gote_narigin45.svg"));
    setPiece('c', QIcon(":/pieces/Gote_uma45.svg"));
    setPiece('u', QIcon(":/pieces/Gote_ryuu45.svg"));
}

void ShogiView::setPiecesFlip()
{
    // ── flip 時：小文字（元・後手）に先手の画像を割り当て ──
    setPiece('p', QIcon(":/pieces/Sente_fu45.svg"));
    setPiece('l', QIcon(":/pieces/Sente_kyou45.svg"));
    setPiece('n', QIcon(":/pieces/Sente_kei45.svg"));
    setPiece('s', QIcon(":/pieces/Sente_gin45.svg"));
    setPiece('g', QIcon(":/pieces/Sente_kin45.svg"));
    setPiece('b', QIcon(":/pieces/Sente_kaku45.svg"));
    setPiece('r', QIcon(":/pieces/Sente_hi45.svg"));
    setPiece('k', QIcon(":/pieces/Sente_gyoku45.svg"));

    // 昇格駒（flip：小文字→先手画像）
    setPiece('q', QIcon(":/pieces/Sente_to45.svg"));
    setPiece('m', QIcon(":/pieces/Sente_narikyou45.svg"));
    setPiece('o', QIcon(":/pieces/Sente_narikei45.svg"));
    setPiece('t', QIcon(":/pieces/Sente_narigin45.svg"));
    setPiece('c', QIcon(":/pieces/Sente_uma45.svg"));
    setPiece('u', QIcon(":/pieces/Sente_ryuu45.svg"));

    // ── flip 時：大文字（元・先手）に後手の画像を割り当て ──
    setPiece('P', QIcon(":/pieces/Gote_fu45.svg"));
    setPiece('L', QIcon(":/pieces/Gote_kyou45.svg"));
    setPiece('N', QIcon(":/pieces/Gote_kei45.svg"));
    setPiece('S', QIcon(":/pieces/Gote_gin45.svg"));
    setPiece('G', QIcon(":/pieces/Gote_kin45.svg"));
    setPiece('B', QIcon(":/pieces/Gote_kaku45.svg"));
    setPiece('R', QIcon(":/pieces/Gote_hi45.svg"));
    setPiece('K', QIcon(":/pieces/Gote_ou45.svg"));

    // 昇格駒（flip：大文字→後手画像）
    setPiece('Q', QIcon(":/pieces/Gote_to45.svg"));
    setPiece('M', QIcon(":/pieces/Gote_narikyou45.svg"));
    setPiece('O', QIcon(":/pieces/Gote_narikei45.svg"));
    setPiece('T', QIcon(":/pieces/Gote_narigin45.svg"));
    setPiece('C', QIcon(":/pieces/Gote_uma45.svg"));
    setPiece('U', QIcon(":/pieces/Gote_ryuu45.svg"));
}

// ─────────────────────────────────────────────────────────────────────────────
// 入力座標変換
// ─────────────────────────────────────────────────────────────────────────────

QPoint ShogiView::getClickedSquare(const QPoint &clickPosition) const
{
    return m_interaction.getClickedSquare(clickPosition, m_layout, m_board);
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
        QPoint pt = getClickedSquare(event->pos());
        emit rightClicked(pt);
        return;
    }

    if (event->button() == Qt::LeftButton) {
        QPoint pt = getClickedSquare(event->pos());

        if (pt.isNull()) return;
        if (m_errorOccurred) return;
        emit clicked(pt);
    }
    else if (event->button() == Qt::RightButton) {
        QPoint pt = getClickedSquare(event->pos());
        if (pt.isNull()) return;
        emit rightClicked(pt);
    }
}

void ShogiView::mouseMoveEvent(QMouseEvent* event)
{
    if (m_interaction.dragging()) {
        m_interaction.updateDragPos(event->pos());
        update();
    }
    QWidget::mouseMoveEvent(event);
}

void ShogiView::resizeEvent(QResizeEvent* e)
{
    QWidget::resizeEvent(e);
    updateBlackClockLabelGeometry();
    updateWhiteClockLabelGeometry();
    relayoutTurnLabels();
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
    m_standPiecePixmapCache.clear();
    m_highlighting->clearDropPieceCache();
    invalidateFieldRectCache();
    recalcLayoutParams();
    updateGeometry();
    updateBlackClockLabelGeometry();
    updateWhiteClockLabelGeometry();
    update();
}

void ShogiView::enlargeBoard(bool emitSignal)
{
    if (m_layout.squareSize() >= 150) {
        return;
    }

    m_layout.setSquareSize(m_layout.squareSize() + 1);
    m_standPiecePixmapCache.clear();
    m_highlighting->clearDropPieceCache();
    invalidateFieldRectCache();
    recalcLayoutParams();

    updateGeometry();
    updateBlackClockLabelGeometry();
    updateWhiteClockLabelGeometry();

    if (emitSignal) {
        emit fieldSizeChanged(m_layout.fieldSize());
    }

    update();
}

void ShogiView::reduceBoard(bool emitSignal)
{
    if (m_layout.squareSize() <= 20) {
        return;
    }

    m_layout.setSquareSize(m_layout.squareSize() - 1);
    m_standPiecePixmapCache.clear();
    m_highlighting->clearDropPieceCache();
    invalidateFieldRectCache();
    recalcLayoutParams();

    updateGeometry();
    updateBlackClockLabelGeometry();
    updateWhiteClockLabelGeometry();

    if (emitSignal) {
        emit fieldSizeChanged(m_layout.fieldSize());
    }

    update();
}

void ShogiView::setErrorOccurred(bool newErrorOccurred)
{
    m_errorOccurred = newErrorOccurred;
}

void ShogiView::setPositionEditMode(bool positionEditMode)
{
    if (m_interaction.positionEditMode() == positionEditMode) return;

    QLabel* tlBlack = this->findChild<QLabel*>(QStringLiteral("turnLabelBlack"));
    QLabel* tlWhite = this->findChild<QLabel*>(QStringLiteral("turnLabelWhite"));
    const bool blackTurnShown = (tlBlack && tlBlack->isVisible());
    const bool whiteTurnShown = (tlWhite && tlWhite->isVisible());

    m_interaction.setPositionEditMode(positionEditMode);

    relayoutTurnLabels();

    if (tlBlack) { tlBlack->setVisible(blackTurnShown); tlBlack->raise(); }
    if (tlWhite) { tlWhite->setVisible(whiteTurnShown); tlWhite->raise(); }

    update();
}

void ShogiView::setStandGapCols(double cols)
{
    m_layout.setStandGapCols(cols);
    recalcLayoutParams();
    updateGeometry();
    updateBlackClockLabelGeometry();
    updateWhiteClockLabelGeometry();
    update();
}

void ShogiView::setFlipMode(bool newFlipMode)
{
    if (m_layout.flipMode() == newFlipMode) return;

    QLabel* tlBlack = this->findChild<QLabel*>(QStringLiteral("turnLabelBlack"));
    QLabel* tlWhite = this->findChild<QLabel*>(QStringLiteral("turnLabelWhite"));
    const bool blackShown = (tlBlack && tlBlack->isVisible());
    const bool whiteShown = (tlWhite && tlWhite->isVisible());

    m_layout.setFlipMode(newFlipMode);
    invalidateFieldRectCache();

    refreshNameLabels();

    updateBlackClockLabelGeometry();
    updateWhiteClockLabelGeometry();

    relayoutTurnLabels();

    if (tlBlack) { tlBlack->setVisible(blackShown); tlBlack->raise(); }
    if (tlWhite) { tlWhite->setVisible(whiteShown); tlWhite->raise(); }

    update();
}

void ShogiView::resetAndEqualizePiecesOnStands()
{
    removeHighlightAllData();
    board()->resetGameBoard();
    update();
}

void ShogiView::initializeToFlatStartingPosition()
{
    removeHighlightAllData();

    QString flatInitialSFENStr =
        "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1";
    board()->setSfen(flatInitialSFENStr);

    board()->initStand();
    update();
}

void ShogiView::shogiProblemInitialPosition()
{
    removeHighlightAllData();

    QString shogiProblemInitialSFENStr =
        "5+r1kl/6p2/6Bpn/9/7P1/9/9/9/9 b RSb4g3s3n3l15p 1";
    board()->setSfen(shogiProblemInitialSFENStr);

    board()->incrementPieceOnStand(Piece::BlackKing);
    update();
}

void ShogiView::flipBoardSides()
{
    removeHighlightAllData();
    board()->flipSides();
    update();
}

void ShogiView::startDrag(const QPoint &from)
{
    m_interaction.startDrag(from, m_board, mapFromGlobal(QCursor::pos()));
    update();
}

void ShogiView::endDrag()
{
    m_interaction.endDrag();
    update();
}

void ShogiView::setClockEnabled(bool enabled)
{
    m_clockEnabled = enabled;
    if (!enabled) {
        if (m_blackClockLabel) m_blackClockLabel->hide();
        if (m_whiteClockLabel) m_whiteClockLabel->hide();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// 手番・ハイライトスタイル
// ─────────────────────────────────────────────────────────────────────────────

void ShogiView::setActiveSide(bool blackTurn)     { m_highlighting->setActiveSide(blackTurn); }
void ShogiView::setHighlightStyle(const QColor& bgOn, const QColor& fgOn, const QColor& fgOff)
{
    m_highlighting->setHighlightStyle(bgOn, fgOn, fgOff);
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
    relayoutTurnLabels();
}

void ShogiView::setUrgencyVisuals(Urgency u) { m_highlighting->setUrgencyVisuals(u); }

void ShogiView::setBlackTimeMs(qint64 ms) { m_blackTimeMs = ms; }
void ShogiView::setWhiteTimeMs(qint64 ms) { m_whiteTimeMs = ms; }

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

    if (m_layout.flipMode()) setPiecesFlip();
    else            setPieces();

    setBoard(board);

    setFieldSize(QSize(squareSize(), qRound(squareSize() * ShogiViewLayout::kSquareAspectRatio)));

    update();
}

void ShogiView::configureFixedSizing(int squarePx)
{
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    const int s = (squarePx > 0) ? squarePx : squareSize();
    setFieldSize(QSize(s, qRound(s * ShogiViewLayout::kSquareAspectRatio)));
    update();
}

void ShogiView::applyClockUrgency(qint64 activeRemainMs)
{
    m_highlighting->applyClockUrgency(activeRemainMs);
}

void ShogiView::clearTurnHighlight()
{
    m_highlighting->clearTurnHighlight();
}

void ShogiView::setGameOverStyleLock(bool locked)
{
    m_gameOverStyleLock = locked;
}

void ShogiView::setUiMuted(bool on) {
    m_uiMuted = on;
}

void ShogiView::setActiveIsBlack(bool activeIsBlack)
{
    m_highlighting->setActiveIsBlack(activeIsBlack);
}

// ─────────────────────────────────────────────────────────────────────────────
// 矢印表示機能（ShogiViewHighlighting へ委譲）
// ─────────────────────────────────────────────────────────────────────────────

void ShogiView::setArrows(const QVector<Arrow>& arrows) { m_highlighting->setArrows(arrows); }
void ShogiView::clearArrows()                           { m_highlighting->clearArrows(); }
