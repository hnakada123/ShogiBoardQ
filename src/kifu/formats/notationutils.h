#ifndef NOTATIONUTILS_H
#define NOTATIONUTILS_H

/// @file notationutils.h
/// @brief 棋譜変換器で共有する USI 指し手の組み立てと手合割の名前（数字・駒の表記は ShogiNotation、手合割の表は Handicaps）

#include <QChar>
#include <QString>

namespace NotationUtils {

// === SFEN 指し手フォーマット ===

/// 盤上移動の USI 文字列を生成 (例: "7g7f", "8b2b+")。盤外のマスなら空文字列
QString formatSfenMove(int fromFile, int fromRank, int toFile, int toRank, bool promote);

/// 駒打ちの USI 文字列を生成 (例: "P*5e")。打てない駒・盤外のマスなら空文字列
QString formatSfenDrop(QChar piece, int toFile, int toRank);

// === 手合 → 初期 SFEN ===

/// 日本語手合ラベル("平手", "二枚落ち" 等) → 初期 SFEN 文字列
/// 前後の空白を除いて完全一致で照合し、未知のラベルは平手にフォールバックする。
QString mapHandicapToSfen(const QString& label);
/// 盤面が平手・駒落ちの開始局面と同じなら手合の名前（"平手"、"香落ち" など）、どれでもなければ空文字列を返す
QString handicapLabelForSfen(const QString& sfen);

} // namespace NotationUtils

#endif // NOTATIONUTILS_H
