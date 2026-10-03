/// @file test_stubs_kifu_export.cpp
/// @brief 棋譜出力テストで使わないUI依存の最小スタブ

#include "gameinfopanecontroller.h"
#include "timecontrolcontroller.h"
#include "shogigamecontroller.h"

QTableWidget* GameInfoPaneController::tableWidget() const { return nullptr; }
QList<KifGameInfoItem> GameInfoPaneController::gameInfo() const { return {}; }
void GameInfoPaneController::commitPendingEditor() {}
void GameInfoPaneController::applyChanges() {}
bool TimeControlController::hasTimeControl() const { return false; }
qint64 TimeControlController::baseTimeMs() const { return 0; }
qint64 TimeControlController::byoyomiMs() const { return 0; }
qint64 TimeControlController::incrementMs() const { return 0; }
QDateTime TimeControlController::gameStartDateTime() const { return {}; }
QDateTime TimeControlController::gameEndDateTime() const { return {}; }
ShogiBoard* ShogiGameController::board() const { return nullptr; }
