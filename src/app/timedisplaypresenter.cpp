#include "timedisplaypresenter.h"
#include "shogiview.h"
#include <QTime>

TimeDisplayPresenter::TimeDisplayPresenter(ShogiView* view, QObject* parent)
    : QObject(parent), m_view(view)
{
}

inline QString TimeDisplayPresenter::fmt_hhmmss(qint64 ms)
{
    if (ms < 0) ms = 0;
    QTime t(0,0); return t.addMSecs(static_cast<int>(ms)).toString("hh:mm:ss");
}

void TimeDisplayPresenter::onMatchTimeUpdated(qint64 p1ms, qint64 p2ms, bool p1turn, qint64 /*urgencyMs*/)
{
    m_lastP1Ms = p1ms;
    m_lastP2Ms = p2ms;

    if (m_view) {
        if (auto* b = m_view->blackClockLabel()) b->setText(fmt_hhmmss(p1ms));
        if (auto* w = m_view->whiteClockLabel()) w->setText(fmt_hhmmss(p2ms));
        m_view->setBlackTimeMs(p1ms);
        m_view->setWhiteTimeMs(p2ms);
    }
    applyTurnHighlights_(p1turn);
}

void TimeDisplayPresenter::applyTurnHighlights_(bool p1turn)
{
    updateUrgencyStyles_(p1turn);
}

void TimeDisplayPresenter::updateUrgencyStyles_(bool p1turn)
{
    if (!m_view) return;

    // アクティブ側（先手=黒か）をビューへ通知
    m_view->setActiveIsBlack(p1turn);

    // 残り時間から緊急度を決定（ShogiView の定数に合わせる）
    const qint64 ms = p1turn ? m_lastP1Ms : m_lastP2Ms;
    ShogiView::Urgency u;
    if (ms <= ShogiView::kWarn5Ms)       u = ShogiView::Urgency::Warn5;
    else if (ms <= ShogiView::kWarn10Ms) u = ShogiView::Urgency::Warn10;
    else                                 u = ShogiView::Urgency::Normal;

    // 見た目の適用は ShogiView に一元化
    m_view->setUrgencyVisuals(u);
}
