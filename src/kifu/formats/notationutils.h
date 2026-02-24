#ifndef NOTATIONUTILS_H
#define NOTATIONUTILS_H

/// @file notationutils.h
/// @brief 棋譜変換器間で共有される座標変換・数字変換・手合マップユーティリティ

#include <QChar>
#include <QString>

namespace NotationUtils {

// === USI 座標変換 ===

/// 段番号(1-9) → USI段文字('a'-'i')。範囲外は QChar() を返す。
QChar rankNumToLetter(int r);

/// USI段文字('a'-'i') → 段番号(1-9)。無効文字は 0 を返す。
int rankLetterToNum(QChar ch);

// === 漢数字変換 ===

/// 漢数字1文字('一'〜'九') → int(1-9)。無効文字は 0 を返す。
int kanjiDigitToInt(QChar c);

// === 全角数字変換（int→全角） ===

/// 整数(1-9) → 全角数字文字列("１"-"９")。範囲外は "？" を返す。
QString intToZenkakuDigit(int d);

// === 漢数字変換（int→漢字） ===

/// 整数(1-9) → 漢数字文字列("一"-"九")。範囲外は "？" を返す。
QString intToKanjiDigit(int d);

// === SFEN 指し手フォーマット ===

/// 盤上移動の USI 文字列を生成 (例: "7g7f", "8b2b+")
QString formatSfenMove(int fromFile, int fromRank, int toFile, int toRank, bool promote);

/// 駒打ちの USI 文字列を生成 (例: "P*5e")
QString formatSfenDrop(QChar piece, int toFile, int toRank);

// === 手合 → 初期 SFEN ===

/// 日本語手合ラベル("平手", "二枚落ち" 等) → 初期 SFEN 文字列
QString mapHandicapToSfen(const QString& label);

} // namespace NotationUtils

#endif // NOTATIONUTILS_H
