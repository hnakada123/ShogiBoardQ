/// @file test_stubs_cleanup.cpp
/// @brief PreStartCleanupHandler テスト用スタブ
///
/// prestartcleanuphandler.cpp が参照するが、テストでは null ポインタ経由で
/// 呼ばれないシンボルのスタブ実装を提供する。

#include <QLabel>

#include "boardinteractioncontroller.h"
#include "shogiview.h"
#include "timecontrolcontroller.h"
#include "evaluationchartwidget.h"
#include "evaluationgraphcontroller.h"
#include "recordpane.h"
#include "matchcoordinator.h"

// === BoardInteractionController スタブ ===
void BoardInteractionController::clearAllHighlights() {}

// === ShogiView スタブ（追加分） ===
QLabel* ShogiView::blackClockLabel() const { return nullptr; }
QLabel* ShogiView::whiteClockLabel() const { return nullptr; }

// === TimeControlController スタブ ===
void TimeControlController::clearGameStartTime() {}

// === EvaluationChartWidget スタブ ===
void EvaluationChartWidget::clearAll() {}

// === EvaluationGraphController スタブ ===
void EvaluationGraphController::clearScores() {}
void EvaluationGraphController::trimToPly(int) {}

// === RecordPane スタブ ===
QTableView* RecordPane::kifuView() const { return nullptr; }

// === MatchCoordinator スタブ ===
void MatchCoordinator::clearGameOverState() {}
