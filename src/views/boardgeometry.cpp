/// @file boardgeometry.cpp
/// @brief 盤・駒台・座標の文字・対局者カードの位置の計算

#include "boardgeometry.h"

#include "boardsurfacepainter.h"

#include <QFontMetrics>
#include <QtMath>

#include <algorithm>

namespace {
/// 先手の駒台（反転していないとき）の並び。[列][段]
constexpr std::array<std::array<Piece, 4>, 2> kBlackStandGrid{{
    {Piece::BlackRook, Piece::BlackGold, Piece::BlackKnight, Piece::BlackPawn},     // 左の列
    {Piece::BlackKing, Piece::BlackBishop, Piece::BlackSilver, Piece::BlackLance},  // 右の列
}};

/// 段の文字の帯に要る幅（最も幅の広い段の文字＋左右の余白）
int rankLabelWidth(const QFont& base, int pixelSize, int squareSize)
{
    QFont font = base;
    font.setPixelSize(pixelSize);
    const QFontMetrics metrics(font);
    int widest = 0;
    for (const char16_t rank : u"一二三四五六七八九") widest = std::max(widest, metrics.horizontalAdvance(QChar(rank)));
    return widest + std::max(2, int(squareSize * 0.12)) * 2;
}
} // namespace

BoardGeometry::BoardGeometry()
    : BoardGeometry(Params{})
{
}

BoardGeometry::BoardGeometry(const Params& params)
    : m_params(params)
{
    m_params.squareSize = std::clamp(m_params.squareSize, kMinSquareSize, kMaxSquareSize);
    m_params.standGapCols = qBound(0.0, m_params.standGapCols, 2.0);
    const int sq = m_params.squareSize;
    m_field = QSize(sq, qRound(sq * kSquareAspectRatio));
    m_labelPixelSize = std::max(8, qRound(m_field.width() * 0.24));
    m_coordinateBand = qRound(m_field.width() * 0.50);

    // 盤と駒台の間。段の文字が収まる幅より狭くしない
    const int gapPx = std::max(qRound(sq * m_params.standGapCols), rankLabelWidth(m_params.font, m_labelPixelSize, sq));
    const double gapCols = double(gapPx) / double(sq);
    const int blackShift = qRound((2.0 + gapCols) * sq);   // 盤の筋2から先手の駒台まで
    const int whiteShift = qRound((9.0 + gapCols) * sq);   // 盤の筋2から後手の駒台まで
    const int standGapPx = qRound(gapCols * sq);

    // 盤の左上。四辺に座標の帯（盤の縁より狭くしない）と影・外側の余白を取る
    const int outerMargin = std::max(4, qRound(m_field.width() * 0.12));
    const int boardMargin = std::max(m_coordinateBand, std::max(2, qRound(sq * 0.28)));
    const QMarginsF shadow = BoardSurfacePainter::shadowMargins(m_field.width());
    const QPoint origin(blackShift, outerMargin + qCeil(shadow.top()) + boardMargin);
    m_board = QRect(origin, QSize(m_field.width() * files(), m_field.height() * ranks()));
    m_surface = QRectF(m_board.adjusted(-boardMargin, -boardMargin, boardMargin, boardMargin));

    // 駒台は2列4段。反転していなければ先手は盤の右下、後手は左上。反転すると入れ替わる
    const QSize standSize(m_field.width() * 2, m_field.height() * 4);
    const QRect lowerLeftOfRight = boardLocalSquare(flipped() ? 1 : 2, flipped() ? 9 : 6);
    const QRect upperLeftOfLeft = boardLocalSquare(flipped() ? 1 : 2, flipped() ? 4 : 1);
    const QRect black(lowerLeftOfRight.topLeft() + QPoint(flipped() ? -blackShift : blackShift, 0) + origin, standSize);
    const QRect white(upperLeftOfLeft.topLeft() + QPoint(flipped() ? whiteShift : -whiteShift, 0) + origin, standSize);
    m_blackStand = black;
    m_whiteStand = white;

    m_viewSize = QSize(m_field.width() * (files() + 4) + standGapPx * 2,
                       qCeil(m_surface.height()) + qCeil(shadow.top()) + qCeil(shadow.bottom()) + outerMargin * 2);
}

QRect BoardGeometry::boardLocalSquare(int file, int rank) const
{
    const int column = flipped() ? file - 1 : files() - file;
    const int row = flipped() ? ranks() - rank : rank - 1;
    return QRect(column * m_field.width(), row * m_field.height(), m_field.width(), m_field.height());
}

QRect BoardGeometry::square(int file, int rank) const
{
    if (file < 1 || file > files() || rank < 1 || rank > ranks()) return {};
    return boardLocalSquare(file, rank).translated(m_board.topLeft());
}

QPoint BoardGeometry::squareAt(const QPoint& pos) const
{
    const QPoint local = pos - m_board.topLeft();
    if (m_field.isEmpty() || local.x() < 0 || local.y() < 0) return {};
    const int column = local.x() / m_field.width();
    const int row = local.y() / m_field.height();
    if (column >= files() || row >= ranks()) return {};
    if (flipped()) return {column + 1, ranks() - row};
    return {files() - column, row + 1};
}

