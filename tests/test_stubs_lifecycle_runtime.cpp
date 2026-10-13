/// @file test_stubs_lifecycle_runtime.cpp
/// @brief tst_lifecycle_runtime 用スタブ

#include "timecontrolcontroller.h"

// --- TimeControlController スタブ（テストに必要な最小限のシンボル） ---

TimeControlController::TimeControlController(QObject* parent)
    : QObject(parent)
{
}

TimeControlController::~TimeControlController() = default;

void TimeControlController::ensureClock() {}
ShogiClock* TimeControlController::clock() const { return nullptr; }
void TimeControlController::setMatchCoordinator(MatchCoordinator*) {}
void TimeControlController::setTimeDisplayPresenter(TimeDisplayPresenter*) {}
void TimeControlController::applyTimeControl(const GameStartCoordinator::TimeSettings&, const QString&,
                                              const QString&, ShogiView*) {}
void TimeControlController::recordTimeControl(const TimeControl&) {}
const TimeControl& TimeControlController::timeControl() const
{
    return m_timeControl;
}
bool TimeControlController::hasTimeControl() const { return false; }
void TimeControlController::recordGameStartTime() {}
QDateTime TimeControlController::gameStartDateTime() const { return {}; }
void TimeControlController::clearGameStartTime() {}
void TimeControlController::recordGameEndTime() {}
QDateTime TimeControlController::gameEndDateTime() const { return {}; }
void TimeControlController::clearGameEndTime() {}


void TimeControlController::onClockPlayerTimeOut(Turn) {}

// MOC 生成ファイルをインクルード（vtable 解決に必要）
#include "moc_timecontrolcontroller.cpp"

// --- CsaGameCoordinator::isMyTurn() スタブ ---
#include "csagamecoordinator.h"

bool CsaGameCoordinator::isMyTurn() const { return false; }
