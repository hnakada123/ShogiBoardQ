#ifndef USIINFOPARSER_H
#define USIINFOPARSER_H

/// @file usiinfoparser.h
/// @brief USI エンジンからの info 行・option 行・id 行を型付きの値にする

#include <QString>
#include <optional>

#include "usiinfo.h"

/// エンジンが報告したオプションの定義（"option name <name> type <type> ..."）
struct UsiOptionDeclaration {
    QString name;
    QString type;
};

/**
 * @brief USI の受信行を解析する関数群（盤面・GUI に依存しない）
 *
 * アプリの中で info 行を読む場所はここだけにする。表示用の変換（読み筋の漢字表記など）は
 * この結果を受け取って行う。
 */
namespace UsiInfoParser {

/// info 行を解析する。先頭の語が "info" でなければ nullopt。"info" だけの行は既定値の UsiInfo
std::optional<UsiInfo> parse(const QString& line);

/// "option name <name> type <type> ..." を解析する。name と type の両方がなければ nullopt
std::optional<UsiOptionDeclaration> parseOptionDeclaration(const QString& line);

/// "id name <name>" の名前を返す。別の行や名前のない行は nullopt
std::optional<QString> parseIdName(const QString& line);

} // namespace UsiInfoParser

#endif // USIINFOPARSER_H
