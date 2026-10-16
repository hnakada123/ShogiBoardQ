#ifndef BRANCHNODEITEM_H
#define BRANCHNODEITEM_H

/// @file branchnodeitem.h
/// @brief 分岐ツリーの図の1手（開始局面を含む）の図形

#include <QColor>
#include <QFont>
#include <QGraphicsPathItem>
#include <QPen>
#include <QString>

class QGraphicsEllipseItem;
class QGraphicsSimpleTextItem;

/**
 * @brief 分岐ツリーの図の1手（角の丸い枠と手の表記・コメントの印・折りたたみの札）
 *
 * どの手を描いたかは KifuBranchNode の ID だけで持つ。行（図の中の変化）と手数は、
 * 配置とキー操作のために図が使う。自動化 API は QGraphicsItem::data の BranchTreeItemRoles で読む。
 */
class BranchNodeItem : public QGraphicsPathItem
{
public:
    enum { Type = UserType + 0x501 };

    /// 描く中身
    struct Content {
        QString label;          ///< 枠の中の文字
        QString tooltip;        ///< 手の説明（HTML。コメントは setComment で足す）
        QFont font;             ///< 枠の中の文字の書体
        QColor fill;            ///< 枠の色
        bool start = false;     ///< 開始局面（少し大きく、太い枠で描く）
    };

    BranchNodeItem(int nodeId, int row, int ply, const Content& content, const QPointF& center);

    int type() const override { return Type; }

    int nodeId() const { return m_nodeId; }
    /// 描いた行（変化）。開始局面は 0
    int row() const { return m_row; }
    int ply() const { return m_ply; }
    /// 枠（シーン座標）
    QRectF frame() const { return mapRectToScene(m_frame); }

    /// 表示中の手として目立たせるか
    void setCurrent(bool current);
    bool isCurrent() const { return m_current; }
    /// コメント（あれば右上に印を付け、説明に足す）
    void setComment(const QString& comment);
    /// 折りたたんだ変化の先頭として、隠れている手の数を出す
    void setCollapsedBadge(int hiddenCount, const QColor& textColor);

private:
    void updateToolTip();
    void applyStyle();

    int m_nodeId = -1;
    int m_row = 0;
    int m_ply = 0;
    QRectF m_frame;
    QColor m_fill;
    QPen m_pen;                                    ///< 表示中でないときの枠の線
    QString m_moveTip;
    QString m_comment;
    bool m_collapsed = false;
    bool m_current = false;
    QGraphicsEllipseItem* m_commentMarker = nullptr;
};

#endif // BRANCHNODEITEM_H
