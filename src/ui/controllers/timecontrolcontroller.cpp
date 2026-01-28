#include "timecontrolcontroller.h"

#include <QDebug>

#include "shogiclock.h"
#include "matchcoordinator.h"
#include "timedisplaypresenter.h"
#include "timecontrolutil.h"
#include "gamestartcoordinator.h"
#include "shogiview.h"

TimeControlController::TimeControlController(QObject* parent)
    : QObject(parent)
{
}

TimeControlController::~TimeControlController() = default;

// --------------------------------------------------------
// ShogiClockの管理
// --------------------------------------------------------

void TimeControlController::ensureClock()
{
    if (m_clock) return;

    m_clock = new ShogiClock(this);

    // TimeDisplayPresenterにClockを設定（秒読み状態の判定に使用）
    if (m_timePresenter) {
        m_timePresenter->setClock(m_clock);
    }

    // タイムアウトシグナルを接続
    QObject::connect(m_clock, &ShogiClock::player1TimeOut,
                     this, &TimeControlController::onClockPlayer1TimeOut);
    QObject::connect(m_clock, &ShogiClock::player2TimeOut,
                     this, &TimeControlController::onClockPlayer2TimeOut);
}

ShogiClock* TimeControlController::clock() const
{
    return m_clock;
}

// --------------------------------------------------------
// 依存オブジェクトの設定
// --------------------------------------------------------

void TimeControlController::setMatchCoordinator(MatchCoordinator* match)
{
    m_match = match;
}

void TimeControlController::setTimeDisplayPresenter(TimeDisplayPresenter* presenter)
{
    m_timePresenter = presenter;

    // 既にClockが存在していれば、Presenterに設定
    if (m_timePresenter && m_clock) {
        m_timePresenter->setClock(m_clock);
    }
}

// --------------------------------------------------------
// 時間設定適用ヘルパー
// --------------------------------------------------------

void TimeControlController::applyTimeControl(const GameStartCoordinator::TimeControl& tc,
                                             MatchCoordinator* match,
                                             const QString& startSfen,
                                             const QString& currentSfen,
                                             ShogiView* shogiView)
{
    // 1) 設定を保存し、開始時刻を記録
    saveTimeControlSettings(tc.enabled, tc.p1.baseMs, tc.p1.byoyomiMs, tc.p1.incrementMs);
    recordGameStartTime();

    // 2) 時計へ適用
    ensureClock();
    ShogiClock* clk = clock();
    TimeControlUtil::applyToClock(clk, tc, startSfen, currentSfen);

    // 3) 司令塔へ反映
    if (match) {
        const bool useByoyomi = (tc.p1.byoyomiMs > 0) || (tc.p2.byoyomiMs > 0);
        match->setTimeControlConfig(useByoyomi,
                                    static_cast<int>(tc.p1.byoyomiMs), static_cast<int>(tc.p2.byoyomiMs),
                                    static_cast<int>(tc.p1.incrementMs), static_cast<int>(tc.p2.incrementMs),
                                    /*loseOnTimeout*/ true);
        match->refreshGoTimes();
    }

    // 4) 表示更新
    if (m_timePresenter && clk) {
        const qint64 p1 = clk->getPlayer1TimeIntMs();
        const qint64 p2 = clk->getPlayer2TimeIntMs();
        const bool p1turn = true;  // 呼び出し元で手番更新済みの想定
        m_timePresenter->onMatchTimeUpdated(p1, p2, p1turn, /*urgencyMs*/ 0);
    }
    if (shogiView) {
        shogiView->update();
    }
}

// --------------------------------------------------------
// 時間制御設定
// --------------------------------------------------------

void TimeControlController::saveTimeControlSettings(bool enabled, qint64 baseMs, qint64 byoyomiMs, qint64 incrementMs)
{
    m_settings.enabled = enabled;
    m_settings.baseMs = baseMs;
    m_settings.byoyomiMs = byoyomiMs;
    m_settings.incrementMs = incrementMs;

    qDebug().noquote() << "[TimeCtrl] saveTimeControlSettings:"
                       << "enabled=" << enabled
                       << "base=" << baseMs
                       << "byoyomi=" << byoyomiMs
                       << "increment=" << incrementMs;
}

const TimeControlController::TimeControlSettings& TimeControlController::settings() const
{
    return m_settings;
}

bool TimeControlController::hasTimeControl() const
{
    return m_settings.enabled;
}

qint64 TimeControlController::baseTimeMs() const
{
    return m_settings.baseMs;
}

qint64 TimeControlController::byoyomiMs() const
{
    return m_settings.byoyomiMs;
}

qint64 TimeControlController::incrementMs() const
{
    return m_settings.incrementMs;
}

// --------------------------------------------------------
// 対局開始時刻
// --------------------------------------------------------

void TimeControlController::recordGameStartTime()
{
    if (!m_gameStartDateTime.isValid()) {
        m_gameStartDateTime = QDateTime::currentDateTime();
        qDebug().noquote() << "[TimeCtrl] recordGameStartTime:" << m_gameStartDateTime.toString(Qt::ISODate);
    }
}

QDateTime TimeControlController::gameStartDateTime() const
{
    return m_gameStartDateTime;
}

void TimeControlController::clearGameStartTime()
{
    m_gameStartDateTime = QDateTime();
}

// --------------------------------------------------------
// 時間取得ヘルパー
// --------------------------------------------------------

qint64 TimeControlController::getRemainingMs(int player) const
{
    if (!m_clock) {
        qDebug() << "[TimeCtrl] getRemainingMs: clock=null";
        return 0;
    }
    const qint64 p1 = m_clock->getPlayer1TimeIntMs();
    const qint64 p2 = m_clock->getPlayer2TimeIntMs();
    qDebug() << "[TimeCtrl] getRemainingMs: P1=" << p1 << " P2=" << p2
             << " req=" << (player == 1 ? "P1" : "P2");
    return (player == 1) ? p1 : p2;
}

qint64 TimeControlController::getIncrementMs(int player) const
{
    if (!m_clock) {
        qDebug() << "[TimeCtrl] getIncrementMs: clock=null";
        return 0;
    }
    const qint64 inc1 = m_clock->getBincMs();
    const qint64 inc2 = m_clock->getWincMs();
    qDebug() << "[TimeCtrl] getIncrementMs: binc=" << inc1 << " winc=" << inc2
             << " req=" << (player == 1 ? "P1" : "P2");
    return (player == 1) ? inc1 : inc2;
}

qint64 TimeControlController::getByoyomiMs() const
{
    const qint64 byo = m_clock ? m_clock->getCommonByoyomiMs() : 0;
    qDebug() << "[TimeCtrl] getByoyomiMs: ms=" << byo;
    return byo;
}

// --------------------------------------------------------
// タイムアウトハンドリング
// --------------------------------------------------------

void TimeControlController::onClockPlayer1TimeOut()
{
    qDebug() << "[TimeCtrl] onClockPlayer1TimeOut";
    if (m_match) {
        m_match->handlePlayerTimeOut(1);  // 1 = 先手
    }
    Q_EMIT player1TimedOut();
}

void TimeControlController::onClockPlayer2TimeOut()
{
    qDebug() << "[TimeCtrl] onClockPlayer2TimeOut";
    if (m_match) {
        m_match->handlePlayerTimeOut(2);  // 2 = 後手
    }
    Q_EMIT player2TimedOut();
}
