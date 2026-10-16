/// @file shogiviewlayout.cpp
/// @brief 将棋盤面のレイアウト計算クラスの実装

#include "shogiviewlayout.h"

#include <array>
#include "boardsurfacepainter.h"

#include <QFontMetrics>
#include <QStringList>
#include <QtMath>

#include <algorithm>

ShogiViewLayout::ShogiViewLayout() = default;

// ─────────────────────────── レイアウト再計算 ────────────────────────────
void ShogiViewLayout::recalcLayoutParams(const QFont& baseFont)
{
    // マスサイズ（実際の将棋盤の縦横比率を反映）
    const int squareWidth  = m_squareSize;
    const int squareHeight = qRound(m_squareSize * kSquareAspectRatio);
    m_fieldSize = QSize(squareWidth, squareHeight);

    // 将棋盤余白
    m_boardMarginPx = qMax(2, qRound(m_squareSize * 0.28));

    // ラベル関連
    m_labelFontPt = std::clamp(m_squareSize * 0.26, 5.0, 18.0);
    m_labelGapPx  = std::max(2,  int(m_squareSize * 0.12));

    // 駒台の実効ギャップ
    const int userGapPx = qRound(m_squareSize * m_standGapCols);
    const int needGapPx = minGapForRankLabelsPx(baseFont);
    const int effGapPx  = std::max(userGapPx, needGapPx);
    const double effCols = double(effGapPx) / double(m_squareSize);

    // 水平オフセット
    m_param1     = qRound((2.0 + effCols) * m_squareSize);
    m_param2     = qRound((9.0 + effCols) * m_squareSize);
    m_offsetX    = m_param1;
    m_standGapPx = qRound(effCols * m_squareSize);
}

// ─────────────────────────── 座標計算 ────────────────────────────────────
QRect ShogiViewLayout::calculateSquareRectangleBasedOnBoardState(
    int file, int rank, int boardFiles, int boardRanks) const
{
    const QSize fs = m_fieldSize;
    int xPosition, yPosition;

    if (m_flipMode) {
        xPosition = (file - 1) * fs.width();
        yPosition = (boardRanks - rank) * fs.height();
    } else {
        xPosition = (boardFiles - file) * fs.width();
        yPosition = (rank - 1) * fs.height();
    }

    return QRect(xPosition, yPosition, fs.width(), fs.height());
}

QPoint ShogiViewLayout::squareAt(const QPoint& boardLocal, int boardFiles, int boardRanks) const
{
    const QSize fs = m_fieldSize;
    if (fs.isEmpty() || boardLocal.x() < 0 || boardLocal.y() < 0) return {};
    const int column = boardLocal.x() / fs.width();
    const int row = boardLocal.y() / fs.height();
    if (column >= boardFiles || row >= boardRanks) return {};
    if (m_flipMode) return {column + 1, boardRanks - row};
    return {boardFiles - column, row + 1};
}

QRect ShogiViewLayout::blackStandBoundingRect(int boardFiles, int boardRanks) const
{
    const QSize fs = m_fieldSize.isValid() ? m_fieldSize
                                           : QSize(m_squareSize, qRound(m_squareSize * kSquareAspectRatio));

    const int rows    = 4;
    const int topRank = m_flipMode ? 9 : 6;
    const int leftCol = m_flipMode ? 1 : 2;

    const QRect cell = calculateSquareRectangleBasedOnBoardState(leftCol, topRank, boardFiles, boardRanks);

    const int x = (m_flipMode ? (cell.left() - m_param1 + m_offsetX)
                              : (cell.left() + m_param1 + m_offsetX));
    const int y = cell.top() + offsetY();
    const int w = fs.width() * 2;
    const int h = fs.height() * rows;

    return QRect(x, y, w, h);
}

