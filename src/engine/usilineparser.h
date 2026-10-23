#ifndef USILINEPARSER_H
#define USILINEPARSER_H

/// @file usilineparser.h
/// @brief USI エンジンからの受信行（info・bestmove・checkmate・option・id）を型付きの値にする

#include <QString>
#include <optional>

#include "engineoption.h"
#include "usiinfo.h"
#include "usimessages.h"

/**
 * @brief USI の受信行を解析する関数群（盤面・GUI に依存しない）
 *
 * アプリの中でエンジンの受信行を読む場所はここだけにする。表示用の変換（読み筋の漢字表記など）は
 * この結果を受け取って行う。どの関数も、別の種類の行には nullopt を返す。
 */
namespace UsiLineParser {

/// info 行。"info" だけの行は既定値の UsiInfo
std::optional<UsiInfo> parseInfo(const QString& line);

/**
 * @brief option 行（"option name <name> type <type> [default/min/max/var ...]"）
 *
 * 形式が違えば error に理由を入れて nullopt。種類に合わない属性より後ろは読まない。
 */
std::optional<EngineOption> parseOption(const QString& line, QString* error = nullptr);

/// "id name <name>" の名前。名前がなければ nullopt
std::optional<QString> parseIdName(const QString& line);

/// "id author <author>" の作者。作者がなければ nullopt
std::optional<QString> parseIdAuthor(const QString& line);

/// bestmove 行かどうか（形式が正しくなくても先頭の語が bestmove なら true）
bool isBestMoveLine(const QString& line);

/**
 * @brief bestmove 行（"bestmove <move> [ponder <move>]"）
 *
 * 指し手の語がなければ nullopt（isBestMoveLine で bestmove 行かどうかを見分ける）。
 * 指し手の形式はここでは確かめない。ponder の後の語がなければ ponder は空。
 */
std::optional<UsiBestMove> parseBestMove(const QString& line);

/// checkmate 行（"checkmate <pv...> | nomate | notimplemented | timeout | unknown"。大文字小文字は問わない）
std::optional<UsiCheckmate> parseCheckmate(const QString& line);

} // namespace UsiLineParser

#endif // USILINEPARSER_H
