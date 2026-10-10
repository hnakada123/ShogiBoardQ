/// @file timedisplaypresenter.cpp
/// @brief 時刻表示プレゼンタクラスの実装

#include "timedisplaypresenter.h"
#include "shogiview.h"
#include "shogiclock.h"
#include "logcategories.h"

namespace {

/// 手番側の時計の緊急度（通常／残り10秒以下／秒読み中・残り5秒以下）を判定する
ShogiView::Urgency urgencyFor(const ShogiClock* clock, qint64 ms, bool p1turn)
{
    using Urgency = ShogiView::Urgency;
    // 無制限対局には時間切れの警告を表示しない。
    if (clock && clock->isUnlimited()) return Urgency::Normal;

    const bool hasByoyomi = clock && (p1turn ? clock->hasByoyomi1() : clock->hasByoyomi2());
    if (hasByoyomi) {
        // 秒読み中（持ち時間0秒の秒読み対局を含む）は最も強い警告にする。
        // 秒読みに入る前は、持ち時間が残り10秒以下で予告の警告にする。
        const bool inByoyomi = (p1turn ? clock->byoyomi1Applied() : clock->byoyomi2Applied()) || ms <= 0;
        if (inByoyomi) return Urgency::Warn5;
        return ms <= ShogiView::kWarn10Ms ? Urgency::Warn10 : Urgency::Normal;
    }
    // 秒読みが無い（切れ負け・加算）場合は、残り時間で2段階に警告する。
    if (ms <= ShogiView::kWarn5Ms) return Urgency::Warn5;
    if (ms <= ShogiView::kWarn10Ms) return Urgency::Warn10;
    return Urgency::Normal;
}

} // namespace

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

void TimeDisplayPresenter::updateUrgencyStyles(bool p1turn)
{
    if (!m_view) return;

    // アクティブ側（先手=黒か）をビューへ通知
    m_view->setActiveIsBlack(p1turn);

    // 秒読み・残り時間から緊急度を決め、見た目の適用は ShogiView に一元化
    m_view->setUrgencyVisuals(urgencyFor(m_clock, p1turn ? m_lastP1Ms : m_lastP2Ms, p1turn));
}
