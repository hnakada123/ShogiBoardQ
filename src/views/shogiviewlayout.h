#ifndef SHOGIVIEWLAYOUT_H
#define SHOGIVIEWLAYOUT_H

/// @file shogiviewlayout.h
/// @brief 将棋盤面のレイアウト計算クラスの定義

#include <QFont>
#include <QPoint>
#include <QRect>
#include <QSize>

#include "shogitypes.h"

#include <array>

/// 盤面・駒台・ラベルの座標/寸法を算出する純粋なレイアウトクラス。
/// QObject を継承せず、描画やウィジェット操作には一切関与しない。
/// ShogiView が値メンバとして保持し、描画時に座標を問い合わせる。
class ShogiViewLayout
{
public:
    ShogiViewLayout();

    // ───────────────────────── レイアウト再計算 ─────────────────────────
    /// m_squareSize / m_standGapCols を元に内部レイアウト定数を再算出する。
    /// @param baseFont  minGapForRankLabelsPx 内のフォント計測に使う基準フォント
    void recalcLayoutParams(const QFont& baseFont);

    // ───────────────────────── 座標計算 ─────────────────────────────────
    /// 盤座標 (file, rank) → マス1つ分の矩形（ウィジェット座標ではなく盤ローカル）
    QRect calculateSquareRectangleBasedOnBoardState(int file, int rank,
                                                    int boardFiles, int boardRanks) const;
    /// calculateSquareRectangleBasedOnBoardState の逆。盤ローカルの点があるマス。盤の外なら QPoint()
    QPoint squareAt(const QPoint& boardLocal, int boardFiles, int boardRanks) const;

    /// 先手（黒）側駒台の外接矩形
    QRect blackStandBoundingRect(int boardFiles, int boardRanks) const;

    /// 後手（白）側駒台の外接矩形
    QRect whiteStandBoundingRect(int boardFiles, int boardRanks) const;

    /// 駒台のセル。駒台は盤の基準マス（筋 1・2、先手は段 6〜9、後手は段 1〜4）を駒台側へ
    /// 水平にずらして描く。baseField はその基準マスの矩形（盤の余白を足す前）
    QRect standCellRect(bool black, const QRect& baseField) const;
    /// 駒台の基準マスに置く駒。駒台のセルでなければ Piece::None
    static Piece standPieceAt(bool black, int file, int rank);
    /// 駒（成駒は生駒）を置く駒台の基準マス。駒台に置けない駒なら QPoint()
    static QPoint standBaseCellOf(Piece piece);
    /// 駒台の基準マス8つ（先手は筋1・2、段6〜9。後手は筋1・2、段1〜4）
    static std::array<QPoint, 8> standBaseCells(bool black);

    /// 駒箱は反転によらず左上から王・飛・角・金・銀・桂・香・歩の2列4段。
    static QRect pieceBoxCellRect(const QRect& box, int rank);
    static QRect pieceBoxLabelRect(const QRect& box);
    static QRect pieceBoxSideSelectorRect(const QRect& box);

    /// 木製部分と全体サイズは同じ余白・座標帯・影の寸法から算出する。
    QRectF boardSurfaceRect(int boardFiles, int boardRanks) const;
    QSize viewSize(int boardFiles, int boardRanks) const;
    int coordinateBandPx() const;
    int outerMarginPx() const;
    QRectF standSurfaceRect(const QRect& stand) const;
    QRectF standPieceArea(const QRect& cell) const;

    // ───────────────────────── 盤・駒台の境界ユーティリティ ────────────
    int boardLeftPx() const;
    int boardRightPx(int boardFiles) const;
    int minGapForRankLabelsPx(const QFont& baseFont) const;

    // ───────────────────────── セッター ─────────────────────────────────
    void setStandGapCols(double cols);
    void setNameFontScale(double scale);
    void setRankFontScale(double scale);

    // ───────────────────────── flipMode アクセサ ────────────────────────
    bool flipMode() const;
    void setFlipMode(bool flip);

    // ───────────────────────── squareSize アクセサ ──────────────────────
    int  squareSize() const;
    void setSquareSize(int size);

    // ───────────────────────── fieldSize アクセサ ──────────────────────
    QSize fieldSize() const;
    void  setFieldSize(const QSize& size);
    int   boardMarginPx() const;
    int   param1() const;
    int   param2() const;
    int   offsetX() const;
    int   offsetY() const;
    int   labelGapPx() const;
    double nameFontScale() const;
    double rankFontScale() const;
    double standGapCols() const;
    int   standGapPx() const;

    /// 将棋盤の実際の縦横比率（≈ 1.098）
    static constexpr double kSquareAspectRatio = 34.8 / 31.7;

private:
    int    m_squareSize   { 50 };
    QSize  m_fieldSize;
    int    m_boardMarginPx{ 0 };
    int    m_param1       { 0 };
    int    m_param2       { 0 };
    int    m_offsetX      { 0 };
    int    m_labelGapPx   { 8 };
    double m_labelFontPt  { 12.0 };
    double m_nameFontScale{ 0.36 };
    double m_rankFontScale{ 0.8 };
    double m_standGapCols { 0.6 };
    int    m_standGapPx   { 0 };
    bool   m_flipMode     { false };
};

// ─────────────────────────── インラインゲッター ──────────────────────────
inline bool   ShogiViewLayout::flipMode()      const { return m_flipMode; }
inline int    ShogiViewLayout::squareSize()    const { return m_squareSize; }
inline QSize  ShogiViewLayout::fieldSize()     const { return m_fieldSize; }
inline int    ShogiViewLayout::boardMarginPx() const { return m_boardMarginPx; }
inline int    ShogiViewLayout::param1()        const { return m_param1; }
inline int    ShogiViewLayout::param2()        const { return m_param2; }
inline int    ShogiViewLayout::offsetX()       const { return m_offsetX; }
inline int    ShogiViewLayout::labelGapPx()    const { return m_labelGapPx; }
inline double ShogiViewLayout::nameFontScale() const { return m_nameFontScale; }
inline double ShogiViewLayout::rankFontScale() const { return m_rankFontScale; }
inline double ShogiViewLayout::standGapCols()  const { return m_standGapCols; }
inline int    ShogiViewLayout::standGapPx()    const { return m_standGapPx; }
inline int    ShogiViewLayout::boardLeftPx()   const { return m_offsetX; }

#endif // SHOGIVIEWLAYOUT_H
