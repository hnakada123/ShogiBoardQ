/// @file shogiviewhighlighting.cpp
/// @brief 将棋盤面のハイライト・矢印の実装

#include "shogiviewhighlighting.h"
#include "shogiview.h"

#include <QLineF>
#include <QPainter>
#include <QtMath>

#include <utility>

ShogiViewHighlighting::ShogiViewHighlighting(ShogiView* view, QObject* parent)
    : QObject(parent)
    , m_view(view)
{
}

// ─────────────────────────────────────────────────────────────────────────────
// ハイライト
// ─────────────────────────────────────────────────────────────────────────────


namespace {

/// 矢印の太さ・矢じり・順位の丸印の大きさ（マスの大きさで決まる。描くときと描き直す範囲で同じ値を使う）
struct ArrowMetrics {
    explicit ArrowMetrics(int squareSize)
        : width(qMax(3, squareSize / 12))
        , headSize(qMax(10, squareSize / 4))
        , labelFontSize(qMax(10, squareSize / 4))
        , circleRadius(static_cast<int>(labelFontSize * 0.8))
    {
    }
    int width;
    int headSize;
    int labelFontSize;
    int circleRadius;
};

/// 矢印の始点と終点のマス。駒打ちは駒台のセルを始点にし、盤上の移動と同じ矢印で描く
std::pair<QRect, QRect> arrowEnds(const BoardGeometry& geometry, const ShogiViewHighlighting::Arrow& arrow)
{
    const QRect from = (arrow.fromFile == 0 || arrow.fromRank == 0)
        ? geometry.standCell(arrow.dropPiece)
        : geometry.square(arrow.fromFile, arrow.fromRank);
    return {from, geometry.square(arrow.toFile, arrow.toRank)};
}

} // namespace

void ShogiViewHighlighting::setHighlights(BoardHighlightLayer layer, const QList<QPoint>& squares)
{
    QList<QPoint>& current = m_layers[static_cast<std::size_t>(layer)];
    if (current == squares) return;
    // 駒を選ぶたびに合法な移動先を数十マス足し引きするので、前と後のマスだけを描き直す
    updateSquares(current);
    current = squares;
    updateSquares(current);
}

const QList<QPoint>& ShogiViewHighlighting::highlights(BoardHighlightLayer layer) const
{
    return m_layers[static_cast<std::size_t>(layer)];
}

