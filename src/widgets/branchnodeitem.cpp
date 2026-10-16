/// @file branchnodeitem.cpp
/// @brief 分岐ツリーの図の1手の図形の実装

#include "branchnodeitem.h"
#include "branchtreeitemroles.h"

#include <QBrush>
#include <QCoreApplication>
#include <QFontMetrics>
#include <QGraphicsEllipseItem>
#include <QGraphicsSimpleTextItem>
#include <QPainterPath>

namespace {
constexpr qreal kRadius = 8.0;
constexpr qreal kMarkerSize = 8.0;
constexpr int kTooltipCommentChars = 400;
const QColor kCurrentFill(255, 235, 80);
} // namespace

BranchNodeItem::BranchNodeItem(int nodeId, int row, int ply, const Content& content, const QPointF& center)
    : m_nodeId(nodeId)
    , m_row(row)
    , m_ply(ply)
    , m_fill(content.fill)
    , m_pen(Qt::black, content.start ? 1.4 : 1.2)
    , m_moveTip(content.tooltip)
{
    const QFontMetrics metrics(content.font);
    const qreal padX = content.start ? 14.0 : 12.0;
    const qreal padY = content.start ? 8.0 : 6.0;
    const qreal width = qMax<qreal>(content.start ? 84.0 : 70.0, metrics.horizontalAdvance(content.label) + padX * 2);
    const qreal height = qMax<qreal>(content.start ? 26.0 : 24.0, metrics.height() + padY * 2);
    m_frame = QRectF(center.x() - width / 2.0, center.y() - height / 2.0, width, height);

    QPainterPath path;
    path.addRoundedRect(m_frame, kRadius, kRadius);
    setPath(path);

    auto* text = new QGraphicsSimpleTextItem(content.label, this);
    text->setFont(content.font);
    const QRectF textRect = text->boundingRect();
    text->setPos(m_frame.center().x() - textRect.width() / 2.0, m_frame.center().y() - textRect.height() / 2.0);

    // 自動化 API が読む（図の中では型付きの値を使う）
    setData(BranchTreeItemRoles::NodeId, nodeId);
    setData(BranchTreeItemRoles::Row, row);
    setData(BranchTreeItemRoles::Ply, ply);

    applyStyle();
    updateToolTip();
}

void BranchNodeItem::setCurrent(bool current)
{
    if (m_current == current) return;
    m_current = current;
    applyStyle();
}

void BranchNodeItem::applyStyle()
{
    QPen pen = m_pen;
    if (m_current) pen.setWidthF(1.8);
    setPen(pen);
    setBrush(m_current ? kCurrentFill : m_fill);
    setZValue(m_current ? 20 : 10);
}

void BranchNodeItem::setComment(const QString& comment)
{
    m_comment = comment.trimmed();
    if (!m_comment.isEmpty() && !m_commentMarker) {
        m_commentMarker = new QGraphicsEllipseItem(m_frame.right() - kMarkerSize - 3.0, m_frame.top() + 3.0,
                                                   kMarkerSize, kMarkerSize, this);
        m_commentMarker->setBrush(QColor(60, 130, 220));
        m_commentMarker->setPen(QPen(Qt::white, 1.0));
    }
    if (m_commentMarker) m_commentMarker->setVisible(!m_comment.isEmpty());
    updateToolTip();
}

void BranchNodeItem::setCollapsedBadge(int hiddenCount, const QColor& textColor)
{
    m_collapsed = true;
    m_pen.setStyle(Qt::DashLine);
    auto* badge = new QGraphicsSimpleTextItem(QStringLiteral("▸ +%1").arg(hiddenCount), this);
    QFont font = badge->font();
    font.setPointSize(9);
    badge->setFont(font);
    badge->setBrush(textColor);
    const QRectF badgeRect = badge->boundingRect();
    badge->setPos(m_frame.right() + 6.0, m_frame.center().y() - badgeRect.height() / 2.0);
    applyStyle();
    updateToolTip();
}

void BranchNodeItem::updateToolTip()
{
    QString tip = m_moveTip;
    if (!m_comment.isEmpty()) {
        QString shown = m_comment.left(kTooltipCommentChars);
        if (m_comment.size() > kTooltipCommentChars) shown += QStringLiteral("…");
        if (!tip.isEmpty()) tip += QStringLiteral("<hr/>");
        tip += QStringLiteral("<p style='white-space:pre-wrap'>%1</p>").arg(shown.toHtmlEscaped());
    }
    if (m_collapsed)
        tip += QCoreApplication::translate("BranchTreeView", "<p>（ダブルクリックで展開）</p>");
    setToolTip(tip.isEmpty() ? QString() : QStringLiteral("<qt>%1</qt>").arg(tip));
}
