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
void TimeControlController::applyTimeControl(const GameStartCoordinator::TimeControl&,
                                              MatchCoordinator*, const QString&,
                                              const QString&, ShogiView*) {}
void TimeControlController::saveTimeControlSettings(bool, qint64, qint64, qint64) {}
const TimeControlController::TimeControlSettings& TimeControlController::settings() const
{
    return m_settings;
}
bool TimeControlController::hasTimeControl() const { return false; }
qint64 TimeControlController::baseTimeMs() const { return 0; }
qint64 TimeControlController::byoyomiMs() const { return 0; }
qint64 TimeControlController::incrementMs() const { return 0; }
void TimeControlController::recordGameStartTime() {}
QDateTime TimeControlController::gameStartDateTime() const { return {}; }
void TimeControlController::clearGameStartTime() {}
void TimeControlController::recordGameEndTime() {}
QDateTime TimeControlController::gameEndDateTime() const { return {}; }
void TimeControlController::clearGameEndTime() {}

qint64 TimeControlController::remainingMs(int) const { return 0; }
qint64 TimeControlController::incrementMs(int) const { return 0; }
qint64 TimeControlController::clockByoyomiMs() const { return 0; }

void TimeControlController::onClockPlayer1TimeOut() {}
void TimeControlController::onClockPlayer2TimeOut() {}

// MOC 生成ファイルをインクルード（vtable 解決に必要）
#include "moc_timecontrolcontroller.cpp"

// --- CsaGameCoordinator::isMyTurn() スタブ ---
#include "csagamecoordinator.h"

bool CsaGameCoordinator::isMyTurn() const { return false; }
