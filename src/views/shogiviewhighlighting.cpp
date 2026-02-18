/// @file shogiviewhighlighting.cpp
/// @brief 将棋盤面のハイライト・矢印・手番表示の実装

#include "shogiviewhighlighting.h"
#include "shogiboard.h"
#include "shogiviewlayout.h"

#include <QPainter>
#include <QLabel>
#include <QDebug>

Q_DECLARE_LOGGING_CATEGORY(lcView)

// 駒台セル矩形の計算ユーティリティ（shogiview.cpp と同一ロジック）
static inline QRect makeStandCellRect(bool flip, int param, int offsetX, int offsetY, const QRect& fieldRect, bool leftSide)
{
    QRect adjustedRect;

    if (flip) {
        adjustedRect.setRect(fieldRect.left() + (leftSide ? -param : +param) + offsetX,
                             fieldRect.top()  + offsetY,
                             fieldRect.width(),
                             fieldRect.height());
    } else {
        adjustedRect.setRect(fieldRect.left() + (leftSide ? +param : -param) + offsetX,
                             fieldRect.top()  + offsetY,
                             fieldRect.width(),
                             fieldRect.height());
    }

    return adjustedRect;
}

ShogiViewHighlighting::ShogiViewHighlighting(ShogiView* view, QObject* parent)
    : QObject(parent)
    , m_view(view)
{
}

// ─────────────────────────────────────────────────────────────────────────────
// ハイライト管理
// ─────────────────────────────────────────────────────────────────────────────

void ShogiViewHighlighting::addHighlight(ShogiView::Highlight* hl)
{
    m_highlights.append(hl);
    m_view->update();
}

void ShogiViewHighlighting::removeHighlight(ShogiView::Highlight* hl)
{
    m_highlights.removeOne(hl);
    m_view->update();
}

void ShogiViewHighlighting::removeHighlightAllData()
{
    qDeleteAll(m_highlights);
    m_highlights.clear();
    emit highlightsCleared();
    m_view->update();
}

// ─────────────────────────────────────────────────────────────────────────────
// 矢印管理
// ─────────────────────────────────────────────────────────────────────────────

void ShogiViewHighlighting::setArrows(const QVector<ShogiView::Arrow>& arrows)
{
    m_arrows = arrows;
    m_view->update();
}

void ShogiViewHighlighting::clearArrows()
{
    m_arrows.clear();
    m_view->update();
}

// ─────────────────────────────────────────────────────────────────────────────
// 手番ハイライト
// ─────────────────────────────────────────────────────────────────────────────

void ShogiViewHighlighting::applyTurnHighlight(bool blackIsActive)
{
    qCDebug(lcView) << "applyTurnHighlight: blackIsActive=" << blackIsActive
                     << "m_blackActive(before)=" << m_blackActive;

    m_blackActive = blackIsActive;
    m_urgency = Urgency::Normal;
    setUrgencyVisuals(Urgency::Normal);
}

void ShogiViewHighlighting::setActiveSide(bool blackTurn)
{
    qCDebug(lcView) << "setActiveSide: blackTurn=" << blackTurn;
    m_blackActive = blackTurn;
    applyTurnHighlight(m_blackActive);
}

void ShogiViewHighlighting::setHighlightStyle(const QColor& bgOn, const QColor& fgOn, const QColor& fgOff)
{
    m_highlightBg    = bgOn;
    m_highlightFgOn  = fgOn;
    m_highlightFgOff = fgOff;
    applyTurnHighlight(m_blackActive);
}

void ShogiViewHighlighting::clearTurnHighlight()
{
    if (m_view->gameOverStyleLock()) return;

    const QColor fg(51, 51, 51);
    const QColor bg(200, 190, 130);
    setLabelStyle(m_view->blackNameLabel(),  fg, bg, 0, QColor(0,0,0,0), /*bold=*/false);
    setLabelStyle(m_view->blackClockLabel(), fg, bg, 0, QColor(0,0,0,0), /*bold=*/false);
    setLabelStyle(m_view->whiteNameLabel(),  fg, bg, 0, QColor(0,0,0,0), /*bold=*/false);
    setLabelStyle(m_view->whiteClockLabel(), fg, bg, 0, QColor(0,0,0,0), /*bold=*/false);

    m_urgency = Urgency::Normal;
}

void ShogiViewHighlighting::setActiveIsBlack(bool activeIsBlack)
{
    m_blackActive = activeIsBlack;
}