QRect ShogiViewLayout::whiteStandBoundingRect(int boardFiles, int boardRanks) const
{
    const QSize fs = m_fieldSize.isValid() ? m_fieldSize
                                           : QSize(m_squareSize, qRound(m_squareSize * kSquareAspectRatio));

    const int rows    = 4;
    const int topRank = m_flipMode ? 4 : 1;
    const int leftCol = m_flipMode ? 1 : 2;

    const QRect cell = calculateSquareRectangleBasedOnBoardState(leftCol, topRank, boardFiles, boardRanks);

    const int x = (m_flipMode ? (cell.left() + m_param2 + m_offsetX)
                              : (cell.left() - m_param2 + m_offsetX));
    const int y = cell.top() + offsetY();
    const int w = fs.width() * 2;
    const int h = fs.height() * rows;

    return QRect(x, y, w, h);
}

QRect ShogiViewLayout::pieceBoxLabelRect(const QRect& box)
{
    if (box.isEmpty()) return {};
    return QRect(box.left(), box.top(), box.width(), qMax(1, box.height() / 9));
}

QRect ShogiViewLayout::pieceBoxSideSelectorRect(const QRect& box)
{
    if (box.isEmpty()) return {};
    const QRect label = pieceBoxLabelRect(box);
    return QRect(box.left(), label.bottom() + 1, box.width(), qMax(1, box.height() / 8));
}

namespace {
// 駒台のセル（基準盤マス file=1/2, rank=6..9）に置く先手の駒。
// 後手の駒台は同じ並びを180度回したもの（file と rank を反転し、後手の駒にする）。
constexpr std::array<std::array<Piece, 4>, 2> kBlackStandPieces{{
    {Piece::BlackKing, Piece::BlackBishop, Piece::BlackSilver, Piece::BlackLance},  // file=1（rank=6..9）
    {Piece::BlackRook, Piece::BlackGold, Piece::BlackKnight, Piece::BlackPawn},     // file=2（rank=6..9）
}};
} // namespace

QRect ShogiViewLayout::standCellRect(bool black, const QRect& baseField) const
{
    // 通常は先手の駒台が右、後手が左。反転すると入れ替わる
    const int shift = black ? m_param1 : m_param2;
    const bool towardRight = black != m_flipMode;
    return QRect(baseField.left() + (towardRight ? shift : -shift) + m_offsetX,
                 baseField.top() + offsetY(), baseField.width(), baseField.height());
}

Piece ShogiViewLayout::standPieceAt(bool black, int file, int rank)
{
    if (!black) {
        file = 3 - file;
        rank = 10 - rank;
    }
    if (file < 1 || file > 2 || rank < 6 || rank > 9) return Piece::None;
    const Piece piece = kBlackStandPieces[static_cast<std::size_t>(file - 1)]
                                         [static_cast<std::size_t>(rank - 6)];
    return black ? piece : toWhite(piece);
}

std::array<QPoint, 8> ShogiViewLayout::standBaseCells(bool black)
{
    const int firstRank = black ? 6 : 1;
    std::array<QPoint, 8> cells;
    for (std::size_t i = 0; i < cells.size(); ++i)
        cells[i] = QPoint(int(i % 2) + 1, firstRank + int(i / 2));
    return cells;
}

QPoint ShogiViewLayout::standBaseCellOf(Piece piece)
{
    const Piece base = demote(piece);
    if (base == Piece::None) return {};
    const bool black = isBlackPiece(base);
    for (const QPoint& cell : standBaseCells(black)) {
        if (standPieceAt(black, cell.x(), cell.y()) == base) return cell;
    }
    return {};
}

QRect ShogiViewLayout::pieceBoxCellRect(const QRect& box, int rank)
{
    if (box.isEmpty() || rank < 1 || rank > 8) return {};
    const int labelHeight = pieceBoxLabelRect(box).height() + pieceBoxSideSelectorRect(box).height();
    const int index = 8 - rank;
    const int x = (index % 2) * box.width() / 2;
    const int y = (index / 2) * (box.height() - labelHeight) / 4;
    const int nextX = (index % 2 + 1) * box.width() / 2;
    const int nextY = (index / 2 + 1) * (box.height() - labelHeight) / 4;
    return QRect(box.left() + x, box.top() + labelHeight + y, nextX - x, nextY - y);
}

