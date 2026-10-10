/// @file timedisplaypresenter.cpp
/// @brief 時刻表示プレゼンタクラスの実装

#include "timedisplaypresenter.h"
#include "shogiview.h"
#include "shogiclock.h"
#include "logcategories.h"

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
    // ShogiClockと同じ切り上げ方式で秒を計算
    // これにより、ShogiClockのtimeUpdatedタイミングと表示が一致する
    const qint64 totalSec = (ms + 999) / 1000;  // 切り上げ
    const int h = static_cast<int>(totalSec / 3600);
    const int m = static_cast<int>((totalSec % 3600) / 60);
    const int s = static_cast<int>(totalSec % 60);
    return QString::asprintf("%02d:%02d:%02d", h, m, s);
}

void TimeDisplayPresenter::onMatchTimeUpdated(qint64 p1ms, qint64 p2ms, bool p1turn, qint64 /*urgencyMs*/)
{
    // デバッグ: 前回値との差分を確認
    const qint64 diffP1 = m_lastP1Ms - p1ms;
    const qint64 diffP2 = m_lastP2Ms - p2ms;
    if ((diffP1 > 1500 && m_lastP1Ms > 0) || (diffP2 > 1500 && m_lastP2Ms > 0)) {
        qCDebug(lcUi) << "Large time jump detected!"
                 << "P1:" << m_lastP1Ms << "->" << p1ms << "(diff=" << diffP1 << "ms)"
                 << "P2:" << m_lastP2Ms << "->" << p2ms << "(diff=" << diffP2 << "ms)";
    }
    
    m_lastP1Ms = p1ms;
    m_lastP2Ms = p2ms;

    if (m_view) {
        // 無制限対局は残り時間ではなく累積消費時間なので、表示の意味を書き添える。
        // 文字数が変わるため、ラベルに収まる文字サイズへ合わせ直す。
        const bool unlimited = m_clock && m_clock->isUnlimited();
        m_view->setBlackClockText(unlimited ? tr("消費 %1").arg(m_clock->player1TimeString())
                                            : fmt_hhmmss(p1ms));
        m_view->setWhiteClockText(unlimited ? tr("消費 %1").arg(m_clock->player2TimeString())
                                            : fmt_hhmmss(p2ms));
    }
    applyTurnHighlights(p1turn);
}

void TimeDisplayPresenter::applyTurnHighlights(bool p1turn)
{
    updateUrgencyStyles(p1turn);
}

bool TimeDisplayPresenter::isInByoyomi(bool p1turn) const
{
    // 無制限対局には時間切れの警告を表示しない。
    if (m_clock && m_clock->isUnlimited()) return false;

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

void TimeDisplayPresenter::updateUrgencyStyles(bool p1turn)
{
    if (!m_view) return;

    // アクティブ側（先手=黒か）をビューへ通知
    m_view->setActiveIsBlack(p1turn);

    // 秒読みに入っているかどうかで緊急度を決定
    const bool inByoyomi = isInByoyomi(p1turn);
    ShogiView::Urgency u = inByoyomi ? ShogiView::Urgency::Warn5 : ShogiView::Urgency::Normal;

    // 見た目の適用は ShogiView に一元化
    m_view->setUrgencyVisuals(u);
}
