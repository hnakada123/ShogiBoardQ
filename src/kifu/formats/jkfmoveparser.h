#ifndef JKFMOVEPARSER_H
#define JKFMOVEPARSER_H

/// @file jkfmoveparser.h
/// @brief JKF形式固有の指し手変換・初期局面構築・時間解析

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QStringView>

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

/// JKF move オブジェクトからUSI形式指し手文字列を生成
/// prevToX, prevToY は更新される
QString convertMoveToUsi(const QJsonObject& move, int& prevToX, int& prevToY);

/// JKF move オブジェクトの指し手の表記（KIF 形式に、relative の修飾語と「不成」を加える）
/// @param previousTo 直前の手の移動先（同じマスなら「同」と書く）
QString convertMoveToPretty(const QJsonObject& move, Turn mover, const UsiMove& usi, BoardSquare previousTo);

// === 時間解析 ===

/// JKF time オブジェクトから時間文字列を生成（mm:ss/HH:MM:SS形式）
/// cumSec は累計消費時間で更新される
QString formatTimeText(const QJsonObject& timeObj, qint64& cumSec);

// === コメント・修飾語 ===

/// JKF move オブジェクトの comments フィールドを改行区切り文字列に変換
QString extractCommentsFromMoveObj(const QJsonObject& moveObj);

/// JKF relative 文字列から修飾語（右/左/上/引/寄/直/打）を生成
QString relativeToModifier(const QString& relative);

/// 修飾語（右/左/上/引/寄/直/打。"左上" のように並べたもの）から JKF relative 文字列（"LU"）を生成
QString modifierToRelative(QStringView modifiers);

} // namespace JkfMoveParser

#endif // JKFMOVEPARSER_H