int ShogiViewLayout::coordinateBandPx() const
{
    return qRound(m_fieldSize.width() * 0.50);
}

int ShogiViewLayout::outerMarginPx() const
{
    return qMax(4, qRound(m_fieldSize.width() * 0.12));
}

int ShogiViewLayout::offsetY() const
{
    const auto shadow = BoardSurfacePainter::shadowMargins(m_fieldSize.width());
    return outerMarginPx() + qCeil(shadow.top())
        + qMax(m_boardMarginPx, coordinateBandPx());
}

QRectF ShogiViewLayout::boardSurfaceRect(int boardFiles, int boardRanks) const
{
    // 四辺に座標帯を確保し、反転しても盤・駒台・対局者情報の基準位置を変えない。
    const int margin = qMax(m_boardMarginPx, coordinateBandPx());
    return QRectF(m_offsetX - margin, offsetY() - margin,
                  m_fieldSize.width() * boardFiles + margin * 2,
                  m_fieldSize.height() * boardRanks + margin * 2);
}

QSize ShogiViewLayout::viewSize(int boardFiles, int boardRanks) const
{
    const auto shadow = BoardSurfacePainter::shadowMargins(m_fieldSize.width());
    return QSize(m_fieldSize.width() * (boardFiles + 4) + m_standGapPx * 2,
                 qCeil(boardSurfaceRect(boardFiles, boardRanks).height())
                     + qCeil(shadow.top()) + qCeil(shadow.bottom()) + outerMarginPx() * 2);
}

QRectF ShogiViewLayout::standSurfaceRect(const QRect& stand) const
{
    const qreal inset = qMax(1.0, m_fieldSize.width() * 0.035);
    return QRectF(stand).adjusted(inset, 0, -inset, 0);
}

QRectF ShogiViewLayout::standPieceArea(const QRect& cell) const
{
    // 両列に同じ余白を設け、先後・盤反転によらずセル中心を保つ。
    const qreal padding = qMax(2.0, m_fieldSize.width() * 0.04);
    return standSurfaceRect(cell).adjusted(padding, padding, -padding, -padding);
}

// ─────────────────────────── 境界ユーティリティ ─────────────────────────
int ShogiViewLayout::boardRightPx(int boardFiles) const
{
    const QSize fs = m_fieldSize;
    return m_offsetX + fs.width() * boardFiles;
}

int ShogiViewLayout::minGapForRankLabelsPx(const QFont& baseFont) const
{
    const int bandH = std::max(10, int(m_squareSize * 0.68));

    QFont f = baseFont;
    f.setPointSizeF(std::clamp(m_labelFontPt, 5.0, double(bandH)));
    QFontMetrics fm(f);

    static const QStringList ranks = { "一","二","三","四","五","六","七","八","九" };

    int wMax = 0;
    for (const QString& s : ranks)
        wMax = std::max(wMax, fm.horizontalAdvance(s));

    const int padding = std::max(2, m_labelGapPx);
    return wMax + padding * 2;
}

// ─────────────────────────── セッター ────────────────────────────────────
void ShogiViewLayout::setStandGapCols(double cols)
{
    m_standGapCols = qBound(0.0, cols, 2.0);
}

void ShogiViewLayout::setNameFontScale(double scale)
{
    m_nameFontScale = std::clamp(scale, 0.2, 1.0);
}

void ShogiViewLayout::setRankFontScale(double scale)
{
    m_rankFontScale = std::clamp(scale, 0.5, 1.2);
}

void ShogiViewLayout::setFieldSize(const QSize& size)
{
    m_fieldSize = size;
}

void ShogiViewLayout::setFlipMode(bool flip)
{
    m_flipMode = flip;
}

void ShogiViewLayout::setSquareSize(int size)
{
    if (size >= 20 && size <= 150) {
        m_squareSize = size;
    }
}
