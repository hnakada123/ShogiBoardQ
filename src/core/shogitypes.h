#ifndef SHOGITYPES_H
#define SHOGITYPES_H

/// @file shogitypes.h
/// @brief 将棋の基本型定義（手番enum等）

#include <QString>

/// 手番を表す型安全な列挙型
enum class Turn { Black, White };

/// Turn → SFEN手番文字列（"b" or "w"）
inline QString turnToSfen(Turn t) { return t == Turn::Black ? QStringLiteral("b") : QStringLiteral("w"); }

/// SFEN手番文字列 → Turn
inline Turn sfenToTurn(const QString& s) { return s == QStringLiteral("b") ? Turn::Black : Turn::White; }

/// 相手番を返す
inline Turn oppositeTurn(Turn t) { return t == Turn::Black ? Turn::White : Turn::Black; }

/// bestmove の特殊手を表す列挙型（エンジンからの受信用）
enum class SpecialMove { None, Resign, Win, Draw };

/// bestmove 文字列を SpecialMove に変換する（通常手は None を返す）
inline SpecialMove parseSpecialMove(const QString& s)
{
    if (s.compare(QStringLiteral("resign"), Qt::CaseInsensitive) == 0) return SpecialMove::Resign;
    if (s.compare(QStringLiteral("win"),    Qt::CaseInsensitive) == 0) return SpecialMove::Win;
    if (s.compare(QStringLiteral("draw"),   Qt::CaseInsensitive) == 0) return SpecialMove::Draw;
    return SpecialMove::None;
}

/// gameover コマンドの結果を表す列挙型（エンジンへの送信用）
enum class GameOverResult { Win, Lose, Draw };

/// GameOverResult を USI gameover コマンド用の文字列に変換する
inline QString gameOverResultToString(GameOverResult r)
{
    switch (r) {
    case GameOverResult::Win:  return QStringLiteral("win");
    case GameOverResult::Lose: return QStringLiteral("lose");
    case GameOverResult::Draw: return QStringLiteral("draw");
    }
    return QStringLiteral("draw");
}

#endif // SHOGITYPES_H
