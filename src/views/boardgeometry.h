#ifndef BOARDGEOMETRY_H
#define BOARDGEOMETRY_H

/// @file boardgeometry.h
/// @brief 盤・駒台・座標の文字・対局者カードの位置（ウィジェット座標の値）

#include <QFont>
#include <QPoint>
#include <QRect>
#include <QRectF>
#include <QSize>

#include <array>

#include "boardsquaresize.h"

#include "shogitypes.h"

/**
 * @brief 盤面のウィジェット上の位置を、マスの大きさ・反転・筋と段の数から1度だけ計算した値
 *
 * 盤のマス・駒台とそのセル・筋と段の文字の帯・木肌・ウィジェット全体の大きさを、すべてウィジェット座標で持つ。
 * 描画・入力・ハイライト・対局者カードの配置は、この値を読むだけにする（補いの計算やオフセットの足し上げをしない）。
 *
 * 駒台は2列4段で、先手の駒台（反転していないとき）は左の列が上から飛・金・桂・歩、右の列が王・角・銀・香。
 * 後手の駒台と反転したときの駒台は、その並びを180度回したもの。
 */
class BoardGeometry
{
public:
    /// マスの縦横比（実際の将棋盤 ≈ 1.098）
    static constexpr double kSquareAspectRatio = 34.8 / 31.7;

    /// 計算の条件
    struct Params {
        int squareSize = BoardSquareSize::kDefault;   ///< マスの幅（px。範囲は core の BoardSquareSize）
        double standGapCols = 0.7;    ///< 盤と駒台の間（マスの幅の何倍か）
        bool flipped = false;         ///< 反転表示（後手を手前にする）
        int files = 9;
        int ranks = 9;
        QFont font;                   ///< 段の文字の幅を測る書体
        /// 駒台の大きさ（マスの幅・高さの何倍か）。盤は2列4段で、standCell() はこの並びのときだけ使える。
        /// 詰将棋問題一覧のプレビューは持ち駒を1列に並べ、枚数を横に書くので、幅を広く段を多くする
        double standColumns = 2.0;
        int standRows = 4;
    };

    BoardGeometry();
    explicit BoardGeometry(const Params& params);

    const Params& params() const { return m_params; }
    int squareSize() const { return m_params.squareSize; }
    bool flipped() const { return m_params.flipped; }
    int files() const { return m_params.files; }
    int ranks() const { return m_params.ranks; }
    /// 1マスの大きさ（縦長）
    QSize fieldSize() const { return m_field; }

    /// ウィジェット全体の大きさ（盤・駒台・座標の帯・影・外側の余白）
    QSize viewSize() const { return m_viewSize; }
    /// 盤のマス目の範囲
    QRect board() const { return m_board; }
    /// 盤の木肌（四辺に座標の帯を含む。反転しても位置は変わらない）
    QRectF surface() const { return m_surface; }
    /// 筋・段の文字の帯の幅
    int coordinateBand() const { return m_coordinateBand; }
    /// 筋・段の文字の大きさ（px）
    int labelPixelSize() const { return m_labelPixelSize; }

    /// 盤のマス（筋・段は 1 から。範囲外なら空）
    QRect square(int file, int rank) const;
    /// 点がある盤のマス。盤の外なら QPoint()
    QPoint squareAt(const QPoint& pos) const;
    /// 筋の文字の帯（反転していなければ盤の上、反転していれば下）
    QRect fileLabel(int file) const;
    /// 段の文字の帯（反転していなければ盤の右、反転していれば左）
    QRect rankLabel(int rank) const;

    /// 駒台（反転していなければ先手は盤の右で下端を揃え、後手は盤の左で上端を揃える。反転すると入れ替わる）
    QRect stand(Turn side) const { return side == Turn::Black ? m_blackStand : m_whiteStand; }
    /// 駒（成駒は生駒）を置く駒台のセル（2列4段の駒台のとき）。駒台に置けない駒なら空
    QRect standCell(Piece piece) const;
    /// 駒台に並ぶ駒（先手は先手の生駒8つ、後手は後手の生駒8つ）
    static const std::array<Piece, 8>& standPieces(Turn side);
    /// 駒台・駒箱の木肌（左右を少し内側へ寄せる）
    QRectF standSurface(const QRect& area) const;
    /// 駒台のセルの中で駒を描く範囲
    QRectF standPieceArea(const QRect& cell) const;

    /**
     * @brief 対局者カード（駒台の外側。反転していなければ先手は駒台の上、後手は下）
     * @param cardHeight カードの高さ（名前と時計の書体で決まる）
     * @param areaHeight カードを収める高さ（ウィジェットの高さ）
     */
    QRect playerCard(Turn side, int cardHeight, int areaHeight) const;
    /// 駒台とカード・駒箱の間
    int standGap() const;

    /**
     * @brief 局面編集の駒箱（左の駒台の外側の対局者カードのさらに下、盤の下端に揃える）
     * @param cardHeight 対局者カードの高さ（カードを出していなければ 0）
     * @param areaHeight カードを収める高さ（ウィジェットの高さ）
     */
    QRect pieceBox(int cardHeight, int areaHeight) const;

    /// 駒箱（反転によらず左上から王・飛・角・金・銀・桂・香・歩の2列4段）の見出し・先後の切り替え・セル
    static QRect pieceBoxLabel(const QRect& box);
    static QRect pieceBoxSideSelector(const QRect& box);
    static QRect pieceBoxCell(const QRect& box, int rank);

private:
    /// 盤の左上を原点にしたマス
    QRect boardLocalSquare(int file, int rank) const;

    Params m_params;
    QSize m_field;
    QSize m_viewSize;
    QRect m_board;
    QRectF m_surface;
    QRect m_blackStand;
    QRect m_whiteStand;
    int m_coordinateBand = 0;
    int m_labelPixelSize = 8;
};

#endif // BOARDGEOMETRY_H
