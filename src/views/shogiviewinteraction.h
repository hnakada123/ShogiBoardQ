#ifndef SHOGIVIEWINTERACTION_H
#define SHOGIVIEWINTERACTION_H

#include "boardvisuals.h"

/// @file shogiviewinteraction.h
/// @brief 将棋盤面のドラッグ操作の状態と持ち上げた駒の描画を担うクラスの定義

#include "shogitypes.h"

#include <QIcon>
#include <QMap>
#include <QPoint>
#include <QRect>

class QPainter;
class ShogiBoard;
class ShogiViewLayout;

/// ドラッグ操作の状態（つまんだ駒・位置・持駒の一時的な枚数）と、持ち上げた駒の描画を担当するクラス。
/// QObject を継承せず、ShogiView が値メンバとして保持する。
class ShogiViewInteraction
{
public:
    ShogiViewInteraction();

    // ───────────────────────── ドラッグ操作 ─────────────────────────
    void startDrag(const QPoint& from, ShogiBoard* board,
                   const QPoint& cursorWidgetPos);
    void endDrag();
    /// ドラッグ中の駒（icon は dragPiece() の画像）をポインタの位置に描く
    void drawDraggingPiece(QPainter& painter, const ShogiViewLayout& layout,
                           const QIcon& icon, const BoardVisuals& visuals) const;
    /// ドラッグ中の駒が塗る範囲（ウィジェット座標）。ドラッグ中でなければ空。
    QRect draggingPieceRect(const ShogiViewLayout& layout, const BoardVisuals& visuals) const;

    // ───────────────────────── ドラッグ位置更新 ─────────────────────
    void updateDragPos(const QPoint& pos);

    // ───────────────────────── モード設定 ─────────────────────────
    void setPositionEditMode(bool positionEditMode);
    void setPieceBoxSide(Turn side);
    Turn pieceBoxSide() const { return m_pieceBoxSide; }

    // ───────────────────────── 状態アクセサ ─────────────────────────
    bool positionEditMode() const { return m_positionEditMode; }
    bool dragging() const { return m_dragging; }
    QPoint dragFrom() const { return m_dragFrom; }
    /// ドラッグ中の駒（駒箱からは選んでいる側の駒）
    Piece dragPiece() const { return m_dragPiece; }
    const QMap<Piece, int>& tempPieceStandCounts() const { return m_tempPieceStandCounts; }

private:
    QRect draggingCellRect(const ShogiViewLayout& layout) const;

    bool   m_positionEditMode = false;

    bool   m_dragging         = false;
    QPoint m_dragFrom;
    Piece  m_dragPiece        = Piece::None;
    Turn   m_pieceBoxSide     = Turn::Black;
    QPoint m_dragPos;
    QMap<Piece, int> m_tempPieceStandCounts;
};

#endif // SHOGIVIEWINTERACTION_H
