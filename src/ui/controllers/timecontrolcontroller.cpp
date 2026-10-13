/// @file timecontrolcontroller.cpp
/// @brief 持ち時間制御コントローラクラスの実装

#include "timecontrolcontroller.h"

#include "logcategories.h"

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
    QObject::connect(m_clock, &ShogiClock::playerTimeOut,
                     this, &TimeControlController::onClockPlayerTimeOut);
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

void TimeControlController::applyTimeControl(const GameStartCoordinator::TimeSettings& tc,
                                             const QString& startSfen,
                                             const QString& currentSfen,
                                             ShogiView* shogiView)
{
    // 1) 設定を保存し、開始時刻を記録
    recordTimeControl(tc.enabled ? tc.rules : TimeControl{});
    recordGameStartTime();

    // 2) 時計へ適用
    ensureClock();
    ShogiClock* clk = clock();
    TimeControlUtil::applyToClock(clk, tc, startSfen, currentSfen);

    // 3) 表示更新（決まりは時計が持ち、エンジンの思考時間も時計から求める）
    if (m_timePresenter && clk) {
        const qint64 p1 = clk->remainingMs(Turn::Black);
        const qint64 p2 = clk->remainingMs(Turn::White);
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

void TimeControlController::recordTimeControl(const TimeControl& timeControl)
{
    m_timeControl = timeControl;

    qCDebug(lcUi).noquote() << "recordTimeControl:"
                            << "black={" << timeControl.black.mainMs << timeControl.black.byoyomiMs
                            << timeControl.black.incrementMs << "}"
                            << "white={" << timeControl.white.mainMs << timeControl.white.byoyomiMs
                            << timeControl.white.incrementMs << "}";
}

void TimeControlController::beginGameWithTimeControl(const TimeControl& timeControl)
{
    recordTimeControl(timeControl);
    clearGameStartTime();
    recordGameStartTime();
    clearGameEndTime();
}

const TimeControl& TimeControlController::timeControl() const
{
    return m_timeControl;
}

bool TimeControlController::hasTimeControl() const
{
    return !m_timeControl.isUnlimited();
}

// --------------------------------------------------------
// 対局開始時刻
// --------------------------------------------------------

void TimeControlController::recordGameStartTime()
{
    if (!m_gameStartDateTime.isValid()) {
        m_gameStartDateTime = QDateTime::currentDateTime();
        qCDebug(lcUi).noquote() << "recordGameStartTime:" << m_gameStartDateTime.toString(Qt::ISODate);
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
// 対局終了時刻
// --------------------------------------------------------

void TimeControlController::recordGameEndTime()
{
    if (!m_gameEndDateTime.isValid()) {
        m_gameEndDateTime = QDateTime::currentDateTime();
        qCDebug(lcUi).noquote() << "recordGameEndTime:" << m_gameEndDateTime.toString(Qt::ISODate);
    }
}

QDateTime TimeControlController::gameEndDateTime() const
{
    return m_gameEndDateTime;
}

void TimeControlController::clearGameEndTime()
{
    m_gameEndDateTime = QDateTime();
}

// --------------------------------------------------------
// タイムアウトハンドリング
// --------------------------------------------------------

void TimeControlController::onClockPlayerTimeOut(Turn player)
{
    qCDebug(lcUi) << "onClockPlayerTimeOut" << player;
    if (m_match) {
        m_match->handlePlayerTimeOut(player);
    }
}
