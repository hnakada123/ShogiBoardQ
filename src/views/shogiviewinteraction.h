#ifndef SHOGIVIEWINTERACTION_H
#define SHOGIVIEWINTERACTION_H

/// @file shogiviewinteraction.h
/// @brief 将棋盤面のマウス操作・ドラッグの責務を担うクラスの定義

#include <QChar>
#include <QIcon>
#include <QMap>
#include <QPoint>

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
    QPoint getClickedSquare(const QPoint& clickPosition,
                            const ShogiViewLayout& layout, ShogiBoard* board) const;
    QPoint getClickedSquareInDefaultState(const QPoint& clickPosition,
                                          const ShogiViewLayout& layout, ShogiBoard* board) const;
    QPoint getClickedSquareInFlippedState(const QPoint& clickPosition,
                                          const ShogiViewLayout& layout, ShogiBoard* board) const;

    // ───────────────────────── ドラッグ操作 ─────────────────────────
    void startDrag(const QPoint& from, ShogiBoard* board,
                   const QPoint& cursorWidgetPos);
    void endDrag();
    void drawDraggingPiece(QPainter& painter, const ShogiViewLayout& layout,
                           const QMap<QChar, QIcon>& pieces);

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
    QChar dragPiece() const { return m_dragPiece; }
    QPoint dragPos() const { return m_dragPos; }
    bool dragFromStand() const { return m_dragFromStand; }
    const QMap<QChar, int>& tempPieceStandCounts() const { return m_tempPieceStandCounts; }

private:
    bool   m_mouseClickMode   = true;
    bool   m_positionEditMode = false;
    bool   m_dragging         = false;
    QPoint m_dragFrom;
    QChar  m_dragPiece        = ' ';
    QPoint m_dragPos;
    bool   m_dragFromStand    = false;
    QMap<QChar, int> m_tempPieceStandCounts;
};

#endif // SHOGIVIEWINTERACTION_H