// ─────────────────────────────────────────────────────────────────────────────
// 緊急度表示
// ─────────────────────────────────────────────────────────────────────────────

void ShogiViewHighlighting::setUrgencyVisuals(Urgency u)
{
    qCDebug(lcView) << "setUrgencyVisuals: urgency=" << static_cast<int>(u)
                     << "m_blackActive=" << m_blackActive;

    QLabel* actName    = m_blackActive ? m_view->blackNameLabel()  : m_view->whiteNameLabel();
    QLabel* actClock   = m_blackActive ? m_view->blackClockLabel() : m_view->whiteClockLabel();
    QLabel* inactName  = m_blackActive ? m_view->whiteNameLabel()  : m_view->blackNameLabel();
    QLabel* inactClock = m_blackActive ? m_view->whiteClockLabel() : m_view->blackClockLabel();

    QLabel* actTurnLabel   = m_blackActive
                             ? m_view->findChild<QLabel*>(QStringLiteral("turnLabelBlack"))
                             : m_view->findChild<QLabel*>(QStringLiteral("turnLabelWhite"));

    // 非手番は font-weight=400 固定、背景は畳色
    auto setInactive = [&](QLabel* name, QLabel* clock){
        const QColor inactiveFg(51, 51, 51);
        const QColor inactiveBg(200, 190, 130);
        setLabelStyle(name,  inactiveFg, inactiveBg, 0, QColor(0,0,0,0), /*bold=*/false);
        setLabelStyle(clock, inactiveFg, inactiveBg, 0, QColor(0,0,0,0), /*bold=*/false);
    };

    switch (u) {
    case Urgency::Normal:
        setLabelStyle(actName,  kTurnFg,   kTurnBg,   0, QColor(0,0,0,0), /*bold=*/true);
        setLabelStyle(actClock, kTurnFg,   kTurnBg,   0, QColor(0,0,0,0), /*bold=*/true);
        if (actTurnLabel) {
            setLabelStyle(actTurnLabel, kTurnFg, kTurnBg, 0, QColor(0,0,0,0), /*bold=*/true);
        }
        setInactive(inactName, inactClock);
        break;

    case Urgency::Warn10:
        setLabelStyle(actName,  kWarn10Fg, kWarn10Bg, 0, kWarn10Border, /*bold=*/true);
        setLabelStyle(actClock, kWarn10Fg, kWarn10Bg, 0, kWarn10Border, /*bold=*/true);
        if (actTurnLabel) {
            setLabelStyle(actTurnLabel, kWarn10Fg, kWarn10Bg, 0, kWarn10Border, /*bold=*/true);
        }
        setInactive(inactName, inactClock);
        break;

    case Urgency::Warn5:
        setLabelStyle(actName,  kWarn5Fg,  kWarn5Bg,  0, kWarn5Border,  /*bold=*/true);
        setLabelStyle(actClock, kWarn5Fg,  kWarn5Bg,  0, kWarn5Border,  /*bold=*/true);
        if (actTurnLabel) {
            setLabelStyle(actTurnLabel, kWarn5Fg, kWarn5Bg, 0, kWarn5Border, /*bold=*/true);
        }
        setInactive(inactName, inactClock);
        break;
    default:
        break;
    }
}

