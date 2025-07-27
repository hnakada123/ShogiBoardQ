#include "kifudisplay.h"

// 棋譜欄を表示するクラス
// コンストラクタ
KifuDisplay::KifuDisplay(QObject *parent) : QObject(parent) { }

// コンストラクタ
KifuDisplay::KifuDisplay(const QString &currentMove, const QString &timeSpent, QObject *parent)
    : KifuDisplay(parent)
{
    // 指し手と消費時間を設定する。
    m_currentMove = currentMove;
    m_timeSpent = timeSpent;
}


// 指し手を取得する。
QString KifuDisplay::currentMove() const
{
    return m_currentMove;
}

// 消費時間を取得する。
QString KifuDisplay::timeSpent() const
{
    return m_timeSpent;
}
