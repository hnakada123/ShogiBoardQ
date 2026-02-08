/// @file kifudisplay.cpp
/// @brief 棋譜表示ウィジェットクラスの実装

#include "kifudisplay.h"

// 棋譜欄を表示するクラス
KifuDisplay::KifuDisplay(QObject *parent) : QObject(parent) { }

// コンストラクタ（2引数）
KifuDisplay::KifuDisplay(const QString &currentMove, const QString &timeSpent, QObject *parent)
    : KifuDisplay(parent)
{
    // 指し手と消費時間を設定する。
    m_currentMove = currentMove;
    m_timeSpent = timeSpent;
}

// コンストラクタ（3引数：コメント付き）
KifuDisplay::KifuDisplay(const QString &currentMove, const QString &timeSpent, const QString &comment, QObject *parent)
    : KifuDisplay(parent)
{
    // 指し手、消費時間、コメントを設定する。
    m_currentMove = currentMove;
    m_timeSpent = timeSpent;
    m_comment = comment;
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

// コメントを取得する。
QString KifuDisplay::comment() const
{
    return m_comment;
}

// コメントを設定する。
void KifuDisplay::setComment(const QString &comment)
{
    m_comment = comment;
}
