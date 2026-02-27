#ifndef JKFMOVEPARSER_H
#define JKFMOVEPARSER_H

/// @file jkfmoveparser.h
/// @brief JKF形式固有の指し手変換・初期局面構築・時間解析

#include <QJsonArray>
#include <QJsonObject>
#include <QString>

namespace JkfMoveParser {

// === プリセット・初期局面 ===

/// JKF preset名（"HIRATE","KY"等）→ SFEN文字列
QString presetToSfen(const QString& preset);

/// initial.data から局面SFENを構築（カスタム初期局面用）
QString buildSfenFromInitialData(const QJsonObject& data, int moveNumber = 1);

// === 駒種変換 ===

/// CSA形式駒種文字列（"FU","KY"等）→ 内部コード（1〜14、0=不明）
int pieceKindFromCsa(const QString& kind);

/// 駒種コード → SFEN文字（先手=大文字, 後手=小文字）
QChar pieceToSfenChar(int kind, bool isBlack);

/// CSA形式駒種文字列 → 日本語漢字表記
QString pieceKindToKanji(const QString& kind);

// === 指し手変換 ===

/// JKF move オブジェクトからUSI形式指し手文字列を生成
/// prevToX, prevToY は更新される
QString convertMoveToUsi(const QJsonObject& move, int& prevToX, int& prevToY);

/// JKF move オブジェクトから日本語表記（▲△付き）を生成
/// prevToX, prevToY は更新される
QString convertMoveToPretty(const QJsonObject& move, int plyNumber, int& prevToX, int& prevToY);

// === 時間解析 ===

/// JKF time オブジェクトから時間文字列を生成（mm:ss/HH:MM:SS形式）
/// cumSec は累計消費時間で更新される
QString formatTimeText(const QJsonObject& timeObj, qint64& cumSec);

// === コメント・修飾語 ===

/// JKF move オブジェクトの comments フィールドを改行区切り文字列に変換
QString extractCommentsFromMoveObj(const QJsonObject& moveObj);

/// JKF relative 文字列から修飾語（右/左/上/引/寄/直/打）を生成
QString relativeToModifier(const QString& relative);

/// 終局語を日本語に変換（parsecommon に委譲）
QString specialToJapanese(const QString& special);

} // namespace JkfMoveParser

#endif // JKFMOVEPARSER_H
