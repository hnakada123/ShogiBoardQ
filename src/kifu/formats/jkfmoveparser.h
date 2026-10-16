#ifndef JKFMOVEPARSER_H
#define JKFMOVEPARSER_H

/// @file jkfmoveparser.h
/// @brief JKF形式固有の指し手変換・初期局面構築・時間解析

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QStringView>
#include <optional>

#include "kifmovetext.h"
#include "shogitypes.h"
#include "usimove.h"

namespace JkfMoveParser {

// === プリセット・初期局面 ===

/// JKF preset名（"HIRATE","KY"等）→ SFEN文字列
QString presetToSfen(const QString& preset);

/// SFEN文字列 → JKF preset名。盤・手番・持ち駒が手合割の開始局面と同じときだけ返す（空のSFENは平手）。
/// どの手合割でもなければ空文字列（initial.data で局面を書く）
QString presetFromSfen(const QString& sfen);

/// initial.data から局面SFENを構築（カスタム初期局面用）
QString buildSfenFromInitialData(const QJsonObject& data, int moveNumber = 1);

// === 指し手変換 ===

/// JKF move オブジェクトの指し手（to・piece・from・promote）。読めなければ nullopt
std::optional<UsiMove> toUsiMove(const QJsonObject& move);

/// JKF move オブジェクトの、棋譜欄の表記に残す修飾（relative の修飾語と、promote: false の「不成」）
KifMoveText::Notes notesOf(const QJsonObject& move);

// === 時間解析 ===

/// JKF time オブジェクトの消費時間
struct Time {
    qint64 elapsedMs = 0;              ///< now
    std::optional<qint64> totalMs;     ///< total（書いていなければ nullopt。累計は読む側が足す）
};
Time timeOf(const QJsonObject& timeObj);

// === コメント・修飾語 ===

/// JKF move オブジェクトの comments フィールドを改行区切り文字列に変換
QString extractCommentsFromMoveObj(const QJsonObject& moveObj);

/// JKF relative 文字列から修飾語（右/左/上/引/寄/直/打）を生成
QString relativeToModifier(const QString& relative);

/// 修飾語（右/左/上/引/寄/直/打。"左上" のように並べたもの）から JKF relative 文字列（"LU"）を生成
QString modifierToRelative(QStringView modifiers);

} // namespace JkfMoveParser

#endif // JKFMOVEPARSER_H