QRect BoardGeometry::fileLabel(int file) const
{
    const QRect cell = square(file, 1);
    if (cell.isEmpty()) return {};
    const int y = flipped() ? m_board.top() + m_board.height() : m_board.top() - m_coordinateBand;
    return QRect(cell.left(), y, cell.width(), m_coordinateBand);
}

QRect BoardGeometry::rankLabel(int rank) const
{
    const QRect cell = square(1, rank);
    if (cell.isEmpty()) return {};
    const int x = flipped() ? m_board.left() - m_coordinateBand : m_board.left() + m_board.width();
    return QRect(x, cell.top(), m_coordinateBand, cell.height());
}

const std::array<Piece, 8>& BoardGeometry::standPieces(Turn side)
{
    static const std::array<Piece, 8> kBlack{Piece::BlackKing, Piece::BlackRook, Piece::BlackBishop, Piece::BlackGold,
                                            Piece::BlackSilver, Piece::BlackKnight, Piece::BlackLance, Piece::BlackPawn};
    static const std::array<Piece, 8> kWhite = [] {
        std::array<Piece, 8> white{};
        std::transform(kBlack.cbegin(), kBlack.cend(), white.begin(), toWhite);
        return white;
    }();
    return side == Turn::Black ? kBlack : kWhite;
}

QRect BoardGeometry::standCell(Piece piece) const
{
    const Piece base = demote(piece);
    if (base == Piece::None) return {};
    const Turn side = isBlackPiece(base) ? Turn::Black : Turn::White;
    const Piece blackBase = toBlack(base);
    for (int column = 0; column < 2; ++column) {
        for (int row = 0; row < 4; ++row) {
            if (kBlackStandGrid[std::size_t(column)][std::size_t(row)] != blackBase) continue;
            // 後手の駒台と反転したときの駒台は、先手の並びを180度回す
            const bool rotated = (side == Turn::White) != flipped();
            const QRect stand = this->stand(side);
            return QRect(stand.left() + (rotated ? 1 - column : column) * m_field.width(),
                         stand.top() + (rotated ? 3 - row : row) * m_field.height(),
                         m_field.width(), m_field.height());
        }
    }
    return {};
}

QRectF BoardGeometry::standSurface(const QRect& area) const
{
    const qreal inset = qMax(1.0, m_field.width() * 0.035);
    return QRectF(area).adjusted(inset, 0, -inset, 0);
}

QRectF BoardGeometry::standPieceArea(const QRect& cell) const
{
    // 両列に同じ余白を設け、先後・盤反転によらずセル中心を保つ。
    const qreal padding = qMax(2.0, m_field.width() * 0.04);
    return standSurface(cell).adjusted(padding, padding, -padding, -padding);
}

int BoardGeometry::standGap() const
{
    return std::max(4, qRound(m_field.height() * 0.08));
}

QRect BoardGeometry::playerCard(Turn side, int cardHeight, int areaHeight) const
{
    const QRect area = stand(side);
    const bool above = (side == Turn::Black) != flipped();
    const int top = above ? area.top() - standGap() - cardHeight : area.bottom() + 1 + standGap();
    return QRect(area.left(), qBound(0, top, std::max(0, areaHeight - cardHeight)), area.width(), cardHeight);
}

QRect BoardGeometry::pieceBox(int cardHeight, int areaHeight) const
{
    const Turn leftSide = flipped() ? Turn::Black : Turn::White;
    const QRect area = stand(leftSide);
    const int above = cardHeight > 0 ? playerCard(leftSide, cardHeight, areaHeight).bottom() : area.bottom();
    const int top = above + standGap() + 1;
    const int bottom = m_board.top() + m_board.height();
    const int height = qMin(bottom - top, qRound(m_field.height() * 3.2));
    if (height <= 0) return {};
    return QRect(area.left(), bottom - height, area.width(), height);
}

QRect BoardGeometry::pieceBoxLabel(const QRect& box)
{
    if (box.isEmpty()) return {};
    return QRect(box.left(), box.top(), box.width(), qMax(1, box.height() / 9));
}

QRect BoardGeometry::pieceBoxSideSelector(const QRect& box)
{
    if (box.isEmpty()) return {};
    const QRect label = pieceBoxLabel(box);
    return QRect(box.left(), label.bottom() + 1, box.width(), qMax(1, box.height() / 8));
}

QRect BoardGeometry::pieceBoxCell(const QRect& box, int rank)
{
    if (box.isEmpty() || rank < 1 || rank > 8) return {};
    const int labelHeight = pieceBoxLabel(box).height() + pieceBoxSideSelector(box).height();
    const int index = 8 - rank;
    const int x = (index % 2) * box.width() / 2;
    const int y = (index / 2) * (box.height() - labelHeight) / 4;
    const int nextX = (index % 2 + 1) * box.width() / 2;
    const int nextY = (index / 2 + 1) * (box.height() - labelHeight) / 4;
    return QRect(box.left() + x, box.top() + labelHeight + y, nextX - x, nextY - y);
}