void ShogiViewHighlighting::updateSquares(const QList<QPoint>& squares)
{
    for (const QPoint& square : squares) {
        const QRect rect = m_view->fieldRect(square);
        if (rect.isEmpty()) {
            m_view->update();
            return;
        }
        m_view->update(rect);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// 矢印管理
// ─────────────────────────────────────────────────────────────────────────────

void ShogiViewHighlighting::setArrows(const QList<ShogiViewHighlighting::Arrow>& arrows)
{
    // エンジンの候補手が届くたびに呼ばれるため、矢印が変わらなければ描き直さない。
    // 変わったときも、変わった矢印（消える矢印と新しい矢印）が掛かる範囲だけを描き直す。前後の矢印を
    // まとめて1つの矩形で囲むと、候補手が1つ替わるだけで盤の半分を描き直す（検討で候補手を3つ表示して
    // いると、1倍で約22万画素）。
    if (m_arrows == arrows) return;
    const QList<Arrow> previous = std::exchange(m_arrows, arrows);
    if (!m_view->board()) {
        m_view->update();
        return;
    }
    QRegion dirty;
    if (keepsCommonArrows(previous, arrows)) {
        QList<Arrow> changed;
        for (const Arrow& arrow : previous)
            if (!arrows.contains(arrow)) changed.append(arrow);
        for (const Arrow& arrow : arrows)
            if (!previous.contains(arrow)) changed.append(arrow);
        dirty = arrowsPaintRegion(changed);
    } else {
        dirty = arrowsPaintRegion(previous) | arrowsPaintRegion(arrows);
    }
    if (!dirty.isEmpty()) m_view->update(dirty);
}

bool ShogiViewHighlighting::keepsCommonArrows(const QList<Arrow>& before, const QList<Arrow>& after)
{
    // 順位の数字は矢印が2本以上のときだけ描くので、1本と2本以上の間で変わると全部の矢印の見た目が変わる
    if ((before.size() >= 2) != (after.size() >= 2)) return false;
    // 半透明の矢印は重なった所の色が描く順で決まる。前後に共通の矢印が同じ順に並んでいれば、共通の矢印
    // どうしが重なる所の見た目は変わらない（同じ矢印が2本ある一覧は扱わない）
    QList<Arrow> commonBefore;
    for (qsizetype i = 0; i < before.size(); ++i) {
        if (before.indexOf(before.at(i)) != i) return false;
        if (after.contains(before.at(i))) commonBefore.append(before.at(i));
    }
    QList<Arrow> commonAfter;
    for (qsizetype i = 0; i < after.size(); ++i) {
        if (after.indexOf(after.at(i)) != i) return false;
        if (before.contains(after.at(i))) commonAfter.append(after.at(i));
    }
    return commonBefore == commonAfter;
}

QRegion ShogiViewHighlighting::arrowsPaintRegion(const QList<ShogiViewHighlighting::Arrow>& arrows) const
{
    if (!m_view->board()) return {};
    const BoardGeometry& geometry = m_view->boardGeometry();
    // 線・矢じり・順位の丸印は、始点と終点のマスの中心を結ぶ線分から、この幅の中に収まる
    // （描く矢印と同じ太さ・矢じり・丸印の大きさに、アンチエイリアスの分を足す）
    const ArrowMetrics metrics(geometry.squareSize());
    const int margin = qMax(metrics.headSize, metrics.circleRadius) + metrics.width + 2;
    QRegion region;
    for (const ShogiViewHighlighting::Arrow& arrow : arrows) {
        const auto [fromRect, toRect] = arrowEnds(geometry, arrow);
        if (fromRect.isEmpty() || toRect.isEmpty()) continue;
        // 斜めの矢印を1つの矩形で囲むと、矢印の無い所まで広く含む。線分をマス1つ分ほどに区切り、
        // 区切りごとの矩形を合わせる
        const QPointF from(fromRect.center());
        const QPointF to(toRect.center());
        const int pieces = qMax(1, qCeil(QLineF(from, to).length() / qMax(1, geometry.squareSize())));
        QPointF start = from;
        for (int piece = 1; piece <= pieces; ++piece) {
            const QPointF end = from + (to - from) * (static_cast<qreal>(piece) / pieces);
            region += QRectF(start, end).normalized().toAlignedRect().adjusted(-margin, -margin, margin, margin);
            start = end;
        }
    }
    return region;
}

void ShogiViewHighlighting::drawHighlights(QPainter& painter, const QRegion& dirty)
{
    if (!m_view->board()) return;

    // 層ごとの色。移動元は木の盤になじむ黄緑系、移動先・選択は橙系、合法な移動先は青で縁取る。
    // マスの内側だけを塗り、盤の罫線と駒の視認性を保つ
    struct LayerStyle {
        QColor fill;
        QColor border;   ///< 無効なら縁取らない
    };
    static const std::array<LayerStyle, kLayerCount> kStyles{{
        {QColor(145, 163, 96, 155), QColor()},           // LastMoveFrom
        {QColor(238, 147, 68, 165), QColor()},           // LastMoveTo
        {QColor(238, 147, 68, 165), QColor()},           // Selection
        {QColor(65, 155, 225, 105), QColor(35, 105, 165, 200)},   // LegalDestinations
    }};

    painter.save();
    for (std::size_t layer = 0; layer < m_layers.size(); ++layer) {
        const LayerStyle& style = kStyles[layer];
        for (const QPoint& square : m_layers[layer]) {
            const QRect rect = m_view->fieldRect(square);
            if (rect.isNull() || !dirty.intersects(rect)) continue;
            const QRect inner = rect.adjusted(1, 1, -1, -1);
            painter.fillRect(inner, style.fill);
            if (style.border.isValid()) {
                painter.setPen(QPen(style.border, 2));
                painter.setBrush(Qt::NoBrush);
                painter.drawRect(inner.adjusted(1, 1, -1, -1));
            }
        }
    }
    painter.restore();
}

void ShogiViewHighlighting::drawArrows(QPainter& painter)
{
    if (m_arrows.isEmpty()) return;

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);

    const BoardGeometry& geometry = m_view->boardGeometry();
    const ArrowMetrics metrics(geometry.squareSize());
    const int arrowWidth = metrics.width;
    const int arrowHeadSize = metrics.headSize;
    for (const Arrow& arrow : std::as_const(m_arrows)) {
        const auto [fromRect, toRect] = arrowEnds(geometry, arrow);
        if (fromRect.isEmpty() || toRect.isEmpty()) continue;

        const QPointF from(fromRect.center());
        const QPointF to(toRect.center());

        QColor color = arrow.color;

        QPen pen(color);
        pen.setWidth(arrowWidth);
        pen.setCapStyle(Qt::RoundCap);
        pen.setStyle(arrow.penStyle);
        painter.setPen(pen);

        QPointF dir = to - from;
        double length = std::sqrt(dir.x() * dir.x() + dir.y() * dir.y());
        if (length < 1.0) continue;

        QPointF unit = dir / length;

        QPointF lineEnd = to - unit * (arrowHeadSize * 0.7);
        painter.drawLine(from, lineEnd);

        QPointF perpendicular(-unit.y(), unit.x());
        QPointF arrowPoint1 = to - unit * arrowHeadSize + perpendicular * (arrowHeadSize * 0.5);
        QPointF arrowPoint2 = to - unit * arrowHeadSize - perpendicular * (arrowHeadSize * 0.5);

        QPolygonF arrowHead;
        arrowHead << to << arrowPoint1 << arrowPoint2;

        painter.setPen(Qt::NoPen);
        painter.setBrush(color);
        painter.drawPolygon(arrowHead);

        if (arrow.priority >= 1 && m_arrows.size() >= 2) {
            QPointF center((from.x() + to.x()) / 2.0, (from.y() + to.y()) / 2.0);

            QFont font = painter.font();
            font.setPointSize(metrics.labelFontSize);
            font.setBold(true);
            painter.setFont(font);

            const int circleRadius = metrics.circleRadius;
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(255, 255, 255, 230));
            painter.drawEllipse(center, circleRadius, circleRadius);

            painter.setPen(arrow.color);
            QString priorityText = QString::number(arrow.priority);
            QRectF textRect(center.x() - circleRadius, center.y() - circleRadius,
                           circleRadius * 2, circleRadius * 2);
            painter.drawText(textRect, Qt::AlignCenter, priorityText);
        }
    }

    painter.restore();
}
