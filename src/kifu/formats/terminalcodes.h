#ifndef TERMINALCODES_H
#define TERMINALCODES_H

/// @file terminalcodes.h
/// @brief 棋譜形式ごとの終局の符号（CSA・JKF・USI・USEN）。読みと書きで同じ表を使う

#include <QString>

#include "shogitypes.h"
#include "terminaltype.h"

namespace TerminalCodes {

/**
 * @brief CSA・JKF の終局の符号（"%" を付けない語。例: TORYO）
 * @param sideToMove 終局した局面の手番（反則勝ちで、反則した相手の符号 +/- を決める）
 * @param perpetualCheck 連続王手の千日手を OUTE_SENNICHITE と書く（JKF）。false なら SENNICHITE（CSA）
 * @return 符号のない終局は空
 */
QString csaSpecial(TerminalType type, Turn sideToMove, bool perpetualCheck);

/// CSA・JKF の終局の符号（"%TORYO" と "TORYO" の両方）を読む。+/-ILLEGAL_ACTION は反則した側が手番側なら反則負け。
/// 終局の符号でなければ None
TerminalType fromCsaSpecial(const QString& code, Turn sideToMove);

/// 棋譜ファイルの USI 形式（position 行の末尾）の終局の符号。符号のない終局は空
QString usi(TerminalType type);
/// USI 形式の終局の符号を読む（大文字小文字を区別しない。過去の版の別名も読む）。不明な符号は Error
TerminalType fromUsi(const QString& code);
bool isUsiCode(const QString& code);

/// USEN の終局の符号（1文字）。符号のない終局は空
QString usen(TerminalType type);
/// USEN の終局の符号を読む。不明な符号は Error
TerminalType fromUsen(const QString& code);
bool isUsenCode(const QString& code);

} // namespace TerminalCodes

#endif // TERMINALCODES_H
