/// @file jishogiscoredialogcontroller.cpp
/// @brief 持将棋点数の計算とダイアログ起動

#include "jishogiscoredialogcontroller.h"

#include "enginemovevalidator.h"
#include "jishogicalculator.h"
#include "jishogiscoredialog.h"
#include "shogiboard.h"

JishogiScoreDialogController::JishogiScoreDialogController(QObject* parent)
    : QObject(parent)
{
}

void JishogiScoreDialogController::showDialog(QWidget* parentWidget, ShogiBoard* board)
{
    if (!board) return;

    const auto result = JishogiCalculator::calculate(board->position());
    const bool senteInCheck = EngineMoveValidator::isInCheck(board->position(), Turn::Black);
    const bool goteInCheck = EngineMoveValidator::isInCheck(board->position(), Turn::White);

    JishogiScoreDialog dialog(result, senteInCheck, goteInCheck, parentWidget);
    dialog.exec();
}
