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

#endif // SHOGITYPES_H