void ShogiViewHighlighting::applyClockUrgency(qint64 activeRemainMs)
{
    Urgency next = Urgency::Normal;
    if      (activeRemainMs <= ShogiView::kWarn5Ms)  next = Urgency::Warn5;
    else if (activeRemainMs <= ShogiView::kWarn10Ms) next = Urgency::Warn10;

    if (next != m_urgency) {
        m_urgency = next;
        setUrgencyVisuals(m_urgency);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// 起動時スタイル
// ─────────────────────────────────────────────────────────────────────────────

void ShogiViewHighlighting::applyStartupTypography()
{
    m_urgency = Urgency::Normal;

    const QColor inactiveFg(51, 51, 51);
    const QColor inactiveBg(200, 190, 130);
    auto setInactive = [&](QLabel* name, QLabel* clock){
        setLabelStyle(name,  inactiveFg, inactiveBg, /*borderPx=*/0, QColor(0,0,0,0), /*bold=*/false);
        setLabelStyle(clock, inactiveFg, inactiveBg, /*borderPx=*/0, QColor(0,0,0,0), /*bold=*/false);
    };

    setInactive(m_view->blackNameLabel(),  m_view->blackClockLabel());
    setInactive(m_view->whiteNameLabel(),  m_view->whiteClockLabel());
}

// ─────────────────────────────────────────────────────────────────────────────
// 描画
// ─────────────────────────────────────────────────────────────────────────────

void ShogiViewHighlighting::drawHighlights(QPainter& painter, const ShogiViewLayout& layout)
{
    if (!m_view->board()) return;

    // 駒台の疑似座標 (10/11, 1..9) → 駒台描画で使う基準盤マス(file=1/2, rank=...) へ変換
    const auto standPseudoToBase = [&](int pseudoFile, int pseudoRank)
        -> std::tuple<bool,int,int,bool> {
        if (pseudoFile == 10) {
            if (pseudoRank == 7 || pseudoRank == 5 || pseudoRank == 3 || pseudoRank == 1) {
                const int baseFile = 2;
                const int baseRank = 6 + ((7 - pseudoRank) / 2);
                return {true, baseFile, baseRank, true};
            }
            if (pseudoRank == 8 || pseudoRank == 6 || pseudoRank == 4 || pseudoRank == 2) {
                const int baseFile = 1;
                const int baseRank = 6 + ((8 - pseudoRank) / 2);
                return {true, baseFile, baseRank, true};
            }
            return {false, 0, 0, true};
        }

        if (pseudoFile == 11) {
            if (pseudoRank == 3 || pseudoRank == 5 || pseudoRank == 7 || pseudoRank == 9) {
                const int baseFile = 1;
                const int baseRank = (11 - pseudoRank) / 2;
                return {true, baseFile, baseRank, false};
            }
            if (pseudoRank == 2 || pseudoRank == 4 || pseudoRank == 6 || pseudoRank == 8) {
                const int baseFile = 2;
                const int baseRank = (10 - pseudoRank) / 2;
                return {true, baseFile, baseRank, false};
            }
            return {false, 0, 0, false};
        }

        return {false, 0, 0, false};
    };

    // 盤/駒台共通：ハイライト矩形を算出
    const auto makeHighlightRect = [&](const ShogiView::FieldHighlight* fhl) -> QRect {
        const int f = fhl->file();
        const int r = fhl->rank();

        if (f == 10 || f == 11) {
            auto [ok, baseFile, baseRank, isBlackStand] = standPseudoToBase(f, r);
            if (!ok) return QRect();

            const QRect base = m_view->calculateSquareRectangleBasedOnBoardState(baseFile, baseRank);
            const int   param = isBlackStand ? layout.param1() : layout.param2();
            const bool  leftSide = isBlackStand;
            const QRect adjusted = makeStandCellRect(layout.flipMode(), param, layout.offsetX(), layout.offsetY(),
                                                     base, leftSide);
            return adjusted;
        }

        const QRect base = m_view->calculateSquareRectangleBasedOnBoardState(f, r);
        return QRect(base.left() + layout.offsetX(),
                     base.top()  + layout.offsetY(),
                     base.width(), base.height());
    };

    // 描画
    painter.save();
    for (int i = 0; i < highlightCount(); ++i) {
        ShogiView::Highlight* hl = highlight(i);
        if (!hl || hl->type() != ShogiView::FieldHighlight::Type) continue;

        const auto* fhl = static_cast<ShogiView::FieldHighlight*>(hl);
        const QRect rect = makeHighlightRect(fhl);
        if (rect.isNull()) continue;

        const QColor originalColor = fhl->color();

        if (originalColor.red() > 200 && originalColor.green() < 150 && originalColor.blue() < 150) {
            const QColor fillColor(255, 100, 100, 80);
            painter.fillRect(rect, fillColor);
        } else {
            const QColor fillColor(255, 255, 100, 120);
            const QRect innerRect = rect.adjusted(1, 1, -1, -1);
            painter.fillRect(innerRect, fillColor);
        }
    }
    painter.restore();
}

void ShogiViewHighlighting::drawArrows(QPainter& painter, const ShogiViewLayout& layout)
{
    if (m_arrows.isEmpty()) return;

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);

    for (const ShogiView::Arrow& arrow : std::as_const(m_arrows)) {
        QRect toRect = m_view->calculateSquareRectangleBasedOnBoardState(arrow.toFile, arrow.toRank);
        QPointF to(toRect.center().x() + layout.offsetX(), toRect.center().y() + layout.offsetY());

        // 駒打ちの場合
        if (arrow.fromFile == 0 || arrow.fromRank == 0) {
            if (arrow.dropPiece != ' ') {
                const QIcon icon = m_view->piece(arrow.dropPiece);
                if (!icon.isNull()) {
                    QRect adjustedRect(toRect.left() + layout.offsetX(),
                                       toRect.top() + layout.offsetY(),
                                       toRect.width(),
                                       toRect.height());

                    QPixmap pixmap = icon.pixmap(adjustedRect.size());
                    painter.setOpacity(0.6);
                    painter.drawPixmap(adjustedRect, pixmap);
                    painter.setOpacity(1.0);

                    QPen borderPen(QColor(255, 0, 0, 200));
                    borderPen.setWidth(qMax(2, layout.squareSize() / 20));
                    painter.setPen(borderPen);
                    painter.setBrush(Qt::NoBrush);
                    painter.drawRect(adjustedRect);
                }
            }

            if (arrow.priority >= 1 && m_arrows.size() >= 2) {
                const int fontSize = qMax(10, layout.squareSize() / 4);
                QFont font = painter.font();
                font.setPointSize(fontSize);
                font.setBold(true);
                painter.setFont(font);

                const int circleRadius = static_cast<int>(fontSize * 0.8);
                QPointF numPos(to.x() + toRect.width() / 3, to.y() + toRect.height() / 3);

                painter.setPen(Qt::NoPen);
                painter.setBrush(QColor(255, 255, 255, 230));
                painter.drawEllipse(numPos, circleRadius, circleRadius);

                painter.setPen(QColor(220, 0, 0));
                QString priorityText = QString::number(arrow.priority);
                QRectF textRect(numPos.x() - circleRadius, numPos.y() - circleRadius,
                               circleRadius * 2, circleRadius * 2);
                painter.drawText(textRect, Qt::AlignCenter, priorityText);
            }
            continue;
        }

        // 通常の移動：矢印を描画
        QRect fromRect = m_view->calculateSquareRectangleBasedOnBoardState(arrow.fromFile, arrow.fromRank);
        QPointF from(fromRect.center().x() + layout.offsetX(), fromRect.center().y() + layout.offsetY());

        const int arrowWidth = qMax(3, layout.squareSize() / 12);
        const int arrowHeadSize = qMax(10, layout.squareSize() / 4);

        QColor color = arrow.color;
        color.setAlpha(200);

        QPen pen(color);
        pen.setWidth(arrowWidth);
        pen.setCapStyle(Qt::RoundCap);
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

            const int fontSize = qMax(10, layout.squareSize() / 4);
            QFont font = painter.font();
            font.setPointSize(fontSize);
            font.setBold(true);
            painter.setFont(font);

            const int circleRadius = static_cast<int>(fontSize * 0.8);
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(255, 255, 255, 230));
            painter.drawEllipse(center, circleRadius, circleRadius);

            painter.setPen(QColor(220, 0, 0));
            QString priorityText = QString::number(arrow.priority);
            QRectF textRect(center.x() - circleRadius, center.y() - circleRadius,
                           circleRadius * 2, circleRadius * 2);
            painter.drawText(textRect, Qt::AlignCenter, priorityText);
        }
    }

    painter.restore();
}

// ─────────────────────────────────────────────────────────────────────────────
// ラベルスタイルヘルパ
// ─────────────────────────────────────────────────────────────────────────────

QString ShogiViewHighlighting::toRgb(const QColor& c)
{
    return QStringLiteral("rgb(%1,%2,%3)")
    .arg(QString::number(c.red()),
         QString::number(c.green()),
         QString::number(c.blue()));
}

void ShogiViewHighlighting::setLabelStyle(QLabel* lbl,
                                          const QColor& fg, const QColor& bg,
                                          int borderPx, const QColor& borderColor,
                                          bool bold)
{
    if (!lbl) return;

    const QString css = QStringLiteral(
                            "color:%1; background:%2; border:%3px solid %4; font-weight:%5; "
                            "padding:2px; border-radius:0px;")
                            .arg(
                                toRgb(fg),
                                toRgb(bg),
                                QString::number(borderPx),
                                toRgb(borderColor),
                                (bold ? QStringLiteral("700")
                                      : QStringLiteral("400"))
                                );

    lbl->setStyleSheet(css);
}
