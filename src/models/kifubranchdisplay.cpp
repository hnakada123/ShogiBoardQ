/// @file kifubranchdisplay.cpp
/// @brief 棋譜分岐候補表示ウィジェットクラスの実装

#include "kifubranchdisplay.h"

// 棋譜欄を表示するクラス
KifuBranchDisplay::KifuBranchDisplay(QObject *parent) : QObject(parent) { }

KifuBranchDisplay::KifuBranchDisplay(const QString &currentMove, QObject *parent)
    : KifuBranchDisplay(parent)
{
    // 指し手と消費時間を設定する。
    m_currentMove = currentMove;
}


// 指し手を取得する。
QString KifuBranchDisplay::currentMove() const
{
    return m_currentMove;
}

void KifuBranchDisplay::setCurrentMove(const QString &newCurrentMove)
{
    m_currentMove = newCurrentMove;
}
