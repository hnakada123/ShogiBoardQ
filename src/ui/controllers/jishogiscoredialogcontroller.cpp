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

    const auto result = JishogiCalculator::calculate(board->boardData(), board->pieceStand());
    EngineMoveValidator validator;
    const bool senteInCheck = validator.checkIfKingInCheck(EngineMoveValidator::BLACK, board->boardData()) > 0;
    const bool goteInCheck = validator.checkIfKingInCheck(EngineMoveValidator::WHITE, board->boardData()) > 0;

    JishogiScoreDialog dialog(result, senteInCheck, goteInCheck, parentWidget);
    dialog.exec();
}
