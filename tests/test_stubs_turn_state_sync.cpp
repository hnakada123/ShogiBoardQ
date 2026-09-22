/// @file test_stubs_turn_state_sync.cpp
/// @brief 手番同期テストで使用しない時計コントローラのスタブ

#include "timecontrolcontroller.h"

ShogiClock* TimeControlController::clock() const { return nullptr; }
