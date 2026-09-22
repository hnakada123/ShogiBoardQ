/// @file test_stubs_undo_flow.cpp
/// @brief 待ったの連動テストで未使用の盤面入力・評価値グラフのスタブ

#include "boardinteractioncontroller.h"
#include "evaluationgraphcontroller.h"

void BoardInteractionController::clearAllHighlights() {}
void BoardInteractionController::cancelPendingClick() {}
void EvaluationGraphController::removeLastP1Score() {}
void EvaluationGraphController::removeLastP2Score() {}
void EvaluationGraphController::setCurrentPly(int) {}
