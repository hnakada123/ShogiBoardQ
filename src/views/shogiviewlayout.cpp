/// @file shogiviewlayout.cpp
/// @brief 将棋盤面のレイアウト計算クラスの実装

#include "shogiviewlayout.h"

#include <QFontMetrics>
#include <QStringList>

#include <algorithm>

ShogiViewLayout::ShogiViewLayout() = default;

// ─────────────────────────── レイアウト再計算 ────────────────────────────
void ShogiViewLayout::recalcLayoutParams(const QFont& baseFont)
{
    constexpr int tweak = 0;

    // マスサイズ（実際の将棋盤の縦横比率を反映）
    const int squareWidth  = m_squareSize;
    const int squareHeight = qRound(m_squareSize * kSquareAspectRatio);
    m_fieldSize = QSize(squareWidth, squareHeight);

    // 将棋盤余白
    m_boardMarginPx = qMax(2, qRound(m_squareSize * 0.22));

    // ラベル関連
    m_labelBandPx = std::max(10, int(m_squareSize * 0.68));
    m_labelFontPt = std::clamp(m_squareSize * 0.26, 5.0, 18.0);
    m_labelGapPx  = std::max(2,  int(m_squareSize * 0.12));

    // 筋番号帯の高さ
    const int fileLabelHeight = std::max(8, int(m_squareSize * 0.35));

    // 縦方向オフセット
    m_offsetY = fileLabelHeight + m_boardMarginPx;

    // 駒台の実効ギャップ
    const int userGapPx = qRound(m_squareSize * m_standGapCols);
    const int needGapPx = minGapForRankLabelsPx(baseFont);
    const int effGapPx  = std::max(userGapPx, needGapPx);
    const double effCols = double(effGapPx) / double(m_squareSize);

    // 水平オフセット
    m_param1     = qRound((2.0 + effCols) * m_squareSize) - tweak;
    m_param2     = qRound((9.0 + effCols) * m_squareSize) - tweak;
    m_offsetX    = m_param1 + tweak;
    m_standGapPx = qRound(effCols * m_squareSize);
}

void ShogiViewLayout::fitBoardToWidget()
{
    // Fixed SizePolicy方式では使用しない
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

QRect ShogiViewLayout::calculateRectangleForRankOrFileLabel(
    int file, int rank, int boardRanks) const
{
    const QSize fs = m_fieldSize;
    int adjustedX = (file - 1) * fs.width();

    int adjustedY;
    if (m_flipMode) {
        adjustedY = (boardRanks - rank) * fs.height();
    } else {
        adjustedY = (rank - 1) * fs.height();
    }

    return QRect(adjustedX, adjustedY, fs.width(), fs.height()).translated(30, 0);
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
    const int y = cell.top() + m_offsetY;
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
    const int y = cell.top() + m_offsetY;
    const int w = fs.width() * 2;
    const int h = fs.height() * rows;

    return QRect(x, y, w, h);
}

// ─────────────────────────── 境界ユーティリティ ─────────────────────────
int ShogiViewLayout::boardRightPx(int boardFiles) const
{
    const QSize fs = m_fieldSize;
    return m_offsetX + fs.width() * boardFiles;
}

int ShogiViewLayout::standInnerEdgePx(bool rightSide, int boardFiles) const
{
    const int gap = m_standGapPx;
    return rightSide ? (boardRightPx(boardFiles) + gap)
                     : (boardLeftPx()            - gap);
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
