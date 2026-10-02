#ifndef SHOGIVIEWINTERACTION_H
#define SHOGIVIEWINTERACTION_H

#include "boardvisuals.h"

/// @file shogiviewinteraction.h
/// @brief 将棋盤面のマウス操作・ドラッグの責務を担うクラスの定義

#include "shogitypes.h"

#include <QChar>
#include <QIcon>
#include <QMap>
#include <QPoint>
#include <QRect>

class QPainter;
class ShogiBoard;
class ShogiViewLayout;

/// マウスクリック座標変換・ドラッグ操作の状態管理を担当するクラス。
/// QObject を継承せず、ShogiView が値メンバとして保持する。
class ShogiViewInteraction
{
public:
    ShogiViewInteraction();

    // ───────────────────────── 入力座標変換 ─────────────────────────
    QPoint clickedSquare(const QPoint& clickPosition,
                            const ShogiViewLayout& layout, ShogiBoard* board,
                            const QRect& pieceBox = {}) const;
    QPoint getClickedSquareInDefaultState(const QPoint& clickPosition,
                                          const ShogiViewLayout& layout, ShogiBoard* board) const;
    QPoint getClickedSquareInFlippedState(const QPoint& clickPosition,
                                          const ShogiViewLayout& layout, ShogiBoard* board) const;

    // ───────────────────────── ドラッグ操作 ─────────────────────────
    void startDrag(const QPoint& from, ShogiBoard* board,
                   const QPoint& cursorWidgetPos);
    void endDrag();
    void drawDraggingPiece(QPainter& painter, const ShogiViewLayout& layout,
                           const QMap<QChar, QIcon>& pieces, const BoardVisuals& visuals);

    // ───────────────────────── ドラッグ位置更新 ─────────────────────
    void updateDragPos(const QPoint& pos);

    // ───────────────────────── モード設定 ─────────────────────────
    void setMouseClickMode(bool mouseClickMode);
    void setPositionEditMode(bool positionEditMode);

    // ───────────────────────── 状態アクセサ ─────────────────────────
    bool mouseClickMode() const { return m_mouseClickMode; }
    bool positionEditMode() const { return m_positionEditMode; }
    bool dragging() const { return m_dragging; }
    QPoint dragFrom() const { return m_dragFrom; }
    const QMap<Piece, int>& tempPieceStandCounts() const { return m_tempPieceStandCounts; }

private:
    bool   m_mouseClickMode   = true;
    bool   m_positionEditMode = false;
    bool   m_dragging         = false;
    QPoint m_dragFrom;
    Piece  m_dragPiece        = Piece::None;
    QPoint m_dragPos;
    QMap<Piece, int> m_tempPieceStandCounts;
};

#endif // SHOGIVIEWINTERACTION_H
