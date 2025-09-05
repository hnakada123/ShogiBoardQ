#include "kifubranchdisplay.h"

// 棋譜欄を表示するクラス
// コンストラクタ
KifuBranchDisplay::KifuBranchDisplay(QObject *parent) : QObject(parent) { }

// コンストラクタ
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
