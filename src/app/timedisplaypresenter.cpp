#include "timedisplaypresenter.h"
#include "shogiview.h"
#include "shogiclock.h"
#include <QTime>

TimeDisplayPresenter::TimeDisplayPresenter(ShogiView* view, QObject* parent)
    : QObject(parent), m_view(view)
{
}

void TimeDisplayPresenter::setClock(ShogiClock* clock)
{
    m_clock = clock;
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

bool TimeDisplayPresenter::isInByoyomi_(bool p1turn) const
{
    // クロックが設定されていない場合
    if (!m_clock) {
        // 秒読み設定がない（クロックなし）場合：残り5秒以下で緊急状態とみなす
        const qint64 ms = p1turn ? m_lastP1Ms : m_lastP2Ms;
        return ms <= ShogiView::kWarn5Ms;
    }

    if (p1turn) {
        // 先手の手番
        if (m_clock->hasByoyomi1()) {
            // 秒読み設定がある場合：秒読みに入っているかどうかのみで判定
            return m_clock->byoyomi1Applied();
        }
        // 秒読み設定が0秒の場合のみ：残り5秒以下で秒読み状態とみなす
        return m_lastP1Ms <= ShogiView::kWarn5Ms;
    } else {
        // 後手の手番
        if (m_clock->hasByoyomi2()) {
            // 秒読み設定がある場合：秒読みに入っているかどうかのみで判定
            return m_clock->byoyomi2Applied();
        }
        // 秒読み設定が0秒の場合のみ：残り5秒以下で秒読み状態とみなす
        return m_lastP2Ms <= ShogiView::kWarn5Ms;
    }
}

void TimeDisplayPresenter::updateUrgencyStyles_(bool p1turn)
{
    if (!m_view) return;

    // アクティブ側（先手=黒か）をビューへ通知
    m_view->setActiveIsBlack(p1turn);

    // 秒読みに入っているかどうかで緊急度を決定
    const bool inByoyomi = isInByoyomi_(p1turn);
    ShogiView::Urgency u = inByoyomi ? ShogiView::Urgency::Warn5 : ShogiView::Urgency::Normal;

    // 見た目の適用は ShogiView に一元化
    m_view->setUrgencyVisuals(u);
}
