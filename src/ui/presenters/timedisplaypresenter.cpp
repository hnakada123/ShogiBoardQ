/// @file timedisplaypresenter.cpp
/// @brief 時刻表示プレゼンタクラスの実装

#include "timedisplaypresenter.h"
#include "boardchromewidget.h"
#include "shogiview.h"
#include "shogiclock.h"
#include "logcategories.h"

namespace {

// 警告のしきい値（ミリ秒）
constexpr qint64 kWarn10Ms = 10'000;
constexpr qint64 kWarn5Ms = 5'000;

/// 手番側の時計の緊急度（通常／残り10秒以下／秒読み中・残り5秒以下）を判定する
BoardChromeWidget::Urgency urgencyFor(const ShogiClock* clock, qint64 ms, Turn side)
{
    using Urgency = BoardChromeWidget::Urgency;
    // 無制限対局には時間切れの警告を表示しない。
    if (clock && clock->isUnlimited()) return Urgency::Normal;

    const bool hasByoyomi = clock && clock->hasByoyomi(side);
    if (hasByoyomi) {
        // 秒読み中（持ち時間0秒の秒読み対局を含む）は最も強い警告にする。
        // 秒読みに入る前は、持ち時間が残り10秒以下で予告の警告にする。
        const bool inByoyomi = clock->byoyomiApplied(side) || ms <= 0;
        if (inByoyomi) return Urgency::Warn5;
        return ms <= kWarn10Ms ? Urgency::Warn10 : Urgency::Normal;
    }
    // 秒読みが無い（切れ負け・加算）場合は、残り時間で2段階に警告する。
    if (ms <= kWarn5Ms) return Urgency::Warn5;
    if (ms <= kWarn10Ms) return Urgency::Warn10;
    return Urgency::Normal;
}

} // namespace

TimeDisplayPresenter::TimeDisplayPresenter(ShogiView* view, const ShogiClock* clock, QObject* parent)
    : QObject(parent)
    , m_view(view)
    , m_clock(clock)
{
    if (m_clock) connect(m_clock, &ShogiClock::timeUpdated, this, &TimeDisplayPresenter::showClock);
}

void TimeDisplayPresenter::showClock()
{
    if (!m_clock) return;
    showTimes(m_clock->remainingMs(Turn::Black), m_clock->remainingMs(Turn::White), m_clock->currentPlayer());
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

void TimeDisplayPresenter::showTimes(qint64 blackMs, qint64 whiteMs, Turn sideToMove)
{
    // デバッグ: 前回値との差分を確認
    const qint64 diffBlack = m_lastBlackMs - blackMs;
    const qint64 diffWhite = m_lastWhiteMs - whiteMs;
    if ((diffBlack > 1500 && m_lastBlackMs > 0) || (diffWhite > 1500 && m_lastWhiteMs > 0)) {
        qCDebug(lcUi) << "Large time jump detected!"
                 << "black:" << m_lastBlackMs << "->" << blackMs << "(diff=" << diffBlack << "ms)"
                 << "white:" << m_lastWhiteMs << "->" << whiteMs << "(diff=" << diffWhite << "ms)";
    }

    m_lastBlackMs = blackMs;
    m_lastWhiteMs = whiteMs;
    m_lastSideToMove = sideToMove;
    m_hasLastValues = true;

    if (m_view) {
        // 無制限対局は残り時間ではなく累積消費時間なので、表示の意味を書き添える。
        // 文字数が変わるため、ラベルに収まる文字サイズへ合わせ直す。
        const bool unlimited = m_clock && m_clock->isUnlimited();
        m_view->chrome()->card(Turn::Black)->setClockText(unlimited ? tr("消費 %1").arg(m_clock->timeString(Turn::Black))
                                            : fmt_hhmmss(blackMs));
        m_view->chrome()->card(Turn::White)->setClockText(unlimited ? tr("消費 %1").arg(m_clock->timeString(Turn::White))
                                            : fmt_hhmmss(whiteMs));
    }
    updateUrgencyStyles(sideToMove);
}

void TimeDisplayPresenter::refresh()
{
    if (!m_hasLastValues) return;
    showTimes(m_lastBlackMs, m_lastWhiteMs, m_lastSideToMove);
}

void TimeDisplayPresenter::updateUrgencyStyles(Turn sideToMove)
{
    if (!m_view) return;

    // 手番の印は盤の手番が決める。ここでは手番側の残り時間から緊急度だけを決める
    const qint64 activeMs = (sideToMove == Turn::Black) ? m_lastBlackMs : m_lastWhiteMs;
    m_view->chrome()->setUrgency(urgencyFor(m_clock, activeMs, sideToMove));
}
